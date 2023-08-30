/*
 * QEMU Reset to GPIO
 *
 * Register for reset callbacks and drive GPIO output
 *
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "trace.h"
#include "qom/object.h"
#include "hw/irq.h"
#include "sysemu/reset.h"
#include "sysemu/runstate.h"

#define TYPE_RESET_GPIO "reset_gpio"
OBJECT_DECLARE_SIMPLE_TYPE(ResetGPIO, RESET_GPIO)

struct ResetGPIO {
    DeviceState parent_obj;
    qemu_irq reset_out;
    char *name;
    bool active;
};

static void reset_gpio_reset(void *dev)
{
    ResetGPIO *s = RESET_GPIO(dev);
    qemu_set_irq(s->reset_out, 1);
    qemu_set_irq(s->reset_out, 0);
}

static void set_active(Object *obj, bool value, Error **errp)
{
    ResetGPIO *s = RESET_GPIO(obj);
    s->active = value;
    if (s->active) {
        qemu_register_reset(reset_gpio_reset, s);
    } else {
        qemu_unregister_reset(reset_gpio_reset, s);
    }
}
static bool get_active(Object *obj, Error **errp)
{
    ResetGPIO *s = RESET_GPIO(obj);
    return s->active;
}

static void reset_gpio_init(Object *dev)
{
    ResetGPIO *s = RESET_GPIO(dev);
    qdev_init_gpio_out_named(DEVICE(s), &s->reset_out, "reset_out", 1);
    object_property_add_bool(dev, "active", get_active, set_active);
    s->active = false;
}

static const TypeInfo reset_gpio_info = {
    .name          = TYPE_RESET_GPIO,
    .parent        = TYPE_DEVICE,
    .instance_init = reset_gpio_init,
    .instance_size = sizeof(ResetGPIO),
};

static void reset_gpio_register_types(void)
{
    type_register_static(&reset_gpio_info);
}

type_init(reset_gpio_register_types)
