/*
 * libqemu
 *
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef _LIBQEMU_WRAPPERS_CONSOLE_H
#define _LIBQEMU_WRAPPERS_CONSOLE_H

#include "qemu/osdep.h"
#include "ui/console.h"

struct sdl2_console;
typedef struct SDL_Window SDL_Window;

struct sdl2_console *libqemu_sdl2_console_new(QemuConsole *, void *);
void libqemu_sdl2_console_set_hidden(struct sdl2_console *, bool);
void libqemu_sdl2_console_set_idx(struct sdl2_console *, int);
void libqemu_sdl2_console_set_opts(struct sdl2_console *, DisplayOptions *);
void libqemu_sdl2_console_set_opengl(struct sdl2_console *, bool);
void libqemu_sdl2_console_set_dcl_ops(struct sdl2_console *,
                                      DisplayChangeListenerOps *);
void libqemu_sdl2_console_set_dgc_ops(struct sdl2_console *, DisplayGLCtxOps *);
SDL_Window *libqemu_sdl2_console_get_real_window(struct sdl2_console *);
DisplayChangeListener *libqemu_sdl2_console_get_dcl(struct sdl2_console *);
DisplayGLCtx *libqemu_sdl2_console_get_dgc(struct sdl2_console *);

#endif
