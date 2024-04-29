/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_TARGET_ELF_H
#define HEXAGON_TARGET_ELF_H

static inline const char *cpu_get_model(uint32_t eflags)
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

#endif
