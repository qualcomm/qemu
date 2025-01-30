/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <string.h>

/*
 * There are a total of 128 predicated store instructions.
 * Lots of variations on the same pattern
 * - 4 addressing modes
 * - old or new value of the predicate
 * - old or new value to store
 * - size of the operand (byte, half word, word, double word)
 * - true or false sense of the predicate
 * - half word stores can also use the high half or the register
 *
 * We use the C preprocessor to our advantage when building the combinations.
 *
 * The test cases are organized by addressing mode.  For each addressing mode,
 * there are 4 snippets of inline asm
 * - old predicate, old value
 * - new predicate, old value
 * - old predicate, new value
 * - new predicate, new value
 * Pay special attention to the way the packets are formed for each case.
 *
 * Next, we define the possible instructions for each operand size.

 * Next, we define the test macros (32 bit and 64 bit) for an instruction.
 * The test macros will invoke the instruction twice
 * - true predicate, check that the value was stored
 * - false predicate, check that the value was not stored
 *
 * Finally, we instantiate all the tests.
 */

int array[10];
int init[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
long long array64[10];
long long init64[] = { 0LL, 1LL, 2LL, 3LL, 4LL, 5LL, 6LL, 7LL, 8LL, 9LL };
int value = 0x12;
long long value64 = 0x12LL;
int err;
int *p;
long long *p64;

#define check(val, expect) \
    do { \
        if (val != expect) { \
            printf("ERROR: %d != %d\n", val, expect); \
            err++; \
        } \
    } while (0)

#define check64(val, expect) \
    do { \
        if (val != expect) { \
            printf("ERROR (64): %lld != %lld\n", val, expect); \
            err++; \
        } \
    } while (0)

#define checkp(val, expect) \
    do { \
        if (val != expect) { \
            printf("ERROR (p): 0x%x != 0x%x\n", \
                   (unsigned int)(val), (unsigned int)(expect)); \
            err++; \
        } \
    } while (0)

/*
 ****************************************************************************
 *  _rr addressing mode (base + index << shift)
 */
#define pstoreX_old_rr(ARG, BASE, INDEX, SH, VAL, PRED, SIZE, PART) \
    asm ("p0 = cmp.eq(%0, #1)\n\t" \
         "if (" #PRED ") mem" #SIZE "(%1 + %2 << #" #SH ") = %3" PART "\n\t" \
         : : "r"(ARG), "r"(BASE), "r"(INDEX), "r"(VAL) \
         : "p0", "memory")
#define pstoreX_new_rr(ARG, BASE, INDEX, SH, VAL, PRED, SIZE, PART) \
    asm ("{\n\t" \
         "    p0 = cmp.eq(%0, #1)\n\t" \
         "    if (" #PRED ".new) " \
                  "mem" #SIZE "(%1 + %2 << #" #SH ") = %3" PART "\n\t" \
         "}\n\t" \
         : : "r"(ARG), "r"(BASE), "r"(INDEX), "r"(VAL) \
         : "p0", "memory")
#define pstore_new_X_old_rr(ARG, BASE, INDEX, SH, VAL, PRED, SIZE) \
    asm ("p0 = cmp.eq(%0, #1)\n\t" \
         "{\n\t" \
         "    r5 = %3\n\t" \
         "    if (" #PRED ") " \
                  "mem" #SIZE "(%1 + %2 << #" #SH ") = r5.new\n\t" \
         "}\n\t" \
         : : "r"(ARG), "r"(BASE), "r"(INDEX), "r"(VAL) \
         : "p0", "r5", "memory")
#define pstore_new_X_new_rr(ARG, BASE, INDEX, SH, VAL, PRED, SIZE) \
    asm ("{\n\t" \
         "    p0 = cmp.eq(%0, #1)\n\t" \
         "    r5 = %3\n\t" \
         "    if (" #PRED ".new) " \
                  "mem" #SIZE "(%1 + %2 << #" #SH ") = r5.new\n\t" \
         "}\n\t" \
         : : "r"(ARG), "r"(BASE), "r"(INDEX), "r"(VAL) \
         : "p0", "r5", "memory")

/* Byte */
#define S4_pstorerbt_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_old_rr(ARG, BASE, INDEX, SH, VAL, p0, b, "")
#define S4_pstorerbf_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_old_rr(ARG, BASE, INDEX, SH, VAL, !p0, b, "")
#define S4_pstorerbtnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_new_rr(ARG, BASE, INDEX, SH, VAL, p0, b, "")
#define S4_pstorerbfnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_new_rr(ARG, BASE, INDEX, SH, VAL, !p0, b, "")
#define S4_pstorerbnewt_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_old_rr(ARG, BASE, INDEX, SH, VAL, p0, b)
#define S4_pstorerbnewf_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_old_rr(ARG, BASE, INDEX, SH, VAL, !p0, b)
#define S4_pstorerbnewtnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_new_rr(ARG, BASE, INDEX, SH, VAL, p0, b)
#define S4_pstorerbnewfnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_new_rr(ARG, BASE, INDEX, SH, VAL, !p0, b)

/* Half word */
#define S4_pstorerht_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_old_rr(ARG, BASE, INDEX, SH, VAL, p0, h, "")
#define S4_pstorerhf_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_old_rr(ARG, BASE, INDEX, SH, VAL, !p0, h, "")
#define S4_pstorerhtnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_new_rr(ARG, BASE, INDEX, SH, VAL, p0, h, "")
#define S4_pstorerhfnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_new_rr(ARG, BASE, INDEX, SH, VAL, !p0, h, "")
#define S4_pstorerft_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_old_rr(ARG, BASE, INDEX, SH, VAL, p0, h, ".H")
#define S4_pstorerff_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_old_rr(ARG, BASE, INDEX, SH, VAL, !p0, h, ".H")
#define S4_pstorerftnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_new_rr(ARG, BASE, INDEX, SH, VAL, p0, h, ".H")
#define S4_pstorerffnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_new_rr(ARG, BASE, INDEX, SH, VAL, !p0, h, ".H")
#define S4_pstorerhnewt_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_old_rr(ARG, BASE, INDEX, SH, VAL, p0, h)
#define S4_pstorerhnewf_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_old_rr(ARG, BASE, INDEX, SH, VAL, !p0, h)
#define S4_pstorerhnewtnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_new_rr(ARG, BASE, INDEX, SH, VAL, p0, h)
#define S4_pstorerhnewfnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_new_rr(ARG, BASE, INDEX, SH, VAL, !p0, h)

/* Word */
#define S4_pstorerit_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_old_rr(ARG, BASE, INDEX, SH, VAL, p0, w, "")
#define S4_pstorerif_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_old_rr(ARG, BASE, INDEX, SH, VAL, !p0, w, "")
#define S4_pstoreritnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_new_rr(ARG, BASE, INDEX, SH, VAL, p0, w, "")
#define S4_pstorerifnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_new_rr(ARG, BASE, INDEX, SH, VAL, !p0, w, "")
#define S4_pstorerinewt_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_old_rr(ARG, BASE, INDEX, SH, VAL, p0, w)
#define S4_pstorerinewf_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_old_rr(ARG, BASE, INDEX, SH, VAL, !p0, w)
#define S4_pstorerinewtnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_new_rr(ARG, BASE, INDEX, SH, VAL, p0, w)
#define S4_pstorerinewfnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstore_new_X_new_rr(ARG, BASE, INDEX, SH, VAL, !p0, w)

/* Double word */
#define S4_pstorerdt_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_old_rr(ARG, BASE, INDEX, SH, VAL, p0, d, "")
#define S4_pstorerdf_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_old_rr(ARG, BASE, INDEX, SH, VAL, !p0, d, "")
#define S4_pstorerdtnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_new_rr(ARG, BASE, INDEX, SH, VAL, p0, d, "")
#define S4_pstorerdfnew_rr(ARG, BASE, INDEX, SH, VAL) \
    pstoreX_new_rr(ARG, BASE, INDEX, SH, VAL, !p0, d, "")

/*
 * For each instruction, perform both a true and a false test
 */
#define do_test_rr(INSN, T, F, INDEX, SH, VAL, EXPECT) \
    do { \
        memcpy(array, init, sizeof(init)); \
        INSN(T, array, INDEX, SH, VAL); \
        check(array[INDEX], EXPECT); \
        memcpy(array, init, sizeof(init)); \
        INSN(F, array, INDEX, SH, VAL); \
        check(array[INDEX], init[INDEX]); \
    } while (0)
#define do_test64_rr(INSN, T, F, INDEX, SH, VAL, EXPECT) \
    do { \
        memcpy(array64, init64, sizeof(init64)); \
        INSN(T, array64, INDEX, SH, VAL); \
        check64(array64[INDEX], EXPECT); \
        memcpy(array64, init64, sizeof(init64)); \
        INSN(F, array64, INDEX, SH, VAL); \
        check64(array64[INDEX], init64[INDEX]); \
    } while (0)

static void test_rr(void)
{
    do_test_rr(S4_pstorerbt_rr,       1, 0, 0, 2, 0xff,       0xff);
    do_test_rr(S4_pstorerbf_rr,       0, 1, 1, 2, 0xab,       0xab);
    do_test_rr(S4_pstorerbtnew_rr,    1, 0, 2, 2, 0x12,       0x12);
    do_test_rr(S4_pstorerbfnew_rr,    0, 1, 3, 2, 0x00,       0x00);
    do_test_rr(S4_pstorerbnewt_rr,    1, 0, 4, 2, 0x56,       0x56);
    do_test_rr(S4_pstorerbnewf_rr,    0, 1, 5, 2, 0x56,       0x56);
    do_test_rr(S4_pstorerbnewtnew_rr, 1, 0, 6, 2, 0x78,       0x78);
    do_test_rr(S4_pstorerbnewfnew_rr, 0, 1, 7, 2, 0xab,       0xab);

    do_test_rr(S4_pstorerht_rr,       1, 0, 0, 2, 0x1234,     0x1234);
    do_test_rr(S4_pstorerhf_rr,       0, 1, 1, 2, 0x2345,     0x2345);
    do_test_rr(S4_pstorerhtnew_rr,    1, 0, 2, 2, 0x3456,     0x3456);
    do_test_rr(S4_pstorerhfnew_rr,    0, 1, 3, 2, 0x4567,     0x4567);
    do_test_rr(S4_pstorerhnewt_rr,    1, 0, 4, 2, 0x5678,     0x5678);
    do_test_rr(S4_pstorerhnewf_rr,    0, 1, 5, 2, 0x6789,     0x6789);
    do_test_rr(S4_pstorerhnewtnew_rr, 1, 0, 6, 2, 0x7890,     0x7890);
    do_test_rr(S4_pstorerhnewfnew_rr, 0, 1, 7, 2, 0x890a,     0x890a);
    do_test_rr(S4_pstorerft_rr,       1, 0, 8, 2, 0xabcdffff, 0xabcd);
    do_test_rr(S4_pstorerff_rr,       0, 1, 9, 2, 0xbcdeffff, 0xbcde);
    do_test_rr(S4_pstorerftnew_rr,    1, 0, 0, 2, 0xcdefffff, 0xcdef);
    do_test_rr(S4_pstorerffnew_rr,    0, 1, 1, 2, 0x1111ffff, 0x1111);

    do_test_rr(S4_pstorerit_rr,       1, 0, 0, 2, 0x12345678, 0x12345678);
    do_test_rr(S4_pstorerif_rr,       0, 1, 1, 2, 0x23456789, 0x23456789);
    do_test_rr(S4_pstoreritnew_rr,    1, 0, 2, 2, 0x3456789a, 0x3456789a);
    do_test_rr(S4_pstorerifnew_rr,    0, 1, 3, 2, 0x456789ab, 0x456789ab);
    do_test_rr(S4_pstorerinewt_rr,    1, 0, 4, 2, 0x56789abc, 0x56789abc);
    do_test_rr(S4_pstorerinewf_rr,    0, 1, 5, 2, 0x6789abcd, 0x6789abcd);
    do_test_rr(S4_pstorerinewtnew_rr, 1, 0, 6, 2, 0x789abcde, 0x789abcde);
    do_test_rr(S4_pstorerinewfnew_rr, 0, 1, 7, 2, 0x89abcdef, 0x89abcdef);

    do_test64_rr(S4_pstorerdt_rr,     1, 0, 0, 3, 0xffffLL,   0xffffLL);
    do_test64_rr(S4_pstorerdf_rr,     0, 1, 1, 3, 0x4567LL,   0x4567LL);
    do_test64_rr(S4_pstorerdtnew_rr,  1, 0, 2, 3, 0xabcdLL,   0xabcdLL);
    do_test64_rr(S4_pstorerdfnew_rr,  0, 1, 3, 3, 0x7654LL,   0x7654LL);
}

/*
 ****************************************************************************
 *  _pi addressing mode (addr ++ increment)
 */
#define pstoreX_old_pi(ARG, ADDR, INC, VAL, PRED, SIZE, PART) \
    asm ("p0 = cmp.eq(%1, #1)\n\t" \
         "if (" #PRED ") mem" #SIZE "(%0 ++ #" #INC ") = %2" PART "\n\t" \
         : "+r"(ADDR) : "r"(ARG), "r"(VAL) \
         : "p0", "memory")
#define pstoreX_new_pi(ARG, ADDR, INC, VAL, PRED, SIZE, PART) \
    asm ("{\n\t" \
         "    p0 = cmp.eq(%1, #1)\n\t" \
         "    if (" #PRED ".new)" \
                  " mem" #SIZE "(%0 ++ #" #INC ") = %2" PART "\n\t" \
         "}\n\t" \
         : "+r"(ADDR) : "r"(ARG), "r"(VAL) \
         : "p0", "memory")
#define pstore_new_X_old_pi(ARG, ADDR, INC, VAL, PRED, SIZE) \
    asm ("p0 = cmp.eq(%1, #1)\n\t" \
         "{\n\t" \
         "    r5 = %2\n\t" \
         "    if (" #PRED ") mem" #SIZE "(%0 ++ #" #INC ") = r5.new\n\t" \
         "}\n\t" \
         : "+r"(ADDR) : "r"(ARG), "r"(VAL) \
         : "p0", "r5", "memory")
#define pstore_new_X_new_pi(ARG, ADDR, INC, VAL, PRED, SIZE) \
    asm ("{\n\t" \
         "    p0 = cmp.eq(%1, #1)\n\t" \
         "    r5 = %2\n\t" \
         "    if (" #PRED ".new) mem" #SIZE "(%0 ++ #" #INC ") = r5.new\n\t" \
         "}\n\t" \
         : "+r"(ADDR) : "r"(ARG), "r"(VAL) \
         : "p0", "r5", "memory")

/* Byte */
#define S2_pstorerbt_pi(ARG, ADDR, INC, VAL) \
    pstoreX_old_pi(ARG, ADDR, INC, VAL, p0, b, "")
#define S2_pstorerbf_pi(ARG, ADDR, INC, VAL) \
    pstoreX_old_pi(ARG, ADDR, INC, VAL, !p0, b, "")
#define S2_pstorerbtnew_pi(ARG, ADDR, INC, VAL) \
    pstoreX_new_pi(ARG, ADDR, INC, VAL, p0, b, "")
#define S2_pstorerbfnew_pi(ARG, ADDR, INC, VAL) \
    pstoreX_new_pi(ARG, ADDR, INC, VAL, !p0, b, "")
#define S2_pstorerbnewt_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_old_pi(ARG, ADDR, INC, VAL, p0, b)
#define S2_pstorerbnewf_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_old_pi(ARG, ADDR, INC, VAL, !p0, b)
#define S2_pstorerbnewtnew_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_new_pi(ARG, ADDR, INC, VAL, p0, b)
#define S2_pstorerbnewfnew_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_new_pi(ARG, ADDR, INC, VAL, !p0, b)

/* Half word */
#define S2_pstorerht_pi(ARG, ADDR, INC, VAL) \
    pstoreX_old_pi(ARG, ADDR, INC, VAL, p0, h, "")
#define S2_pstorerhf_pi(ARG, ADDR, INC, VAL) \
    pstoreX_old_pi(ARG, ADDR, INC, VAL, !p0, h, "")
#define S2_pstorerhtnew_pi(ARG, ADDR, INC, VAL) \
    pstoreX_new_pi(ARG, ADDR, INC, VAL, p0, h, "")
#define S2_pstorerhfnew_pi(ARG, ADDR, INC, VAL) \
    pstoreX_new_pi(ARG, ADDR, INC, VAL, !p0, h, "")
#define S2_pstorerft_pi(ARG, ADDR, INC, VAL) \
    pstoreX_old_pi(ARG, ADDR, INC, VAL, p0, h, ".H")
#define S2_pstorerff_pi(ARG, ADDR, INC, VAL) \
    pstoreX_old_pi(ARG, ADDR, INC, VAL, !p0, h, ".H")
#define S2_pstorerftnew_pi(ARG, ADDR, INC, VAL) \
    pstoreX_new_pi(ARG, ADDR, INC, VAL, p0, h, ".H")
#define S2_pstorerffnew_pi(ARG, ADDR, INC, VAL) \
    pstoreX_new_pi(ARG, ADDR, INC, VAL, !p0, h, ".H")
#define S2_pstorerhnewt_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_old_pi(ARG, ADDR, INC, VAL, p0, h)
#define S2_pstorerhnewf_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_old_pi(ARG, ADDR, INC, VAL, !p0, h)
#define S2_pstorerhnewtnew_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_new_pi(ARG, ADDR, INC, VAL, p0, h)
#define S2_pstorerhnewfnew_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_new_pi(ARG, ADDR, INC, VAL, !p0, h)

/* Word */
#define S2_pstorerit_pi(ARG, ADDR, INC, VAL) \
    pstoreX_old_pi(ARG, ADDR, INC, VAL, p0, w, "")
#define S2_pstorerif_pi(ARG, ADDR, INC, VAL) \
    pstoreX_old_pi(ARG, ADDR, INC, VAL, !p0, w, "")
#define S2_pstoreritnew_pi(ARG, ADDR, INC, VAL) \
    pstoreX_new_pi(ARG, ADDR, INC, VAL, p0, w, "")
#define S2_pstorerifnew_pi(ARG, ADDR, INC, VAL) \
    pstoreX_new_pi(ARG, ADDR, INC, VAL, !p0, w, "")
#define S2_pstorerinewt_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_old_pi(ARG, ADDR, INC, VAL, p0, w)
#define S2_pstorerinewf_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_old_pi(ARG, ADDR, INC, VAL, !p0, w)
#define S2_pstorerinewtnew_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_new_pi(ARG, ADDR, INC, VAL, p0, w)
#define S2_pstorerinewfnew_pi(ARG, ADDR, INC, VAL) \
    pstore_new_X_new_pi(ARG, ADDR, INC, VAL, !p0, w)

/* Double word */
#define S2_pstorerdt_pi(ARG, ADDR, INC, VAL) \
    pstoreX_old_pi(ARG, ADDR, INC, VAL, p0, d, "")
#define S2_pstorerdf_pi(ARG, ADDR, INC, VAL) \
    pstoreX_old_pi(ARG, ADDR, INC, VAL, !p0, d, "")
#define S2_pstorerdtnew_pi(ARG, ADDR, INC, VAL) \
    pstoreX_new_pi(ARG, ADDR, INC, VAL, p0, d, "")
#define S2_pstorerdfnew_pi(ARG, ADDR, INC, VAL) \
    pstoreX_new_pi(ARG, ADDR, INC, VAL, !p0, d, "")

#define do_test_pi(INSN, T, F, INDEX, INC, VAL, EXPECT) \
    do { \
        memcpy(array, init, sizeof(init)); \
        p = &array[INDEX]; \
        INSN(T, p, INC, VAL); \
        check(array[INDEX], EXPECT); \
        checkp(p, &array[INDEX + INC / 4]); \
        memcpy(array, init, sizeof(init)); \
        p = &array[INDEX]; \
        INSN(F, p, INC, 0xffff); \
        check(array[INDEX], init[INDEX]); \
        checkp(p, &array[INDEX]); \
    } while (0)
#define do_test64_pi(INSN, T, F, INDEX, INC, VAL, EXPECT) \
    do { \
        memcpy(array64, init64, sizeof(init64)); \
        p64 = &array64[INDEX]; \
        INSN(T, p64, INC, VAL); \
        check64(array64[INDEX], EXPECT); \
        checkp(p64, &array64[INDEX + INC / 8]); \
        memcpy(array64, init64, sizeof(init64)); \
        p64 = &array64[INDEX]; \
        INSN(F, p64, INC, VAL); \
        check64(array64[INDEX], init64[INDEX]); \
        checkp(p64, &array64[INDEX]); \
    } while (0)

static void test_pi(void)
{
    do_test_pi(S2_pstorerbt_pi,       1, 0, 0, 4, 0x12,       0x12);
    do_test_pi(S2_pstorerbf_pi,       0, 1, 1, 4, 0x23,       0x23);
    do_test_pi(S2_pstorerbtnew_pi,    1, 0, 2, 4, 0x34,       0x34);
    do_test_pi(S2_pstorerbfnew_pi,    0, 1, 3, 4, 0x45,       0x45);
    do_test_pi(S2_pstorerbnewt_pi,    1, 0, 4, 4, 0x56,       0x56);
    do_test_pi(S2_pstorerbnewf_pi,    0, 1, 5, 4, 0x67,       0x67);
    do_test_pi(S2_pstorerbnewtnew_pi, 1, 0, 6, 4, 0x78,       0x78);
    do_test_pi(S2_pstorerbnewfnew_pi, 0, 1, 7, 4, 0x89,       0x89);

    do_test_pi(S2_pstorerht_pi,       1, 0, 0, 8, 0x1234,     0x1234);
    do_test_pi(S2_pstorerhf_pi,       0, 1, 1, 8, 0x2345,     0x2345);
    do_test_pi(S2_pstorerhtnew_pi,    1, 0, 2, 8, 0x3456,     0x3456);
    do_test_pi(S2_pstorerhfnew_pi,    0, 1, 3, 8, 0x4567,     0x4567);
    do_test_pi(S2_pstorerft_pi,       1, 0, 4, 8, 0x5678ffff, 0x5678);
    do_test_pi(S2_pstorerff_pi,       0, 1, 5, 8, 0x6789ffff, 0x6789);
    do_test_pi(S2_pstorerftnew_pi,    1, 0, 6, 8, 0x789affff, 0x789a);
    do_test_pi(S2_pstorerffnew_pi,    0, 1, 7, 8, 0x89abffff, 0x89ab);
    do_test_pi(S2_pstorerhnewt_pi,    1, 0, 8, 8, 0x9abc,     0x9abc);
    do_test_pi(S2_pstorerhnewf_pi,    0, 1, 0, 8, 0xabcd,     0xabcd);
    do_test_pi(S2_pstorerhnewtnew_pi, 1, 0, 1, 8, 0xbcde,     0xbcde);
    do_test_pi(S2_pstorerhnewfnew_pi, 0, 1, 2, 8, 0xcdef,     0xcdef);

    do_test_pi(S2_pstorerit_pi,       1, 0, 4, 8, 0x12345678, 0x12345678);
    do_test_pi(S2_pstorerif_pi,       0, 1, 4, 8, 0x23456789, 0x23456789);
    do_test_pi(S2_pstoreritnew_pi,    1, 0, 4, 8, 0x3456789a, 0x3456789a);
    do_test_pi(S2_pstorerifnew_pi,    0, 1, 4, 8, 0x456789ab, 0x456789ab);
    do_test_pi(S2_pstorerinewt_pi,    1, 0, 4, 8, 0x56789abc, 0x56789abc);
    do_test_pi(S2_pstorerinewf_pi,    0, 1, 4, 8, 0x6789abcd, 0x6789abcd);
    do_test_pi(S2_pstorerinewtnew_pi, 1, 0, 4, 8, 0x789abcde, 0x789abcde);
    do_test_pi(S2_pstorerinewfnew_pi, 0, 1, 4, 8, 0x89abcdef, 0x89abcdef);

    do_test64_pi(S2_pstorerdt_pi,     1, 0, 3, 8, 0x1234LL,   0x1234LL);
    do_test64_pi(S2_pstorerdf_pi,     0, 1, 3, 8, 0x2345LL,   0x2345LL);
    do_test64_pi(S2_pstorerdtnew_pi,  1, 0, 3, 8, 0x3456LL,   0x3456LL);
    do_test64_pi(S2_pstorerdfnew_pi,  0, 1, 3, 8, 0x4567LL,   0x4567LL);
}

/*
 ****************************************************************************
 * _io addressing mode (addr + offset)
 */
#define pstoreX_old_io(ARG, ADDR, OFF, VAL, PRED, SIZE, PART) \
    asm ("p0 = cmp.eq(%1, #1)\n\t" \
         "if (" #PRED ") mem" #SIZE "(%0 + #" #OFF ") = %2" PART "\n\t" \
         : : "r"(ADDR), "r"(ARG), "r"(VAL) \
         : "p0", "memory")
#define pstoreX_new_io(ARG, ADDR, OFF, VAL, PRED, SIZE, PART) \
    asm ("{\n\t" \
         "    p0 = cmp.eq(%1, #1)\n\t" \
         "    if (" #PRED ".new)" \
                  " mem" #SIZE "(%0 + #" #OFF ") = %2" PART "\n\t" \
         "}\n\t" \
         : : "r"(ADDR), "r"(ARG), "r"(VAL) \
         : "p0", "memory")
#define pstore_new_X_old_io(ARG, ADDR, OFF, VAL, PRED, SIZE) \
    asm ("p0 = cmp.eq(%0, #1)\n\t" \
         "{\n\t" \
         "    r5 = %2\n\t" \
         "    if (" #PRED ") mem" #SIZE "(%1 + #" #OFF ") = r5.new\n\t" \
         "}\n\t" \
         : : "r"(ARG), "r"(ADDR), "r"(VAL) \
         : "p0", "r5", "memory")
#define pstore_new_X_new_io(ARG, ADDR, OFF, VAL, PRED, SIZE) \
    asm ("{\n\t" \
         "    p0 = cmp.eq(%0, #1)\n\t" \
         "    r5 = %2\n\t" \
         "    if (" #PRED ".new) mem" #SIZE "(%1 + #" #OFF ") = r5.new\n\t" \
         "}\n\t" \
         : : "r"(ARG), "r"(ADDR), "r"(VAL) \
         : "p0", "r5", "memory")

/* Byte */
#define S2_pstorerbt_io(ARG, ADDR, OFF, VAL) \
    pstoreX_old_io(ARG, ADDR, OFF, VAL, p0, b, "")
#define S2_pstorerbf_io(ARG, ADDR, OFF, VAL) \
    pstoreX_old_io(ARG, ADDR, OFF, VAL, !p0, b, "")
#define S4_pstorerbtnew_io(ARG, ADDR, OFF, VAL) \
    pstoreX_new_io(ARG, ADDR, OFF, VAL, p0, b, "")
#define S4_pstorerbfnew_io(ARG, ADDR, OFF, VAL) \
    pstoreX_new_io(ARG, ADDR, OFF, VAL, !p0, b, "")
#define S2_pstorerbnewt_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_old_io(ARG, ADDR, OFF, VAL, p0, b)
#define S2_pstorerbnewf_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_old_io(ARG, ADDR, OFF, VAL, !p0, b)
#define S4_pstorerbnewtnew_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_new_io(ARG, ADDR, OFF, VAL, p0, b)
#define S4_pstorerbnewfnew_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_new_io(ARG, ADDR, OFF, VAL, !p0, b)

/* Half word */
#define S2_pstorerht_io(ARG, ADDR, OFF, VAL) \
    pstoreX_old_io(ARG, ADDR, OFF, VAL, p0, h, "")
#define S2_pstorerhf_io(ARG, ADDR, OFF, VAL) \
    pstoreX_old_io(ARG, ADDR, OFF, VAL, !p0, h, "")
#define S4_pstorerhtnew_io(ARG, ADDR, OFF, VAL) \
    pstoreX_new_io(ARG, ADDR, OFF, VAL, p0, h, "")
#define S4_pstorerhfnew_io(ARG, ADDR, OFF, VAL) \
    pstoreX_new_io(ARG, ADDR, OFF, VAL, !p0, h, "")
#define S2_pstorerft_io(ARG, ADDR, OFF, VAL) \
    pstoreX_old_io(ARG, ADDR, OFF, VAL, p0, h, ".H")
#define S2_pstorerff_io(ARG, ADDR, OFF, VAL) \
    pstoreX_old_io(ARG, ADDR, OFF, VAL, !p0, h, ".H")
#define S4_pstorerftnew_io(ARG, ADDR, OFF, VAL) \
    pstoreX_new_io(ARG, ADDR, OFF, VAL, p0, h, ".H")
#define S4_pstorerffnew_io(ARG, ADDR, OFF, VAL) \
    pstoreX_new_io(ARG, ADDR, OFF, VAL, !p0, h, ".H")
#define S2_pstorerhnewt_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_old_io(ARG, ADDR, OFF, VAL, p0, h)
#define S2_pstorerhnewf_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_old_io(ARG, ADDR, OFF, VAL, !p0, h)
#define S4_pstorerhnewtnew_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_new_io(ARG, ADDR, OFF, VAL, p0, h)
#define S4_pstorerhnewfnew_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_new_io(ARG, ADDR, OFF, VAL, !p0, h)

/* Word */
#define S2_pstorerit_io(ARG, ADDR, OFF, VAL) \
    pstoreX_old_io(ARG, ADDR, OFF, VAL, p0, w, "")
#define S2_pstorerif_io(ARG, ADDR, OFF, VAL) \
    pstoreX_old_io(ARG, ADDR, OFF, VAL, !p0, w, "")
#define S4_pstoreritnew_io(ARG, ADDR, OFF, VAL) \
    pstoreX_new_io(ARG, ADDR, OFF, VAL, p0, w, "")
#define S4_pstorerifnew_io(ARG, ADDR, OFF, VAL) \
    pstoreX_new_io(ARG, ADDR, OFF, VAL, !p0, w, "")
#define S2_pstorerinewt_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_old_io(ARG, ADDR, OFF, VAL, p0, w)
#define S2_pstorerinewf_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_new_io(ARG, ADDR, OFF, VAL, !p0, w)
#define S4_pstorerinewtnew_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_new_io(ARG, ADDR, OFF, VAL, p0, w)
#define S4_pstorerinewfnew_io(ARG, ADDR, OFF, VAL) \
    pstore_new_X_new_io(ARG, ADDR, OFF, VAL, !p0, w)

/* Double word */
#define S2_pstorerdt_io(ARG, ADDR, OFF, VAL) \
    pstoreX_old_io(ARG, ADDR, OFF, VAL, p0, d, "")
#define S2_pstorerdf_io(ARG, ADDR, OFF, VAL) \
    pstoreX_old_io(ARG, ADDR, OFF, VAL, !p0, d, "")
#define S4_pstorerdtnew_io(ARG, ADDR, OFF, VAL) \
    pstoreX_new_io(ARG, ADDR, OFF, VAL, p0, d, "")
#define S4_pstorerdfnew_io(ARG, ADDR, OFF, VAL) \
    pstoreX_new_io(ARG, ADDR, OFF, VAL, !p0, d, "")

#define do_test_io(INSN, T, F, INDEX, OFF, VAL, EXPECT) \
    do { \
        memcpy(array, init, sizeof(init)); \
        INSN(T, &array[INDEX], OFF, VAL); \
        check(array[INDEX + OFF / 4], EXPECT); \
        memcpy(array, init, sizeof(init)); \
        INSN(F, &array[INDEX], OFF, VAL); \
        check(array[INDEX + OFF / 4], init[INDEX + OFF / 4]); \
    } while (0)

#define do_test64_io(INSN, T, F, INDEX, OFF, VAL, EXPECT) \
    do { \
        memcpy(array64, init64, sizeof(init64)); \
        INSN(T, &array64[INDEX], OFF, VAL); \
        check64(array64[INDEX + OFF / 8], EXPECT); \
        memcpy(array64, init64, sizeof(init64)); \
        INSN(F, &array64[INDEX], OFF, VAL); \
        check64(array64[INDEX + OFF / 8], init64[INDEX + OFF / 8]); \
    } while (0)

static void test_io(void)
{
    do_test_io(S2_pstorerbt_io,       1, 0, 0, 0, 0x12,       0x12);
    do_test_io(S2_pstorerbf_io,       0, 1, 1, 4, 0x23,       0x23);
    do_test_io(S4_pstorerbtnew_io,    1, 0, 2, 8, 0x34,       0x34);
    do_test_io(S4_pstorerbfnew_io,    0, 1, 3, 0, 0x45,       0x45);
    do_test_io(S2_pstorerbnewt_io,    1, 0, 4, 4, 0x56,       0x56);
    do_test_io(S2_pstorerbnewf_io,    0, 1, 5, 8, 0x67,       0x67);
    do_test_io(S4_pstorerbnewtnew_io, 1, 0, 7, 0, 0x78,       0x78);
    do_test_io(S4_pstorerbnewfnew_io, 0, 1, 7, 4, 0x89,       0x89);

    do_test_io(S2_pstorerht_io,       1, 0, 0, 0, 0x1234,     0x1234);
    do_test_io(S2_pstorerhf_io,       0, 1, 1, 4, 0x2345,     0x2345);
    do_test_io(S4_pstorerhtnew_io,    1, 0, 2, 8, 0x3456,     0x3456);
    do_test_io(S4_pstorerhfnew_io,    0, 1, 3, 0, 0x4567,     0x4567);
    do_test_io(S2_pstorerft_io,       1, 0, 4, 4, 0x5678ffff, 0x5678);
    do_test_io(S2_pstorerff_io,       0, 1, 5, 8, 0x6789ffff, 0x6789);
    do_test_io(S4_pstorerftnew_io,    1, 0, 6, 0, 0x789affff, 0x789a);
    do_test_io(S4_pstorerffnew_io,    0, 1, 7, 4, 0x89abffff, 0x89ab);
    do_test_io(S2_pstorerhnewt_io,    1, 0, 0, 8, 0x9abc,     0x9abc);
    do_test_io(S2_pstorerhnewf_io,    0, 1, 1, 0, 0xabcd,     0xabcd);
    do_test_io(S4_pstorerhnewtnew_io, 1, 0, 2, 4, 0xbcde,     0xbcde);
    do_test_io(S4_pstorerhnewfnew_io, 0, 1, 3, 8, 0xcdef,     0xcdef);

    do_test_io(S2_pstorerit_io,       1, 0, 5, 8, 0x12345678, 0x12345678);
    do_test_io(S2_pstorerif_io,       0, 1, 5, 8, 0x23456789, 0x23456789);
    do_test_io(S4_pstoreritnew_io,    1, 0, 5, 8, 0x3456789a, 0x3456789a);
    do_test_io(S4_pstorerifnew_io,    0, 1, 5, 8, 0x456789ab, 0x456789ab);
    do_test_io(S2_pstorerinewt_io,    1, 0, 5, 8, 0x56789abc, 0x56789abc);
    do_test_io(S2_pstorerinewf_io,    0, 1, 5, 8, 0x6789abcd, 0x6789abcd);
    do_test_io(S4_pstorerinewtnew_io, 1, 0, 5, 8, 0x789abcde, 0x789abcde);
    do_test_io(S4_pstorerinewfnew_io, 0, 1, 5, 8, 0x89abcdef, 0x89abcdef);

    do_test64_io(S2_pstorerdt_io,     1, 0, 5, 8, 0x1234LL,   0x1234LL);
    do_test64_io(S2_pstorerdf_io,     0, 1, 5, 8, 0x2345LL,   0x2345LL);
    do_test64_io(S4_pstorerdtnew_io,  1, 0, 5, 8, 0x3456LL,   0x3456LL);
    do_test64_io(S4_pstorerdfnew_io,  0, 1, 5, 8, 0x4567LL,   0x4567LL);
}

/*
 ****************************************************************************
 * _abs addressing mode (##addr)
 */
#define pstoreX_old_abs(ARG, ADDR, VAL, PRED, SIZE, PART) \
    asm ("p0 = cmp.eq(%0, #1)\n\t" \
         "if (" #PRED ") mem" #SIZE "(##" #ADDR ") = %1" PART "\n\t" \
         : : "r"(ARG), "r"(VAL) \
         : "p0", "memory")
#define pstoreX_new_abs(ARG, ADDR, VAL, PRED, SIZE, PART) \
    asm ("{\n\t" \
         "    p0 = cmp.eq(%0, #1)\n\t" \
         "    if (" #PRED ".new) mem" #SIZE "(##" #ADDR ") = %1" PART "\n\t" \
         "}\n\t" \
         : : "r"(ARG), "r"(VAL) \
         : "p0", "memory")
#define pstore_new_X_old_abs(ARG, ADDR, VAL, PRED, SIZE) \
    asm ("p0 = cmp.eq(%0, #1)\n\t" \
         "{\n\t" \
         "    r5 = %1\n\t" \
         "    if (" #PRED ") mem" #SIZE "(##" #ADDR ") = r5.new\n\t" \
         "}\n\t" \
         : : "r"(ARG), "r"(VAL) \
         : "p0", "r5", "memory")
#define pstore_new_X_new_abs(ARG, ADDR, VAL, PRED, SIZE) \
    asm ("{\n\t" \
         "    p0 = cmp.eq(%0, #1)\n\t" \
         "    r5 = %1\n\t" \
         "    if (" #PRED ".new) mem" #SIZE "(##" #ADDR ") = r5.new\n\t" \
         "}\n\t" \
         : : "r"(ARG), "r"(VAL) \
         : "p0", "r5", "memory")

/* Byte */
#define S4_pstorerbt_abs(ARG, ADDR, VAL) \
    pstoreX_old_abs(ARG, ADDR, VAL, p0, b, "")
#define S4_pstorerbf_abs(ARG, ADDR, VAL) \
    pstoreX_old_abs(ARG, ADDR, VAL, !p0, b, "")
#define S4_pstorerbtnew_abs(ARG, ADDR, VAL) \
    pstoreX_new_abs(ARG, ADDR, VAL, p0, b, "")
#define S4_pstorerbfnew_abs(ARG, ADDR, VAL) \
    pstoreX_new_abs(ARG, ADDR, VAL, !p0, b, "")
#define S4_pstorerbnewt_abs(ARG, ADDR, VAL) \
    pstore_new_X_old_abs(ARG, ADDR, VAL, p0, b)
#define S4_pstorerbnewf_abs(ARG, ADDR, VAL) \
    pstore_new_X_old_abs(ARG, ADDR, VAL, !p0, b)
#define S4_pstorerbnewtnew_abs(ARG, ADDR, VAL) \
    pstore_new_X_new_abs(ARG, ADDR, VAL, p0, b)
#define S4_pstorerbnewfnew_abs(ARG, ADDR, VAL) \
    pstore_new_X_new_abs(ARG, ADDR, VAL, !p0, b)

/* Half word */
#define S4_pstorerht_abs(ARG, ADDR, VAL) \
    pstoreX_old_abs(ARG, ADDR, VAL, p0, h, "")
#define S4_pstorerhf_abs(ARG, ADDR, VAL) \
    pstoreX_old_abs(ARG, ADDR, VAL, !p0, h, "")
#define S4_pstorerhtnew_abs(ARG, ADDR, VAL) \
    pstoreX_new_abs(ARG, ADDR, VAL, p0, h, "")
#define S4_pstorerhfnew_abs(ARG, ADDR, VAL) \
    pstoreX_new_abs(ARG, ADDR, VAL, !p0, h, "")
#define S4_pstorerft_abs(ARG, ADDR, VAL) \
    pstoreX_old_abs(ARG, ADDR, VAL, p0, h, ".H")
#define S4_pstorerff_abs(ARG, ADDR, VAL) \
    pstoreX_old_abs(ARG, ADDR, VAL, !p0, h, ".H")
#define S4_pstorerftnew_abs(ARG, ADDR, VAL) \
    pstoreX_new_abs(ARG, ADDR, VAL, p0, h, ".H")
#define S4_pstorerffnew_abs(ARG, ADDR, VAL) \
    pstoreX_new_abs(ARG, ADDR, VAL, !p0, h, ".H")
#define S4_pstorerhnewt_abs(ARG, ADDR, VAL) \
    pstore_new_X_old_abs(ARG, ADDR, VAL, p0, h)
#define S4_pstorerhnewf_abs(ARG, ADDR, VAL) \
    pstore_new_X_old_abs(ARG, ADDR, VAL, !p0, h)
#define S4_pstorerhnewtnew_abs(ARG, ADDR, VAL) \
    pstore_new_X_new_abs(ARG, ADDR, VAL, p0, h)
#define S4_pstorerhnewfnew_abs(ARG, ADDR, VAL) \
    pstore_new_X_new_abs(ARG, ADDR, VAL, !p0, h)

/* Word */
#define S4_pstorerit_abs(ARG, ADDR, VAL) \
    pstoreX_old_abs(ARG, ADDR, VAL, p0, w, "")
#define S4_pstorerif_abs(ARG, ADDR, VAL) \
    pstoreX_old_abs(ARG, ADDR, VAL, !p0, w, "")
#define S4_pstoreritnew_abs(ARG, ADDR, VAL) \
    pstoreX_new_abs(ARG, ADDR, VAL, p0, w, "")
#define S4_pstorerifnew_abs(ARG, ADDR, VAL) \
    pstoreX_new_abs(ARG, ADDR, VAL, !p0, w, "")
#define S4_pstorerinewt_abs(ARG, ADDR, VAL) \
    pstore_new_X_old_abs(ARG, ADDR, VAL, p0, w)
#define S4_pstorerinewf_abs(ARG, ADDR, VAL) \
    pstore_new_X_old_abs(ARG, ADDR, VAL, !p0, w)
#define S4_pstorerinewtnew_abs(ARG, ADDR, VAL) \
    pstore_new_X_new_abs(ARG, ADDR, VAL, p0, w)
#define S4_pstorerinewfnew_abs(ARG, ADDR, VAL) \
    pstore_new_X_new_abs(ARG, ADDR, VAL, !p0, w)

/* Double word */
#define S4_pstorerdt_abs(ARG, ADDR, VAL) \
    pstoreX_old_abs(ARG, ADDR, VAL, p0, d, "")
#define S4_pstorerdf_abs(ARG, ADDR, VAL) \
    pstoreX_old_abs(ARG, ADDR, VAL, !p0, d, "")
#define S4_pstorerdtnew_abs(ARG, ADDR, VAL) \
    pstoreX_new_abs(ARG, ADDR, VAL, p0, d, "")
#define S4_pstorerdfnew_abs(ARG, ADDR, VAL) \
    pstoreX_new_abs(ARG, ADDR, VAL, !p0, d, "")

#define do_test_abs(INSN, T, F, VAL, EXPECT) \
    do { \
        value = 0xff; \
        INSN(T, value, VAL); \
        check(value, EXPECT); \
        value = 0xff; \
        INSN(F, value, VAL); \
        check(value, 0xff); \
    } while (0)
#define do_test64_abs(INSN, T, F, VAL, EXPECT) \
    do { \
        value64 = 0xffLL; \
        INSN(T, value64, VAL); \
        check64(value64, EXPECT); \
        value64 = 0xffLL; \
        INSN(F, value64, VAL); \
        check64(value64, 0xffLL); \
    } while (0)

static void test_abs(void)
{
    do_test_abs(S4_pstorerbt_abs,       1, 0, 0x12,       0x12);
    do_test_abs(S4_pstorerbf_abs,       0, 1, 0x23,       0x23);
    do_test_abs(S4_pstorerbtnew_abs,    1, 0, 0x34,       0x34);
    do_test_abs(S4_pstorerbfnew_abs,    0, 1, 0x45,       0x45);
    do_test_abs(S4_pstorerbnewt_abs,    1, 0, 0x56,       0x56);
    do_test_abs(S4_pstorerbnewf_abs,    0, 1, 0x67,       0x67);
    do_test_abs(S4_pstorerbnewtnew_abs, 1, 0, 0x78,       0x78);
    do_test_abs(S4_pstorerbnewfnew_abs, 0, 1, 0x89,       0x89);

    do_test_abs(S4_pstorerht_abs,       1, 0, 0x1234,     0x1234);
    do_test_abs(S4_pstorerhf_abs,       0, 1, 0x2345,     0x2345);
    do_test_abs(S4_pstorerhtnew_abs,    1, 0, 0x3456,     0x3456);
    do_test_abs(S4_pstorerhfnew_abs,    0, 1, 0x4567,     0x4567);
    do_test_abs(S4_pstorerft_abs,       1, 0, 0x56781234, 0x5678);
    do_test_abs(S4_pstorerff_abs,       0, 1, 0x67891234, 0x6789);
    do_test_abs(S4_pstorerftnew_abs,    1, 0, 0x789a1234, 0x789a);
    do_test_abs(S4_pstorerffnew_abs,    0, 1, 0x89ab1234, 0x89ab);
    do_test_abs(S4_pstorerhnewt_abs,    1, 0, 0x9abc,     0x9abc);
    do_test_abs(S4_pstorerhnewf_abs,    0, 1, 0xabcd,     0xabcd);
    do_test_abs(S4_pstorerhnewtnew_abs, 1, 0, 0xbcde,     0xbcde);
    do_test_abs(S4_pstorerhnewfnew_abs, 0, 1, 0xcdef,     0xcdef);

    do_test_abs(S4_pstorerit_abs,       1, 0, 0x1234,     0x1234);
    do_test_abs(S4_pstorerif_abs,       0, 1, 0x2345,     0x2345);
    do_test_abs(S4_pstoreritnew_abs,    1, 0, 0x3456,     0x3456);
    do_test_abs(S4_pstorerifnew_abs,    0, 1, 0x4567,     0x4567);
    do_test_abs(S4_pstorerinewt_abs,    1, 0, 0x5678,     0x5678);
    do_test_abs(S4_pstorerinewf_abs,    0, 1, 0x6789,     0x6789);
    do_test_abs(S4_pstorerinewtnew_abs, 1, 0, 0x789a,     0x789a);
    do_test_abs(S4_pstorerinewfnew_abs, 0, 1, 0x89ab,     0x89ab);

    do_test64_abs(S4_pstorerdt_abs,     1, 0, 0x1234LL,   0x1234LL);
    do_test64_abs(S4_pstorerdf_abs,     0, 1, 0x2345LL,   0x2345LL);
    do_test64_abs(S4_pstorerdtnew_abs,  1, 0, 0x3456LL,   0x3456LL);
    do_test64_abs(S4_pstorerdfnew_abs,  0, 1, 0x4567LL,   0x4567LL);
}

/*
 ****************************************************************************
 */
int main()
{
    test_rr();
    test_pi();
    test_io();
    test_abs();

    puts(err ? "FAIL" : "PASS");
    return err;
}
