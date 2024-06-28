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

#include "qemu/osdep.h"
#include "mmvec_qfloat.h"
#include <stdbool.h>
#include <assert.h>
#include <math.h>

static unfloat parse_type_to_unfloat(f_type type, uint32_t input, uint8_t ext, bool daz_mode) {
    switch(type){
        case BF: return parse_sf(((uint32_t)input) << 16);
        case SF: return parse_sf_daz(input,daz_mode);
        case HF: return parse_hf(input);
        case EXTQF32: return parse_extqf32(input,ext,0);
        case EXTQF16: return parse_extqf16(input,ext,0);
        case F8: return parse_f8(input);
        default: g_assert_not_reached(); //This is the legacy QF32 and QF16
    }
}

static int get_emin(f_type type) {
    switch(type) {
        case BF: return E_MIN_SF;
        case SF: return E_MIN_SF;
        case HF: return E_MIN_HF;
        case EXTQF32: return E_MIN_EXTQF32;
        case EXTQF16: return E_MIN_EXTQF16;
        case F8: return E_MIN_F8;
        default: g_assert_not_reached();
    }
}

static double unfloat_to_double(unfloat u) {
    return (u.sign ? -1: 1) * ldexp(u.sig,u.exp);
}

static bool is_unfloat_normalized(f_type type, unfloat a) {
    if(a.inf || a.nan) return true;
    else if(fabs(a.sig) <= 1.0L)
    {
        if(a.sig == 1.0L && a.inexact >= 0) return true;
        else if(a.sig == -1.0L && a.inexact <= 0) return true;
        else return false;
    }
    return true;
}

void check_cmpgt_compliance(f_type type, uint32_t ain, uint32_t bin, int result) {
    unfloat a = parse_type_to_unfloat(type,ain,0,0);
    unfloat b = parse_type_to_unfloat(type,bin,0,0);
    
    //NaN results will always be false. If we have a nan input and result is true, assert
    if(a.nan || b.nan) {
        assert(result == false);
        return;
    }
    //Two zeroes should not be greater than
    if(a.zero && b.zero) assert(result == false);
    
    //Infinities with same sign will result in true
    if(a.inf && b.inf && (a.sign == b.sign)) assert(result == false);

    double af = unfloat_to_double(a);
    double bf = unfloat_to_double(b);

    //A-B is a positive value, that means A is greater than B.
    if(result == true) assert((af-bf) > 0);
}

void check_cmpeq_compliance(f_type type, uint32_t ain, uint32_t bin, int result) {
    //Result of 3 means 
    unfloat a = parse_type_to_unfloat(type,ain,0,0);
    unfloat b = parse_type_to_unfloat(type,bin,0,0);

    //NaN results will always be false. If we have a nan input and result is true, assert
    if(a.nan || b.nan) {
        assert(result == false);
        return;
    }

    //When a cmp is true, it sets the Q bits for the entire word or halfword.
    //So for 32-bit (SF), result is b1111.
    //For 16-bit (HF), result is b11
    int cmptrue = (type == SF) ? 0xF : 0x3;

    //Two zeroes should always equal no matter the sign
    if(a.zero && b.zero) assert(result == cmptrue);

    //-inf does not equal inf, so check if signs are opposite but result is still true
    if(a.inf && b.inf && (a.sign ^ b.sign)) assert(result == 0);
    
    //Infinities with same sign will result in true
    if(a.inf && b.inf && (a.sign == b.sign)) assert(result == cmptrue);
    double af = unfloat_to_double(a);
    double bf = unfloat_to_double(b);

    //If A-B is zero, that means they are equal
    if(result == true) assert((af-bf) == 0);
}

void check_minmax_compliance(f_type type, bool max, uint32_t ain, uint32_t bin, uint32_t result) {
	unfloat a = parse_type_to_unfloat(type,ain,0,0);
	unfloat b = parse_type_to_unfloat(type,bin,0,0);
	unfloat r = parse_type_to_unfloat(type,result,0,0);

	//MinMax does not filter nan
	if(a.nan || b.nan) {
        assert(r.nan);
        return;
    }

    //max will always return an +inf if input is +inf and other input is non nan
    if((max && ((!a.sign && a.inf) || (!b.sign && b.inf)))) assert(!r.sign && r.inf);

    //min will always return an -inf if input is -inf and other input is non nan
    if((!max && ((a.sign && a.inf) || (b.sign && b.inf)))) assert(r.sign && r.inf);

    //If inputs are finite, then check more conditions
    if(!a.inf && !a.nan && !b.inf && !b.nan) {
        //If result does not equal either input then assert
        if(result != ain && result != bin) assert(0);

        //Max should always return a positive value if signs are opposite
        if(max && (a.sign ^ b.sign)) assert(!r.sign);

        //Min should always return a negative value if signs are opposite
        if(!max && (a.sign ^ b.sign)) assert(r.sign);

        double af = unfloat_to_double(a);
        double bf = unfloat_to_double(b);
        double rf = unfloat_to_double(r);

        //If A-B is 0, then A and B are equal so result should equal both
        if((af-bf) == 0) assert(af==rf && bf==rf);
        if(max) {
            //If max, output must be greater than or equal to inputs
            assert(rf >= af && rf >= bf);

            //If A-B greater than 0, then A is larger than B and R should be A
            if((af-bf) > 0) assert(result == ain);
            //If A-B less than 0, then B is larger than A and R should be B
            else if((af-bf) < 0) assert(result == bin);
        } else {
            //If min, output must be less than or equal to inputs
            assert(rf <= af && rf <= bf);

            //If A-B greater than 0, then A is larger than B and R should be B
            if((af-bf) > 0) assert(result == bin);
            //If A-B less than 0, then B is larger than A and R should be A
            else if((af-bf) < 0) assert(result == ain);
        }

        if (max) assert(fmax(af,bf) == rf);
        else assert(fmin(af,bf) == rf); 
    }
}

static void check_unary_compliance(f_type atype, f_type rtype, unfloat a, unfloat r) {
    //A NaN input is going to always result in NaN
    assert(a.nan == r.nan);
    //A Inf input is going to always result in inf
    if(a.inf) assert(r.inf);
    //A zero input is going to always result in zero
    assert(a.zero == r.zero);
    //Any result that is not at emin must be normalized
    if(r.exp != get_emin(rtype)) {
       assert(is_unfloat_normalized(rtype,r));
    }
    //Absolute values should match
    if(!(a.nan || a.inf || r.nan || r.inf)) {
        assert(ldexp(fabs(a.sig),a.exp) == ldexp(fabs(r.sig),r.exp));
    }
    //Unary converts are exact with no rounding
    //except on overflow, where result is an inexact inf
    if(!(r.nan || r.inf)) assert(r.inexact == 0);
}

void check_vabs_compliance(f_type atype, f_type rtype, uint32_t ain, uint8_t a_ext, uint32_t rin, uint8_t r_ext, bool daz_mode) {
    unfloat a = parse_type_to_unfloat(atype,ain,a_ext,daz_mode);
    unfloat r = parse_type_to_unfloat(rtype,rin,r_ext,daz_mode);

    //Vabs will always have the sign as positive
    assert(!is_unfloat_neg(r));
    check_unary_compliance(atype,rtype,a,r);
}

void check_vneg_compliance(f_type atype, f_type rtype, uint32_t ain, uint8_t a_ext, uint32_t rin, uint8_t r_ext, bool daz_mode) {
    unfloat a = parse_type_to_unfloat(atype,ain,a_ext,daz_mode);
    unfloat r = parse_type_to_unfloat(rtype,rin,r_ext,daz_mode);

    //Vneg will flip the sign
    assert(is_unfloat_neg(a) != is_unfloat_neg(r));
    check_unary_compliance(atype,rtype,a,r);
}

void check_vconv_compliance(f_type atype, f_type rtype, uint32_t ain, uint8_t a_ext, uint32_t rin, uint8_t r_ext, bool daz_mode) {
    unfloat a = parse_type_to_unfloat(atype,ain,a_ext,daz_mode);
    unfloat r = parse_type_to_unfloat(rtype,rin,r_ext,daz_mode);

    //Converts dont change the sign
    assert(is_unfloat_neg(a) == is_unfloat_neg(r));
    check_unary_compliance(atype,rtype,a,r);
}

void check_narrowing_vconv_compliance(f_type atype, f_type rtype, uint32_t ain, uint8_t a_ext, uint32_t rin, uint8_t r_ext) {
    unfloat a = parse_type_to_unfloat(atype,ain,a_ext,0);
    unfloat r = parse_type_to_unfloat(rtype,rin,r_ext,0);

    //Converts dont change the sign
    assert(is_unfloat_neg(a) == is_unfloat_neg(r));
    //A NaN input is going to always result in NaN
    assert(a.nan == r.nan);
    //A Inf input is going to always result in inf
    if(a.inf) assert(r.inf);
    //A zero input is going to always result in zero
    if(a.zero) assert(r.zero);
    //Any result that is not at emin must be normalized
    if(r.exp != get_emin(rtype)) {
       assert(is_unfloat_normalized(rtype,r));
    }
    //Unary converts are exact with no rounding
    //except on overflow, where result is an inexact inf
    if(!(r.nan || r.inf)) assert(r.inexact == 0);
}

void check_vilog2_compliance(f_type atype, uint32_t ain, uint8_t a_ext, int result) {
    unfloat a = parse_type_to_unfloat(atype,ain,a_ext,0);

    if(a.zero || a.inf || a.nan) {
        int assert_val = 0;
        if(atype == EXTQF32 || atype == SF) {
            if(a.zero) {
                assert_val = (result == QF32_ILOG2_ZERO_EXP);
            }
            else if(a.nan) {
                assert_val = (result == QF32_ILOG2_NAN_EXP);
            }
            else if(a.inf) {
                assert_val = (result == QF32_ILOG2_INF_EXP);
            }
        } else if(atype == EXTQF16 || atype == HF) {
            if(a.zero) {
                assert_val = (result == (short)QF16_ILOG2_ZERO_EXP); 
            }
            else if(a.nan) { 
                assert_val = (result == (short)QF16_ILOG2_NAN_EXP);
            }
            else if(a.inf) {
                assert_val = (result == (short)QF16_ILOG2_INF_EXP); 
            }
        }
        
        if(assert_val == 0) {
            printf("Exponent from vilog2 special case is incorrect\nInput 0x%x input_ext: 0x%x exp: %d", ain, a_ext, result);
            assert(0);
        }
        return;
    }

    double af = ldexp(a.sig,result);
    double exp = floor(log2(fabs(af)));
    double sig = fabs(af) * pow(2,-exp);

    //Check if constructed floating point using the calculated exponent is in normal range
    if((sig >= 1 && sig < 2) == false) {
        printf("Exponent from vilog2 does not produce normalized sig\nInput 0x%x input_ext: 0x%x exp: %d", ain, a_ext, result);
        assert(0);
    }
}
