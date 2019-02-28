/*
 * libqemu
 *
 * Copyright (c) 2019 Luc Michel <luc.michel@greensocs.com>
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

#include "ctors.h"

static GArray *ctor_fcts = NULL;
static size_t ctor_count = 0;

void libqemu_early_register_ctor(QemuCtorFct fct)
{
    if (ctor_fcts == NULL) {
        ctor_fcts = g_array_new(FALSE, FALSE, sizeof(QemuCtorFct));
    }

    g_array_append_val(ctor_fcts, fct);
    ctor_count++;
}


void libqemu_call_ctors(void)
{
    size_t i;
    QemuCtorFct ctor;

    for (i = 0; i < ctor_count; i++) {
        ctor = g_array_index(ctor_fcts, QemuCtorFct, i);
        ctor();
    }

    g_array_free(ctor_fcts, TRUE);
    ctor_fcts = NULL;
    ctor_count = 0;
}
