/*
 * Hexagon Global Registers QOM Object
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_GLOBALREG_H
#define HEXAGON_GLOBALREG_H

#include "qemu/thread.h"

#include "hw/core/qdev.h"
#include "hw/core/sysbus.h"
#include "hw/intc/hex-l2vic.h"
#include "hw/timer/qct-qtimer.h"
#include "qom/object.h"
#include "target/hexagon/cpu.h"

#define TYPE_HEXAGON_GLOBALREG "hexagon-globalreg"
OBJECT_DECLARE_SIMPLE_TYPE(HexagonGlobalRegState, HEXAGON_GLOBALREG)

/* Shared user-DMA configuration registers: DM0 is per thread, DM1..DM12. */
#define HEX_DMA_NUM_CFG_REGS 13

enum {
    HEX_DM2_CTRL = 2,
    HEX_DM4_ERROR = 4,
    HEX_DM5_ERROR_ADDR = 5,
    HEX_DM8_CFG = 8,
    HEX_DM9_CFG = 9,
    HEX_DM10_CFG = 10,
    HEX_DM11_CFG = 11,
    HEX_DM12_CFG = 12,
};

#define HEX_DM2_WRITE_CLEAR_MASK   BIT(2)
#define HEX_DM4_ERROR_VALID        BIT(0)
#define HEX_DM4_ERROR_HTID_SHIFT   4
#define HEX_DM4_ERROR_SYNDROME_SHIFT 8
#define HEX_DM8_WRITABLE_MASK      0x00ffffff
#define HEX_DM11_WRITABLE_MASK     0x000000ff

struct HexagonGlobalRegState {
    SysBusDevice parent_obj;

    /* Array of system registers */
    uint32_t regs[NUM_SREGS];

    /* L2VIC interface used to back the VID/VID1 registers */
    HexL2VicInterface *l2vic;

    /* QTimer interface used to back the TIMERLO/TIMERHI registers */
    QctQtimerInterface *qtimer;

    /* Global performance cycle counter base */
    uint64_t g_pcycle_base;

    /* Global performance cycle counter clock state */
    int64_t pcycle_start_ns;
    bool pcycle_running;

    /* Shared user-DMA control and syndrome registers, indexed by DM number. */
    uint32_t dma[HEX_DMA_NUM_CFG_REGS];
    /*
     * Protects dma[]: the DMA helpers of every vCPU thread access it
     * without holding the BQL.
     */
    QemuMutex dma_lock;

    /* Properties for global register reset values */
    uint32_t boot_evb;           /* Boot Exception Vector Base (HEX_SREG_EVB) */
    uint64_t config_table_addr;  /* Configuration table base */
    uint32_t dsp_rev;           /* DSP revision register (HEX_SREG_REV) */

    /* ISDB properties */
    bool isdben_etm_enable;     /* ISDB ETM enable bit */
    bool isdben_dfd_enable;     /* ISDB DFD enable bit */
    bool isdben_trusted;        /* ISDB trusted mode bit */
    bool isdben_secure;         /* ISDB secure mode bit */
};

/* Public interface functions */
uint32_t hexagon_globalreg_read(HexagonGlobalRegState *s, uint32_t reg,
                                uint32_t htid);
void hexagon_globalreg_write(HexagonGlobalRegState *s, uint32_t reg,
                             uint32_t value, uint32_t htid);
uint32_t hexagon_globalreg_masked_value(HexagonGlobalRegState *s, uint32_t reg,
                                        uint32_t value);
void hexagon_globalreg_write_masked(HexagonGlobalRegState *s, uint32_t reg,
                                    uint32_t value);

/* Global performance cycle counter access */
uint64_t hexagon_globalreg_read_pcycle(HexagonGlobalRegState *s);
uint64_t hexagon_globalreg_get_pcycle_base(HexagonGlobalRegState *s);
void hexagon_globalreg_set_pcycle_base(HexagonGlobalRegState *s,
                                       uint64_t value);

uint32_t hexagon_dma_config_read(HexagonGlobalRegState *s, uint32_t index);
void hexagon_dma_config_write(HexagonGlobalRegState *s, uint32_t index,
                              uint32_t value);
void hexagon_dma_capture_error(HexagonGlobalRegState *s, uint32_t htid,
                               uint32_t syndrome, uint32_t address);

#endif /* HEXAGON_GLOBALREG_H */
