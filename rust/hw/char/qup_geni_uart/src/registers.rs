// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: GPL-2.0-or-later

#![allow(dead_code)]

//! Register definitions for QUP GENI UART
//!
//! This module contains the register layout and bit field definitions
//! for the Qualcomm QUP GENI UART device.

pub const fn genmask(h: u32, l: u32) -> u32 {
    ((!0u32) - (1u32 << l) + 1) & (!0u32 >> (32 - 1 - h))
}

// Hardware version for QUP >= 3.10
pub const QUP_HW_VER_REG_VAL: u32 = 0x30100000;

// Register offsets
#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RegisterOffset {
    // Common SE registers
    GeniForceDefault = 0x020,
    GeniOutputCtrl = 0x024,
    SeGeniStatus = 0x040,
    GeniSerMClkCfg = 0x048,
    GeniSerSClkCfg = 0x04c,
    GeniIfDisableRo = 0x064,
    GeniFwRevisionRo = 0x068,
    SeGeniClkSel = 0x07c,
    SeGeniCfgSeqStart = 0x084,
    SeGeniDmaModeEn = 0x258,
    SeGeniTxPackingCfg0 = 0x260,
    SeGeniTxPackingCfg1 = 0x264,
    SeGeniRxPackingCfg0 = 0x284,
    SeGeniRxPackingCfg1 = 0x288,
    SeGeniMCmd0 = 0x600,
    SeGeniMCmdCtrlReg = 0x604,
    SeGeniMIrqStatus = 0x610,
    SeGeniMIrqEn = 0x614,
    SeGeniMIrqClear = 0x618,
    SeGeniMIrqEnSet = 0x61c,
    SeGeniMIrqEnClear = 0x620,
    SeGeniSCmd0 = 0x630,
    SeGeniSCmdCtrlReg = 0x634,
    SeGeniSIrqStatus = 0x640,
    SeGeniSIrqEn = 0x644,
    SeGeniSIrqClear = 0x648,
    SeGeniSIrqEnSet = 0x64c,
    SeGeniSIrqEnClear = 0x650,
    SeGeniTxFifoN = 0x700,
    SeGeniRxFifoN = 0x780,
    SeGeniTxFifoStatus = 0x800,
    SeGeniRxFifoStatus = 0x804,
    SeGeniTxWatermarkReg = 0x80c,
    SeGeniRxWatermarkReg = 0x810,
    SeGeniRxRfrWatermarkReg = 0x814,
    SeGeniIos = 0x908,
    SeGeniMGpLength = 0x910,
    SeGeniSGpLength = 0x914,
    SeGsiEventEn = 0xe18,
    SeIrqEn = 0xe1c,
    SeHwParam0 = 0xe24,
    SeHwParam1 = 0xe28,

    // DMA registers
    SeDmaTxPtrL = 0xc30,
    SeDmaTxPtrH = 0xc34,
    SeDmaTxAttr = 0xc38,
    SeDmaTxLen = 0xc3c,
    SeDmaTxIrqEn = 0xc48,
    SeDmaTxIrqEnSet = 0xc4c,
    SeDmaTxIrqEnClr = 0xc50,
    SeDmaTxIrqStat = 0xc40,
    SeDmaTxIrqClr = 0xc44,
    SeDmaRxPtrL = 0xd30,
    SeDmaRxPtrH = 0xd34,
    SeDmaRxAttr = 0xd38,
    SeDmaRxLen = 0xd3c,
    SeDmaRxIrqEn = 0xd48,
    SeDmaRxIrqEnSet = 0xd4c,
    SeDmaRxIrqEnClr = 0xd50,
    SeDmaRxIrqStat = 0xd40,
    SeDmaRxIrqClr = 0xd44,

    // UART specific registers
    SeUartLoopbackCfg = 0x22c,
    SeUartIoMacroCtrl = 0x240,
    SeUartTxTransCfg = 0x25c,
    SeUartTxWordLen = 0x268,
    SeUartTxStopBitLen = 0x26c,
    SeUartTxTransLen = 0x270,
    SeUartRxTransCfg = 0x280,
    SeUartRxWordLen = 0x28c,
    SeUartRxStaleCnt = 0x294,
    SeUartTxParityCfg = 0x2a4,
    SeUartRxParityCfg = 0x2a8,
    SeUartManualRfr = 0x2ac,
}

impl TryFrom<u64> for RegisterOffset {
    type Error = u64;

    fn try_from(offset: u64) -> Result<Self, Self::Error> {
        use RegisterOffset::*;
        match offset as u32 {
            0x020 => Ok(GeniForceDefault),
            0x024 => Ok(GeniOutputCtrl),
            0x040 => Ok(SeGeniStatus),
            0x048 => Ok(GeniSerMClkCfg),
            0x04c => Ok(GeniSerSClkCfg),
            0x064 => Ok(GeniIfDisableRo),
            0x068 => Ok(GeniFwRevisionRo),
            0x07c => Ok(SeGeniClkSel),
            0x084 => Ok(SeGeniCfgSeqStart),
            0x258 => Ok(SeGeniDmaModeEn),
            0x260 => Ok(SeGeniTxPackingCfg0),
            0x264 => Ok(SeGeniTxPackingCfg1),
            0x284 => Ok(SeGeniRxPackingCfg0),
            0x288 => Ok(SeGeniRxPackingCfg1),
            0x600 => Ok(SeGeniMCmd0),
            0x604 => Ok(SeGeniMCmdCtrlReg),
            0x610 => Ok(SeGeniMIrqStatus),
            0x614 => Ok(SeGeniMIrqEn),
            0x618 => Ok(SeGeniMIrqClear),
            0x61c => Ok(SeGeniMIrqEnSet),
            0x620 => Ok(SeGeniMIrqEnClear),
            0x630 => Ok(SeGeniSCmd0),
            0x634 => Ok(SeGeniSCmdCtrlReg),
            0x640 => Ok(SeGeniSIrqStatus),
            0x644 => Ok(SeGeniSIrqEn),
            0x648 => Ok(SeGeniSIrqClear),
            0x64c => Ok(SeGeniSIrqEnSet),
            0x650 => Ok(SeGeniSIrqEnClear),
            0x700 => Ok(SeGeniTxFifoN),
            0x780 => Ok(SeGeniRxFifoN),
            0x800 => Ok(SeGeniTxFifoStatus),
            0x804 => Ok(SeGeniRxFifoStatus),
            0x80c => Ok(SeGeniTxWatermarkReg),
            0x810 => Ok(SeGeniRxWatermarkReg),
            0x814 => Ok(SeGeniRxRfrWatermarkReg),
            0x908 => Ok(SeGeniIos),
            0x910 => Ok(SeGeniMGpLength),
            0x914 => Ok(SeGeniSGpLength),
            0xe18 => Ok(SeGsiEventEn),
            0xe1c => Ok(SeIrqEn),
            0xe24 => Ok(SeHwParam0),
            0xe28 => Ok(SeHwParam1),
            0x22c => Ok(SeUartLoopbackCfg),
            0x240 => Ok(SeUartIoMacroCtrl),
            0x25c => Ok(SeUartTxTransCfg),
            0x268 => Ok(SeUartTxWordLen),
            0x26c => Ok(SeUartTxStopBitLen),
            0x270 => Ok(SeUartTxTransLen),
            0x280 => Ok(SeUartRxTransCfg),
            0x28c => Ok(SeUartRxWordLen),
            0x294 => Ok(SeUartRxStaleCnt),
            0x2a4 => Ok(SeUartTxParityCfg),
            0x2a8 => Ok(SeUartRxParityCfg),
            0x2ac => Ok(SeUartManualRfr),
            // DMA registers
            0xc30 => Ok(SeDmaTxPtrL),
            0xc34 => Ok(SeDmaTxPtrH),
            0xc38 => Ok(SeDmaTxAttr),
            0xc3c => Ok(SeDmaTxLen),
            0xc40 => Ok(SeDmaTxIrqStat),
            0xc44 => Ok(SeDmaTxIrqClr),
            0xc48 => Ok(SeDmaTxIrqEn),
            0xc4c => Ok(SeDmaTxIrqEnSet),
            0xc50 => Ok(SeDmaTxIrqEnClr),
            0xd30 => Ok(SeDmaRxPtrL),
            0xd34 => Ok(SeDmaRxPtrH),
            0xd38 => Ok(SeDmaRxAttr),
            0xd3c => Ok(SeDmaRxLen),
            0xd40 => Ok(SeDmaRxIrqStat),
            0xd44 => Ok(SeDmaRxIrqClr),
            0xd48 => Ok(SeDmaRxIrqEn),
            0xd4c => Ok(SeDmaRxIrqEnSet),
            0xd50 => Ok(SeDmaRxIrqEnClr),
            _ => Err(offset),
        }
    }
}

// Status register fields
pub const M_GENI_CMD_ACTIVE: u32 = 1 << 0;
pub const S_GENI_CMD_ACTIVE: u32 = 1 << 12;

// GENI_M_CMD_CTRL_REG fields
pub const M_GENI_DISABLE: u32 = 1 << 0;
pub const M_GENI_CMD_ABORT: u32 = 1 << 1;
pub const M_GENI_CMD_CANCEL: u32 = 1 << 2;

// GENI_S_CMD_CTRL_REG fields
pub const S_GENI_DISABLE: u32 = 1 << 0;
pub const S_GENI_CMD_ABORT: u32 = 1 << 1;
pub const S_GENI_CMD_CANCEL: u32 = 1 << 2;

// FW Revision fields
pub const FW_REV_PROTOCOL_MSK: u32 = genmask(15, 8);
pub const FW_REV_PROTOCOL_SHFT: u32 = 8;

// Protocol constants (from Linux kernel enum geni_se_protocol_type)
pub const GENI_SE_NONE: u32 = 0;
pub const GENI_SE_SPI: u32 = 1;
pub const GENI_SE_UART: u32 = 2;
pub const GENI_SE_I2C: u32 = 3;
pub const GENI_SE_I3C: u32 = 4;
pub const GENI_SE_SPI_SLAVE: u32 = 5;

// M_CMD0 fields
pub const M_OPCODE_MSK: u32 = genmask(31, 27);
pub const M_OPCODE_SHFT: u32 = 27;
pub const M_PARAMS_MSK: u32 = genmask(26, 0);

// UART M_CMD OP codes
pub const UART_START_TX: u32 = 0x1;
pub const UART_ABORT: u32 = 0x2;

// UART S_CMD OP codes
pub const UART_START_READ: u32 = 0x1;
pub const UART_ABORT_READ: u32 = 0x2;
pub const UART_PARAM: u32 = 0x1;
pub const UART_PARAM_RFR_OPEN: u32 = 1 << 7;

// Non-UART protocol command opcodes (would indicate protocol switching attempts)
// SPI command opcodes
pub const SPI_TX_ONLY: u32 = 0x1;
pub const SPI_RX_ONLY: u32 = 0x2;
pub const SPI_TX_RX: u32 = 0x7;
pub const SPI_CS_ASSERT: u32 = 0x8;
pub const SPI_CS_DEASSERT: u32 = 0x9;
pub const SPI_SCK_ONLY: u32 = 0xa;

// I2C command opcodes
pub const I2C_WRITE: u32 = 0x1;
pub const I2C_READ: u32 = 0x2;
pub const I2C_WRITE_READ: u32 = 0x3;
pub const I2C_ADDR_ONLY: u32 = 0x4;
pub const I2C_BUS_CLEAR: u32 = 0x6;
pub const I2C_STOP_ON_BUS: u32 = 0x7;

// TX configuration
pub const UART_TX_PAR_EN: u32 = 1 << 0;
pub const UART_CTS_MASK: u32 = 1 << 1;

// Stop bit length
pub const TX_STOP_BIT_LEN_1: u32 = 0;
pub const TX_STOP_BIT_LEN_2: u32 = 2;

// RX configuration
pub const UART_RX_PAR_EN: u32 = 1 << 3;

// RX word length
pub const RX_WORD_LEN_MASK: u32 = genmask(9, 0);

// RX stale count
pub const RX_STALE_CNT: u32 = genmask(23, 0);

// Parity configuration
pub const PAR_CALC_EN: u32 = 1 << 0;
pub const PAR_EVEN: u32 = 0x00;
pub const PAR_ODD: u32 = 0x01;
pub const PAR_SPACE: u32 = 0x10;

// M interrupt enable fields
pub const M_CMD_DONE_EN: u32 = 1 << 0;
pub const S_CMD_DONE_EN: u32 = 1 << 0;
pub const S_CMD_OVERRUN_EN: u32 = 1 << 1;
pub const S_ILLEGAL_CMD_EN: u32 = 1 << 2;
pub const S_CMD_CANCEL_EN: u32 = 1 << 4;
pub const S_CMD_ABORT_EN: u32 = 1 << 5;
pub const M_CMD_OVERRUN_EN: u32 = 1 << 1;
pub const M_ILLEGAL_CMD_EN: u32 = 1 << 2;
pub const M_CMD_FAILURE_EN: u32 = 1 << 3;
pub const M_CMD_CANCEL_EN: u32 = 1 << 4;
pub const M_CMD_ABORT_EN: u32 = 1 << 5;
pub const M_TIMESTAMP_EN: u32 = 1 << 6;
pub const M_RX_IRQ_EN: u32 = 1 << 7;
pub const M_GP_SYNC_IRQ_0_EN: u32 = 1 << 8;
pub const M_GP_IRQ_0_EN: u32 = 1 << 9;
pub const M_GP_IRQ_1_EN: u32 = 1 << 10;
pub const M_GP_IRQ_2_EN: u32 = 1 << 11;
pub const M_GP_IRQ_3_EN: u32 = 1 << 12;
pub const M_GP_IRQ_4_EN: u32 = 1 << 13;
pub const M_GP_IRQ_5_EN: u32 = 1 << 14;
pub const M_TX_FIFO_NOT_EMPTY_EN: u32 = 1 << 21;
pub const M_IO_DATA_DEASSERT_EN: u32 = 1 << 22;
pub const M_IO_DATA_ASSERT_EN: u32 = 1 << 23;
pub const M_RX_FIFO_RD_ERR_EN: u32 = 1 << 24;
pub const M_RX_FIFO_WR_ERR_EN: u32 = 1 << 25;
pub const M_RX_FIFO_WATERMARK_EN: u32 = 1 << 26;
pub const M_RX_FIFO_LAST_EN: u32 = 1 << 27;
pub const M_TX_FIFO_RD_ERR_EN: u32 = 1 << 28;
pub const M_TX_FIFO_WR_ERR_EN: u32 = 1 << 29;
pub const M_TX_FIFO_WATERMARK_EN: u32 = 1 << 30;
pub const M_SEC_IRQ_EN: u32 = 1 << 31;

// S interrupt enable/status fields
pub const S_GP_IRQ_0_EN: u32 = 1 << 9;
pub const S_GP_IRQ_1_EN: u32 = 1 << 10;
pub const S_GP_IRQ_2_EN: u32 = 1 << 11;
pub const S_GP_IRQ_3_EN: u32 = 1 << 12;
pub const S_RX_FIFO_RD_ERR_EN: u32 = 1 << 24;
pub const S_RX_FIFO_WR_ERR_EN: u32 = 1 << 25;
pub const S_RX_FIFO_WATERMARK_EN: u32 = 1 << 26;
pub const S_RX_FIFO_LAST_EN: u32 = 1 << 27;

// TX FIFO status fields
pub const TX_FIFO_WC: u32 = genmask(27, 0);

// RX FIFO status fields
pub const RX_LAST: u32 = 1 << 31;
pub const RX_LAST_BYTE_VALID_MSK: u32 = genmask(30, 28);
pub const RX_LAST_BYTE_VALID_SHFT: u32 = 28;
pub const RX_FIFO_WC_MSK: u32 = genmask(24, 0);

// HW parameter fields
pub const TX_FIFO_WIDTH_MSK: u32 = genmask(29, 24);
pub const TX_FIFO_WIDTH_SHFT: u32 = 24;
pub const TX_FIFO_DEPTH_MSK_256_BYTES: u32 = genmask(23, 16);
pub const TX_FIFO_DEPTH_SHFT: u32 = 16;

pub const RX_FIFO_WIDTH_MSK: u32 = genmask(29, 24);
pub const RX_FIFO_WIDTH_SHFT: u32 = 24;
pub const RX_FIFO_DEPTH_MSK_256_BYTES: u32 = genmask(23, 16);
pub const RX_FIFO_DEPTH_SHFT: u32 = 16;

// Hardware parameters
pub const BYTES_PER_FIFO_WORD: u32 = 4;
pub const UART_FIFO_DEPTH_WORDS: u32 = 64;
pub const UART_FIFO_WIDTH_BITS: u32 = 32;
pub const UART_OVERSAMPLING: u32 = 32;
pub const STALE_TIMEOUT: u32 = 160;
pub const DEFAULT_BITS_PER_CHAR: u32 = 10;
pub const DEF_TX_WM: u32 = 2;
pub const UART_RX_WM: u32 = 2;

// DMA interrupt enable/status fields
pub const DMA_DONE_EN: u32 = 1 << 0;
pub const DMA_EOT_EN: u32 = 1 << 1;
pub const DMA_AHB_ERR_EN: u32 = 1 << 2;

// DMA attribute fields
pub const DMA_EOT_BUF: u32 = 1 << 0;
