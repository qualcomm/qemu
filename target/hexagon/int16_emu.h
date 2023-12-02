/*
 * Copyright (c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
 */

#include "hex_arch_types.h"

#ifndef INT16_EMU_H
#define INT16_EMU_H 1

#define count_leading_ones_8(src) clo64(src)

#ifdef HEX_CONFIG_INT128
#define cast8s_to_16s(a) int128_exts64(a)
#define cast16s_to_8s(a) (size8s_t)int128_getlo(a)
#define cast16s_to_4s(a) (size4s_t)int128_getlo(a)

static inline QEMU_ALWAYS_INLINE size4u_t count_leading_zeros_16(size16s_t src)
{
    size8u_t a;
    if (int128_gethi(src) == 0) {
        a = int128_getlo(src);
        return 64 + count_leading_ones_8(~a);
    }
    a = (size8u_t)int128_gethi(src);
    return count_leading_ones_8(~a);
}

#define add128(a,b)         int128_add(a,b)
#define sub128(a,b)         int128_sub(a,b)
#define shiftr128(a,b)      int128_rshift(a,b)
#define shiftl128(a,b)      int128_lshift(a,b)
#define and128(a,b)         int128_and(a,b)
#define conv_round64(a,b)   int128_getlo(a)

static inline QEMU_ALWAYS_INLINE size16s_t mult64_to_128(size8s_t X, size8s_t Y)
{
    return int128_exts64(X) * int128_exts64(Y);
}
#else
size16s_t cast8s_to_16s(int64_t a);
int64_t cast16s_to_8s(size16s_t a);
int32_t cast16s_to_4s(size16s_t a);
uint32_t count_leading_zeros_16(size16s_t src);
size16s_t add128(size16s_t a, size16s_t b);
size16s_t sub128(size16s_t a, size16s_t b);
size16s_t shiftr128(size16s_t a, uint32_t n);
size16s_t shiftl128(size16s_t a, uint32_t n);
size16s_t and128(size16s_t a, size16s_t b);
int64_t conv_round64(int64_t a, uint32_t n);
size16s_t mult64_to_128(int64_t X, int64_t Y);
#endif

#endif
