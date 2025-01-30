/*
 *  Copyright(c) 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

//FP8 defines
//#define FP8_DEF_NAN      0x7F
//#define isNaNF8UI( a ) (((~(a) & 0x78) == 0) && ((a) & 0x07))
#define FP8_DEF_NAN      0x80
#define isNaNF8UI( a ) ((a) & 0x80)
#define isInfF8UI( a ) (((~(a) & 0x78) == 0) && (((a) & 0x07) == 0))
#define signF8UI( a ) ((bool) ((uint8_t) (a)>>7))
#define expF8UI( a ) ((int_fast8_t) ((a)>>3) & 0xF)
#define fracF8UI( a ) ((a) & 0x07)

struct exp8_sig8 { int_fast8_t exp; uint_fast8_t sig; };
//struct exp8_sig8 { int_fast8_t exp; uint_fast32_t sig; };
struct exp8_sig8 normSubnormalF8Sig( uint_fast8_t sig );

uint_fast8_t countLeadingZeros8( uint8_t a );

union ui8_f8 { uint8_t ui; float  f; };

uint32_t f8_to_f32( uint8_t a );
uint8_t f32_to_f8( uint32_t a );

uint16_t f8_to_f16( uint8_t a );
uint8_t f16_to_f8( uint16_t a, uint32_t option );

#define packToF8UI( sign, exp, sig ) (((uint8_t) (sign)<<7) + ((uint8_t) (exp)<<3) + (sig))

//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// FP8 ADD/SUB/MPY instructions
//--------------------------------------------------------------------------

uint16_t fp_add_8f_8f (uint8_t op1, uint8_t op2);
uint16_t fp_sub_8f_8f (uint8_t op1, uint8_t op2);
uint16_t fp_mult_8f_8f (uint8_t op1, uint8_t op2);
uint16_t fp_mult_8f_8f_acc (uint8_t op1, uint8_t op2, uint16_t acc);

uint8_t fp_neg_8f(uint8_t op1);
uint8_t fp_abs_8f(uint8_t op1);

uint8_t fp_max_8f(uint8_t op1,uint8_t op2);
uint8_t fp_min_8f(uint8_t op1,uint8_t op2);

//--------------------------------------------------------------------------

