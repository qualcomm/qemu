/*
 * QEMU Universal Flash Storage (UFS) Controller
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Written by Jeuk Kim <jeuk20.kim@samsung.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/**
 * Reference Specs: https://www.jedec.org/, 4.0
 *
 * Usage
 * -----
 *
 * Add options:
 *      -drive file=<file>,if=none,id=<drive_id>
 *      -device ufs,serial=<serial>,id=<bus_name>, \
 *              nutrs=<N[optional]>,nutmrs=<N[optional]>
 *      -device ufs-lu,drive=<drive_id>,bus=<bus_name>
 */

#include "qemu/osdep.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "scsi/constants.h"
#include "trace.h"
#include "ufs.h"

static MemTxResult ufs_pci_dma_read(UfsHc *u, dma_addr_t addr, void *buf,
                                    dma_addr_t len)
{
    return pci_dma_read(PCI_DEVICE(u), addr, buf, len);
}

static MemTxResult ufs_pci_dma_write(UfsHc *u, dma_addr_t addr, const void *buf,
                                     dma_addr_t len)
{
    return pci_dma_write(PCI_DEVICE(u), addr, buf, len);
}

static void ufs_pci_dma_sglist_init(UfsHc *u, QEMUSGList *qsg, int alloc_hint)
{
    pci_dma_sglist_init(qsg, PCI_DEVICE(u), alloc_hint);
}

static void ufs_pci_irq_raise(UfsHc *u)
{
    pci_irq_assert(PCI_DEVICE(u));
}

static void ufs_pci_irq_lower(UfsHc *u)
{
    pci_irq_deassert(PCI_DEVICE(u));
}

static void ufs_init_pci(UfsHc *u, PCIDevice *pci_dev)
{
    uint8_t *pci_conf = pci_dev->config;

    pci_conf[PCI_INTERRUPT_PIN] = 1;
    pci_config_set_prog_interface(pci_conf, 0x1);

    mem_reg_init_io(u, "ufs");

    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &u->iomem);
    u->irq = pci_allocate_irq(pci_dev);
}


static void ufs_realize_pci(PCIDevice *pci_dev, Error **errp)
{
    UfsHc *u = UFS(pci_dev);
    u->dma_read = ufs_pci_dma_read;
    u->dma_write = ufs_pci_dma_write;
    u->dma_sglist_init = ufs_pci_dma_sglist_init;
    u->irq_raise = ufs_pci_irq_raise;
    u->irq_lower = ufs_pci_irq_lower;

    if (!ufs_check_constraints(u, errp)) {
        return;
    }

    qbus_init(&u->bus, sizeof(UfsBus), TYPE_UFS_BUS, &pci_dev->qdev,
              u->parent_obj.qdev.id);

    ufs_init_state(u);
    ufs_init_hc(u);
    ufs_init_pci(u, pci_dev);

    ufs_init_wlu(&u->report_wlu, UFS_UPIU_REPORT_LUNS_WLUN);
    ufs_init_wlu(&u->dev_wlu, UFS_UPIU_UFS_DEVICE_WLUN);
    ufs_init_wlu(&u->boot_wlu, UFS_UPIU_BOOT_WLUN);
    ufs_init_wlu(&u->rpmb_wlu, UFS_UPIU_RPMB_WLUN);
}

static void ufs_exit_pci(PCIDevice *pci_dev)
{
    UfsHc *u = UFS(pci_dev);

    ufs_exit_common(u);
}

static bool ufs_bus_check_address(BusState *qbus, DeviceState *qdev,
                                  Error **errp)
{
    if (strcmp(object_get_typename(OBJECT(qdev)), TYPE_UFS_LU) != 0) {
        error_setg(errp, "%s cannot be connected to ufs-bus",
                   object_get_typename(OBJECT(qdev)));
        return false;
    }

    return true;
}

static void ufs_bus_class_init(ObjectClass *class, void *data)
{
    BusClass *bc = BUS_CLASS(class);
    bc->get_dev_path = ufs_bus_get_dev_path;
    bc->check_address = ufs_bus_check_address;
}

static void ufs_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(oc);

    pc->realize = ufs_realize_pci;
    pc->exit = ufs_exit_pci;
    pc->vendor_id = PCI_VENDOR_ID_REDHAT;
    pc->device_id = PCI_DEVICE_ID_REDHAT_UFS;
    pc->class_id = PCI_CLASS_STORAGE_UFS;

    ufs_class_init_common(dc);
}

static const TypeInfo ufs_info = {
    .name = TYPE_UFS,
    .parent = TYPE_PCI_DEVICE,
    .class_init = ufs_class_init,
    .instance_size = sizeof(UfsHc),
    .interfaces = (InterfaceInfo[]){ { INTERFACE_PCIE_DEVICE }, {} },
};

static const TypeInfo ufs_bus_info = {
    .name = TYPE_UFS_BUS,
    .parent = TYPE_BUS,
    .class_init = ufs_bus_class_init,
    .class_size = sizeof(UfsBusClass),
    .instance_size = sizeof(UfsBus),
};

static void ufs_register_types(void)
{
    type_register_static(&ufs_info);
    type_register_static(&ufs_bus_info);
}

type_init(ufs_register_types)
