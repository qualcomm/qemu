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

#ifndef HW_SYSBUS_OF_H
#define HW_SYSBUS_OF_H

#include "qemu/osdep.h"
#include "hw/qdev-core.h"
#include "exec/memory.h"
#include "qom/object.h"
#include "system/device_tree.h"
#include "sysbus.h"

#define TYPE_OF_SYS_BUS_DEVICE "of-sys-bus-device"
OBJECT_DECLARE_TYPE(OfSysBusDevice, OfSysBusDeviceClass,
                    OF_SYS_BUS_DEVICE)

typedef void (*OfSysBusRealize)(OfSysBusDevice *ofdev, Error **errp);
typedef void (*OfSysBusUnrealize)(OfSysBusDevice *ofdev);

#define OF_SYSBUS_PARAM_IN_FDT          "in-fdt"
#define OF_SYSBUS_PARAM_FDT             "fdt"
#define OF_SYSBUS_PARAM_NODE_LABEL      "node-label"
#define OF_SYSBUS_PARAM_NODE_PATH       "node-path"

/*
 * Struct used for matching a device
 *
 * Taken from the Linux kernel.
 */
struct of_device_id {
	char	name[32];
	char	type[32];
	char	compatible[128];
	const void *data;
};

struct OfSysBusDeviceClass {
    /*< private >*/
    SysBusDeviceClass parent_class;
    /*< public >*/

    /*
     * Same purpose as Device realize / unrealize.
     * Refer to Device doc for more info.
     * Please do NOT change Device realize directly!
     */
    OfSysBusRealize realize;
    OfSysBusUnrealize unrealize;

    // match table, must be null terminated.
    const struct of_device_id* of_match_table;
};

struct OfSysBusDevice {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    // the fdt, to which the node is added.
    void* fdt;
    
    // Input fdt, if there is any.
    void* in_fdt;

    // data of the type from dev_id->data.
    // this is solely for convenience.
    const void* data;

    // the label to the target node.
    // may be NULL
    const char* node_label;

    // the node full path, used if no label is provided.
    const char* node_path;

    // the node offset in the fdt.
    int node_offset;

    // base address, as extracted during init.
    // if NULL, no base address was found.
    hwaddr* base_addr;

    // the matching device id, if any compatible is found.
    const struct of_device_id* dev_id;

    // registers
    // addresses are normalized at 0.
    // in other words, the first register always starts at 0
    struct fdt_reg* regs;
    uint32_t nb_regs;

    // iterrupts
    // it is the device parent responsibility to interpret and plug interrupts accordingly.
    struct fdt_interrupts* interrupts;
};

bool of_sysbus_access_in_reg(OfSysBusDevice* ofdev, uint32_t reg_idx, hwaddr addr, unsigned size);

#endif /* HW_SYSBUS_H */
