// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: GPL-2.0-or-later

use std::{
    ffi::{c_void, CStr},
    io::ErrorKind,
    ptr::addr_of_mut,
    sync::atomic::{fence, Ordering},
};

use bql::prelude::*;
use chardev::prelude::*;
use common::prelude::*;
use hwcore::{prelude::*, IRQState};
use migration::{self, prelude::*};
use qom::prelude::*;
use system::{
    bindings::{address_space_memory, address_space_rw},
    prelude::*,
    MEMTXATTRS_UNSPECIFIED,
};
use util::{self, log::Log, log_mask_ln, prelude::*};

use crate::registers::{self, RegisterOffset, UART_FIFO_DEPTH_WORDS};

// FIFO depth is 256 bytes = 64 words
pub const QUP_FIFO_DEPTH: u32 = UART_FIFO_DEPTH_WORDS;

// FIFOs use 32-bit indices for compatibility with migration stream
#[repr(transparent)]
#[derive(Debug)]
pub struct Fifo([u32; QUP_FIFO_DEPTH as usize]);

impl Default for Fifo {
    fn default() -> Self {
        Self([0u32; QUP_FIFO_DEPTH as usize])
    }
}
impl_vmstate_forward!(Fifo);

impl std::ops::IndexMut<u32> for Fifo {
    fn index_mut(&mut self, idx: u32) -> &mut Self::Output {
        &mut self.0[idx as usize]
    }
}

impl std::ops::Index<u32> for Fifo {
    type Output = u32;

    fn index(&self, idx: u32) -> &Self::Output {
        &self.0[idx as usize]
    }
}

#[repr(C)]
#[derive(Debug, Default)]
pub struct QupGeniUartRegs {
    // Common GENI registers
    pub geni_status: u32,
    pub geni_m_cmd0: u32,
    pub geni_m_cmd_ctrl: u32,
    pub geni_m_irq_status: u32,
    pub geni_m_irq_en: u32,
    pub geni_s_cmd0: u32,
    pub geni_s_cmd_ctrl: u32,
    pub geni_s_irq_status: u32,
    pub geni_s_irq_en: u32,
    pub geni_tx_watermark: u32,
    pub geni_rx_watermark: u32,
    pub geni_rx_rfr_watermark: u32,
    pub geni_m_gp_length: u32,
    pub geni_s_gp_length: u32,
    pub geni_tx_packing_cfg0: u32,
    pub geni_tx_packing_cfg1: u32,
    pub geni_rx_packing_cfg0: u32,
    pub geni_rx_packing_cfg1: u32,

    // UART specific registers
    pub uart_tx_trans_cfg: u32,
    pub uart_rx_trans_cfg: u32,
    pub uart_tx_word_len: u32,
    pub uart_rx_word_len: u32,
    pub uart_tx_stop_bit_len: u32,
    pub uart_rx_stale_cnt: u32,
    pub uart_tx_parity_cfg: u32,
    pub uart_rx_parity_cfg: u32,
    pub uart_loopback_cfg: u32,
    pub uart_io_macro_ctrl: u32,
    pub uart_manual_rfr: u32,
    pub uart_tx_trans_len: u32,

    // FIFOs
    pub tx_fifo: Fifo,
    pub rx_fifo: Fifo,
    pub tx_fifo_len: u32,
    pub rx_fifo_len: u32,
    /// Valid bytes in the final RX FIFO word; 0 means the word is full.
    /// Reported to the guest via RX_LAST/RX_LAST_BYTE_VALID.
    pub rx_last_byte_cnt: u32,
    /// Bytes still owed by the active TX command, from SE_UART_TX_TRANS_LEN.
    pub tx_remaining: u32,

    // DMA registers
    pub dma_tx_ptr_l: u32,
    pub dma_tx_ptr_h: u32,
    pub dma_tx_attr: u32,
    pub dma_tx_len: u32,
    pub dma_tx_sent: u32,
    pub dma_tx_irq_en: u32,
    pub dma_tx_irq_stat: u32,
    pub dma_rx_ptr_l: u32,
    pub dma_rx_ptr_h: u32,
    pub dma_rx_attr: u32,
    pub dma_rx_len: u32,
    pub dma_rx_len_in: u32,
    pub dma_rx_irq_en: u32,
    pub dma_rx_irq_stat: u32,

    // State
    pub tx_enabled: bool,
    pub rx_enabled: bool,
    pub clk_rate: u32,
    pub dma_mode_enabled: bool,
    pub dma_tx_active: bool,

    // Stale timeout support
    pub rx_stale_timeout_active: bool,
    pub dma_rx_active: bool,
}

#[repr(C)]
#[derive(qemu_macros::Object, hwcore::Device)]
/// QUP GENI UART Device Model in QEMU
pub struct QupGeniUartState {
    pub parent_obj: ParentField<SysBusDevice>,
    pub iomem: MemoryRegion,
    #[property(rename = "chardev")]
    pub char_frontend: CharFrontend,
    pub regs: BqlRefCell<QupGeniUartRegs>,
    pub irq: InterruptSource,
    pub tx_watch: BqlCell<u32>,
}

qom_isa!(QupGeniUartState : SysBusDevice, DeviceState, Object);

unsafe impl ObjectType for QupGeniUartState {
    type Class = <SysBusDevice as ObjectType>::Class;
    const TYPE_NAME: &'static CStr = crate::TYPE_QUP_GENI_UART;
}

impl ObjectImpl for QupGeniUartState {
    type ParentType = SysBusDevice;

    const INSTANCE_INIT: Option<unsafe fn(ParentInit<Self>)> = Some(Self::init);
    const INSTANCE_POST_INIT: Option<fn(&Self)> = Some(Self::post_init);
    const CLASS_INIT: fn(&mut Self::Class) = Self::Class::class_init::<Self>;
}

impl DeviceImpl for QupGeniUartState {
    const VMSTATE: Option<VMStateDescription<Self>> = Some(VMSTATE_QUP_GENI_UART);
    const REALIZE: Option<fn(&Self) -> util::Result<()>> = Some(Self::realize);
}

impl ResettablePhasesImpl for QupGeniUartState {
    const HOLD: Option<fn(&Self, ResetType)> = Some(Self::reset_hold);
}

impl SysBusDeviceImpl for QupGeniUartState {}

/// RX_LAST_BYTE_VALID count for a FIFO word carrying `len` bytes. A full word
/// reports zero, which the guest treats as all four bytes valid.
fn last_byte_cnt(len: usize) -> u32 {
    if len < registers::BYTES_PER_FIFO_WORD as usize {
        len as u32
    } else {
        0
    }
}

impl QupGeniUartRegs {
    /// TX watermark is a level condition, not a latched event: it asserts
    /// whenever the FIFO sits at or below the threshold, independent of any
    /// active command. The guest arms a transfer by enabling this interrupt
    /// and waiting for it, so gating it on an active command would deadlock.
    /// A threshold of zero means the guest has disabled the watermark.
    fn m_irq_status_level(&self) -> u32 {
        let mut status = self.geni_m_irq_status;
        if self.geni_tx_watermark > 0 && self.tx_fifo_len <= self.geni_tx_watermark {
            status |= registers::M_TX_FIFO_WATERMARK_EN;
        }
        status
    }

    pub fn read(&mut self, offset: RegisterOffset) -> (bool, u32) {
        use RegisterOffset::*;

        let mut update_irq = false;
        let result = match offset {
            SeGeniStatus => self.geni_status,
            GeniFwRevisionRo => {
                // Return UART protocol in firmware revision
                (registers::GENI_SE_UART << registers::FW_REV_PROTOCOL_SHFT) | 0x0001
            }
            SeGeniMIrqStatus => self.m_irq_status_level(),
            SeGeniMIrqEn => self.geni_m_irq_en,
            SeGeniMCmd0 => self.geni_m_cmd0,
            SeGeniSCmd0 => self.geni_s_cmd0,
            SeGeniTxWatermarkReg => self.geni_tx_watermark,
            SeGeniRxWatermarkReg => self.geni_rx_watermark,
            SeGeniRxRfrWatermarkReg => self.geni_rx_rfr_watermark,
            SeUartTxWordLen => self.uart_tx_word_len,
            SeUartRxWordLen => self.uart_rx_word_len,
            SeUartTxStopBitLen => self.uart_tx_stop_bit_len,
            SeUartRxStaleCnt => self.uart_rx_stale_cnt,
            SeUartTxTransCfg => self.uart_tx_trans_cfg,
            SeUartRxTransCfg => self.uart_rx_trans_cfg,
            SeUartLoopbackCfg => self.uart_loopback_cfg,
            SeUartIoMacroCtrl => self.uart_io_macro_ctrl,
            SeUartManualRfr => self.uart_manual_rfr,
            SeGeniTxPackingCfg0 => self.geni_tx_packing_cfg0,
            SeGeniTxPackingCfg1 => self.geni_tx_packing_cfg1,
            SeGeniRxPackingCfg0 => self.geni_rx_packing_cfg0,
            SeGeniRxPackingCfg1 => self.geni_rx_packing_cfg1,
            SeGeniDmaModeEn => self.dma_mode_enabled as u32,
            SeGeniTxFifoStatus => {
                // Return TX FIFO word count
                self.tx_fifo_len & registers::TX_FIFO_WC
            }
            SeGeniRxFifoStatus => {
                let mut status = self.rx_fifo_len & registers::RX_FIFO_WC_MSK;
                // A short trailing word must be advertised as RX_LAST, otherwise
                // the guest assumes a full word and consumes padding bytes.
                if self.rx_fifo_len > 0 && self.rx_last_byte_cnt > 0 {
                    status |= registers::RX_LAST
                        | (self.rx_last_byte_cnt << registers::RX_LAST_BYTE_VALID_SHFT)
                            & registers::RX_LAST_BYTE_VALID_MSK;
                }
                status
            }
            SeGeniTxFifoN => {
                log_mask_ln!(Log::GuestError, "QUP GENI UART: TX FIFO read not supported");
                0
            }
            SeGeniRxFifoN => {
                if self.rx_fifo_len > 0 {
                    let data = self.rx_fifo[0];
                    // Shift FIFO contents
                    for i in 0..self.rx_fifo_len - 1 {
                        self.rx_fifo[i] = self.rx_fifo[i + 1];
                    }
                    self.rx_fifo_len -= 1;
                    // Watermark is level-triggered; LAST clears once the short
                    // word it referred to has been consumed.
                    if self.rx_fifo_len < self.geni_rx_watermark {
                        self.geni_s_irq_status &= !registers::S_RX_FIFO_WATERMARK_EN;
                    }
                    if self.rx_fifo_len == 0 {
                        self.geni_s_irq_status &= !registers::S_RX_FIFO_LAST_EN;
                        self.rx_last_byte_cnt = 0;
                        self.rx_stale_timeout_active = false;
                    }
                    update_irq = true;
                    data
                } else {
                    0
                }
            }
            SeGeniSIrqStatus => self.geni_s_irq_status,
            SeGeniSIrqEn => self.geni_s_irq_en,
            GeniSerMClkCfg | GeniSerSClkCfg | SeGeniClkSel => 0,
            GeniOutputCtrl | GeniForceDefault => 0,
            GeniIfDisableRo => 0,
            SeGeniIos => 0x3, // TX/RX lines idle high
            SeGeniMGpLength => self.geni_m_gp_length,
            SeGeniSGpLength => self.geni_s_gp_length,
            SeUartTxTransLen => self.uart_tx_trans_len,
            SeUartTxParityCfg => self.uart_tx_parity_cfg,
            SeUartRxParityCfg => self.uart_rx_parity_cfg,
            SeGsiEventEn | SeIrqEn => 0,
            SeHwParam0 => {
                // TX FIFO parameters: width=32, depth=64
                (registers::UART_FIFO_WIDTH_BITS << registers::TX_FIFO_WIDTH_SHFT)
                    | (registers::UART_FIFO_DEPTH_WORDS << registers::TX_FIFO_DEPTH_SHFT)
            }
            SeHwParam1 => {
                // RX FIFO parameters: width=32, depth=64
                (registers::UART_FIFO_WIDTH_BITS << registers::RX_FIFO_WIDTH_SHFT)
                    | (registers::UART_FIFO_DEPTH_WORDS << registers::RX_FIFO_DEPTH_SHFT)
            }
            // DMA register reads
            SeDmaTxPtrL => self.dma_tx_ptr_l,
            SeDmaTxPtrH => self.dma_tx_ptr_h,
            SeDmaTxAttr => self.dma_tx_attr,
            SeDmaTxLen => self.dma_tx_len,
            SeDmaTxIrqEn => self.dma_tx_irq_en,
            SeDmaTxIrqStat => self.dma_tx_irq_stat,
            SeDmaRxPtrL => self.dma_rx_ptr_l,
            SeDmaRxPtrH => self.dma_rx_ptr_h,
            SeDmaRxAttr => self.dma_rx_attr,
            SeDmaRxLen => self.dma_rx_len,
            SeDmaRxLenIn => self.dma_rx_len_in,
            SeDmaRxIrqEn => self.dma_rx_irq_en,
            SeDmaRxIrqStat => self.dma_rx_irq_stat,
            _ => {
                log_mask_ln!(
                    Log::Unimp,
                    "QUP GENI UART: Unimplemented read from offset {:x}",
                    offset as u32
                );
                0
            }
        };
        (update_irq, result)
    }

    pub fn write(
        &mut self,
        offset: RegisterOffset,
        value: u32,
        char_frontend: &CharFrontend,
    ) -> bool {
        use RegisterOffset::*;

        match offset {
            SeGeniMIrqClear => {
                self.geni_m_irq_status &= !value;
                return true;
            }
            SeGeniMIrqEn => {
                self.geni_m_irq_en = value;
                return true;
            }
            SeGeniTxWatermarkReg => {
                self.geni_tx_watermark = value;
                // Changing the threshold re-evaluates the level condition
                return true;
            }
            SeGeniRxWatermarkReg => {
                self.geni_rx_watermark = value;
            }
            SeGeniRxRfrWatermarkReg => {
                self.geni_rx_rfr_watermark = value;
            }
            SeUartTxWordLen => {
                self.uart_tx_word_len = value;
            }
            SeUartRxWordLen => {
                self.uart_rx_word_len = value;
            }
            SeUartTxStopBitLen => {
                self.uart_tx_stop_bit_len = value;
            }
            SeUartRxStaleCnt => {
                self.uart_rx_stale_cnt = value;
                if self.rx_fifo_len > 0 && value > 0 {
                    self.start_rx_stale_timeout();
                }
            }
            SeUartTxTransCfg => {
                self.uart_tx_trans_cfg = value;
            }
            SeUartRxTransCfg => {
                self.uart_rx_trans_cfg = value;
            }
            SeUartLoopbackCfg => {
                self.uart_loopback_cfg = value;
            }
            SeUartIoMacroCtrl => {
                self.uart_io_macro_ctrl = value;
            }
            SeUartManualRfr => {
                self.uart_manual_rfr = value;
            }
            SeGeniTxPackingCfg0 => {
                self.geni_tx_packing_cfg0 = value;
            }
            SeGeniTxPackingCfg1 => {
                self.geni_tx_packing_cfg1 = value;
            }
            SeGeniRxPackingCfg0 => {
                self.geni_rx_packing_cfg0 = value;
            }
            SeGeniRxPackingCfg1 => {
                self.geni_rx_packing_cfg1 = value;
            }
            SeUartTxTransLen => {
                self.uart_tx_trans_len = value;
            }
            GeniSerMClkCfg | GeniSerSClkCfg | SeGeniClkSel => {
                // Clock configuration - accept silently
            }
            GeniOutputCtrl | GeniForceDefault => {
                // I/O control - accept silently
            }
            SeGeniMIrqEnSet => {
                self.geni_m_irq_en |= value;
                return true;
            }
            SeGeniMIrqEnClear => {
                self.geni_m_irq_en &= !value;
                return true;
            }
            SeGeniSIrqStatus => {
                // Read-only, ignore writes
            }
            SeGeniSIrqEn => {
                self.geni_s_irq_en = value;
                return true;
            }
            SeGeniSIrqClear => {
                self.geni_s_irq_status &= !value;
                return true;
            }
            SeGeniSIrqEnSet => {
                self.geni_s_irq_en |= value;
                return true;
            }
            SeGeniSIrqEnClear => {
                self.geni_s_irq_en &= !value;
                return true;
            }
            SeGsiEventEn | SeIrqEn => {
                // Interrupt routing - accept silently
            }
            SeGeniMGpLength => {
                self.geni_m_gp_length = value;
            }
            SeGeniSGpLength => {
                self.geni_s_gp_length = value;
            }
            SeUartTxParityCfg => {
                self.uart_tx_parity_cfg = value;
            }
            SeUartRxParityCfg => {
                self.uart_rx_parity_cfg = value;
            }
            SeGeniCfgSeqStart => {
                if value & 1 != 0 {
                    log_mask_ln!(
                        Log::Unimp,
                        "QUP GENI UART: Configuration sequence start attempted - protocol \
                         switching not supported"
                    );
                }
            }
            SeGeniDmaModeEn => {
                self.dma_mode_enabled = (value & 1) != 0;
            }
            SeGeniMCmd0 => {
                self.geni_m_cmd0 = value;
                let opcode = (value & registers::M_OPCODE_MSK) >> registers::M_OPCODE_SHFT;

                match opcode {
                    registers::UART_START_TX => {
                        self.geni_status |= registers::M_GENI_CMD_ACTIVE;
                        self.tx_enabled = true;
                        self.tx_fifo_len = 0;
                        // The command transfers exactly TX_TRANS_LEN bytes
                        self.tx_remaining = self.uart_tx_trans_len;
                        if self.tx_remaining == 0 {
                            self.geni_status &= !registers::M_GENI_CMD_ACTIVE;
                            self.tx_enabled = false;
                            self.geni_m_irq_status |= registers::M_CMD_DONE_EN;
                        }
                        return true;
                    }
                    registers::UART_ABORT => {
                        self.geni_status &= !registers::M_GENI_CMD_ACTIVE;
                        self.tx_enabled = false;
                        self.dma_tx_active = false;
                        self.tx_fifo_len = 0;
                        self.tx_remaining = 0;
                        self.geni_m_irq_status |= registers::M_CMD_DONE_EN;
                        return true;
                    }
                    registers::SPI_CS_ASSERT
                    | registers::SPI_CS_DEASSERT
                    | registers::SPI_SCK_ONLY => {
                        log_mask_ln!(
                            Log::Unimp,
                            "QUP GENI UART: SPI command opcode 0x{:x} attempted - device only \
                             supports UART mode",
                            opcode
                        );
                    }
                    registers::I2C_WRITE_READ
                    | registers::I2C_ADDR_ONLY
                    | registers::I2C_BUS_CLEAR => {
                        log_mask_ln!(
                            Log::Unimp,
                            "QUP GENI UART: I2C command opcode 0x{:x} attempted - device only \
                             supports UART mode",
                            opcode
                        );
                    }
                    0x7 => {
                        log_mask_ln!(
                            Log::Unimp,
                            "QUP GENI UART: SPI/I2C command opcode 0x{:x} attempted - device only \
                             supports UART mode",
                            opcode
                        );
                    }
                    0x2..=0x6 | 0x8..=0x1F => {
                        log_mask_ln!(
                            Log::Unimp,
                            "QUP GENI UART: Unknown/unsupported command opcode 0x{:x} - device \
                             only supports UART mode",
                            opcode
                        );
                    }
                    _ => {
                        log_mask_ln!(
                            Log::GuestError,
                            "QUP GENI UART: Invalid command opcode 0x{:x}",
                            opcode
                        );
                    }
                }
            }
            SeGeniMCmdCtrlReg => {
                if value & registers::M_GENI_CMD_ABORT != 0 {
                    self.geni_status &= !registers::M_GENI_CMD_ACTIVE;
                    self.tx_enabled = false;
                    self.dma_tx_active = false;
                    self.tx_fifo_len = 0;
                    self.tx_remaining = 0;
                    self.geni_m_irq_status |= registers::M_CMD_ABORT_EN;
                    return true;
                }
                if value & registers::M_GENI_CMD_CANCEL != 0 {
                    self.geni_status &= !registers::M_GENI_CMD_ACTIVE;
                    self.tx_enabled = false;
                    self.dma_tx_active = false;
                    self.tx_fifo_len = 0;
                    self.tx_remaining = 0;
                    self.geni_m_irq_status |= registers::M_CMD_CANCEL_EN;
                    return true;
                }
            }
            SeGeniSCmdCtrlReg => {
                if value & registers::S_GENI_CMD_ABORT != 0 {
                    self.geni_status &= !registers::S_GENI_CMD_ACTIVE;
                    self.rx_enabled = false;
                    self.dma_rx_active = false;
                    self.rx_fifo_len = 0;
                    self.rx_last_byte_cnt = 0;
                    self.geni_s_irq_status &=
                        !(registers::S_RX_FIFO_WATERMARK_EN | registers::S_RX_FIFO_LAST_EN);
                    self.rx_stale_timeout_active = false;
                    self.geni_s_irq_status |= registers::S_CMD_ABORT_EN;
                    return true;
                }
                if value & registers::S_GENI_CMD_CANCEL != 0 {
                    self.geni_status &= !registers::S_GENI_CMD_ACTIVE;
                    self.rx_enabled = false;
                    self.dma_rx_active = false;
                    self.rx_fifo_len = 0;
                    self.rx_last_byte_cnt = 0;
                    self.geni_s_irq_status &=
                        !(registers::S_RX_FIFO_WATERMARK_EN | registers::S_RX_FIFO_LAST_EN);
                    self.rx_stale_timeout_active = false;
                    if self.dma_mode_enabled {
                        self.dma_rx_irq_stat |= registers::DMA_EOT_EN;
                    }
                    self.geni_s_irq_status |= registers::S_CMD_CANCEL_EN;
                    return true;
                }
            }
            SeGeniSCmd0 => {
                self.geni_s_cmd0 = value;
                let opcode = (value & registers::M_OPCODE_MSK) >> registers::M_OPCODE_SHFT;

                match opcode {
                    registers::UART_START_READ => {
                        self.geni_status |= registers::S_GENI_CMD_ACTIVE;
                        self.rx_enabled = true;
                    }
                    registers::UART_ABORT_READ => {
                        self.geni_status &= !registers::S_GENI_CMD_ACTIVE;
                        self.rx_enabled = false;
                        self.dma_rx_active = false;
                        self.rx_fifo_len = 0;
                        self.rx_last_byte_cnt = 0;
                        self.geni_s_irq_status &=
                            !(registers::S_RX_FIFO_WATERMARK_EN | registers::S_RX_FIFO_LAST_EN);
                        self.rx_stale_timeout_active = false;
                        self.geni_s_irq_status |= registers::S_CMD_DONE_EN;
                        return true;
                    }
                    0x3..=0x1F => {
                        log_mask_ln!(
                            Log::Unimp,
                            "QUP GENI UART: Non-UART secondary command opcode 0x{:x} attempted - \
                             device only supports UART mode",
                            opcode
                        );
                    }
                    _ => {
                        log_mask_ln!(
                            Log::GuestError,
                            "QUP GENI UART: Invalid secondary command opcode 0x{:x}",
                            opcode
                        );
                    }
                }
            }
            SeGeniTxFifoN => {
                if self.tx_enabled {
                    // The active command declares its exact byte count, so the
                    // final word is partial rather than NUL-padded.
                    let tx_data = value.to_le_bytes();
                    let len = self.tx_remaining.min(registers::BYTES_PER_FIFO_WORD) as usize;
                    if self.uart_loopback_cfg & 0x1 != 0 {
                        if self.rx_fifo_len < QUP_FIFO_DEPTH {
                            let idx = self.rx_fifo_len;
                            self.rx_fifo[idx] = value;
                            self.rx_fifo_len += 1;
                            self.rx_last_byte_cnt = last_byte_cnt(len);
                        }
                    } else {
                        let _ = char_frontend.write_all(&tx_data[..len]);
                    }
                    self.tx_remaining -= len as u32;
                    if self.tx_remaining == 0 {
                        self.geni_status &= !registers::M_GENI_CMD_ACTIVE;
                        self.tx_enabled = false;
                        self.geni_m_irq_status |= registers::M_CMD_DONE_EN;
                    }
                    return true;
                } else if self.tx_fifo_len < QUP_FIFO_DEPTH {
                    let idx = self.tx_fifo_len;
                    self.tx_fifo[idx] = value;
                    self.tx_fifo_len += 1;
                }
            }
            // DMA register writes
            SeDmaTxPtrL => {
                self.dma_tx_ptr_l = value;
            }
            SeDmaTxPtrH => {
                self.dma_tx_ptr_h = value;
            }
            SeDmaTxAttr => {
                self.dma_tx_attr = value;
            }
            SeDmaTxLen => {
                self.dma_tx_len = value;
                self.dma_tx_sent = 0;
                if self.dma_mode_enabled && value > 0 && self.tx_enabled {
                    self.dma_tx_active = true;
                    self.start_dma_tx_transfer(char_frontend);
                    return true;
                }
            }
            SeDmaTxIrqEn => {
                self.dma_tx_irq_en = value;
                return true;
            }
            SeDmaTxIrqEnSet => {
                self.dma_tx_irq_en |= value;
                return true;
            }
            SeDmaTxIrqEnClr => {
                self.dma_tx_irq_en &= !value;
                return true;
            }
            SeDmaTxIrqClr => {
                self.dma_tx_irq_stat &= !value;
                return true;
            }
            SeDmaTxFsmRst => {
                if value & 1 != 0 {
                    self.dma_tx_active = false;
                    self.dma_tx_irq_stat |= 1 << 3;
                    return true;
                }
            }
            SeDmaRxPtrL => {
                self.dma_rx_ptr_l = value;
            }
            SeDmaRxPtrH => {
                self.dma_rx_ptr_h = value;
            }
            SeDmaRxAttr => {
                self.dma_rx_attr = value;
            }
            SeDmaRxLen => {
                self.dma_rx_len = value;
                self.dma_rx_len_in = 0;
                if self.dma_mode_enabled && value > 0 {
                    self.dma_rx_active = true;
                    if self.dma_tx_active && self.uart_loopback_cfg & 1 != 0 {
                        self.start_dma_tx_transfer(char_frontend);
                        return true;
                    }
                }
            }
            SeDmaRxFsmRst => {
                if value & 1 != 0 {
                    self.dma_rx_active = false;
                    self.dma_rx_irq_stat |= 1 << 3;
                    return true;
                }
            }
            SeDmaRxIrqEn => {
                self.dma_rx_irq_en = value;
                return true;
            }
            SeDmaRxIrqEnSet => {
                self.dma_rx_irq_en |= value;
                return true;
            }
            SeDmaRxIrqEnClr => {
                self.dma_rx_irq_en &= !value;
                return true;
            }
            SeDmaRxIrqClr => {
                self.dma_rx_irq_stat &= !value;
                return true;
            }
            _ => {
                log_mask_ln!(
                    Log::Unimp,
                    "QUP GENI UART: Unimplemented write to offset {:x} value {:x}",
                    offset as u32,
                    value
                );
            }
        }
        false
    }

    fn start_dma_tx_transfer(&mut self, char_frontend: &CharFrontend) {
        let addr = (u64::from(self.dma_tx_ptr_h) << 32) | u64::from(self.dma_tx_ptr_l);
        let len = self.dma_tx_len.min(self.uart_tx_trans_len);
        let mut buf = [0u8; 4096];
        while self.dma_tx_active && self.dma_tx_sent < len {
            let mut count = (len - self.dma_tx_sent).min(buf.len() as u32) as usize;
            let loopback = self.uart_loopback_cfg & 1 != 0;
            if loopback && self.rx_enabled {
                if !self.dma_rx_active {
                    return;
                }
                count = count.min(self.dma_rx_len as usize);
            }
            let Some(addr) = addr.checked_add(u64::from(self.dma_tx_sent)) else {
                self.dma_tx_irq_stat |= registers::DMA_AHB_ERR_EN;
                self.dma_tx_active = false;
                return;
            };
            // SAFETY: the buffer is writable for count bytes and the system
            // address space is initialized before the device is realized.
            fence(Ordering::SeqCst);
            let result = unsafe {
                address_space_rw(
                    addr_of_mut!(address_space_memory),
                    addr,
                    MEMTXATTRS_UNSPECIFIED,
                    buf.as_mut_ptr().cast(),
                    count as u64,
                    false,
                )
            };
            if result != 0 {
                self.dma_tx_irq_stat |= registers::DMA_AHB_ERR_EN;
                self.dma_tx_active = false;
                return;
            }
            let sent = if loopback {
                if self.rx_enabled {
                    self.handle_dma_rx_transfer(&buf[..count]);
                }
                count
            } else {
                match char_frontend.write(&buf[..count]) {
                    Ok(0) => count,
                    Ok(sent) => sent,
                    Err(err) if err.kind() == ErrorKind::WouldBlock => return,
                    Err(_) => count,
                }
            };
            self.dma_tx_sent += sent as u32;
            self.tx_remaining = self.tx_remaining.saturating_sub(sent as u32);
        }
        if self.dma_tx_active {
            self.dma_tx_active = false;
            self.dma_tx_irq_stat |= registers::DMA_DONE_EN;
            if self.tx_remaining == 0 {
                self.tx_enabled = false;
                self.geni_status &= !registers::M_GENI_CMD_ACTIVE;
                self.geni_m_irq_status |= registers::M_CMD_DONE_EN;
            }
        }
    }

    fn handle_dma_rx_transfer(&mut self, data: &[u8]) {
        let addr = (u64::from(self.dma_rx_ptr_h) << 32) | u64::from(self.dma_rx_ptr_l);
        // SAFETY: data is readable for its length and the system address space
        // is initialized before the device is realized.
        fence(Ordering::SeqCst);
        let result = unsafe {
            address_space_rw(
                addr_of_mut!(address_space_memory),
                addr,
                MEMTXATTRS_UNSPECIFIED,
                data.as_ptr().cast_mut().cast(),
                data.len() as u64,
                true,
            )
        };
        self.dma_rx_active = false;
        if result != 0 {
            self.dma_rx_irq_stat |= registers::DMA_AHB_ERR_EN;
            return;
        }
        self.dma_rx_len_in = data.len() as u32;
        self.dma_rx_irq_stat |= registers::DMA_DONE_EN;
    }

    pub fn reset(&mut self) {
        self.geni_status = 0;
        self.geni_m_cmd0 = 0;
        self.geni_m_cmd_ctrl = 0;
        self.geni_m_irq_status = 0;
        self.geni_m_irq_en = 0;
        self.geni_s_cmd0 = 0;
        self.geni_s_cmd_ctrl = 0;
        self.geni_s_irq_status = 0;
        self.geni_s_irq_en = 0;
        self.geni_tx_watermark = registers::DEF_TX_WM;
        self.geni_rx_watermark = registers::UART_RX_WM;
        self.geni_rx_rfr_watermark = registers::UART_RX_WM;
        self.geni_m_gp_length = 0;
        self.geni_s_gp_length = 0;
        self.geni_tx_packing_cfg0 = 0;
        self.geni_tx_packing_cfg1 = 0;
        self.geni_rx_packing_cfg0 = 0;
        self.geni_rx_packing_cfg1 = 0;

        self.uart_tx_trans_cfg = 0;
        self.uart_rx_trans_cfg = 0;
        self.uart_tx_word_len = 0;
        self.uart_rx_word_len = 0;
        self.uart_tx_stop_bit_len = 0;
        self.uart_rx_stale_cnt = registers::STALE_TIMEOUT;
        self.uart_tx_parity_cfg = 0;
        self.uart_rx_parity_cfg = 0;
        self.uart_loopback_cfg = 0;
        self.uart_io_macro_ctrl = 0;
        self.uart_manual_rfr = 0;
        self.uart_tx_trans_len = 0;

        self.tx_fifo_len = 0;
        self.rx_fifo_len = 0;
        self.rx_last_byte_cnt = 0;
        self.tx_remaining = 0;
        self.tx_enabled = false;
        self.rx_enabled = false;
        self.clk_rate = 19200000;
        self.dma_mode_enabled = false;

        self.dma_tx_ptr_l = 0;
        self.dma_tx_ptr_h = 0;
        self.dma_tx_attr = 0;
        self.dma_tx_len = 0;
        self.dma_tx_sent = 0;
        self.dma_tx_irq_en = 0;
        self.dma_tx_irq_stat = 0;
        self.dma_rx_ptr_l = 0;
        self.dma_rx_ptr_h = 0;
        self.dma_rx_attr = 0;
        self.dma_rx_len = 0;
        self.dma_rx_len_in = 0;
        self.dma_rx_irq_en = 0;
        self.dma_rx_irq_stat = 0;
        self.dma_tx_active = false;
        self.dma_rx_active = false;

        self.rx_stale_timeout_active = false;
    }

    fn start_rx_stale_timeout(&mut self) {
        if self.uart_rx_stale_cnt > 0 && !self.rx_stale_timeout_active {
            self.rx_stale_timeout_active = true;
        }
    }

    #[allow(dead_code)]
    fn handle_rx_stale_timeout(&mut self) -> bool {
        if self.rx_stale_timeout_active && self.rx_fifo_len > 0 {
            self.geni_m_irq_status |= registers::M_RX_FIFO_LAST_EN;
            self.rx_stale_timeout_active = false;
            return true;
        }
        false
    }
}

impl QupGeniUartState {
    /// # Safety
    ///
    /// `self` must point to a correctly sized and aligned location for the
    /// `QupGeniUartState` type. It must not be called more than once on the
    /// same location/instance. All its fields are expected to hold
    /// uninitialized values with the sole exception of `parent_obj`.
    unsafe fn init(mut this: ParentInit<Self>) {
        static QUP_GENI_UART_OPS: MemoryRegionOps<QupGeniUartState> =
            MemoryRegionOpsBuilder::<QupGeniUartState>::new()
                .read(&QupGeniUartState::read)
                .write(&QupGeniUartState::write)
                .little_endian()
                .impl_sizes(4, 4)
                .build();

        // SAFETY: this and this.iomem are guaranteed to be valid at this point
        MemoryRegion::init_io(
            &mut uninit_field_mut!(*this, iomem),
            &QUP_GENI_UART_OPS,
            "qup-geni-uart",
            0x6000,
        );

        uninit_field_mut!(*this, regs).write(Default::default());
        uninit_field_mut!(*this, tx_watch).write(Default::default());
    }

    fn post_init(&self) {
        self.init_mmio(&self.iomem);
        self.init_irq(&self.irq);
    }

    fn read(&self, offset: hwaddr, _size: u32) -> u64 {
        match RegisterOffset::try_from(offset) {
            Ok(reg) => {
                let (update_irq, result) = self.regs.borrow_mut().read(reg);
                if update_irq {
                    self.update_irq();
                    self.char_frontend.accept_input();
                }
                result.into()
            }
            Err(_) => {
                log_mask_ln!(
                    Log::GuestError,
                    "QupGeniUartState::read: Bad offset {:x}",
                    offset
                );
                0
            }
        }
    }

    fn write(&self, offset: hwaddr, value: u64, _size: u32) {
        if let Ok(reg) = RegisterOffset::try_from(offset) {
            let update_irq = self
                .regs
                .borrow_mut()
                .write(reg, value as u32, &self.char_frontend);
            if update_irq {
                self.update_irq();
            }
            self.update_tx_watch();
            if matches!(
                reg,
                RegisterOffset::SeGeniSCmd0 | RegisterOffset::SeDmaRxLen
            ) {
                self.char_frontend.accept_input();
            }
        } else {
            log_mask_ln!(
                Log::GuestError,
                "QupGeniUartState::write: Bad offset {:x} value {:x}",
                offset,
                value
            );
        }
    }

    fn can_receive(&self) -> u32 {
        let regs = self.regs.borrow();
        if regs.uart_loopback_cfg & 1 != 0 {
            0
        } else if regs.rx_enabled && regs.dma_mode_enabled {
            if regs.dma_rx_active {
                regs.dma_rx_len.min(4096)
            } else {
                0
            }
        } else if regs.rx_enabled {
            QUP_FIFO_DEPTH - regs.rx_fifo_len
        } else {
            0
        }
    }

    fn receive(&self, buf: &[u8]) {
        let mut regs = self.regs.borrow_mut();
        if !regs.rx_enabled || (regs.uart_loopback_cfg & 0x1 != 0) {
            return;
        }

        if regs.dma_mode_enabled {
            if regs.dma_rx_active {
                regs.handle_dma_rx_transfer(buf);
            }
            drop(regs);
            self.update_irq();
            return;
        }

        let mut update_irq = false;

        // Pack bytes into 32-bit words and add to RX FIFO
        for chunk in buf.chunks(registers::BYTES_PER_FIFO_WORD as usize) {
            if regs.rx_fifo_len >= QUP_FIFO_DEPTH {
                regs.geni_s_irq_status |= registers::S_RX_FIFO_WR_ERR_EN;
                update_irq = true;
                break;
            }

            let mut word = 0u32;
            for (i, &byte) in chunk.iter().enumerate() {
                word |= (byte as u32) << (i * 8);
            }
            let idx = regs.rx_fifo_len;
            regs.rx_fifo[idx] = word;
            regs.rx_fifo_len += 1;
            regs.rx_last_byte_cnt = last_byte_cnt(chunk.len());

            if regs.rx_fifo_len >= regs.geni_rx_watermark {
                regs.geni_s_irq_status |= registers::S_RX_FIFO_WATERMARK_EN;
                update_irq = true;
            }
        }

        // A short trailing word ends the burst, which real hardware reports as
        // a stale/LAST condition. Without it the guest sits below the watermark
        // and never drains the tail of the input.
        if regs.rx_last_byte_cnt > 0 {
            regs.geni_s_irq_status |= registers::S_RX_FIFO_LAST_EN;
            regs.rx_stale_timeout_active = false;
            update_irq = true;
        }

        drop(regs);
        if update_irq {
            self.update_irq();
        }
    }

    fn event(&self, _event: Event) {}

    fn realize(&self) -> util::Result<()> {
        self.char_frontend
            .enable_handlers(self, Self::can_receive, Self::receive, Self::event);
        Ok(())
    }

    fn reset_hold(&self, _type: ResetType) {
        self.clear_tx_watch();
        self.regs.borrow_mut().reset();
        self.update_irq();
    }

    fn clear_tx_watch(&self) {
        let tag = self.tx_watch.replace(0);
        CharFrontend::remove_watch(tag);
    }

    fn update_tx_watch(&self) {
        if !self.regs.borrow().dma_tx_active || self.regs.borrow().uart_loopback_cfg & 1 != 0 {
            self.clear_tx_watch();
        } else if self.tx_watch.get() == 0 {
            // SAFETY: the source is removed on reset and before object drop.
            let tag = unsafe {
                self.char_frontend.add_write_watch(
                    Some(Self::tx_ready),
                    (self as *const Self).cast_mut().cast(),
                )
            };
            self.tx_watch.set(tag);
            if tag == 0 {
                let mut regs = self.regs.borrow_mut();
                let pending = regs
                    .dma_tx_len
                    .min(regs.uart_tx_trans_len)
                    .saturating_sub(regs.dma_tx_sent);
                regs.tx_remaining = regs.tx_remaining.saturating_sub(pending);
                regs.dma_tx_active = false;
                regs.dma_tx_irq_stat |= registers::DMA_DONE_EN;
                if regs.tx_remaining == 0 {
                    regs.tx_enabled = false;
                    regs.geni_status &= !registers::M_GENI_CMD_ACTIVE;
                    regs.geni_m_irq_status |= registers::M_CMD_DONE_EN;
                }
                drop(regs);
                self.update_irq();
            }
        }
    }

    unsafe extern "C" fn tx_ready(
        _unused: *mut c_void,
        _condition: u32,
        opaque: *mut c_void,
    ) -> i32 {
        // SAFETY: the watch holds a pointer to this live QOM object.
        let this = unsafe { &*opaque.cast::<Self>() };
        this.tx_watch.set(0);
        this.regs
            .borrow_mut()
            .start_dma_tx_transfer(&this.char_frontend);
        this.update_irq();
        this.update_tx_watch();
        0
    }

    fn post_load(&self, _version: u8) -> Result<(), migration::InvalidError> {
        self.update_irq();
        self.update_tx_watch();
        Ok(())
    }

    fn update_irq(&self) {
        let regs = self.regs.borrow();
        let irq_active = (regs.m_irq_status_level() & regs.geni_m_irq_en) != 0
            || (regs.geni_s_irq_status & regs.geni_s_irq_en) != 0
            || (regs.dma_tx_irq_stat & regs.dma_tx_irq_en) != 0
            || (regs.dma_rx_irq_stat & regs.dma_rx_irq_en) != 0;
        self.irq.set(irq_active);
    }
}

impl Drop for QupGeniUartState {
    fn drop(&mut self) {
        self.clear_tx_watch();
    }
}

/// # Safety
///
/// We expect the FFI user of this function to pass a valid pointer for `chr`
/// and `irq`.
#[no_mangle]
pub unsafe extern "C" fn qup_geni_uart_create(
    addr: u64,
    irq: *mut IRQState,
    chr: *mut Chardev,
) -> *mut DeviceState {
    // SAFETY: The callers promise that they have owned references.
    let irq = unsafe { Owned::<IRQState>::from(&*irq) };

    let dev = QupGeniUartState::new();
    if !chr.is_null() {
        let chr = unsafe { Owned::<Chardev>::from(&*chr) };
        dev.prop_set_chr("chardev", &chr);
    }
    dev.sysbus_realize().unwrap_fatal();
    dev.mmio_map(0, addr);
    dev.connect_irq(0, &irq);

    dev.as_mut_ptr()
}

// VMState implementation for QupGeniUartRegs
impl_vmstate_struct!(
    QupGeniUartRegs,
    VMStateDescriptionBuilder::<QupGeniUartRegs>::new()
        .name(c"qup_geni_uart/regs")
        .version_id(3)
        .minimum_version_id(3)
        .fields(vmstate_fields! {
        vmstate_of!(QupGeniUartRegs, geni_status),
        vmstate_of!(QupGeniUartRegs, geni_m_cmd0),
        vmstate_of!(QupGeniUartRegs, geni_m_cmd_ctrl),
        vmstate_of!(QupGeniUartRegs, geni_m_irq_status),
        vmstate_of!(QupGeniUartRegs, geni_m_irq_en),
        vmstate_of!(QupGeniUartRegs, geni_s_cmd0),
        vmstate_of!(QupGeniUartRegs, geni_s_cmd_ctrl),
        vmstate_of!(QupGeniUartRegs, geni_s_irq_status),
        vmstate_of!(QupGeniUartRegs, geni_s_irq_en),
        vmstate_of!(QupGeniUartRegs, geni_tx_watermark),
        vmstate_of!(QupGeniUartRegs, geni_rx_watermark),
        vmstate_of!(QupGeniUartRegs, geni_rx_rfr_watermark),
        vmstate_of!(QupGeniUartRegs, geni_m_gp_length),
        vmstate_of!(QupGeniUartRegs, geni_s_gp_length),
        vmstate_of!(QupGeniUartRegs, geni_tx_packing_cfg0),
        vmstate_of!(QupGeniUartRegs, geni_tx_packing_cfg1),
        vmstate_of!(QupGeniUartRegs, geni_rx_packing_cfg0),
        vmstate_of!(QupGeniUartRegs, geni_rx_packing_cfg1),
        vmstate_of!(QupGeniUartRegs, uart_tx_trans_cfg),
        vmstate_of!(QupGeniUartRegs, uart_rx_trans_cfg),
        vmstate_of!(QupGeniUartRegs, uart_tx_word_len),
        vmstate_of!(QupGeniUartRegs, uart_rx_word_len),
        vmstate_of!(QupGeniUartRegs, uart_tx_stop_bit_len),
        vmstate_of!(QupGeniUartRegs, uart_rx_stale_cnt),
        vmstate_of!(QupGeniUartRegs, uart_tx_parity_cfg),
        vmstate_of!(QupGeniUartRegs, uart_rx_parity_cfg),
        vmstate_of!(QupGeniUartRegs, uart_loopback_cfg),
        vmstate_of!(QupGeniUartRegs, uart_io_macro_ctrl),
        vmstate_of!(QupGeniUartRegs, uart_manual_rfr),
        vmstate_of!(QupGeniUartRegs, uart_tx_trans_len),
        vmstate_of!(QupGeniUartRegs, tx_fifo),
        vmstate_of!(QupGeniUartRegs, rx_fifo),
        vmstate_of!(QupGeniUartRegs, tx_fifo_len),
        vmstate_of!(QupGeniUartRegs, rx_fifo_len),
        vmstate_of!(QupGeniUartRegs, rx_last_byte_cnt),
        vmstate_of!(QupGeniUartRegs, tx_remaining),
        vmstate_of!(QupGeniUartRegs, tx_enabled),
        vmstate_of!(QupGeniUartRegs, rx_enabled),
        vmstate_of!(QupGeniUartRegs, clk_rate),
        vmstate_of!(QupGeniUartRegs, dma_mode_enabled),
        vmstate_of!(QupGeniUartRegs, dma_tx_ptr_l),
        vmstate_of!(QupGeniUartRegs, dma_tx_ptr_h),
        vmstate_of!(QupGeniUartRegs, dma_tx_attr),
        vmstate_of!(QupGeniUartRegs, dma_tx_len),
        vmstate_of!(QupGeniUartRegs, dma_tx_sent),
        vmstate_of!(QupGeniUartRegs, dma_tx_irq_en),
        vmstate_of!(QupGeniUartRegs, dma_tx_irq_stat),
        vmstate_of!(QupGeniUartRegs, dma_rx_ptr_l),
        vmstate_of!(QupGeniUartRegs, dma_rx_ptr_h),
        vmstate_of!(QupGeniUartRegs, dma_rx_attr),
        vmstate_of!(QupGeniUartRegs, dma_rx_len),
        vmstate_of!(QupGeniUartRegs, dma_rx_len_in),
        vmstate_of!(QupGeniUartRegs, dma_rx_irq_en),
        vmstate_of!(QupGeniUartRegs, dma_rx_irq_stat),
        vmstate_of!(QupGeniUartRegs, dma_tx_active),
        vmstate_of!(QupGeniUartRegs, dma_rx_active),
        vmstate_of!(QupGeniUartRegs, rx_stale_timeout_active),
        })
        .build()
);

// VMState implementation for QupGeniUartState
pub const VMSTATE_QUP_GENI_UART: VMStateDescription<QupGeniUartState> =
    VMStateDescriptionBuilder::<QupGeniUartState>::new()
        .name(c"qup_geni_uart")
        .post_load(&QupGeniUartState::post_load)
        .version_id(3)
        .minimum_version_id(3)
        .fields(vmstate_fields! {
            vmstate_of!(QupGeniUartState, regs),
        })
        .build();
