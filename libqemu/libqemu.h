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

#ifndef _LIBQEMU_H
#define _LIBQEMU_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "libqemu/exports/typedefs.h"
#include "libqemu/exports/struct.h"

#define libqemu_strify_(a) #a
#define libqemu_strify(a) libqemu_strify_(a)
#define LIBQEMU_INIT_SYM libqemu_init
#define LIBQEMU_INIT_SYM_STR libqemu_strify(LIBQEMU_INIT_SYM)

typedef LibQemuExports * (*LibQemuInitFct)(int argc, char **argv);

LibQemuExports *LIBQEMU_INIT_SYM(int argc, char **argv);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
