
/*
 * Copyright (c) 2026 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <qemu-plugin.h>

/**
 * global_set_cci_param() set cci parameter, it is used to set the unique plugin
 * ID for a unique plugin key.
 *
 * @key: unique plugin key
 * @val: unique plugin ID (cci parameter)
 */
/* checkpatch-ignore: AVOID_EXTERNS */
extern void global_set_cci_param(char *key, uint64_t val);

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

/*
 * The qemu_plugin_install installs the plugin, processes a command-line
 * argument to extract a key, and sets the CCI parameter using the plugin ID and
 * the extracted key.
 */
QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc,
                        char **argv)
{
    if (argc != 1) {
        fprintf(stderr, "Expected exactly one argument, but got %d\n", argc);
        return -1;
    }

    char *opt = argv[0];
    g_autoptr(GString) key = g_string_new("");
    g_auto(GStrv) tokens = g_strsplit(opt, "=", 2);
    if (g_strcmp0(tokens[0], "key") == 0) {
        g_string_append_printf(key, "%s", tokens[1]);
    } else {
        fprintf(stderr, "option parsing failed: %s\n", opt);
        return -1;
    }

    char *c_key = g_strdup(key->str);
    global_set_cci_param(c_key, id);
    g_free(c_key);
    return 0;
}
