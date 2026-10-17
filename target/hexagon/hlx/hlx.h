/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef HEXAGON_HLX_H
#define HEXAGON_HLX_H

#define NUM_LREGS 16
#define HLX_VEC_SIZE_BYTES 2048

/* Element type for HLX register pairs - all fields overlap in 4 bytes */
typedef union {
    uint8_t  ub[4];
    int8_t   b[4];
    uint16_t uh[2];
    int16_t  h[2];
    uint32_t uw;
    int32_t  w;
} lreg_element_t;

typedef struct {
    union {
        uint64_t ud[HLX_VEC_SIZE_BYTES / 8];
        int64_t  d[HLX_VEC_SIZE_BYTES / 8];
        uint32_t uw[HLX_VEC_SIZE_BYTES / 4];
        int32_t  w[HLX_VEC_SIZE_BYTES / 4];
        uint16_t uh[HLX_VEC_SIZE_BYTES / 2];
        int16_t  h[HLX_VEC_SIZE_BYTES / 2];
        uint8_t  ub[HLX_VEC_SIZE_BYTES / 1];
        int8_t   b[HLX_VEC_SIZE_BYTES / 1];
    };
} HLXVector;

typedef union {
    lreg_element_t l[HLX_VEC_SIZE_BYTES / 2];  /* Access as element pairs */
} HLXVectorPair;

static inline HLXVector hlx_zero_vector(void)
{
    HLXVector ret;
    memset(&ret, 0, sizeof(ret));
    return ret;
}

/*
 * HLX word-pair marshaling.
 *
 * On hardware, word-pair element i has its low halfword at lo.h[i] and
 * high halfword at hi.h[i], where lo and hi are separate L-registers
 * stored contiguously (lo at offset 0, hi at offset 2048).
 *
 * The HLXVectorPair type stores elements as l[i] = {h[0]=lo, h[1]=hi}
 * in an interleaved layout.  These functions convert between the
 * "two contiguous registers" physical layout and the interleaved
 * semantic layout used by instruction helpers.
 */
static inline void hlx_interleave_pair(HLXVectorPair *pair,
                                       const HLXVector *lo,
                                       const HLXVector *hi)
{
    for (int i = 0; i < HLX_VEC_SIZE_BYTES / 2; i++) {
        pair->l[i].h[0] = lo->h[i];
        pair->l[i].h[1] = hi->h[i];
    }
}

static inline void hlx_deinterleave_pair(HLXVector *lo, HLXVector *hi,
                                         const HLXVectorPair *pair)
{
    for (int i = 0; i < HLX_VEC_SIZE_BYTES / 2; i++) {
        lo->h[i] = pair->l[i].h[0];
        hi->h[i] = pair->l[i].h[1];
    }
}

#endif
