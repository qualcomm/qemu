/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifdef COPROC_STANDALONE_BUILD
#include "hex_arch_types.h"
#else
#include "qemu/osdep.h"
#include "exec/cpu_ldst.h"
#include "exec/exec-all.h"
#include "cpu.h"
#endif
#include "coproc.h"

/* this is called from the client side */
void coproc(const CoprocArgs *args)
{
#if !defined(CONFIG_USER_ONLY) && !defined(_WIN32)
    if (ATOMIC_LOAD(hexagon_coproc_available) == true) {
        hexagon_coproc_rpclib_call((const void *)args);
    }
#endif
}

