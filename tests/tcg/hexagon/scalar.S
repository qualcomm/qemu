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

#define SYS_write		 64
#define SYS_exit_group           94
#define SYS_exit                 93

#define FD_STDOUT                1

	.type	str,@object
	.section	.rodata
pass_str:
	.string	"PASS\n"
	.size	str, 6
fail_str:
	.string	"FAIL\n"
	.size	str, 6

    .data
.align 16
tmp:
    .word  0x0
    .word  0x0

.text
.global _start
_start:
    r0 = #tmp
    r1 = #1
    r2 = #0x11223344
    r3 = #0x55667788
    r4 = #0
    p0 = r1

    memw(r0+#0)=r4
    .word 0xaa00e200 /* { if (p0) memb(r0++#0x0):nt = r2 } */
    r5 = memb(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

    memw(r0+#0)=r4
    .word 0xaa40e200 /* { if (p0) memh(r0++#0x0):nt = r2 } */
    r5 = memh(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

    memw(r0+#0)=r4
    .word 0xaa80e200 /* { if (p0) memw(r0++#0x0):nt = r2 } */
    r5 = memw(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

    memw(r0+#0)=r4
    .word 0xaac0e200 /* { if (p0) memd(r0++#0x0):nt = r3:2 } */
    r5 = memw(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail
    r5 = memw(r0+#4)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

    r1 = #0
    p0 = r1

    memw(r0+#0)=r4
    .word 0xaa00e204 /* { if (!p0) memb(r0++#0x0):nt = r2 } */
    r5 = memb(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

    memw(r0+#0)=r4
    .word 0xaa40e204 /* { if (!p0) memh(r0++#0x0):nt = r2 } */
    r5 = memh(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

    memw(r0+#0)=r4
    .word 0xaa80e204 /* { if (!p0) memw(r0++#0x0):nt = r2 } */
    r5 = memw(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

    memw(r0+#0)=r4
    memw(r0+#4)=r4
    .word 0xaac0e204 /* { if (!p0) memd(r0++#0x0):nt = r3:2 } */
    r5 = memw(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail
    r5 = memw(r0+#4)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

    memw(r0+#0)=r4
    .word 0xaa00c200 /* { memb(r0++#0x0):nt = r2 } */
    r5 = memb(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

    memw(r0+#0)=r4
    .word 0xaa40c200 /* { memh(r0++#0x0):nt = r2 } */
    r5 = memh(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

    memw(r0+#0)=r4
    .word 0xaa80c200 /* { memw(r0++#0x0):nt = r2 } */
    r5 = memw(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

    memw(r0+#0)=r4
    memw(r0+#4)=r4
    .word 0xaac0c200 /* { memd(r0++#0x0):nt = r3:2 } */
    r5 = memw(r0+#0)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail
    r5 = memw(r0+#4)
    p1 = !cmp.eq(r5, r4)
    if (!p1) jump fail

pass:
    r1 = ##pass_str
	r6 = #SYS_write
	r0 = #FD_STDOUT
	r2 = #5
	trap0(#1)
	r0 = #0
	r6 = #SYS_exit_group
	trap0(#1)

fail:
    r1 = ##fail_str
	r6 = #SYS_write
	r0 = #FD_STDOUT
	r2 = #5
	trap0(#1)
	r0 = #-1
	r6 = #SYS_exit_group
	trap0(#1)

