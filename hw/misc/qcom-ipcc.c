/*
 * Qualcomm IPCC (Inter-Processor Communication Controller)
 *
 * This device implements the IPCC controller which provides inter-processor
 * communication via signals and interrupts between different subsystems.
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/sysbus.h"
#include "hw/core/qdev-properties.h"
#include "hw/misc/qcom-ipcc.h"
#include "hw/core/irq.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/module.h"

/* 0xFFFFFFFF marks "no pending signal" in the RECV_ID register (0x010) */
#define IPCC_NO_PENDING_IRQ 0xFFFFFFFF

static void client_reset(QcomIPCCClient *client,
                          uint32_t protocol, uint32_t client_id)
{
    memset(client->regs, 0, sizeof(client->regs));
    client->regs[0] = client->version;
    client->regs[1] = (client_id << 16) | protocol;
    client->regs[0x010 / 4] = IPCC_NO_PENDING_IRQ;
}

static bool client_signal_status(QcomIPCCClient *client,
                                  uint32_t src_client, uint32_t signal)
{
    if (src_client >= client->client_size || signal >= client->client_size) {
        return false;
    }

    if (signal < 32) {
        uint32_t reg_offset = 0x600 / 4 + src_client;
        return (client->regs[reg_offset] & (1U << signal)) != 0;
    } else {
        uint32_t reg_offset = 0x700 / 4 + src_client;
        return (client->regs[reg_offset] & (1U << (signal - 32))) != 0;
    }
}

static void client_set_signal_status(QcomIPCCClient *client,
                                      uint32_t src_client,
                                      uint32_t signal, bool active)
{
    if (src_client >= client->client_size || signal >= client->client_size) {
        return;
    }

    if (signal < 32) {
        uint32_t reg_offset = 0x600 / 4 + src_client;
        if (active) {
            client->regs[reg_offset] |= (1U << signal);
        } else {
            client->regs[reg_offset] &= ~(1U << signal);
        }
    } else {
        uint32_t reg_offset = 0x700 / 4 + src_client;
        if (active) {
            client->regs[reg_offset] |= (1U << (signal - 32));
        } else {
            client->regs[reg_offset] &= ~(1U << (signal - 32));
        }
    }
}

static bool client_signal_enable(QcomIPCCClient *client,
                                  uint32_t src_client, uint32_t signal)
{
    if (src_client >= client->client_size || signal >= client->client_size) {
        return false;
    }

    if (signal < 32) {
        uint32_t reg_offset = 0x800 / 4 + src_client;
        return (client->regs[reg_offset] & (1U << signal)) != 0;
    } else {
        uint32_t reg_offset = 0x900 / 4 + src_client;
        return (client->regs[reg_offset] & (1U << (signal - 32))) != 0;
    }
}

static void client_set_signal_enable(QcomIPCCClient *client,
                                      uint32_t src_client,
                                      uint32_t signal, bool enable)
{
    if (src_client >= client->client_size || signal >= client->client_size) {
        return;
    }

    if (signal < 32) {
        uint32_t reg_offset = 0x800 / 4 + src_client;
        if (enable) {
            client->regs[reg_offset] |= (1U << signal);
        } else {
            client->regs[reg_offset] &= ~(1U << signal);
        }
    } else {
        uint32_t reg_offset = 0x900 / 4 + src_client;
        if (enable) {
            client->regs[reg_offset] |= (1U << (signal - 32));
        } else {
            client->regs[reg_offset] &= ~(1U << (signal - 32));
        }
    }
}

static void send_signal(QcomIPCCState *s, QcomIPCCClient *src_client,
                        uint32_t dst_client_id, uint32_t signal)
{
    if (dst_client_id >= s->num_clients) {
        return;
    }

    uint32_t src_protocol = src_client->regs[1] & 0x3F;
    uint32_t src_client_id = (src_client->regs[1] >> 16) & 0x3F;

    QcomIPCCClient *dst_client = &s->clients[src_protocol][dst_client_id];
    client_set_signal_status(dst_client, src_client_id, signal, true);
}

static void update_irq(QcomIPCCState *s)
{
    for (uint32_t c = 0; c < s->num_clients; c++) {
        bool has_irq = false;
        uint32_t highest_priority = 0;
        uint32_t pending_signal = 0;
        uint32_t pending_client = 0;

        for (uint32_t p = 0; p < IPCC_MAX_PROTOCOLS; p++) {
            QcomIPCCClient *client = &s->clients[p][c];

            if (client->regs[0x008 / 4] & (1U << 31)) {
                continue;
            }

            for (uint32_t src_client = 0; src_client < s->num_clients;
                 src_client++) {
                for (uint32_t signal = 0; signal < s->num_clients; signal++) {
                    if (client_signal_status(client, src_client, signal) &&
                        client_signal_enable(client, src_client, signal)) {

                        uint32_t priority_offset = 0x100 / 4 + src_client;
                        uint32_t priority = client->regs[priority_offset];

                        if (!has_irq || priority > highest_priority) {
                            highest_priority = priority;
                            pending_signal = signal;
                            pending_client = src_client;
                            has_irq = true;
                        }
                    }
                }
            }

            if (has_irq) {
                client->regs[0x010 / 4] =
                    (pending_client << 16) | pending_signal;
            } else {
                client->regs[0x010 / 4] = IPCC_NO_PENDING_IRQ;
            }
        }

        if (has_irq && !s->irq_status[c]) {
            s->irq_status[c] = true;
            qemu_irq_raise(s->irq[c]);
        } else if (!has_irq && s->irq_status[c]) {
            s->irq_status[c] = false;
            qemu_irq_lower(s->irq[c]);
        }
    }
}

static uint64_t qcom_ipcc_read(void *opaque, hwaddr offset, unsigned size)
{
    QcomIPCCState *s = QCOM_IPCC(opaque);

    if (size != 4) {
        return 0;
    }

    uint32_t protocol = (offset / IPCC_PROTOCOL_REG_SIZE) & 0x3;
    uint32_t client_id = ((offset % IPCC_PROTOCOL_REG_SIZE) /
                          IPCC_CLIENT_REG_SIZE) & 0x3F;
    uint32_t reg_offset = offset & (IPCC_CLIENT_REG_SIZE - 1);

    if (protocol >= IPCC_MAX_PROTOCOLS || client_id >= s->num_clients) {
        return 0;
    }

    QcomIPCCClient *client = &s->clients[protocol][client_id];
    uint32_t value = client->regs[reg_offset / 4];

    /* RECV_ID (0x010): clearing on read, when configured, consumes signal */
    if (reg_offset == 0x010) {
        if ((client->regs[0x008 / 4] & 0x1) &&
            value != IPCC_NO_PENDING_IRQ) {
            uint32_t src_client = (value >> 16) & 0xFFFF;
            uint32_t signal = value & 0xFFFF;
            client_set_signal_status(client, src_client, signal, false);
            update_irq(s);
        }
    }

    return value;
}

static void qcom_ipcc_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    QcomIPCCState *s = QCOM_IPCC(opaque);

    if (size != 4) {
        return;
    }

    uint32_t protocol = (offset / IPCC_PROTOCOL_REG_SIZE) & 0x3;
    uint32_t client_id = ((offset % IPCC_PROTOCOL_REG_SIZE) /
                          IPCC_CLIENT_REG_SIZE) & 0x3F;
    uint32_t reg_offset = offset & (IPCC_CLIENT_REG_SIZE - 1);

    if (protocol >= IPCC_MAX_PROTOCOLS || client_id >= s->num_clients) {
        return;
    }

    QcomIPCCClient *client = &s->clients[protocol][client_id];

    switch (reg_offset) {
    case 0x00C: /* SEND_ID */
        {
            uint32_t signal = value & 0xFFFF;
            if (value & (1U << 31)) {
                for (uint32_t dst_client = 0; dst_client < s->num_clients;
                     dst_client++) {
                    send_signal(s, client, dst_client, signal);
                }
            } else {
                uint32_t dst_client = (value & 0xFFFF0000) >> 16;
                send_signal(s, client, dst_client, signal);
            }
            update_irq(s);
            break;
        }
    case 0x014: /* RECV_SIGNAL_ENABLE */
        {
            uint32_t src_client = (value & 0xFFFF0000) >> 16;
            uint32_t signal = value & 0xFFFF;
            client_set_signal_enable(client, src_client, signal, true);
            update_irq(s);
            break;
        }
    case 0x018: /* RECV_SIGNAL_DISABLE */
        {
            uint32_t src_client = (value & 0xFFFF0000) >> 16;
            uint32_t signal = value & 0xFFFF;
            client_set_signal_enable(client, src_client, signal, false);
            update_irq(s);
            break;
        }
    case 0x01C: /* RECV_SIGNAL_CLEAR */
        {
            uint32_t src_client = (value & 0xFFFF0000) >> 16;
            uint32_t signal = value & 0xFFFF;
            client_set_signal_status(client, src_client, signal, false);
            if (s->irq_status[client_id]) {
                s->irq_status[client_id] = false;
                qemu_irq_lower(s->irq[client_id]);
            }
            update_irq(s);
            break;
        }
    case 0x038: /* CLIENT_CLEAR */
        {
            if (value & 0x1) {
                uint32_t saved_id = client->regs[1];
                client_reset(client, protocol, client_id);
                client->regs[1] = saved_id;
                update_irq(s);
            }
            break;
        }
    default:
        client->regs[reg_offset / 4] = value;
        break;
    }
}

static const MemoryRegionOps qcom_ipcc_ops = {
    .read = qcom_ipcc_read,
    .write = qcom_ipcc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_ipcc_reset_hold(Object *obj, ResetType type)
{
    QcomIPCCState *s = QCOM_IPCC(obj);

    for (uint32_t p = 0; p < IPCC_MAX_PROTOCOLS; p++) {
        for (uint32_t c = 0; c < s->num_clients; c++) {
            QcomIPCCClient *client = &s->clients[p][c];
            client->version = s->version;
            client->client_size = s->num_clients;
            client_reset(client, p, c);
        }
    }

    for (uint32_t i = 0; i < s->num_clients; i++) {
        s->irq_status[i] = false;
        qemu_irq_lower(s->irq[i]);
    }
}

static void qcom_ipcc_realize(DeviceState *dev, Error **errp)
{
    QcomIPCCState *s = QCOM_IPCC(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &qcom_ipcc_ops, s,
                          TYPE_QCOM_IPCC,
                          IPCC_MAX_PROTOCOLS * IPCC_PROTOCOL_REG_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    for (uint32_t i = 0; i < s->num_clients; i++) {
        sysbus_init_irq(sbd, &s->irq[i]);
        s->irq_status[i] = false;
    }
}

static const Property qcom_ipcc_properties[] = {
    DEFINE_PROP_UINT32("num-clients", QcomIPCCState, num_clients,
                       IPCC_MAX_CLIENTS),
    DEFINE_PROP_UINT32("version", QcomIPCCState, version, 0x10200),
};

static const VMStateDescription vmstate_qcom_ipcc_client = {
    .name = "qcom-ipcc-client",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, QcomIPCCClient, IPCC_CLIENT_REG_SIZE / 4),
        VMSTATE_UINT32(version, QcomIPCCClient),
        VMSTATE_UINT32(client_size, QcomIPCCClient),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_qcom_ipcc = {
    .name = "qcom-ipcc",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_STRUCT_2DARRAY(clients, QcomIPCCState,
                               IPCC_MAX_PROTOCOLS, IPCC_MAX_CLIENTS, 0,
                               vmstate_qcom_ipcc_client, QcomIPCCClient),
        VMSTATE_BOOL_ARRAY(irq_status, QcomIPCCState, IPCC_MAX_CLIENTS),
        VMSTATE_UINT32(num_clients, QcomIPCCState),
        VMSTATE_UINT32(version, QcomIPCCState),
        VMSTATE_END_OF_LIST()
    }
};

static void qcom_ipcc_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = qcom_ipcc_realize;
    dc->vmsd = &vmstate_qcom_ipcc;
    device_class_set_props(dc, qcom_ipcc_properties);
    rc->phases.hold = qcom_ipcc_reset_hold;
    dc->desc = "Qualcomm IPCC (Inter-Processor Communication Controller)";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo qcom_ipcc_info = {
    .name          = TYPE_QCOM_IPCC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomIPCCState),
    .class_init    = qcom_ipcc_class_init,
};

static void qcom_ipcc_register_types(void)
{
    type_register_static(&qcom_ipcc_info);
}

type_init(qcom_ipcc_register_types)
