/*
 * libqemu
 *
 * Copyright (c) 2023 Qualcomm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/qmp/qlist.h"
#include "hw/qdev-properties.h"

#include "qdev.h"

void libqemu_qdev_prop_set_uint_array(DeviceState *dev, const char *label,
                                      unsigned int *arr, int size)
{
    QList *list = qlist_new();
    for (int i = 0; i < size; i++) {
        qlist_append_int(list, arr[i]);
    }
    return qdev_prop_set_array(dev, label, list);
}
