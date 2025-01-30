/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>


#define VEC_SIZE 128

#define QUOTE_IT(s) #s
#define DO_ASSIGN(DST, SRC) QUOTE_IT(DST = SRC)
#define DO_VSPLAT(DST, SRC) QUOTE_IT(DST = vsplat(SRC))
#define SET_GPR(REG, VAL)   __asm__ __volatile__(DO_ASSIGN(REG, VAL) : : : #REG)
#define SET_VREG(VREG, REG) __asm__ __volatile__(DO_VSPLAT(VREG, REG) : : : #VREG)
#define INIT_REGS(VREG, REG, VAL) SET_GPR(REG, VAL) ; SET_VREG(VREG, REG)

void reg_init()

{
    INIT_REGS(v10, r10, #0xffffff1c);
    INIT_REGS(v11, r11, #0xffffffea);
    INIT_REGS(v21, r21, #0x95bcf74c);
}

void reg_init2()

{
    INIT_REGS(v16, r16, #0x000000b7);
    INIT_REGS(v17, r17, #0x31fe88e7);
    INIT_REGS(v24, r24, #0x00000017);
    INIT_REGS(v25, r25, #0xd5b9a407);
    INIT_REGS(v26, r26, #0xffffff4e);
    INIT_REGS(v27, r27, #0x7fffff79);
}

void test_case1(void *pout)

{
    __asm__ __volatile__("v21.uw = vadd(v11.uw,v10.uw):sat" : : : "v21");
    __asm__ __volatile__("vmemu(%0+#0) = v21" : : "r"(pout) : "memory");
}

void test_case2(void *pout, void *pout2)

{
	__asm__ __volatile__("v25:24.uw=vsub(v17:16.uw,v27:26.uw):sat" : : : "v24", "v25");
    __asm__ __volatile__("vmemu(%0+#0) = v24" : : "r"(pout) : "memory");
    __asm__ __volatile__("vmemu(%0+#0) = v25" : : "r"(pout2) : "memory");
}

void dump_vec(const char *str, void *pout)

{
    unsigned char *ptr = (unsigned char *)pout;

    printf("\n%s = 0x", str);
    for (int i = 0; i < VEC_SIZE ; ++i) {
        printf("%2.2x", ptr[i]);
    }
    puts("\n");
}

int main()

{
    unsigned char vec[VEC_SIZE];
    unsigned char vec2[VEC_SIZE];
    unsigned char golden[VEC_SIZE];

    memset(golden, 0xff, VEC_SIZE);
    memset(vec, 0x00, VEC_SIZE);
    memset(vec2, 0x00, VEC_SIZE);
    reg_init();
    test_case1(&vec[0]);
    if (memcmp(vec, golden, VEC_SIZE) != 0) {
        printf("FAIL (output should be all %2.2Xs)\n", golden[0]);
        dump_vec("v21", &vec[0]);
        exit(-1);
    }


    memset(golden, 0x00, VEC_SIZE);
    memset(vec, 0xff, VEC_SIZE);
    memset(vec2, 0xff, VEC_SIZE);
    reg_init2();
    test_case2(&vec[0], &vec2[0]);
    if ((memcmp(vec, golden, VEC_SIZE) != 0) ||
        (memcmp(vec2, golden, VEC_SIZE) != 0)) {
        printf("FAIL (output should be all %2.2Xs)\n", golden[0]);
        dump_vec("v24", &vec[0]);
        dump_vec("v25", &vec2[0]);
        exit(-1);
    }

    printf("PASS\n");
    exit(0);
}
