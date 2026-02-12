
/*
 * Copyright (c) 2026 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <qemu-plugin.h>

#if defined _WIN32 || defined __CYGWIN__
    #define QEMU_PLUGIN_API_EXTERNAL __declspec(dllimport)
#else
    #define QEMU_PLUGIN_API_EXTERNAL
#endif

/**
 * global_set_cci_param() set cci parameter, it is used to set the unique plugin
 * ID for a unique plugin key.
 *
 * @key: unique plugin key
 * @val: unique plugin ID (cci parameter)
 */
QEMU_PLUGIN_API_EXTERNAL
void global_set_cci_param(char *key, uint64_t val);

