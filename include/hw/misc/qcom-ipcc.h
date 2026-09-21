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

/* Hardware limits */
#define IPCC_MAX_PROTOCOLS         3    /* Number of protocols */
#define IPCC_MAX_CLIENTS           64   /* Maximum clients per protocol */
#define IPCC_CLIENT_REG_SIZE       0x1000  /* 4KB per client */
#define IPCC_PROTOCOL_REG_SIZE     0x40000  /* 256KB per protocol */

/* Register offsets within each client space */
#define IPCC_REG_VERSION                    0x000
#define IPCC_REG_ID                         0x004
#define IPCC_REG_CONFIG                     0x008
#define IPCC_REG_SEND_ID                    0x00C
#define IPCC_REG_RECV_ID                    0x010
#define IPCC_REG_RECV_SIGNAL_ENABLE         0x014
#define IPCC_REG_RECV_SIGNAL_DISABLE        0x018
#define IPCC_REG_RECV_SIGNAL_CLEAR          0x01C
#define IPCC_REG_CLIENT_CLEAR               0x038

/* Client-specific register arrays */
#define IPCC_REG_RECV_CLIENT_PRIORITY       0x100
#define IPCC_REG_CLIENT_SIGNAL_STATUS_0     0x600
#define IPCC_REG_CLIENT_SIGNAL_STATUS_1     0x700
#define IPCC_REG_CLIENT_ENABLE_STATUS_0     0x800
#define IPCC_REG_CLIENT_ENABLE_STATUS_1     0x900

/* Register field definitions */
#define IPCC_CONFIG_CLEAR_ON_RECV_RD        BIT(0)
#define IPCC_CONFIG_DISABLE_MODE            BIT(31)
#define IPCC_SIGNAL_ID_MASK                 0xFFFF
#define IPCC_CLIENT_ID_MASK                 0xFFFF0000
#define IPCC_CLIENT_ID_SHIFT                16
#define IPCC_SEND_BROADCAST_FLAG            BIT(31)
#define IPCC_NO_PENDING_IRQ                 0xFFFFFFFF

/* Default values */
#define IPCC_DEFAULT_VERSION                0x10200

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
