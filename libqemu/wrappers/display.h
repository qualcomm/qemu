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

#ifndef _LIBQEMU_WRAPPERS_DISPLAY_H
#define _LIBQEMU_WRAPPERS_DISPLAY_H

typedef struct DisplayOptions DisplayOptions;
typedef struct DisplaySurface DisplaySurface;
typedef struct QemuConsole QemuConsole;
typedef struct DisplayGLCtxOps DisplayGLCtxOps;
typedef struct DisplayGLCtx DisplayGLCtx;
typedef struct DisplayChangeListenerOps DisplayChangeListenerOps;
typedef struct DisplayChangeListener DisplayChangeListener;
typedef struct QEMUCursor QEMUCursor;
typedef void *QEMUGLContext;
typedef struct QEMUGLParams QEMUGLParams;

typedef void (*LibQemuGfxUpdateFn)(DisplayChangeListener *, int, int, int, int);
typedef void (*LibQemuGfxSwitchFn)(DisplayChangeListener *, DisplaySurface *);
typedef void (*LibQemuRefreshFn)(DisplayChangeListener *);

typedef bool (*LibQemuIsCompatibleDclFn)(DisplayGLCtx *,
                                         DisplayChangeListener *);

DisplayOptions *libqemu_display_options_new(void);

DisplayGLCtxOps *libqemu_display_gl_ctx_ops_new(LibQemuIsCompatibleDclFn);

const DisplayChangeListenerOps *libqemu_dcl_get_ops(DisplayChangeListener *);
void *libqemu_dcl_get_user_data(DisplayChangeListener *);

DisplayChangeListenerOps *libqemu_dcl_ops_new(void);
void libqemu_dcl_ops_set_name(DisplayChangeListenerOps *, const char *);
void libqemu_dcl_ops_set_gfx_update(DisplayChangeListenerOps *,
                                    LibQemuGfxUpdateFn);
void libqemu_dcl_ops_set_gfx_switch(DisplayChangeListenerOps *,
                                    LibQemuGfxSwitchFn);
void libqemu_dcl_ops_set_refresh(DisplayChangeListenerOps *, LibQemuRefreshFn);
#endif
