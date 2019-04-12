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

#ifndef _LIBQEMU_WRAPPERS_TIMER_H
#define _LIBQEMU_WRAPPERS_TIMER_H

#include <stdint.h>

typedef struct QemuTimer QemuTimer;

typedef void (*LibQemuTimerCb)(void *);

int64_t libqemu_clock_virtual_get_ns(void);
QemuTimer *libqemu_timer_new_virtual_ns(LibQemuTimerCb cb, void *opaque);


#endif
