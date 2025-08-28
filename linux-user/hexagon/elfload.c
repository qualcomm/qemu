/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "qemu/osdep.h"
#include "qemu.h"
#include "loader.h"


const char *get_elf_cpu_model(uint32_t eflags)
{
    static char buf[32];
    int err;

    switch (eflags) {
    case 0x04:   /* v5  */
    case 0x05:   /* v55 */
    case 0x60:   /* v60 */
    case 0x61:   /* v61 */
    case 0x62:   /* v62 */
    case 0x65:   /* v65 */
    case 0x66:   /* v66 */
        return "v66";
    case 0x67:   /* v67 */
    case 0x8067: /* v67t */
        return "v67";
    case 0x68:   /* v68 */
        return "v68";
    case 0x69:   /* v69 */
        return "v69";
    case 0x71:   /* v71 */
    case 0x8071: /* v71t */
        return "v71";
    case 0x73:   /* v73 */
        return "v73";
    case 0x75:   /* v75 */
        return "v75";
    case 0x77:   /* v77 */
        return "v77";
    case 0x79:   /* v79 */
        return "v79";
    case 0x81:
        return "v81";
    }

    err = snprintf(buf, sizeof(buf), "unknown (0x%x)", eflags);
    return err >= 0 && err < sizeof(buf) ? buf : "unknown";
}
