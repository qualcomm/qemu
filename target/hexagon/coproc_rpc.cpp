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

#if !defined(CONFIG_USER_ONLY) && !defined(_WIN32)

#pragma GCC diagnostic ignored "-Wundef"
#if !defined(__clang__)
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef uint64_t    hwaddr;
#include "coproc.h"
#include "coproc_rpc_imp.h"

#define ERROR 1
#define OK    0

static void *local_rpc;

extern "C" void rpc_exit_handler(void);
void rpc_exit_handler(void)
{
    if (local_rpc) {
        delete static_cast<RemoteRPC * >(local_rpc);
        local_rpc = NULL;
    }
}

extern "C" int hexagon_coproc_rpclib_init(const char *coproc_path);
int hexagon_coproc_rpclib_init(const char *coproc_path)
{
    char coproc_full_name[4096];

    if (!coproc_path) {
        return ERROR;
    }
    strncpy(coproc_full_name, coproc_path, sizeof(coproc_full_name) - 1);
    strncat(coproc_full_name, "//coproc_rpc_remote",
        sizeof(coproc_full_name) - 1);
    if (access(coproc_full_name, F_OK) != 0) {
        fprintf(stderr, "Fatal error: Hexagon COPROC not found: (%s)\n",
            coproc_full_name);
        return ERROR;
    }

    local_rpc = static_cast<void * >(new RemoteRPC(coproc_full_name));
    static_cast<RemoteRPC * >(local_rpc)->init();
    atexit(rpc_exit_handler);
    return OK;
}

extern "C" int hexagon_coproc_rpclib_call(const void *args);
int hexagon_coproc_rpclib_call(const void *args)
{
    if (local_rpc) {
        const CoprocArgs *coproc_args = (const CoprocArgs *)args;
        static_cast<RemoteRPC * >(local_rpc)->call_coproc(coproc_args->opcode,
            coproc_args->vtcm_base, coproc_args->vtcm_size,
            coproc_args->reg_usr, coproc_args->fd, coproc_args->page_size,
            coproc_args->arg1, coproc_args->arg2);
    }
    return 0;
}
#endif
