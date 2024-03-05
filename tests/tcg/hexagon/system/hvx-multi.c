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


#include "hvx-multi.h"
#include <stdio.h>
#include "filename.h"
#include <stdint.h>

enum {
  resource_unmapped = -1,
  resource_max      = 4,
  vector_unit_mask  = 0x38000000,
  vector_unit_0     = 0x20000000,  /* XA:100b */
  vector_unit_1     = 0x28000000,  /* XA:101b */
  vector_unit_2     = 0x30000000,  /* XA:110b */
  vector_unit_3     = 0x38000000,  /* XA:111b */
};

static inline uint32_t ssr()
{
    uint32_t reg;
    asm volatile ("%0=ssr;"
                  : "=r"(reg));
    return reg;
}

int main()
{
    unsigned int va[32] = {0};
    unsigned int vb[32] = {0};

    enable_vector_unit(vector_unit_0);
    setv0();
    store_vector_0(va);
    enable_vector_unit(vector_unit_1);
    store_vector_0(vb);

/*
 * At this point vector unit 0 v0 is all 1's
 * vector unit 1 v0 is all 0's
 * This test verifies that a new unit was selected and is
 * independend of the thread.
 */
    if ((vb[0] == 0) && (va[1] == 0xffffffff)) {
        printf("PASS : %s: vb[0] = 0x%x\n", __FILENAME__, vb[0]);
        return 0;
    }
    printf("FAIL : %s: va[0] = 0x%x\n", __FILENAME__, va[0]);
    printf("      vb[0] = 0x%x\n", vb[0]);
    return 1;

}
