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

/*
 * Test the range of the l2vic interface.
 */

#include "filename.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "cfgtable.h"

#define L2VIC_INT_ENABLE(b, n)                                               \
    ((volatile unsigned int *)((b) + 0x100 + 4 * (n / 32))) /* device mem */

#define L2VIC_INT_ENABLE_SET(b, n)                                           \
    ((volatile unsigned int *)((b) + 0x200 + 4 * (n / 32))) /* device mem */

#define L2VIC_INT_ENABLE_CLEAR(b, n)                                         \
    ((volatile unsigned int *)((b) + 0x180 + 4 * (n / 32))) /* device mem */

#define L2VIC_SOFT_INT_SET(b, n)                                             \
    ((volatile unsigned int *)((b) + 0x480 + 4 * (n / 32))) /* device mem */

#define L2VIC_INT_TYPE(b, n)                                                 \
    ((volatile unsigned int *)((b) + 0x280 + 4 * (n / 32))) /* device mem */

volatile int pass; /* must use volatile */
int g_irq;
volatile uint32_t g_l2vic_base; /* must use volatile */


/*
 * When complete the irqlog will contain the value of the vid when the
 * handler was active.
 */
#define INTMAX 1024
#define LEFT_SET 666

int main()
{
    unsigned int irq_bit;
    unsigned int left_set = 0;
    int ret = 0;

    /* setup the fastl2vic interface and setup an indirect mapping */
    g_l2vic_base = GET_SUBSYSTEM_BASE() + 0x10000;

    /* Setup interrupts */
    for (int irq = 1; irq < INTMAX; irq++) {
        irq_bit = (1 << (irq % 32));
        *L2VIC_INT_ENABLE(g_l2vic_base, irq) |= irq_bit;
    }

    /* Read them all back and check */
    for (int irq = 1; irq < INTMAX; irq++) {
        if ((*L2VIC_INT_ENABLE(g_l2vic_base, irq) & (1 << (irq % 32))) !=
            (1 << irq % 32)) {
            printf("%d: ERROR: irq: %d: 0x%x\n", __LINE__, irq,
                   *L2VIC_INT_ENABLE(g_l2vic_base, irq));
            ret = 1;
        }
    }
    /* Clear them all, except int 1 and LEFT_SET (test)  */
    for (int irq = 1; irq < INTMAX; irq++) {
        if (!(irq % LEFT_SET)) {
            continue;
        }
        irq_bit = (1 << (irq % 32));
        *L2VIC_INT_ENABLE_CLEAR(g_l2vic_base, irq) |= irq_bit;
    }

    /* make sure just LEFT_SET is set */
    for (int irq = 0; irq < INTMAX; irq++) {
        if ((*L2VIC_INT_ENABLE(g_l2vic_base, irq) & (1 << (irq % 32))) !=
            (0 << irq % 32)) {
            if (irq != LEFT_SET) {
                printf("%d: ERROR: irq: %d: 0x%x\n", __LINE__, irq,
                       *L2VIC_INT_ENABLE(g_l2vic_base, irq));
                ret = 1;
            } else {
                left_set = irq;
            }
        }
    }
    if (left_set == LEFT_SET) {
        printf("PASS : %s\n", __FILENAME__);
    }
    return ret;
}
