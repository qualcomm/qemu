/*
 *  Copyright(c) 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#if !defined(CONFIG_USER_ONLY) && !defined(_WIN32)

static const char *get_coproc_relative_install_path(void)
{
    struct stat st;
    /*
     * By default, qemu is installed at:
     * <install-dir>/Tools/QEMUHexagon/bin/qemu-system-hexagon
     * And the coproc at:
     * <install-dir>/Tools/QEMUCoprocPlugin/
     */
    char *path = get_relocated_path(CONFIG_BINDIR "/../../QEMUCoprocPlugin/");
    if (stat(path, &st) || !S_ISDIR(st.st_mode)) {
	free(path);
        return NULL;
    }
    return path;
}

static const char *get_coproc_path(CPUHexagonState *env)

{
    HexagonCPU *cpu = env_archcpu(env);

    /* try command line first */
    if (cpu->coproc_path && *cpu->coproc_path != '\0') {
        /*
         * qemu_log("Hexagon COPROC: coproc=<path> found:\n"
         *    "\tUsing (%s)\n",
         *    cpu->coproc_path);
         */
        return g_strdup(cpu->coproc_path);
    }

    /* fallback to expected install path */
    const char *coproc_install_path = get_coproc_relative_install_path();
    if (coproc_install_path) {
        return coproc_install_path;
    }

    error_report("WARNING: Hexagon COPROC path not found:"
        " Either choose a machine with no coproc or specify its"
        " path through the -cpu coproc=<path> command line option.");
    return NULL;
}
#endif
