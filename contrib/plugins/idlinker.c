/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <qemu-plugin.h>

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
    char key[256] = {0};
    char *tokens[2] = {0};

    tokens[0] = strtok(opt, "=");
    tokens[1] = strtok(NULL, "=");

    if (strcmp(tokens[0], "key") == 0) {
        snprintf(key, sizeof(key), "%s", tokens[1]);
    } else {
        fprintf(stderr, "option parsing failed: %s\n", opt);
        return -1;
    }

    char *c_key = strdup(key);
    global_set_cci_param(c_key, id);
    free(c_key);
    return 0;
}
