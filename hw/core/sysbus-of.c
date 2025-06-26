/*
 * A generic OF (OpenFirmware) style device for the System Bus.
 *
 * Copyright (c) 2025 Qualcomm
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/sysbus.h"
#include "hw/sysbus-of.h"
#include "hw/qdev-properties.h"

#include "system/device_tree.h"

bool of_sysbus_access_in_reg(OfSysBusDevice* ofdev, uint32_t reg_idx, hwaddr addr, unsigned size)
{
    if (reg_idx >= ofdev->nb_regs) {
        return false;
    }

    hwaddr reg_base = ofdev->regs[reg_idx].addr;
    uint64_t reg_size = ofdev->regs[reg_idx].size;

    return addr >= reg_base && (addr + size) < (reg_base + reg_size);
}

// returns the matching device id from the list.
// this is compatible with the linux way of matching nodes.
static const struct of_device_id* of_match_node(void* fdt, const char* of_node_path, const struct of_device_id* matches)
{
    int best_score = 0;
    const struct of_device_id* best_match = NULL;

    if (!matches) {
        return NULL;
    }

    for (; matches->name[0] || matches->type[0] || matches->compatible[0]; ++matches) {
        int score = qemu_fdt_of_is_compatible(fdt, of_node_path, matches->compatible, matches->type, matches->name);
        if (score > best_score) {
            best_match = matches;
            best_score = score;
        }
    }

    return best_match;
}

static void of_sysbus_device_instance_init(Object *obj)
{
}

static void of_sysbus_realize(DeviceState* dev, Error **errp)
{
    OfSysBusDevice* ofdev = OF_SYS_BUS_DEVICE(dev);
    OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_GET_CLASS(ofdev);
    const char* of_node_path;
    const struct of_device_id* dev_table;
    hwaddr base_addr;

    assert(ofdev->fdt);
    assert(ofdev->in_fdt != ofdev->fdt);
    assert(ofdev->node_path || ofdev->node_label);

    if (ofdev->node_label) {
        ofdev->node_path = qemu_fdt_node_path_by_label(ofdev->in_fdt, ofdev->node_label, errp);
    }

    of_node_path = ofdev->node_path;

    assert(of_node_path);

    if (ofdev->in_fdt) {
        qemu_fdt_copy_node(ofdev->fdt, ofdev->in_fdt, of_node_path, errp);
    }

    dev_table = kofdev->of_match_table;
    if (dev_table) {
        ofdev->dev_id = of_match_node(ofdev->fdt, of_node_path, dev_table);
        ofdev->data = ofdev->dev_id->data;
    }

    if (qemu_fdt_get_node_addr(ofdev->fdt, of_node_path, &base_addr, errp)) {
        ofdev->base_addr = g_new(hwaddr, 1);
        *ofdev->base_addr = base_addr;
    }

    qemu_fdt_getprop_reg(ofdev->fdt, of_node_path, &ofdev->regs, &ofdev->nb_regs, errp);

    // set the base at 0
    for(size_t i = 0; i < ofdev->nb_regs; ++i) {
        assert(base_addr <= ofdev->regs[i].addr);
        ofdev->regs[i].addr -= base_addr;
    }

    // find the parent interrupt phandle, if there is any.
    qemu_fdt_getprop_interrupts(
        ofdev->fdt,
        of_node_path,
        &ofdev->interrupts,
        errp
    );

    if(kofdev->realize) {
        kofdev->realize(ofdev, errp);
    }
}

static void of_sysbus_unrealize(DeviceState* dev)
{
    OfSysBusDevice* ofdev = OF_SYS_BUS_DEVICE(dev);
    OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_GET_CLASS(ofdev);

    if (kofdev->unrealize) {
        kofdev->unrealize(ofdev);
    }
}

static const Property of_sysbus_properties[] = {
    DEFINE_PROP_PTR_VOID(OF_SYSBUS_PARAM_IN_FDT, OfSysBusDevice, in_fdt, NULL),
    DEFINE_PROP_PTR_VOID(OF_SYSBUS_PARAM_FDT, OfSysBusDevice, fdt, NULL),
    DEFINE_PROP_CONST_STRING(OF_SYSBUS_PARAM_NODE_LABEL, OfSysBusDevice, node_label),
    DEFINE_PROP_CONST_STRING(OF_SYSBUS_PARAM_NODE_PATH, OfSysBusDevice, node_path),
};

static void of_sysbus_device_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = of_sysbus_realize;
    dc->unrealize = of_sysbus_unrealize;
    device_class_set_props(dc, of_sysbus_properties);
}

static const TypeInfo of_sysbus_type[] = {
    {
        .name = TYPE_OF_SYS_BUS_DEVICE,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(OfSysBusDevice),
        .instance_init = of_sysbus_device_instance_init,
        .abstract = true,
        .class_size = sizeof(OfSysBusDeviceClass),
        .class_init = of_sysbus_device_class_init,
    }
};

DEFINE_TYPES(of_sysbus_type);
