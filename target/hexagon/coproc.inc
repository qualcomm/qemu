/*
 *  Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
