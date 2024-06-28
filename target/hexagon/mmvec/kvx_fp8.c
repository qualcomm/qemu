/*
 *  Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
File content : FP8 instruction functional definitions
Author : Sundaravalli Aravazhi
*/
//#include <iostream>

#include "qemu/osdep.h"
#include "cpu.h"
#include "kvx_ieee.h"
#include "kvx_fp8.h"

/*
//FP8 defines
#define FP8_DEF_NAN      0x7F
#define isNaNF8UI( a ) (((~(a) & 0x78) == 0) && ((a) & 0x07))
#define isInfF8UI( a ) (((~(a) & 0x78) == 0) && (((a) & 0x07) == 0))

uint16_t fp_add_8f_8f (uint8_t op1, uint8_t op2);
uint16_t fp_sub_8f_8f (uint8_t op1, uint8_t op2);
uint16_t fp_mult_8f_8f (uint8_t op1, uint8_t op2);

uint32_t f8_to_f32( uint8_t a );
*/

static uint32_t shiftRightJam16( uint16_t a, uint_fast8_t dist )
{
    return
        (dist < 15) ? a>>dist | ((uint16_t) (a<<(-dist & 15)) != 0) : (a != 0);
}



static uint16_t roundPackToF8( bool sign, int_fast8_t exp, uint_fast8_t sig )
{
    bool roundNearEven;
    uint_fast8_t roundIncrement, roundBits;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    roundNearEven  = 1;
    roundIncrement = 0x4;
    roundBits      = sig & 0x7;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( 0xF <= (unsigned int) exp ) {
        if ( exp < 0 ) {
            sig = shiftRightJam16( sig, -exp );
            exp = 0;
            roundBits = sig & 0x7;}
        else if ( (0x6 < exp) || (0x40 <= sig + roundIncrement) ) {
	        return packToF8UI( sign, 0xF, 7 ) - ! roundIncrement;	
              }
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    sig = (sig + roundIncrement)>>3;
    sig &= ~(uint_fast8_t) (! (roundBits ^ 4) & roundNearEven);
    if ( ! sig ) exp = 0;
    
    return packToF8UI( sign, exp, sig );

}



uint_fast8_t countLeadingZeros8( uint8_t a )
{
    uint_fast8_t count;
    const uint_least8_t countLeadingZeros8[8] = {
        4, 3, 2, 2, 1, 1, 1, 1
    };

    count = countLeadingZeros8[a];
    return count;

}

struct exp8_sig8 normSubnormalF8Sig( uint_fast8_t sig )
{
    int_fast8_t shiftDist;
    struct exp8_sig8 z;

    shiftDist = countLeadingZeros8( sig );
    z.exp = -shiftDist;
    z.sig = sig<<shiftDist;
    return z;

}

uint32_t f8_to_f32( uint8_t a )
{
    bool sign;
    int_fast8_t exp;
    uint_fast8_t frac;
    struct exp8_sig8 normExpSig;

    sign = signF8UI( a );
    exp  = expF8UI ( a );
    frac = fracF8UI( a );


    /*if ( exp == 0xF ) {
        if ( frac ) {
           return FP32_DEF_NAN;
        } else {
            return packToF32UI( sign, 0xFF, 0 );
        }
    }*/


    if ( ! exp ) {
        if ( ! frac ) {
            return packToF32UI( sign, 0, 0 );
        }
        normExpSig = normSubnormalF8Sig( frac );
        exp = normExpSig.exp;
        frac = normExpSig.sig;
    }

    return packToF32UI( sign, exp + 0x77, (uint_fast32_t) frac<<20 );

}

uint16_t f8_to_f16( uint8_t a )
{

    bool sign;
    int_fast8_t exp;
    uint_fast8_t frac;
    struct exp8_sig8 normExpSig;

    sign = signF8UI( a );
    exp  = expF8UI ( a );
    frac = fracF8UI( a );

    if (a == 0x80) return 0x7FFF;

    if ( ! exp ) {
        if ( ! frac ) {
            return packToF16UI( sign, 0, 0 );
        }
        normExpSig = normSubnormalF8Sig( frac );
        exp = normExpSig.exp;
        frac = normExpSig.sig;
    }

    return packToF16UI( sign, exp + 0x7, (uint_fast32_t) frac<<7 );

    
}

uint8_t f16_to_f8( uint16_t a, uint32_t option )
{

    bool sign;
    int_fast8_t exp;
    uint_fast16_t frac;
    uint_fast8_t frac8;
    uint8_t result;

    //bool option = 0; // 1 - map to max norm ; 0 - map to 0x80

    sign = signF16UI( a );
    exp  = expF16UI ( a );
    frac = fracF16UI( a );

    // Inf and NaN case
    if ( exp == 0x1F ) {
        if ( frac ) {
	          result = 0x80; // NaN case
        } 
        else { // Inf case
	          if(!option) result = 0x80; // map to -ve zero
	          else result = packToF8UI( sign, 0XF, 7 ); // map to max norm 	    
        }
    }
    else {
        // underflow case
        if ((exp-15)<=(-11)) {
          if ((exp-15 == -11) && frac) result = packToF8UI( sign, 0, 1 ); //LOWEST VALUE IN FP8
          //else if ((exp == 4) && (frac == 0)) return 0x00;
          else result = 0x00; 
        }
        else {
            /*------------------------------------------------------------------------
            frac>>4              : keeping 6 bit of precision out ot 10 bits in FP16
            (frac & 0xF) != 0) : setting the sticky bit required for rounding
            *------------------------------------------------------------------------*/
            frac8 = frac>>4 | ((frac & 0xF) != 0);

            //If input =  Zero
            if ( ! (exp | frac8) ) {
                result = packToF8UI( sign, 0, 0 );
            }
            else {
                //if ((exp>=22) && ((((frac>>7)+1) & 0x8) == 0x8)) result = packToF8UI( sign, 0XF, 7 ); //Overflow beyond 240 and -240
                if ((exp>22) || ((exp==22) && ((((frac>>6)+1) & 0x10) == 0x10))) { //Overflow beyond 240 and -240
                    if (option) result = packToF8UI( sign, 0XF, 7 ); // map to max norm
                    else result = 0x80; // map to NaN
                }
                else result = roundPackToF8( sign, exp - 0x8, frac8 | 0x40 );
            } 
        }
    }
    
    #ifdef DEBUG
    printf("ISSDebug :  USR_24_bit = %d\n", option);
    printf("ISSDebug :  Input for vcvt_f8_hf = 0x%04x\n", a);
    printf("ISSDebug :  Output for vcvt_f8_hf = 0x%02x\n", result);
    #endif

    return result;

}

uint16_t fp_add_8f_8f (uint8_t op1, uint8_t op2) //changed size of i/p
{

    union ui32_f32 u_op1;
    union ui32_f32 u_op2;
    union ui32_f32 u_rslt;

    uint32_t op1_f32;
    uint32_t op2_f32;

    float a,b,rslt;
    uint32_t result_f32;
    uint16_t result;  // changed size

    #ifdef DEBUG_MMVEC_QF
    printf("Debug : op1 =0x%08x\n",op1);
    printf("Debug : op2 =0x%08x\n",op2);
    #endif

    if ((op1 == 0x80) | (op2 == 0x80)) return 0x7FFF;
    //if(isNaNF8UI(op1) || isNaNF8UI(op2)) 
    //   return FP8_DEF_NAN;

    op1_f32 = f8_to_f32(op1);
    op2_f32 = f8_to_f32(op2);

    u_op1.ui = op1_f32;
    u_op2.ui = op2_f32;
    a = u_op1.f;
    b = u_op2.f;
    rslt = a+b;
    u_rslt.f = rslt;
    result_f32 = u_rslt.ui;

    result = f32_to_f16(result_f32);

    #ifdef DEBUG_MMVEC_QF
    printf("Debug : a = %f\n",a);
    printf("Debug : b = %f\n",b);
    printf("Debug : rslt = %f\n",rslt);
    printf("Debug : result =0x%08x\n",result);
    #endif

    return result;
}

uint16_t fp_sub_8f_8f (uint8_t op1, uint8_t op2)
{

    union ui32_f32 u_op1;
    union ui32_f32 u_op2;
    union ui32_f32 u_rslt;

    uint32_t op1_f32;
    uint32_t op2_f32;

    float a,b,rslt;
    uint32_t result_f32;
    uint16_t result;

    #ifdef DEBUG_MMVEC_QF
    printf("Debug : op1 =0x%08x\n",op1);
    printf("Debug : op2 =0x%08x\n",op2);
    #endif

    if ((op1 == 0x80) | (op2 == 0x80)) return 0x7FFF;
    //if(isNaNF8UI(op1) || isNaNF8UI(op2)) 
    //   return FP8_DEF_NAN;

    op1_f32 = f8_to_f32(op1);
    op2_f32 = f8_to_f32(op2);

    u_op1.ui = op1_f32;
    u_op2.ui = op2_f32;
    a = u_op1.f;
    b = u_op2.f;
    rslt = a-b;
    u_rslt.f = rslt;
    result_f32 = u_rslt.ui;

    result = f32_to_f16(result_f32);

    #ifdef DEBUG_MMVEC_QF
    printf("Debug : a = %f\n",a);
    printf("Debug : b = %f\n",b);
    printf("Debug : rslt = %f\n",rslt);
    printf("Debug : result =0x%08x\n",result);
    #endif

    return result;
}

uint16_t fp_mult_8f_8f (uint8_t op1, uint8_t op2)
{

    union ui32_f32 u_op1;
    union ui32_f32 u_op2;
    union ui32_f32 u_rslt;

    uint32_t op1_f32;
    uint32_t op2_f32;

    float a,b,rslt;
    uint32_t result_f32;
    uint16_t result;

    #ifdef DEBUG_MMVEC_QF
    printf("Debug : op1 =0x%08x\n",op1);
    printf("Debug : op2 =0x%08x\n",op2);
    #endif

    if ((op1 == 0x80) | (op2 == 0x80)) return 0x7FFF;
    //if(isNaNF8UI(op1) || isNaNF8UI(op2)) 
    //   return FP8_DEF_NAN;

    op1_f32 = f8_to_f32(op1);
    op2_f32 = f8_to_f32(op2);

    u_op1.ui = op1_f32;
    u_op2.ui = op2_f32;
    a = u_op1.f;
    b = u_op2.f;
    rslt = a*b;
    u_rslt.f = rslt;
    result_f32 = u_rslt.ui;

    result = f32_to_f16(result_f32);

    #ifdef DEBUG_MMVEC_QF
    printf("Debug : a = %f\n",a);
    printf("Debug : b = %f\n",b);
    printf("Debug : rslt = %f\n",rslt);
    printf("Debug : result =0x%08x\n",result);
    #endif

    return result;
}

uint16_t fp_mult_8f_8f_acc (uint8_t op1, uint8_t op2, uint16_t acc)
{
    union ui32_f32 u_op1;
    union ui32_f32 u_op2;
    union ui32_f32 u_acc;
    union ui32_f32 u_rslt;

    uint32_t op1_f32;
    uint32_t op2_f32;
    uint32_t acc_f32;

    float a,b,facc,rslt;
    uint32_t result_f32;
    uint16_t result;

    #ifdef DEBUG_MMVEC_QF
    printf("Debug : op1 =0x%04x\n",op1);
    printf("Debug : op2 =0x%04x\n",op2);
    printf("Debug : acc =0x%04x\n",acc);
    #endif

    if((op1 == 0x80) | (op2 == 0x80) | (acc == 0x7FFF)) return 0x7FFF;
    //if(isNaNF16UI(op1) || isNaNF16UI(op2) || isNaNF16UI(acc)) 
    //   return FP16_DEF_NAN;

    op1_f32 = f8_to_f32(op1);
    op2_f32 = f8_to_f32(op2);
    acc_f32 = f16_to_f32(acc);

    u_op1.ui = op1_f32;
    u_op2.ui = op2_f32;
    u_acc.ui = acc_f32;
    a = u_op1.f;
    b = u_op2.f;
    facc = u_acc.f;
    rslt = (a * b) + facc;
    u_rslt.f = rslt;
    result_f32 = u_rslt.ui;

    result = f32_to_f16(result_f32);

    #ifdef DEBUG_MMVEC_QF
    printf("Debug : a = %f\n",a);
    printf("Debug : b = %f\n",b);
    printf("Debug : facc = %f\n",facc);
    printf("Debug : rslt = %f\n",rslt);
    printf("Debug : result =0x%04x\n",result);
    #endif

    return result;
}

uint8_t fp_neg_8f(uint8_t op1)
{
    union ui8_f8 u_op1;

    float result_f;
    //uint32_t result_f32;
    uint8_t result;

    if (op1 == 0x80) return 0x80; // NaN/INF case 
    else if (op1 == 0x00) return 0x00; //pure zero

    u_op1.ui = op1^0x80;

    result_f = u_op1.f;
    u_op1.f = result_f;
    
    result = u_op1.ui;
    return result;
}

uint8_t fp_abs_8f(uint8_t op1)
{
    union ui8_f8 u_op1;

    float result_f;
    //uint32_t result_f32;
    uint8_t result;

    if (op1 == 0x80) return 0x80;
    //else if (op1 == 0x00) return 0x00; 

    u_op1.ui = signF8UI(op1) ? (op1 ^ 0x80) : op1; //op1;

    result_f = u_op1.f;
    u_op1.f = result_f;
    result = u_op1.ui;
    return result;
}

uint8_t fp_max_8f(uint8_t op1,uint8_t op2)
{
    union ui8_f8 u_rslt;

    uint8_t result;

    if ((op1 == 0x80) | (op2 == 0x80)) return 0x80;


    if ((signF8UI(op1) == 0) && (signF8UI(op2) == 0)) {
        if ((op1&0x7F) > (op2&0x7F)) u_rslt.ui = op1;
        else u_rslt.ui = op2;
    }
    else if ((signF8UI(op1) == 0) && (signF8UI(op2) == 1)) {
        u_rslt.ui = op1;
    }
    else if ((signF8UI(op1) == 1) && (signF8UI(op2) == 0)) {
        u_rslt.ui = op2;
    }
    else {
        if ((op1&0x7F) > (op2&0x7F)) u_rslt.ui = op2;
        else u_rslt.ui = op1;
    }

    
    result = u_rslt.ui;

    return result;

}

uint8_t fp_min_8f(uint8_t op1,uint8_t op2)
{
    union ui8_f8 u_rslt;

    uint8_t result;

    if ((op1 == 0x80) | (op2 == 0x80)) return 0x80;

    if ((signF8UI(op1) == 0) && (signF8UI(op2) == 0)) {
        if ((op1&0x7F) < (op2&0x7F)) u_rslt.ui = op1;
        else u_rslt.ui = op2;
    }
    else if ((signF8UI(op1) == 0) && (signF8UI(op2) == 1)) {
        u_rslt.ui = op2;
    }
    else if ((signF8UI(op1) == 1) && (signF8UI(op2) == 0)) {
        u_rslt.ui = op1;
    }
    else {
        if ((op1&0x7F) < (op2&0x7F)) u_rslt.ui = op2;
        else u_rslt.ui = op1;
    }

    result = u_rslt.ui;

    return result;

}

