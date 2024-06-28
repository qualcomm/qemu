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
#ifndef MMVEC_QFLOAT_H
#define MMVEC_QFLOAT_H 1

typedef enum float_type{
  QF32,
  QF16,
  SF,
  HF,
  EXTQF32,
  EXTQF16,
  F8,
  BF
} f_type;

typedef enum {
        RND_TO_NEAREST_EVEN,
        RND_TO_ZERO,
        RND_TOWARDS_NEG_INF,
        RND_TOWARDS_POS_INF,
	MAX_RND_MODES
} qfrnd_mode_enum_t;

#include "cpu.h"
#include "hex_arch_types.h"

#define HF_MAX 131008  //pow(2,17)-pow(2,6) =(2-1.0/pow(2,10))*pow(2,16) 
#define HF_MIN 1.0/pow(2,24)
#define SF_MAX pow(2,129)-pow(2,105) //(2-1.0/pow(2,23))*pow(2,128)
#define SF_MIN 1.0/pow(2,149)
#define df_BIAS 1023
#define sf_BIAS 127
#define hf_BIAS 15

#define EMAX_16 16
#define EMAX_32 128

//Bit Breakdown of each type
#define df_MANTBITS 52
#define sf_MANTBITS 23
#define hf_MANTBITS 10
#define bf_MANTBITS 7
#define f8_MANTBITS 3

#define df_EXPBITS 11
#define sf_EXPBITS 8
#define hf_EXPBITS 5
#define bf_EXPBITS 8
#define f8_EXPBITS 4

#define E_MAX_QF32 128
#define E_MIN_QF32 -127
#define E_MAX_EXTQF32 256
#define E_MIN_EXTQF32 -255
#define E_MAX_QF16 16
#define E_MIN_QF16 -15
#define E_MAX_EXTQF16 E_MAX_QF16
#define E_MIN_EXTQF16 E_MIN_QF16
#define E_MAX_SF 128
#define E_MIN_SF -126 
#define E_MAX_HF 16
#define E_MIN_HF -14
#define E_MAX_F8 8
#define E_MIN_F8 -6

#define BIAS_QF32 127
#define BIAS_EXTQF32 383
#define BIAS_QF16 15
#define BIAS_DF 1023
#define BIAS_SF 127
#define BIAS_HF 15

#define FRAC_HF 10
#define FRAC_SF 23
#define FRAC_32 FRAC_SF
#define FRAC_16 FRAC_HF
#define EXT32_BITMASK 0x0F
#define EXT16_BITMASK 0x03
#define QF32_BITMASK 0xFFFFFFFF
#define EXTQF32_BITMASK 0xFFFFFFFFF
#define QF16_BITMASK 0xFFFF
#define EXTQF16_BITMASK 0x3FFFF
#define isNaNF32( a ) (((~(a) & 0x7F800000) == 0) && ((a) & 0x007FFFFF))
#define isInfF32( a ) (((~(a) & 0x7F800000) == 0) && (((a) & 0x007FFFFF) == 0))
#define isNaNF16( a ) (((~(a) & 0x7C00) == 0) && ((a) & 0x03FF))
#define isInfF16( a ) (((~(a) & 0x7C00) == 0) && (((a) & 0x03FF) == 0))

#define MAX_SIG_QF16 0x3FF
#define MAX_SIG_QF32 0x7FFFFF
#define MAX_NEG_SIG_QF32 0x800000
#define MAX_BIASED_E_SF (E_MAX_SF + BIAS_SF)

#define MIN_BIASED_E_SF (E_MIN_SF + BIAS_SF)

#define MAX_BIASED_E_HF (E_MAX_HF + BIAS_HF)
#define MIN_BIASED_E_HF (E_MIN_HF + BIAS_HF)

//extqf32 NaNs
#define extqf32_pos_nan 0X7FFFFF7FD   //L=1, R=1, Q=1 (assuming that inexactness is not triggered for nans)
#define extqf32_neg_nan 0X8000007F0   //L=0, R=0, Q=0 (assuming that inexactness is not triggered for nans)
#define extqf32_pos_nan_inexact 0X7FFFFF7FC   //L=1, R=1, Q=0 (assuming that inexactness is not triggered for nans)
#define extqf32_neg_nan_inexact 0X8000007F1   //L=0, R=0, Q=1 (assuming that inexactness is not triggered for nans)

//extqf32 infinities
#define extqf32_pos_inf extqf32_pos_inf_exact  //Used in handle_infinity call in macros
#define extqf32_pos_inf_exact 0x7FFFFF7F8  //L=1, R=0, Q=0 Represents exact infinity, used for infinity inputs handling
#define extqf32_pos_inf_inexact 0x7FFFFF7F9  //L=1, R=0, Q=1 Represents inexact infinity, used for infinity inputs handling

#define extqf32_neg_inf extqf32_neg_inf_exact  //Used in handle_infinity call in macros
#define extqf32_neg_inf_exact 0x8000007F5  //L=0, R=1, Q=1 Represents exact -infinity, used for -infinity input handling
#define extqf32_neg_inf_inexact 0x8000007F4  //L=0, R=1, Q=0 Represents inexact -infinity, used for -infinity input handling

//extqf16 NaNs
#define extqf16_pos_nan 0x7FFF3
#define extqf16_neg_nan 0x801F0

//extqf16 infinities
#define extqf16_pos_inf 0x7FFF2
#define extqf16_neg_inf 0x801F1



//ieee NaNs can have multiple representations, do not use these for comparision
//using a single representation of ieee nan for the convert from qf to ieee in cases of nan
//avoid payloads information as opposed to ieee
#define ieee_pos_NaN_32 0x7FFFFFFF
#define ieee_neg_NaN_32 0XFFFFFFFF

#define ieee_pos_NaN_16 0x7FFF
#define ieee_neg_NaN_16 0XFFFF

#define ieee_pos_inf_32 0x7f800000
#define ieee_neg_inf_32 0xff800000

#define ieee_pos_inf_16 0x7C00
#define ieee_neg_inf_16 0XFC00

#define BF_POS_NAN 0x7FFF
#define BF_POS_INF 0x7f80

#define QF32_ILOG2_ZERO_EXP 0x80000000
#define QF32_ILOG2_NAN_EXP  0x7FFFFFFF
#define QF32_ILOG2_INF_EXP  0x7FFFFFFE

#define QF16_ILOG2_ZERO_EXP 0x8000
#define QF16_ILOG2_NAN_EXP  0x7FFF
#define QF16_ILOG2_INF_EXP  0x7FFE



#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#define EPSILON32 1.0/pow(2,23)
#define UNITS32 1.0*pow(2,23)

#define EPSILON16 1.0/pow(2,10)
#define UNITS16 1.0*pow(2,10)

#define EPSILON8 1.0/pow(2,3)
#define UNITS8 1.0*pow(2,3)
#define F8_BIAS 0x7
#define F8_INFNAN 0x80

#define EPSILONBF 1.0/pow(2,bf_MANTBITS)
#define UNITSBF 1.0*pow(2,bf_MANTBITS)

#define INIT_UNFLOAT(U) unfloat U = {.sign = 0, .exp = 0, .sig = 0, .inexact = 0, .inf = false, .nan = false, .zero=false};

/* Unfloat documentation
	Un-Normalized Float
	exp: biased exponent of unfloat
	sig: mantissa of float, unormalized
	inf: is unfloat an infinity value
	nan: is unfloat a nan value
	inexact: inexact of calculated add, can only be -1, 0, or 1
    sign: sign of unfloat, sign bit for IEEE and defer_neg for qfloat
*/
#define type_qf32 EXTQF32
#define type_qf16 EXTQF16
#define type_sf SF
#define type_hf HF

typedef struct {
  int exp;
  double sig;
  int inexact;
  bool inf;
  bool nan;
  int sign;
  bool zero;
} unfloat;

typedef struct {
  bool ovf;
  bool undf;
  bool inexact;
} qf_exception_t;

typedef union {
	float f;
	uint32_t raw;
	uint32_t i;
	struct {
		size4u_t mant:sf_MANTBITS;
		size4u_t exp:sf_EXPBITS;
		size4u_t sign:1;
	} x;
} sf_t;

typedef union {
        uint16_t raw;
        uint16_t i;
        struct {
                uint16_t mant:hf_MANTBITS;
                uint16_t exp:hf_EXPBITS;
                uint16_t sign:1;
        } x;
} hf_t;

typedef union {
        uint16_t raw;
        uint16_t i;
        struct {
                uint16_t mant:bf_MANTBITS;
                uint16_t exp:bf_EXPBITS;
                uint16_t sign:1;
        } x;
} bf_t;

typedef union {
	uint8_t raw;
	struct {
		uint8_t mant:f8_MANTBITS;
		uint8_t exp:f8_EXPBITS;
		uint8_t sign:1;
	} x;
} f8_t;

/* Extended bits notated in abstract format 
    L: Least significant bit of sig
    R: Rounding bit
    E: Extended range of exponent. In IEEE range, E = 1. Out of range, E = 0
    Q: 1/4 ULP
*/
typedef union {
	uint8_t raw;
	struct {
		uint8_t Q: 1;
		uint8_t E: 1;
		uint8_t R: 1;
		uint8_t L: 1;
	} bits;
	struct {
		uint8_t EQ: 2;
		uint8_t LR: 2;
	} bits16;
} LREQ_t;


/* Fixed float is the unsigned integer format of a result and extended bits
    Used to fill in register values after rounding */
typedef struct fixed_float {
  uint32_t sig;
  uint32_t exp;
  LREQ_t LREQ;
} fixed_float_t;


/* ULP broken into integer and fractional component */
typedef struct ulp {
	double fractional;
	double integer;
} ulp_t;

/* Struct to store rounding mode information*/
typedef struct qfrnd_mode {
	double even_pos_threshold; 
	double odd_neg_threshold; 
	bool rnd_to_pos_neg;
	uint32_t ovf_val32;
	uint32_t undf_val32;
	uint16_t ovf_val16;
	uint16_t undf_val16;
        sf_t ieee_pos_overflow;
        sf_t ieee_neg_overflow;
        hf_t ieee_hf_pos_overflow;
        hf_t ieee_hf_neg_overflow;
	qfrnd_mode_enum_t rnd_mode;
} qfrnd_mode_t;

unfloat parse_qf32(size4s_t a);
unfloat parse_qf16(size2s_t a);
unfloat parse_sf(size4s_t a);
unfloat parse_sf_daz(uint32_t in, bool daz_mode);
unfloat parse_hf(size2s_t a);
unfloat parse_f8(uint8_t in);
size4s_t rnd_sat_sf(int exp, double sig);
size2s_t rnd_sat_hf(int exp, double sig);
size4s_t rnd_sat_w(int exp, double sig);
size4u_t rnd_sat_uw(int exp, double sig);
size2s_t rnd_sat_h(int exp, double sig);
size2u_t rnd_sat_uh(int exp, double sig);
size1s_t rnd_sat_b(int exp, double sig);
size1u_t rnd_sat_ub(int exp, double sig);
size4s_t rnd_sat_qf32(int exp, double sig, double sig_low);
size2s_t rnd_sat_qf16(int exp, double sig, double sig_low);
size4s_t negate32(size4s_t);
size2s_t negate16(size2s_t);
size4s_t negate_sf(size4s_t);
size2s_t negate_hf(size2s_t);

//Convert
size4s_t conv_sf_qf32(size4s_t a);
size4s_t conv_sf_w(size4s_t a);
size4s_t conv_sf_uw(size4u_t a);
size2s_t conv_hf_qf16(size2s_t a);
size2s_t conv_hf_h(size2s_t a);
size2s_t conv_hf_uh(size2u_t a);
size4s_t conv_hf_qf32(size8s_t a);
size4s_t conv_hf_w(size8s_t a);
size4s_t conv_hf_uw(size8u_t a);

size4s_t conv_w_qf32(size4s_t a);
size4u_t conv_uw_qf32(size4s_t a);
size2s_t conv_h_qf16(size2s_t a);
size2u_t conv_uh_qf16(size2s_t a);
size4s_t conv_h_qf32(size8s_t a);
size4u_t conv_uh_qf32(size8s_t a);
size2s_t conv_b_qf16(size4s_t a);
size2u_t conv_ub_qf16(size4s_t a);

size4s_t conv_w_sf(size4s_t a, bool daz_mode);
size2s_t conv_h_hf(size2s_t a);
uint16_t conv_h_hf_rnd(uint16_t a);
uint8_t conv_qf16_to_f8(uint16_t a, uint8_t aext, qfrnd_mode_enum_t qfrnd_mode, int qfcoproc_mode);
uint16_t conv_qf32_to_bf(uint32_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd_mode, int qfcoproc_mode);

//Neg/Abs
size4s_t neg_qf32(size4s_t a);
size4s_t abs_qf32(size4s_t a);
size2s_t neg_qf16(size2s_t a);
size2s_t abs_qf16(size2s_t a);
size4s_t neg_sf(size4s_t a);
size4s_t abs_sf(size4s_t a);
size2s_t neg_hf(size2s_t a);
size2s_t abs_hf(size2s_t a);

//Compare Greater than
int cmpgt_fp(unfloat a,  unfloat b, bool nan_detect); 
int cmpgt_qf32(size4s_t a,  size4s_t b); 
int cmpgt_qf16(size2s_t a,  size2s_t b); 
int cmpgt_sf(size4s_t a,  size4s_t b, bool nan_detect, bool daz_mode); 
int cmpgt_hf(size2s_t a,  size2s_t b, bool nan_detect); 
int cmpgt_qf32_sf(size4s_t a,  size4s_t b); 
int cmpgt_qf16_hf(size2s_t a,  size2s_t b); 

//Compare equal
int cmpeq_fp(unfloat a,  unfloat b); 
int cmpeq(size4s_t a,  size4s_t b, f_type type, bool daz_mode); 

//max/min
size4s_t max_qf32(size4s_t a, size4s_t b);
size4s_t min_qf32(size4s_t a, size4s_t b);
size4s_t max_qf32_sf(size4s_t a, size4s_t b);
size4s_t min_qf32_sf(size4s_t a, size4s_t b);
size4s_t max_sf(size4s_t a, size4s_t b);
size4s_t min_sf(size4s_t a, size4s_t b);
size2s_t max_qf16(size2s_t a, size2s_t b);
size2s_t min_qf16(size2s_t a, size2s_t b);
size2s_t max_qf16_hf(size2s_t a, size2s_t b);
size2s_t min_qf16_hf(size2s_t a, size2s_t b);
size2s_t max_hf(uint16_t a, uint16_t b);
size2s_t min_hf(uint16_t a, uint16_t b);

//Return True if unfloat is negative
bool is_unfloat_neg(unfloat u);
//Return True if unfloat is zero
bool is_unfloat_zero(unfloat u); 

//Return True if double value is negative
bool is_double_neg(double f);

//Returns 1 or -1 if input is positive or negative
int signum(double f);
size4s_t test_extqf32_pre_rounding(size4s_t orig32_mantissa, uint8_t LREQ);
unfloat parse_extqf32 (uint32_t in, uint8_t in_ext, int coproc_mode);
unfloat parse_extqf16 (uint16_t in, uint8_t in_ext, int coproc_mode);

uint8_t test_produce_extQF_LRQ_bits(double sig_remainder,double sig_low);
uint64_t rnd_sat_extqf32 (unfloat u, qfrnd_mode_enum_t qfrnd);
uint64_t rnd_sat_extqf16 (unfloat u, qfrnd_mode_enum_t qfrnd);
size4s_t conv_sf_extqf32 (uint32_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd, int coproc_mode);
size2s_t conv_hf_extqf32 (uint32_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd, int coproc_mode);
size2s_t conv_hf_extqf16 (uint16_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd, int coproc_mode);
uint64_t conv_qf16_f8(uint8_t a, qfrnd_mode_enum_t qfrnd);

int is_sf_infinity(unfloat u); 
int is_sf_nan(unfloat u);
int is_sf_zero(unfloat u);

int is_hf_infinity(unfloat u);
int is_hf_nan(unfloat u);
int is_hf_zero(unfloat u);

uint64_t handle_infinity_nan_mpy(unfloat u, unfloat v, uint64_t pos_nan_val, uint64_t neg_nan_val, uint64_t pos_inf_val, uint64_t neg_inf_val);
uint64_t handle_infinity_nan_add(unfloat u, unfloat v, uint64_t pos_nan_val, uint64_t neg_nan_val, uint64_t pos_inf_val, uint64_t neg_inf_val);

void check_mpy_compliance(unfloat a, unfloat b, int size, uint32_t result, uint8_t rext);
void check_add_compliance(unfloat a, unfloat b, int size, uint32_t result, uint8_t rext);
void test_ieee_compliance (uint32_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd, size4s_t in1, size4s_t in2);
int get_unfloat_exp(unfloat A, unfloat B, int sub, int e_min);
int qf_vilog2(f_type type, uint32_t a, uint8_t a_ext);
#endif
