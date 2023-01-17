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

#include "console.h"
#include "display.h"

#include "ui/sdl2.h"

DisplayOptions *libqemu_display_options_new(void)
{
    struct DisplayOptions *opts = g_new0(struct DisplayOptions, 1);
    opts->has_gl = true;
    opts->gl = DISPLAYGL_MODE_ES;
    return opts;
}

struct sdl2_console *libqemu_sdl2_console_new(QemuConsole *con, void *user_data)
{
    struct sdl2_console *sdl2_con = g_new0(struct sdl2_console, 1);
    sdl2_con->dcl.con = con;
    sdl2_con->dcl.user_data = user_data;
    return sdl2_con;
}

void libqemu_sdl2_console_set_hidden(struct sdl2_console *sdl2_con, bool hidden)
{
    sdl2_con->hidden = hidden;
}

void libqemu_sdl2_console_set_idx(struct sdl2_console *sdl2_con, int idx)
{
    sdl2_con->idx = idx;
}

void libqemu_sdl2_console_set_opts(struct sdl2_console *sdl2_con,
                                   DisplayOptions *opts)
{
    sdl2_con->opts = opts;
}

void libqemu_sdl2_console_set_opengl(struct sdl2_console *sdl2_con, bool opengl)
{
    sdl2_con->opengl = opengl;
}

void libqemu_sdl2_console_set_dcl_ops(struct sdl2_console *sdl2_con,
                                      DisplayChangeListenerOps *dcl_ops)
{
    sdl2_con->dcl.ops = dcl_ops;
}

void libqemu_sdl2_console_set_dgc_ops(struct sdl2_console *sdl2_con,
                                      DisplayGLCtxOps *dgc_ops)
{
    sdl2_con->dgc.ops = dgc_ops;
}

SDL_Window *libqemu_sdl2_console_get_real_window(struct sdl2_console *sdl2_con)
{
    return sdl2_con->real_window;
}

DisplayChangeListener *
libqemu_sdl2_console_get_dcl(struct sdl2_console *sdl2_con)
{
    return &sdl2_con->dcl;
}

DisplayGLCtx *libqemu_sdl2_console_get_dgc(struct sdl2_console *sdl2_con)
{
    return &sdl2_con->dgc;
}

DisplayGLCtxOps *
libqemu_display_gl_ctx_ops_new(LibQemuIsCompatibleDclFn is_compatible_dcl_fn)
{
    DisplayGLCtxOps *ops = g_new0(DisplayGLCtxOps, 1);
    ops->dpy_gl_ctx_is_compatible_dcl = is_compatible_dcl_fn;
    ops->dpy_gl_ctx_create = sdl2_gl_create_context;
    ops->dpy_gl_ctx_destroy = sdl2_gl_destroy_context;
    ops->dpy_gl_ctx_make_current = sdl2_gl_make_context_current;
    return ops;
}

const DisplayChangeListenerOps *libqemu_dcl_get_ops(DisplayChangeListener *dcl)
{
    return dcl->ops;
}

void *libqemu_dcl_get_user_data(DisplayChangeListener *dcl)
{
    return dcl->user_data;
}

DisplayChangeListenerOps *libqemu_dcl_ops_new()
{
    DisplayChangeListenerOps *ops = g_new0(DisplayChangeListenerOps, 1);
    // Trivial functions, no need to run on main thread
    ops->dpy_gfx_check_format = console_gl_check_format;
    ops->dpy_mouse_set = sdl_mouse_warp;
    ops->dpy_cursor_define = sdl_mouse_define;
    ops->dpy_gl_scanout_disable = sdl2_gl_scanout_disable;
    ops->dpy_gl_scanout_texture = sdl2_gl_scanout_texture;
    ops->dpy_gl_update = sdl2_gl_scanout_flush;
    return ops;
}

void libqemu_dcl_ops_set_name(DisplayChangeListenerOps *ops, const char *name)
{
    ops->dpy_name = name;
}

void libqemu_dcl_ops_set_gfx_update(DisplayChangeListenerOps *ops,
                                    LibQemuGfxUpdateFn gfx_update_fn)
{
    ops->dpy_gfx_update = gfx_update_fn;
}

void libqemu_dcl_ops_set_gfx_switch(DisplayChangeListenerOps *ops,
                                    LibQemuGfxSwitchFn gfx_switch_fn)
{
    ops->dpy_gfx_switch = gfx_switch_fn;
}

void libqemu_dcl_ops_set_refresh(DisplayChangeListenerOps *ops,
                                 LibQemuRefreshFn refresh_fn)
{
    ops->dpy_refresh = refresh_fn;
}
