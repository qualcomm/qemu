/*
 * QEMU UFS Logical Unit
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Written by Jeuk Kim <jeuk20.kim@samsung.com>
 *
 * This code is licensed under the GNU GPL v2 or later.
 */

#include "qemu/osdep.h"
#include "hw/scsi/scsi.h"
#include "qapi/error.h"
#include "qemu/cutils.h"
#include "qemu/memalign.h"
#include "qemu/units.h"
#include "system/block-backend.h"
#include "scsi/constants.h"
#include "trace.h"
#include "ufs.h"


static void ufs_lu_realize(DeviceState *dev, Error **errp)
{
    UfsLu *lu = DO_UPCAST(UfsLu, qdev, dev);
    BusState *s = qdev_get_parent_bus(dev);
    UfsHc *u = UFS(s->parent);
    BlockBackend *blk = lu->conf.blk;

    ufs_lu_realize_common(lu, blk, u, errp);
}

static void ufs_lu_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = ufs_lu_realize;
    dc->bus_type = TYPE_UFS_BUS;
    ufs_lu_class_init_common(dc);
}

static const TypeInfo ufs_lu_info = {
    .name = TYPE_UFS_LU,
    .parent = TYPE_DEVICE,
    .class_init = ufs_lu_class_init,
    .instance_size = sizeof(UfsLu),
};

static void ufs_lu_register_types(void)
{
    type_register_static(&ufs_lu_info);
}

type_init(ufs_lu_register_types)
