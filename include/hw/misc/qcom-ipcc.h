/*
 * Qualcomm IPCC (Inter-Processor Communication Controller)
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_QCOM_IPCC_H
#define HW_MISC_QCOM_IPCC_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_QCOM_IPCC "qcom-ipcc"
OBJECT_DECLARE_TYPE(QcomIPCCState, QcomIPCCClass, QCOM_IPCC)

#define IPCC_MAX_PROTOCOLS         3
#define IPCC_MAX_CLIENTS           64
#define IPCC_CLIENT_REG_SIZE       0x1000  /* 4KB per client */
#define IPCC_PROTOCOL_REG_SIZE     0x40000  /* 256KB per protocol */

typedef struct QcomIPCCClient {
    uint32_t regs[IPCC_CLIENT_REG_SIZE / 4];
    uint32_t version;
    uint32_t client_size;
} QcomIPCCClient;

struct QcomIPCCState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    /* Client state for [protocol][client] */
    QcomIPCCClient clients[IPCC_MAX_PROTOCOLS][IPCC_MAX_CLIENTS];

    /* IRQ outputs - one per client */
    qemu_irq irq[IPCC_MAX_CLIENTS];
    bool irq_status[IPCC_MAX_CLIENTS];

    /* Properties */
    uint32_t num_clients;
    uint32_t version;
};

struct QcomIPCCClass {
    SysBusDeviceClass parent_class;
};

#endif /* HW_MISC_QCOM_IPCC_H */
