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

#ifndef _LIBQEMU_CTORS_H
#define _LIBQEMU_CTORS_H

/*
 * Constructors registering and calling helpers.
 *
 * When QEMU is built with CONFIG_LIBQEMU enabled, the functions marked with
 * QEMU_ATTR_CONSTRUCTOR are registered for defered call, instead of being
 * called on dlopen().
 */

typedef void (*QemuCtorFct)(void);

/* Used by the QEMU_ATTR_CONSTRUCTOR macro */
void libqemu_early_register_ctor(QemuCtorFct fct);

/* Call all the QEMU contructors */
void libqemu_call_ctors(void);

#endif
