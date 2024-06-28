/*
 *  Copyright(c) 2019-2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#ifndef HEXAGON_MMVEC_H
#define HEXAGON_MMVEC_H

#include "qemu/bitmap.h"

/* FIXME - Is this needed? */
enum ext_mem_access_types {
	access_type_vstore,
	access_type_vload_nt,
	access_type_vstore_nt,
    access_type_vgather_load,
    access_type_vscatter_store,
    access_type_vscatter_release,    
	access_type_vgather_release,
    access_type_vfetch,
	NUM_EXT_ACCESS_TYPES
};

// This cast is safe as long as the enums do not overlap
#define ext_ma_as_core_ma(access) ((enum mem_access_types)access)

#define MAX_VEC_SIZE_LOGBYTES 7
#define MAX_VEC_SIZE_BYTES  (1<<MAX_VEC_SIZE_LOGBYTES)
// #define VEC_CONTEXTS        2

#define NUM_ZREGS			8
#define NUM_VREGS           32
#define NUM_QREGS           4



typedef uint32_t ZRegMask; // at least NUM_VREGS bits
typedef uint32_t VRegMask; // at least NUM_VREGS bits
typedef uint32_t QRegMask; // at least NUM_QREGS bits

// Use software vector length?
#define VECTOR_SIZE_BYTE    MAX_VEC_SIZE_BYTES

typedef struct {
    union {
        uint64_t ud[MAX_VEC_SIZE_BYTES/8];
        int64_t  d[MAX_VEC_SIZE_BYTES/8];
        uint32_t uw[MAX_VEC_SIZE_BYTES/4];
        int32_t  w[MAX_VEC_SIZE_BYTES/4];
        uint16_t uh[MAX_VEC_SIZE_BYTES/2];
        int16_t  h[MAX_VEC_SIZE_BYTES/2];
        uint8_t ub[MAX_VEC_SIZE_BYTES/1];
        int8_t  b[MAX_VEC_SIZE_BYTES/1];
        int8_t  f8[MAX_VEC_SIZE_BYTES/1];
        int32_t qf32[MAX_VEC_SIZE_BYTES/4];
        int16_t qf16[MAX_VEC_SIZE_BYTES/2];

        int32_t sf[MAX_VEC_SIZE_BYTES/4];
        int16_t hf[MAX_VEC_SIZE_BYTES/2];
        int16_t bf[MAX_VEC_SIZE_BYTES/2];
    };

    union {
        uint64_t ud_ext[MAX_VEC_SIZE_BYTES/32];
        uint32_t uw_ext[MAX_VEC_SIZE_BYTES/16];
        uint8_t ext[MAX_VEC_SIZE_BYTES/4];
    };
    
} MMVector;

typedef struct {
    MMVector v[2];
} MMVectorPair;

typedef union {
	uint64_t ud[MAX_VEC_SIZE_BYTES/8/8];
	int64_t  d[MAX_VEC_SIZE_BYTES/8/8];
	uint32_t uw[MAX_VEC_SIZE_BYTES/4/8];
	int32_t  w[MAX_VEC_SIZE_BYTES/4/8];
	uint16_t uh[MAX_VEC_SIZE_BYTES/2/8];
	int16_t  h[MAX_VEC_SIZE_BYTES/2/8];
	uint8_t ub[MAX_VEC_SIZE_BYTES/1/8];
	int8_t  b[MAX_VEC_SIZE_BYTES/1/8];
	int8_t  f8[MAX_VEC_SIZE_BYTES/1/8];
} MMQReg;

typedef struct {
    MMVector data;
    DECLARE_BITMAP(mask, MAX_VEC_SIZE_BYTES);
    uint32_t va[MAX_VEC_SIZE_BYTES];
    int op;
    int op_size;
    int size;
#ifndef CONFIG_USER_ONLY
    MMVectorPair offsets;
    uint64_t pa[MAX_VEC_SIZE_BYTES];
    uint64_t pa_base;
    uint32_t va_base;
    int oob_access;
#endif
} VTCMStoreLog;

#include "mmvec_qfloat_compliance.h"

static inline MMVector mmvec_zero_vector(void)
{
	MMVector ret;
	memset(&ret,0,sizeof(ret));
	return ret;
}

static inline void set_extqfloat_rnd_mode(CPUHexagonState *thread, int rnd_mode)
{
#ifdef EXPERIMENTAL_QF
    thread->qfrnd_mode = rnd_mode;
#endif
}
static inline void set_extqfloat_coproc_mode(CPUHexagonState *thread, int mode)
{
#ifdef EXPERIMENTAL_QF
    thread->qfcoproc_mode = mode;
#endif
}

#define V_EXTENDED_DWORDVAL 0x0A0A0A0A0A0A0A0A
#define V_EXTENDED_WORDVAL  0x0A0A0A0A
#define V_EXTENDED_HWORDVAL 0x0A0A
#define V_EXTENDED_BYTEVAL  0x0A

#define DECL_EXT_ZREG(VAR) \
	MMVector VAR;

#define READ_EXT_ZREG(NUM, VAR, VTMP) \
	VAR = thread->ZRegs[NUM]; 
	
#define SET_DEFAULT_EXTENDED(VAR) \
       VAR.ud_ext[0] = V_EXTENDED_DWORDVAL; \
       VAR.ud_ext[1] = V_EXTENDED_DWORDVAL; \
       VAR.ud_ext[2] = V_EXTENDED_DWORDVAL; \
       VAR.ud_ext[3] = V_EXTENDED_DWORDVAL;

#define SET_DEFAULT_EXTENDED_PAIR(VAR) \
       SET_DEFAULT_EXTENDED(VAR.v[0]) \
       SET_DEFAULT_EXTENDED(VAR.v[1])

#define SET_DEFAULT_EXTENDED_QUAD(VAR) \
       SET_DEFAULT_EXTENDED(VAR.v[0]) \
       SET_DEFAULT_EXTENDED(VAR.v[1]) \
       SET_DEFAULT_EXTENDED(VAR.v[2]) \
       SET_DEFAULT_EXTENDED(VAR.v[3])

#define DECL_EXT_VREG(VAR) \
	MMVector VAR; \
	memset(&VAR, 0, sizeof(MMVector)); \
	SET_DEFAULT_EXTENDED(VAR)

#define WRITE_BASE_VREG(NUM, VAR, VNEW) \
do { \
	VRegMask regnum_mask = ((VRegMask)1)<<(NUM); \
	thread->VRegs_updated |= ((VNEW != EXT_TMP) && (VNEW != EXT_REMAP)) ? regnum_mask : 0; \
	thread->VRegs_select |= (VNEW==EXT_NEW) ? regnum_mask : 0; \
    thread->VRegs_updated_tmp  |= ((VNEW==EXT_TMP) || (VNEW==EXT_REMAP)) ? regnum_mask : 0; \
	thread->future_VRegs[NUM] = VAR; \
    thread->tmp_VRegs[NUM] = ((VNEW==EXT_TMP) || (VNEW==EXT_REMAP)) ? VAR : thread->tmp_VRegs[NUM]; \
} while (0);

#define WRITE_EXT_VREG(NUM, VAR, VNEW) WRITE_BASE_VREG(NUM,VAR,VNEW)

#define NEW_WRITTEN(NUM) ((thread->VRegs_select >> (NUM)) & 1)
#define TMP_WRITTEN(NUM) (((thread->VRegs_updated_tmp >> (NUM)) & 1) && (thread->VRegs_updated_tmp != 0xFFFFFFFF))

//This macro handles the read according to tmp, new, or just regular load
#define READ_BASE_VREG(NUM, VAR, VTMP) \
do {\
	VAR = ((NEW_WRITTEN(NUM)) ? thread->future_VRegs[NUM] : thread->VRegs[NUM]); \
	VAR = ((TMP_WRITTEN(NUM)) ? thread->tmp_VRegs[NUM] : VAR);   \
	if (VTMP == EXT_TMP) {\
		if (thread->VRegs_updated & ((VRegMask)1)<<(NUM)) {\
			VAR = thread->future_VRegs[NUM];\
			thread->VRegs_updated ^= ((VRegMask)1)<<(NUM);\
		}\
	}\
} while (0);

//This macro handles reading the entire vector register, which includes extended bits and vector.
#define READ_EXT_VREG(NUM, VAR, VTMP) READ_BASE_VREG(NUM,VAR,VTMP)       
    
#define READ_ZREG(NUM) ({ DECL_EXT_ZREG(__tmpVR); READ_EXT_ZREG(NUM,__tmpVR, 0); __tmpVR; })
#define READ_VREG(NUM) ({ DECL_EXT_VREG(__tmpVR); READ_EXT_VREG(NUM,__tmpVR, 0); __tmpVR; })
#define WRITE_VREG(NUM,VAR) ({ WRITE_EXT_VREG(NUM, VAR, EXT_DFL); })

static inline void set_extended_bits(MMVector *v, int i, int size, uint8_t val) {
	if(size==32) {
		v->ext[i] = val;
	} else {
		//For ext, bits are set as LRLR, so we need to shift them appropriately
		//half precision indexes from 0 - 64 but vreg.ext only has 32 elements
		// idx 0 and 1 will map to idx 0, 2-2 map to 1, etc...
		if (i % 2 == 1) {
			//If odd idx, set upper two bits
			v->ext[i/2] = val << 2 | (v->ext[i/2] & 0x3);
		} else {
			//If even idx, set lower two bits
			v->ext[i/2] = (v->ext[i/2] & 0xC) | val;
		}
	}
}

static inline uint8_t get_extended_bits(CPUHexagonState *thread, MMVector *v, int i, int size) {
	if(size==32) {
		return v->ext[i];
	} else {
		//For ext, bits are set as LRLR, so we need to shift them appropriately
		//half precision indexes from 0 - 64 but vreg.ext only has 32 elements
		// idx 0 and 1 will map to idx 0, 2-2 map to 1, etc...
		if (i % 2 == 1) {
			//If odd idx, return upper two bits
			return (v->ext[i/2] >> 2) & 0x3;
		} else {
			//If even idx, return upper two bits
			return (v->ext[i/2] & 0x3);
		}
	}
}

bool is_daz_mode(CPUHexagonState* thread);
uint32_t get_usr_reg_fpsat_field(CPUHexagonState* thread);

#endif
