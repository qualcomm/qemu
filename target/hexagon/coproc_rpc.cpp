/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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

extern "C" int hexagon_coproc_rpclib_init(const char *coproc_path, int hex_rev);
int hexagon_coproc_rpclib_init(const char *coproc_path, int hex_rev)
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

    local_rpc = static_cast<void * >(new RemoteRPC(coproc_full_name, hex_rev));
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
            coproc_args->reg_usr, coproc_args->subsystem_id, coproc_args->page_size,
            coproc_args->arg1, coproc_args->arg2);
    }
    return 0;
}
#endif
