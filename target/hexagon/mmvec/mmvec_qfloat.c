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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#if !defined(__clang__)
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#include "qemu/osdep.h"
#include "mmvec_qfloat.h"
#include "mmvec_qfloat_compliance.h"
#include <math.h>

//Rounding mode parameters
qfrnd_mode_t qfrnd_modes[MAX_RND_MODES] = {
	//Round to nearest even
	{ .even_pos_threshold = 0.75,
	  .odd_neg_threshold  = 0.5,
	  .rnd_to_pos_neg = false,
	  .ovf_val32 = MAX_SIG_QF32,
	  .undf_val32 = 0,
	  .ovf_val16 = MAX_SIG_QF16,
	  .undf_val16 = 0,
          .ieee_pos_overflow = {.x = {.sign = 0, .exp = MAX_BIASED_E_SF, .mant = 0x0}},
          .ieee_neg_overflow = {.x = {.sign = 1, .exp = MAX_BIASED_E_SF, .mant = 0x0}},
          .ieee_hf_pos_overflow = {.x = {.sign = 0, .exp = MAX_BIASED_E_HF, .mant = 0x0}},
          .ieee_hf_neg_overflow = {.x = {.sign = 1, .exp = MAX_BIASED_E_HF, .mant = 0x0}},
	  .rnd_mode = RND_TO_NEAREST_EVEN
	},
	//Round Towards Zero
	{ .even_pos_threshold = 1.0,
	  .odd_neg_threshold  = 0.25,
	  .rnd_to_pos_neg = true,
	  .ovf_val32 = MAX_SIG_QF32,
	  .undf_val32 = 0,
	  .ovf_val16 = MAX_SIG_QF16,
	  .undf_val16 = 0,
          .ieee_pos_overflow = {.x = {.sign = 0, .exp = MAX_BIASED_E_SF-1, .mant = MAX_SIG_QF32}},
          .ieee_neg_overflow = {.x = {.sign = 1, .exp = MAX_BIASED_E_SF-1, .mant = MAX_SIG_QF32}},
          .ieee_hf_pos_overflow = {.x = {.sign = 0, .exp = MAX_BIASED_E_HF-1, .mant = MAX_SIG_QF16}},
          .ieee_hf_neg_overflow = {.x = {.sign = 1, .exp = MAX_BIASED_E_HF-1, .mant = MAX_SIG_QF16}},
	  .rnd_mode = RND_TO_ZERO
	},
	//Round Negative Infinity
	{ .even_pos_threshold = 1.0,
	  .odd_neg_threshold  = 1.0,
	  .rnd_to_pos_neg = true,
	  .ovf_val32 = MAX_SIG_QF32,
	  .undf_val32 = 0,
	  .ovf_val16 = MAX_SIG_QF16,
	  .undf_val16 = 0,
          .ieee_pos_overflow = {.x = {.sign = 0, .exp = MAX_BIASED_E_SF-1, .mant = MAX_SIG_QF32}},
          .ieee_neg_overflow = {.x = {.sign = 1, .exp = MAX_BIASED_E_SF, .mant = 0x0}},
          .ieee_hf_pos_overflow = {.x = {.sign = 0, .exp = MAX_BIASED_E_HF-1, .mant = MAX_SIG_QF16}},
          .ieee_hf_neg_overflow = {.x = {.sign = 1, .exp = MAX_BIASED_E_HF, .mant = 0x0}},
	  .rnd_mode = RND_TOWARDS_NEG_INF
	},
	//Round Towards Positive Infinity
	{ .even_pos_threshold = 0.25,
	  .odd_neg_threshold  = 0.25,
	  .rnd_to_pos_neg = true,
	  .ovf_val32 = MAX_SIG_QF32,
	  .undf_val32 = 0,
	  .ovf_val16 = MAX_SIG_QF16,
	  .undf_val16 = 0,
          .ieee_pos_overflow = {.x = {.sign = 0, .exp = MAX_BIASED_E_SF, .mant = 0x0}},
          .ieee_neg_overflow = {.x = {.sign = 1, .exp = MAX_BIASED_E_SF-1, .mant = MAX_SIG_QF32}},
          .ieee_hf_pos_overflow = {.x = {.sign = 0, .exp = MAX_BIASED_E_HF, .mant = 0x0}},
          .ieee_hf_neg_overflow = {.x = {.sign = 1, .exp = MAX_BIASED_E_HF-1, .mant = MAX_SIG_QF16}},
	  .rnd_mode = RND_TOWARDS_POS_INF
	}
};

/*###########################################################################################
#############################################################################################
############################## Common indicators and functions ##############################
#############################################################################################
#############################################################################################*/

bool is_double_pos(double f) { return signbit(f) == 0; }
bool is_double_neg(double f) { return ((signbit(f) == 0) ? 0 : 1); }
bool is_unfloat_neg(unfloat u) { return (is_double_neg(u.sig) ^ u.sign); }
int signum(double val) { return (val > 0) - (val < 0); }


//Take one's complement of the mantissa for QF32
size4s_t negate32(size4s_t in)
{
  size4s_t out;
  out = in>>8;
  out = ~out;
  out = (out<<8) | (in & 0xFF);
  return out;
}
//Take one's complement of the mantissa for QF16
size2s_t negate16(size2s_t in)
{
  size2s_t out;
  out = in>>5;
  out = ~out;
  out = (out<<5) | (in & 0x1F);
  return out;
}
//Change sign for SF
size4s_t negate_sf(size4s_t in)
{
  size4s_t out;
  int sign;
  sign = (in>>31) & 1;
  sign = ~sign;
  out = (sign<<31) | (in & 0x7FFFFFFF);
  return out;
}
//Change sign for SF
size2s_t negate_hf(size2s_t in)
{
  size2s_t out;
  int sign;
  sign = (in>>15) & 1;
  sign = ~sign;
  out = (sign<<15) | (in & 0x7FFF);
  return out;
}

//Negate LREQ bits, special behavior since E bit isnt flipped
LREQ_t negate_LREQ(LREQ_t LREQ)
{
  LREQ_t neg_LREQ;

  //Flip LRQ bits but keep original E bit
  neg_LREQ.raw    = ~LREQ.raw & 0xF;
  neg_LREQ.bits.E = LREQ.bits.E;
  return neg_LREQ;
}

//functions to check for ieee inf, nans and zeros
bool is_unfloat_zero(unfloat u){ return (fabs(u.sig) == 0.0);}

int is_sf_infinity(unfloat u) { return (u.exp >= E_MAX_SF) && (fabs(u.sig) == 1.0);}
int is_sf_nan(unfloat u) { return (u.exp >= E_MAX_SF) && (fabs(u.sig) != 1.0);}

int is_hf_infinity(unfloat u) { return (u.exp >= E_MAX_HF) && (fabs(u.sig) == 1.0);}
int is_hf_nan(unfloat u) { return (u.exp >= E_MAX_HF) && (fabs(u.sig) != 1.0);}

int is_extqf32_nan(uint64_t in) { return (in == extqf32_pos_nan || in == extqf32_neg_nan || in == extqf32_pos_nan_inexact || in == extqf32_neg_nan_inexact); }
int is_extqf32_inf(uint64_t in) { return (in == extqf32_pos_inf_exact || in == extqf32_pos_inf_inexact || in == extqf32_neg_inf_exact || in == extqf32_neg_inf_inexact);}

int is_extqf16_nan(uint32_t in) { return (in == extqf16_pos_nan || in == extqf16_neg_nan);} 
int is_extqf16_inf(uint32_t in) { return (in == extqf16_pos_inf || in == extqf16_neg_inf);}



/*###########################################################################################
#############################################################################################
############################## NAN/INF input behavior #######################################
#############################################################################################
#############################################################################################*/

//use these functions within the check for max exponent - refer macros_def.py
uint64_t handle_infinity_nan_add(unfloat u, unfloat v, uint64_t pos_nan_val, uint64_t neg_nan_val,\
	uint64_t pos_inf_val, uint64_t neg_inf_val)
{
	bool u_neg, v_neg;
	uint64_t result;

	u_neg = (is_unfloat_neg(u));
	v_neg = (is_unfloat_neg(v));
	
	//if either of the inputs is a NaN
        if(u.nan || v.nan)
        {
		//+NaN has highest priority, so check if one input is +NaN and return
		if((!u_neg && u.nan) || (!v_neg && v.nan)) {
			result = pos_nan_val;
		} 
		//If none of the NaN inputs were +NaN, must be -NaN which is 2nd highest priority
		else {
			result = neg_nan_val;
		}
        }
	else
        {
		//if both operands are infinites and have opposing signs result in nan
		if(u.inf && v.inf && (u_neg ^ v_neg))
		{
			result = pos_nan_val;
		}
		else {
			if((u_neg && u.inf) || (v_neg && v.inf)) {
				result = neg_inf_val;
			} else {
				result = pos_inf_val;
			}
		}
        }
	return result;
}

//function to handle infiniteis and nan for 16 bit mpys
//use within the max exponent condition: same as above
uint64_t handle_infinity_nan_mpy(unfloat u, unfloat v, uint64_t pos_nan_val, uint64_t neg_nan_val,\
	uint64_t pos_inf_val, uint64_t neg_inf_val)
{
	bool u_neg, v_neg;

	u_neg = is_unfloat_neg(u);
	v_neg = is_unfloat_neg(v);

  if(u.nan || v.nan) //either one of the inputs is a nan
  {
     //result is nan and sign is the xor: usual mpy logic
     return (u_neg ^ v_neg) ? neg_nan_val : pos_nan_val;
  }
  else //implicitly means either one is a infinity
  {
     //if one of the operands is a zero and the other a infinity we result in a nan
     if(u.zero || v.zero)
     {
       return (u_neg ^ v_neg) ? neg_nan_val : pos_nan_val;
     }
     return (u_neg ^ v_neg) ? neg_inf_val : pos_inf_val; 
  }
}

/*###########################################################################################
#############################################################################################
######################## Parse SF/HF/F8/BF/QF16/QF32 to unfloat ################################
#############################################################################################
#############################################################################################*/

//Rounds qf32/qf16 values to a floating point format
double extqf32_pre_rounding(double orig32_mantissa, LREQ_t LREQ) {
	return round(orig32_mantissa + (LREQ.bits.R * 0.5) + (LREQ.bits.Q * 0.25) + 0.125);
}

double extqf16_pre_rounding(double orig16_mantissa, LREQ_t LREQ) {
	return round(orig16_mantissa + (LREQ.bits.R * 0.5) + 0.25);
}

unfloat parse_hf(size2s_t in)
{
    INIT_UNFLOAT(out)
    hf_t hf = {.raw = in};
    size2u_t sig; 

    out.exp = hf.x.exp;
    sig = hf.x.mant;

    /*implied MSB=1*/
    if(out.exp>0) 
        sig = (1<<10) | sig;

    out.exp = out.exp - BIAS_HF;
    if(out.exp<E_MIN_HF)
        out.exp = E_MIN_HF;

    //Legacy, used for min/max and older functions
    out.sign = hf.x.sign;
    out.sig = (double)sig * EPSILON16;
    out.inf = is_hf_infinity(out);
    out.nan = is_hf_nan(out);
    out.zero = is_unfloat_zero(out);

    return out;
}

unfloat parse_sf(size4s_t in)
{
    INIT_UNFLOAT(out)
    sf_t sf;
    size4u_t sig; 

    sf.raw = in;
    
    out.exp = sf.x.exp; 

    //take signif and sign extend
    sig = sf.x.mant;

    /*implied MSB=1*/
    if(out.exp>0) 
        sig = (1<<23) | sig;

    out.exp = out.exp - BIAS_SF;

    if(out.exp<E_MIN_SF)
        out.exp = E_MIN_SF;

    out.sign = sf.x.sign;
    out.sig = (double)sig * EPSILON32;
    out.inf = is_sf_infinity(out);
    out.nan = is_sf_nan(out);
    out.zero = is_unfloat_zero(out);

    return out;
}

//Parse SF but consider Denorm as Zero mode
unfloat parse_sf_daz(uint32_t in, bool daz_mode) {
	unfloat a = parse_sf(in);

	if(daz_mode) {
		if (a.exp == E_MIN_SF && fabs(a.sig) < 1.0) {
			a.zero = true;
			a.sig = 0.0;
		}
	}
	return a;
}


unfloat parse_extqf16(uint16_t in, uint8_t in_ext, int coproc_mode)
{
	INIT_UNFLOAT(out)

	LREQ_t LREQ = {.bits16.LR = (in_ext & 0x3)};
  
	int16_t orig16 = (int16_t)(in); 
	
	out.exp = ((uint8_t)(orig16 & 0x1F)) - BIAS_QF16;

 	//add L bit to at LSB to produce a 11-bit mantissa
  	int16_t orig16_mantissa = ((orig16 >> 4) & ~0x1) | LREQ.bits.L;

	//Handles pre-rounding
	double mantissa = extqf16_pre_rounding((double)orig16_mantissa,LREQ);

	//Handle normalization & denormalization next
	//Handle overflow to IEEE infinity here & underflow to IEEE zero
	//Because Ext_QF32 number does not necessarily begin with 1.xx, it could be 0.0001xx
	out.sig = mantissa * EPSILON16;
	out.sign = LREQ.bits.R;
	if(out.sign) out.sig = -out.sig;
/*
	if(coproc_mode >= 2) { 
        out.inf = is_extqf16_inf(out);
        out.nan = is_extqf16_nan(out);
    }
*/
	out.inf = is_extqf16_inf(((uint32_t)in << 4) | in_ext);
	out.nan = is_extqf16_nan(((uint32_t)in << 4) | in_ext);
    out.zero = is_unfloat_zero(out);

    return out;
}



unfloat parse_extqf32 (uint32_t in, uint8_t in_ext, int coproc_mode) {
  INIT_UNFLOAT(out)
  LREQ_t LREQ = {.raw = (in_ext & 0xF)};
  size4s_t orig32;
  uint16_t orig32_exponent;  
  size4s_t orig32_mantissa; 
  double mantissa;
  size2s_t exponent;
  

  orig32 = ((size4s_t)(in)); 
  orig32_exponent = orig32 & 0xFF;

  //add L bit to at LSB to produce a 24-bit mantissa
  orig32_mantissa = ((orig32 >> 7) & ~0x1) | LREQ.bits.L; //24 bits

  mantissa = extqf32_pre_rounding((double)orig32_mantissa,LREQ);

  //add E bit at MSB of expoent
  exponent = (size2s_t) (orig32_exponent | (LREQ.bits.E ? 0x0100 : 0x0));

  //First step, sub 128 to get to IEEE like
  //Second step, sub 255 for qf32 bias
  out.exp = ((exponent - 128) & 0x1FF) - 255; //FIXME move to macro

  //How about using float instead of double here ?
  out.sig = (double) mantissa * EPSILON32;

  out.sign = LREQ.bits.R;
  if(out.sign) out.sig = -out.sig;
  out.inexact = LREQ.bits.R != LREQ.bits.Q; 

  out.inf = is_extqf32_inf(((uint64_t)in << 4) | in_ext);
  out.nan = is_extqf32_nan(((uint64_t)in << 4) | in_ext);
  out.zero = is_unfloat_zero(out);
  return out;
}

unfloat parse_f8(uint8_t in) {
	INIT_UNFLOAT(out)
	f8_t f8 = {.raw = in};
	uint8_t sig;

	out.exp = f8.x.exp;
	sig = f8.x.mant;
	
	/*implied MSB=1 for normal*/
	if(out.exp > 0) {
		sig = (1<<f8_MANTBITS) | sig;
	}
	
	out.sign = f8.x.sign;
	out.exp = out.exp - F8_BIAS;
	out.sig = sig * EPSILON8;

	if(out.exp < E_MIN_F8) out.exp = E_MIN_F8;

	/*special encodings 0x80 is INF/NaN*/
	out.inf = (in == F8_INFNAN);
	out.nan = out.inf;
	out.zero = is_unfloat_zero(out) && !out.inf;

	return out;
}

/*###########################################################################################
#############################################################################################
############################## Rounding Behavior ############################################
#############################################################################################
#############################################################################################*/

double qmod(double sig_s,double modnum) {
    double R2, R3;
    //Get remainder from the scaled significand
    R2 = floor(sig_s/modnum)*modnum;
    R3 = sig_s - R2;
    return R3;
}

int get_inc(qfrnd_mode_t qfrnd, ulp_t ulp) {
	double cond, threshold;
	double odd_neg_threshold = qfrnd.odd_neg_threshold;
	double even_pos_threshold = qfrnd.even_pos_threshold;
	bool rnd_to_pos_neg = qfrnd.rnd_to_pos_neg;

	if(rnd_to_pos_neg) {
		cond = ulp.integer < 0.0;
	} else {
		cond = qmod(ulp.integer,2.0); 
	}

	if(cond) {
		threshold = odd_neg_threshold;
	} else {
		threshold = even_pos_threshold;
	}
	
	return ulp.fractional >= threshold;
}


double floor_to_frac(double v, double mod) {
	double integral;
	double fractional = modf(v,&integral);
	int positive = is_double_pos(integral);
	//Positive case
	if(positive) {
		 if(fractional >= mod) integral += mod;
	}
	//Negative case
	else {
		if(fabs(fractional) <= mod && fractional != 0.0)
			integral -= mod;
		else if(fabs(fractional) > mod)
			integral -= 1;
	}
	
	return integral;
}

ulp_t ulp_modf(double sig_scaled_in_ulp, int inexact) {
    ulp_t ulp;
    double sig_scaled_in_ulp_floorhalf = floor_to_frac(sig_scaled_in_ulp, 0.5);
    double sig_scaled_in_ulp_modhalf = sig_scaled_in_ulp - sig_scaled_in_ulp_floorhalf;
    double ulp_rx = sig_scaled_in_ulp_floorhalf - 0.5 * (sig_scaled_in_ulp_modhalf == 0)*(inexact < 0) \
	+ .25*(sig_scaled_in_ulp_modhalf != 0 || inexact != 0);

    //Split apart fractional and integer of significant, similar to modf but for ulp
    ulp.fractional = qmod(ulp_rx,1.0);
    ulp.integer    = floor(ulp_rx);
    return ulp;
}

//produce a result in extended QF32 format
fixed_float_t rnd_sat_extqf_sig(unfloat u, qfrnd_mode_enum_t qfrnd_mode, f_type ft, int e_min, int e_max, int bias, double _epsilon, double _units)
{
    fixed_float_t f; //return value of function
    LREQ_t LREQ = {.raw=0}; //LREQ bits returned in f
    double scale = 1.0;
    double sig_scaled;
    int exp_ovf=0; //overflow on exponent
    int exp_undf=0; //underflow on exponent
    int prod_ovf=0; //product overflow 

    //Values used to calculate sig and LREQ
    uint32_t sig_32 = 0;
    double sig_scaled_in_ULP;
    ulp_t ulp;
    int inc;
    double ulp_RQ1;
    double ulp2; 
    double LRQ;

    //parse out values from unfloat
    int exp = u.exp;
    double sig = u.sig;
    int inexact = u.inexact;

    //Check if sig is out of range before normalization
    if(fabs(sig) >= 2.0L)
    {
        if((sig == 2.0L && inexact < 0) || (sig == -2.0L && inexact >= 0)) {
           prod_ovf = 0; 
        } else {
            prod_ovf = 1;
        }
    }
    
    //Set scale factor if we need to denormalize
    if(prod_ovf && (exp < e_max)) {
      // right-shift result and adjust exponent by 1
      scale = 0.5;
      exp += 1;
    }

    //Example : To do right-shift by 1 bit 
    //i.e. if operand1 = operand2 = 1.5 => sig = 3.0, prod_ovf=1, sig_scaled should be 1.5
    sig_scaled = sig * scale;

    //Check for exponent overflow/underflow
    //Overflow exp is out of range (> E_MAX) or exp is at E_MAX but is not a denorm
    if((exp > e_max) \
	|| ((exp == e_max) && (sig_scaled > 1.0L || (sig_scaled == 1.0L && inexact >= 0) || sig_scaled < -1.0L || (sig_scaled == -1.0L && inexact < 0)))) {
            exp_ovf = 1; // Exponent overflow, make result to infinity
    } else if((exp < e_min)) {
        exp_undf = 1; // Exponent underflow, make result to 0
    }

    //No overflow or underflow, so calculate sig and exponent
    if(!exp_ovf && !exp_undf) {
	//Scale sig_scaled to ULP representation
        sig_scaled_in_ULP = (sig_scaled * _units);

	//Split scaled ulp into integer and fractional component
        ulp = ulp_modf(sig_scaled_in_ULP, inexact);
    
        //Calculate if we need to increment ULP by 0.5 or by 0.25
        inc = get_inc(qfrnd_modes[qfrnd_mode], ulp);

	//Add 0.5 or 0.25 depending on inc and add 0.125, which is the sticky inexact
	if(ft == EXTQF32) {
        	ulp_RQ1 = ulp.integer + inc*0.5 + (inc ^ (ulp.fractional != 0.0))*.25 + 0.125;
	} else {
        	ulp_RQ1 = ulp.integer + inc*0.5 + .25;
	}

	//Before going to 2's place, flip  sign if defer negation is set since sign will affect nearest 2
        if(u.sign) ulp_RQ1 = -ulp_RQ1;
	
	//Calculate ULP to nearest 2s place
	//floor ulp_RQ1 to get rid of fractional
	//If floor value is odd number, subtract by 1 to take to nearest even
        ulp2 = floor(ulp_RQ1) - (qmod(floor(ulp_RQ1),2.0));
    
	//Calculate LRQ in float notation
        LRQ = ulp_RQ1 - ulp2 - 0.125;

	//Extract out LRQ bits from float
        LREQ.bits.L = floor(LRQ);
        LREQ.bits.R = (((uint32_t)floor(2.0*LRQ)) % 2);
        LREQ.bits.Q = (((uint32_t)floor(4.0*LRQ)) % 2);

        sig_32 = ((int32_t)ulp2)>>1;
        //R and Q bits need to be the same irrespective of sign for the unsymmetrical rounding modes.
        //the defer neg would have flipped the R and Q bits too. Undoing those here
        if(u.sign && (LREQ.bits.R != LREQ.bits.Q) && ((qfrnd_mode == RND_TOWARDS_NEG_INF) || (qfrnd_mode == RND_TOWARDS_POS_INF)))
        {
           LREQ.bits.R = !LREQ.bits.R;
           LREQ.bits.Q = !LREQ.bits.Q; 
        }
    } else {
        // overflow to infinity
        if(exp_ovf) {
            exp = e_max; 
            if(qfrnd_mode == RND_TO_ZERO) exp--;
            if(qfrnd_mode == RND_TOWARDS_NEG_INF && !is_unfloat_neg(u)) exp--;
            if(qfrnd_mode == RND_TOWARDS_POS_INF && is_unfloat_neg(u)) exp--;
            if(ft==EXTQF32) {
                sig_32 = qfrnd_modes[qfrnd_mode].ovf_val32;
    	    } else if(ft==EXTQF16) {
                sig_32 = qfrnd_modes[qfrnd_mode].ovf_val16;
    	    }
    	    LREQ.bits.L = 0x1; 
    	    LREQ.bits.Q = 0x1;

    	    //If value was negative, set to -inf
            if(is_unfloat_neg(u)) { 
                sig_32 = ~sig_32;
                LREQ = negate_LREQ(LREQ);
            }
        }
        // underflow to 0
        else if(exp_undf) {
            exp = e_min; 
            if(ft == EXTQF32) {
                sig_32 = qfrnd_modes[qfrnd_mode].undf_val32;
    	    } else if(ft == EXTQF16) {
                sig_32 = qfrnd_modes[qfrnd_mode].undf_val16;
    	    }
	    
            //If we underflow when the sig is 0, we dont have inexactness
            if(qfrnd_mode == RND_TOWARDS_POS_INF)
            {
              LREQ.bits.R = 0x1;
              LREQ.bits.Q = 0x0;
            }
            else if(qfrnd_mode == RND_TOWARDS_NEG_INF)
            {
              LREQ.bits.R = 0x0;
              LREQ.bits.Q = 0x1;

            }
            else LREQ.bits.Q = 1;

            //If value was negative, set to -0
            if(is_unfloat_neg(u)) { 
                 LREQ = negate_LREQ(LREQ);
                 sig_32 = ~sig_32;
            }
        }
    }

    //Add bias to exponent
    exp += bias;

    //Produce extra exponent bit
    //EXTQF32 exponent is 9bit,including 1 extra bit E bit
    //Not used for 16 bit
    LREQ.bits.E = (exp >> 8) & 0x1;

    f.exp = exp;
    f.sig = sig_32;
    f.LREQ = LREQ;
    return f;
}

//produce a 36bit extended QF number from an unfloat
uint64_t rnd_sat_extqf32 (unfloat u, qfrnd_mode_enum_t qfrnd_mode)
{
    fixed_float_t f;
    uint64_t result;

    //Round and saturate unfloat representation of qfloat operation to qf32
    f = rnd_sat_extqf_sig(u, qfrnd_mode, EXTQF32, E_MIN_EXTQF32, E_MAX_EXTQF32, BIAS_EXTQF32, EPSILON32, UNITS32);
    
    //Setup 32 bit representation of qf32
    result = ((f.sig << 8) | (f.exp & 0xFF)) & QF32_BITMASK;

    //Append LREQ bits after the LSB of result to produce 36 bit result
    result = ((result << 4) | (f.LREQ.raw & 0xF)) & EXTQF32_BITMASK;
    return result;
}

//produce a 20bit extended QF number from an unfloat
uint64_t rnd_sat_extqf16 (unfloat u, qfrnd_mode_enum_t qfrnd_mode)
{
    fixed_float_t f;
    uint64_t result;

    //Round and saturate unfloat representation of qfloat operation to qf16
    f = rnd_sat_extqf_sig(u, qfrnd_mode, EXTQF16, E_MIN_QF16, E_MAX_QF16, BIAS_QF16, EPSILON16, UNITS16);
    result = ((f.sig << 5) | (f.exp & 0x1F)) & QF16_BITMASK;

    //Append LREQ bits after the LSB of result to produce 36 bit result
    result = ((result << 2) | (f.LREQ.bits16.LR & 0x3)) & EXTQF16_BITMASK;
    return result;
}


//TODO: is log2 slow?
double qf_ilogb(double sig) {
	if(is_double_neg(sig)) {
		return ceil(log2(-sig)) - 1;
	} else {
		return floor(log2(sig));
	}
}

int get_unfloat_exp(unfloat A, unfloat B, int sub, int e_min) {
    int result;
	if (A.exp > B.exp) {
		result = A.exp + qf_ilogb(A.sig) - sub;
		if (result < B.exp || A.sig == 0.0) result = B.exp;
	}  else {
		result = B.exp + qf_ilogb(B.sig) - sub;
		if (result < A.exp || B.sig == 0.0) result = A.exp;
	}
	return result;
}

/*###########################################################################################
#############################################################################################
############################## Convert QF32/QF16 to HF/SF ###################################
#############################################################################################
#############################################################################################*/

//Negates the sign bit by type set in ft
uint32_t negate_by_type(f_type ft, uint32_t res) {
    //Need to jam in sign bit
    if(ft == SF) {
        res = (res | 0x80000000) & 0xFFFFFFFF;
    } 
    else if(ft == F8) {
        if(res != 0x0) res = (res | 0x80) & 0xFF;
    }
    //Covers HF and BF
    else {
            res = (res | 0x8000) & 0xFFFF;
    }
    return res;
}

//Convert a unfloat to either HF/SF depending on ft
uint32_t conv_unfloat_to_ieee(unfloat u, f_type ft, qfrnd_mode_enum_t qfrnd, double _units, double _epsilon, int max_exp, int sig_int) {
    uint32_t res;
    double sig = fabs(u.sig);
    int minlg = -(max_exp-2) - u.exp;
    int lgslg0, inc, exp;
    double sig_scaled_in_ulp, exp_frac;
    ulp_t ulp;

    //Handle infinity NaN first
    if(u.nan)
    {
        uint32_t result;
        switch(ft) {
            case SF: result = ieee_pos_NaN_32; break;
            case HF: result = ieee_pos_NaN_16; break;
            case BF: result = BF_POS_NAN; break;
            default: g_assert_not_reached(); //shoulndt hit this case.
        }
        if(is_unfloat_neg(u)) result = negate_by_type(ft,result);
        return result;
    }
    if(u.inf) {
        uint32_t result;
        switch(ft) {
            case SF: result = ieee_pos_inf_32; break;
            case HF: result = ieee_pos_inf_16; break;
            case BF: result = BF_POS_INF; break;
            default: g_assert_not_reached(); //shoulndt hit this case.
        }
        if(is_unfloat_neg(u)) result = negate_by_type(ft,result);
        return result;
    }


    //on qf inputs rtl flips for every input that has a deferred increment and assumes on defer neg
    //so the parse fun assumes defer neg whenever the R bit is set
    //works for symmetical rounding modes like round to even or zero but can't be used
    //for unsymmetrical ones i.e round to pos inf/neg inf
    if((qfrnd == RND_TOWARDS_POS_INF || qfrnd == RND_TOWARDS_NEG_INF) && u.sign)
    {
       u.sign = !u.sign;
       u.sig = -u.sig;
       u.inexact = -u.inexact;
    }

    if(sig < pow(2,minlg)) { 
	    lgslg0 = minlg;
    } else {
	    lgslg0 = floor(log2f(sig));
    }
    exp = u.exp + lgslg0;
    sig = u.sig * pow(2,-lgslg0);

    //Check normalized value for overflow
    //For overflow, 0 case always returns 0/-0
    //Otherwise, check if exp is greater than max
    //If exponent is right at max and the sig is a denorm, it's not overflow
    if((ft != F8) && (sig != 0.0)
	&& (exp > max_exp
	|| ((exp == max_exp) && (sig >= 1.0L || sig < -1.0L))))
    {
        uint32_t result;
   	    switch(ft) {
            case SF: result = qfrnd_modes[qfrnd].ieee_pos_overflow.raw; break;
            case HF: result = qfrnd_modes[qfrnd].ieee_hf_pos_overflow.raw; break;
            case BF: result = (qfrnd_modes[qfrnd].ieee_pos_overflow.raw >> 16); break;
            default: g_assert_not_reached(); //We shouldnt hit this case
        }

        if(is_unfloat_neg(u)) result = negate_by_type(ft,result);
        return result;
    }
    
    //using q bit information on going from normalised to denorm to avoid double round errors
    //adding the inexact information to avoid double round errors
    sig_scaled_in_ulp = sig * _units;
    ulp = ulp_modf(sig_scaled_in_ulp, u.inexact);
    inc = get_inc(qfrnd_modes[qfrnd], ulp);
    ulp.integer += inc;

    if((ft != F8) && exp > (max_exp-1)) { 
	    exp = max_exp;
	    ulp.integer = pow(2,(sig_int+(ulp.integer==0))) - (ulp.integer==0);
    }
    sig = fabs(ulp.integer) * _epsilon; 
    exp_frac = exp + fabs(sig) - 1;
    res = (exp_frac + max_exp-1) * _units;

    //Need to jam in sign bit
    if(is_unfloat_neg(u)) res = negate_by_type(ft,res);
    return res;
}


//FP conversion: extended QF32 to IEEE SF
size4s_t conv_sf_extqf32 (uint32_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd, int coproc_mode) {
	unfloat u = parse_extqf32(a, a_ext, coproc_mode);
	return conv_unfloat_to_ieee(u, SF, qfrnd, UNITS32, EPSILON32, E_MAX_SF, sf_MANTBITS);
}

size2s_t conv_hf_extqf32 (uint32_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd, int coproc_mode) {
	unfloat u = parse_extqf32(a, a_ext, coproc_mode);

	//Convert unfloat to uint16_t IEEE half precision 
	return conv_unfloat_to_ieee(u, HF, qfrnd, UNITS16, EPSILON16, E_MAX_HF, hf_MANTBITS);
}

size2s_t conv_hf_extqf16 (uint16_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd, int coproc_mode) {
	unfloat u = parse_extqf16(a, a_ext, coproc_mode);

	//Convert unfloat to uint16_t IEEE half precision 
	return conv_unfloat_to_ieee(u, HF, qfrnd, UNITS16, EPSILON16, E_MAX_HF, hf_MANTBITS);
}

uint64_t conv_qf16_f8(uint8_t a, qfrnd_mode_enum_t qfrnd_mode)
{
	fixed_float_t f;
	uint64_t result;
	unfloat u = parse_f8(a);

	if(u.nan) {
		//TODO: Change this to return a mmvec_t so we can directly set extendedbits
		return extqf16_neg_nan>>2; 
	}

	if(u.zero) {
		u.exp = E_MIN_EXTQF16;
	} else {
		//We parse a finite non-zero as IEEE first to normalize
		u = parse_hf(conv_unfloat_to_ieee(u, HF, qfrnd_mode, UNITS16, EPSILON16, E_MAX_HF, hf_MANTBITS));
	}
	
	//Convert normalized or zero to qf16 
	f = rnd_sat_extqf_sig(u, qfrnd_mode, EXTQF16, E_MIN_QF16, E_MAX_QF16, BIAS_QF16, EPSILON16, UNITS16);

	result = ((f.sig << 5) | (f.exp & 0x1F)) & QF16_BITMASK;

	//Append LREQ bits after the LSB of result to produce 36 bit result
	result = ((result << 2) | (f.LREQ.bits16.LR & 0x3)) & EXTQF16_BITMASK;
	return result;
}

uint8_t conv_unfloat_to_f8(unfloat a, bool usr_fp, qfrnd_mode_enum_t qfrnd) {
	f8_t result = {.raw = 0};

	//NaN/Inf default to 0x80
	//If option is set, inf will go to max finite value
	if (a.inf || a.nan) {
		if (!usr_fp || a.nan) result.raw = F8_INFNAN;
		else result = (f8_t){.x = {.sign = a.sign, .exp=0xF, .mant = 0x7}};
	}
	//Zero returns 0x0 even for negative 0
	else if(a.zero) {
		result.raw = 0x0;
	}
	//underflow case, bias - number of mantissa bits because we can underflow by shifting the sig by mantbits
	else if(a.exp <= (-F8_BIAS-f8_MANTBITS)) {
		//Exp with -10 and a non-zero sig will be smallest value in FP8 
		if (a.exp == (-F8_BIAS-f8_MANTBITS) && a.sig > 1.0) result = (f8_t){.x = {.sign = a.sign, .exp=0x0, .mant = 0x1}};
		else result.raw = 0x0;
	}
	//overflow case, F8 only overflows if final exponent after normalizing and rounding is greater than EMAX 
	else if(a.exp > E_MAX_F8) {
		result.raw = F8_INFNAN;
	}
	//No special cases, convert to f8
	else {
		result.raw = (uint8_t) conv_unfloat_to_ieee(a, F8, qfrnd, UNITS8, EPSILON8, E_MAX_F8, f8_MANTBITS);
	}

	return result.raw;
}

uint8_t conv_qf16_to_f8(uint16_t a, uint8_t aext, qfrnd_mode_enum_t qfrnd_mode, int qfcoproc_mode) {
	uint16_t hf = conv_hf_extqf16(a, aext, qfrnd_mode, qfcoproc_mode);
	unfloat uhf = parse_hf(hf);
	return conv_unfloat_to_f8(uhf,0,qfrnd_mode);
}

uint16_t conv_qf32_to_bf(uint32_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd_mode, int qfcoproc_mode) {
    unfloat qf = parse_extqf32(a, a_ext, qfcoproc_mode);
    return conv_unfloat_to_ieee(qf, BF, qfrnd_mode, UNITSBF, EPSILONBF, E_MAX_SF, bf_MANTBITS);
}


int qf_vilog2(f_type type, uint32_t a, uint8_t a_ext) {
    unfloat u = { 0 };
    int exponent = 0;
    if(type == EXTQF32) {
        u = parse_extqf32(a,a_ext,0);
    } else if(type == EXTQF16) {
        u = parse_extqf16(a,a_ext,0);
    } else if(type == SF) {
        u = parse_sf(a);
    } else if(type == HF) {
        u = parse_hf(a);
    }

    double af = ldexp(u.sig,u.exp);
    //Check corner cases depending on 32 bit or 16 bit
    if(type == EXTQF32 || type == SF) {
        if(u.zero) exponent =  QF32_ILOG2_ZERO_EXP;
        else if(u.nan) exponent = QF32_ILOG2_NAN_EXP;
        else if(u.inf) exponent = QF32_ILOG2_INF_EXP;
        else exponent = ilogb(fabs(af));
    } else if(type == EXTQF16 || type == HF) {
        if(u.zero) exponent =  QF16_ILOG2_ZERO_EXP;
        else if(u.nan) exponent = QF16_ILOG2_NAN_EXP;
        else if(u.inf) exponent = QF16_ILOG2_INF_EXP;
        else exponent = ilogb(fabs(af));
    }

    return exponent;
}

/*###########################################################################################
#############################################################################################
############################## SF/HF/U/W/UH/H converts  #####################################
#############################################################################################
#############################################################################################*/

size4u_t get_ieee_sig(int *exp, double sig, f_type ft)
{
    //Extract bits from double precision significand
    uint64_t sig_64_org=0, sig_52=0, sig_53=0;
    double value = 0.0;
    int exp_d=0, exp_org=*exp;
    int E_MIN;
    E_MIN = (ft==SF)?  E_MIN_SF: E_MIN_HF;
    uint32_t sig_32=0;
    size4s_t signif=0;
   
    value = ldexp(sig, exp_org);

    sig_64_org = *(uint64_t *)&value;
    exp_d = (sig_64_org >> 52) & 0x7FF;
    exp_d = exp_d - BIAS_DF;
    sig_52 = (sig_64_org & 0xFFFFFFFFFFFFF);
    sig_53 = sig_52     | 0x10000000000000;

    //Check if exp is one less than the MIN
    //exp_d + BIAS = 0; subnormal
    //shifting right the excess amount of bits from E_MIN
    int shift = E_MIN - exp_d;

    if(exp_d <= (E_MIN-1))
    {
        sig_53 = sig_53 >> shift;
    }

    if(shift >=53)
        sig_53=0;

    double R1, R2, R3;
    if(ft==SF)
    {
        signif = sig_53 >> 29;
        sig_32 = signif & 0x7FFFFF;

        R1 = sig_53/pow(2,29);
        R2 = floor(R1/2.0)*2;
        R3 = R1 - R2;

        if(fabs(value) >= SF_MAX)
        {
            sig_32 = 0x7FFFFF;
        }
        else if((R3>0.5 && R3<1.0) || (R3>=1.5))
        {
            if(sig_32 == 0x7FFFFF)
            {
                sig_32 = 0;
                exp_d = exp_d +1;
            }
            else
                sig_32 = sig_32 +1;
        }
    }
    else
    {
        signif = sig_53 >> 42;
        sig_32 = signif & 0x3FF;

        R1 = sig_53/pow(2,42);
        R2 = floor(R1/2.0)*2;
        R3 = R1 - R2;

        if(fabs(value) >= HF_MAX)
        {
            sig_32 = 0x3FF;
        }
        else if((R3>0.5 && R3<1.0) || (R3>=1.5))
        {
            if(sig_32 == 0x3FF)
            {
                sig_32 = 0;
                exp_d = exp_d +1;
            }
            else
                sig_32 = sig_32 +1;
        }
    }

    if(sig ==0.0 && exp_org == (E_MIN-1))
    {
        sig_64_org = 0;
        exp_d = 0;
        sig_32=0;
    }
    *exp = exp_d;

    return sig_32;
}

size2s_t rnd_sat_hf(int exp, double sig)
{

    hf_t hf = {.raw = 0};
    int sign = (sig>=0.0)? 0:1;
    size4u_t sig_32 = get_ieee_sig(&exp, sig, HF);

    //exp is unbiased
    if(exp==(E_MIN_HF-1) && sig==0.0) {
        hf.raw = 0;
    } else if(exp > E_MAX_HF) {
	hf.x.sign = sign;
	hf.x.exp = 0x1F;
	hf.x.mant = 0x3FF;
    } 
    else {
        exp = exp + BIAS_HF;
        if(exp < 0)
            exp = 0;
        else if(exp > 31)
            exp = 31;
	hf.x.sign = sign;
	hf.x.exp = exp & 0x1F;
	hf.x.mant = sig_32 & 0x3FF;
    }

    return hf.raw;
}


//Take signed sig, produce normalized ieee sf output
size4s_t rnd_sat_sf(int exp, double sig)
{

    int sign = (sig>=0.0)? 0: 1;
    size4u_t sig_32 = get_ieee_sig(&exp, sig, SF);

    size4s_t result;

    if(exp==0 && sig==0.0)
    {
        result = 0;
    }
    else
    {
        exp = exp + BIAS_SF;
        if(exp < 0)
            exp = 0;
        else if(exp > 255)
            exp = 255;
        result = (sign<<31) | ((exp & 0xFF)<< 23) | (sig_32 & 0x7FFFFF);
    }

    return result;
}

//FP conversion W to IEEE SF
size4s_t conv_sf_w(size4s_t a)
{

    size4s_t result;
    int exp=0;
    double sig=0.0;
    if(a !=0) 
    {
        exp = ilogb(a);
        sig = (double)a/scalbn(1.0, exp);
    }
    result = rnd_sat_sf(exp, sig);

#ifdef DEBUG_MMVEC_QF
    double final = ldexp(sig, exp);
    printf("[SF_parse_conv_sf_w] sig:%lf, exp:%d, ldexp:%10.20f \n",sig, exp, final);
#endif

    return result;
}

//FP conversion UW to IEEE SF
size4s_t conv_sf_uw(size4u_t a)
{

    size4s_t result;
    int exp=0;
    double sig=0.0;
    if(a !=0) 
    {
        exp = ilogb(a);
        sig = (double)(unsigned)a/scalbn(1.0, exp);
    }
    result = rnd_sat_sf(exp, sig);

//#ifdef DEBUG_MMVEC_QF
//    double final = ldexp(sig, exp);
//    printf("[SF_parse_conv_sf_uw] sig:%lf, exp:%d, ldexp:%10.20f \n",sig, exp, final);
//#endif

    return result;
}

//FP conversion H to IEEE HF
size2s_t conv_hf_h(size2s_t a)
{
    size2s_t result;
    int exp=0;
    double sig=0.0;
    if(a !=0) 
    {
        exp = ilogb(a);
        sig = (double)a/scalbn(1.0, exp);
    }
    result = rnd_sat_hf(exp, sig);

#ifdef DEBUG_MMVEC_QF
    double final = ldexp(sig, exp);
    double f_rint = rint(final);
    printf("[HF_parse_conv_hf_h] sig:%lf, exp:%d, ldexp:%10.20f, rint:%lf \n",sig, exp, final, f_rint);
#endif
    return result;
}

//FP conversion UH to IEEE HF
size2s_t conv_hf_uh(size2u_t a)
{

    size2s_t result;
    int exp=0;
    double sig=0.0;
    if(a !=0) 
    {
        exp = ilogb(a);
        sig = (double)(unsigned)a/scalbn(1.0, exp);
    }
    result = rnd_sat_hf(exp, sig);

//#ifdef DEBUG_MMVEC_QF
//    double final = ldexp(sig, exp);
//    printf("[SF_parse_conv_hf_uh] sig:%lf, exp:%d, ldexp:%10.20f \n",sig, exp, final);
//#endif

    return result;
}

//FP conversion two W to two IEEE HF
size4s_t conv_hf_w(size8s_t a)
{
    size2s_t result0, result1;
    size4s_t result;
    size4s_t a0, a1;
    a0 = a & 0xFFFFFFFF;
    a1 = (a>>32) & 0xFFFFFFFF;

    int exp0=0, exp1=0;
    double sig0=0.0, sig1=0.0;
    if(a0 !=0) 
    {
        exp0 = ilogb(a0);
        sig0 = (double)a0/scalbn(1.0, exp0);
    }
    if(a1 !=0) 
    {
        exp1 = ilogb(a1);
        sig1 = (double)a1/scalbn(1.0, exp1);
    }
    result0 = rnd_sat_hf(exp0, sig0);
    result1 = rnd_sat_hf(exp1, sig1);

    result = ((size4s_t)result1 << 16) | (result0 & 0xFFFF);

/*
#ifdef DEBUG_MMVEC_QF
    double final0 = ldexp(sig0, exp0);
    double final1 = ldexp(sig1, exp1);

    printf("[HF_parse_conv_hf_w] sig0:%lf, exp0:%d, ldexp0:%10.20f \n",sig0, exp0, final0);
    printf("[HF_parse_conv_hf_w] sig1:%lf, exp1:%d, ldexp1:%10.20f \n",sig1, exp1, final1);
#endif
*/
    return result;
}

//FP conversion two UW to two IEEE HF
size4s_t conv_hf_uw(size8u_t a)
{
    size2s_t result0, result1;
    size4s_t result;
    size4u_t a0, a1;
    a0 = a & 0xFFFFFFFF;
    a1 = (a>>32) & 0xFFFFFFFF;

    int exp0=0, exp1=0;
    double sig0=0.0, sig1=0.0;
    if(a0 !=0) 
    {
        exp0 = ilogb(a0);
        sig0 = (double)(unsigned)a0/scalbn(1.0, exp0);
    }
    if(a1 !=0) 
    {
        exp1 = ilogb(a1);
        sig1 = (double)(unsigned)a1/scalbn(1.0, exp1);
    }
    result0 = rnd_sat_hf(exp0, sig0);
    result1 = rnd_sat_hf(exp1, sig1);

    result = ((size4s_t)result1 << 16) | (result0 & 0xFFFF);
/*
#ifdef DEBUG_MMVEC_QF
    double final0 = ldexp(sig0, exp0);
    double final1 = ldexp(sig1, exp1);

    printf("[HF_parse_conv_hf_uw] sig0:%lf, exp0:%d, ldexp0:%10.20f \n",sig0, exp0, final0);
    printf("[HF_parse_conv_hf_uw] sig1:%lf, exp1:%d, ldexp1:%10.20f \n",sig1, exp1, final1);
#endif
*/
    return result;
}

size4s_t rnd_sat_w(int exp, double sig)
{
    size4s_t result=0;
    size4s_t W_MAX = 0x7fffffff;
    size4s_t W_MIN = 0x80000000;

    int sign = (sig>=0.0)? 0: 1;

    double R1=0.0; 
    double R2=0.0; 
    double R3=0.0; 
    if(exp > 30)
    {
        result = (sign)? W_MIN:W_MAX;
        result = (sign <<31) | result;
    }
    else
    {
        R1 = ldexp(sig, exp);
        R2 = floor(R1/2.0)*2;
        R3 = R1 - R2;
        if(sign==0)
        {
            if(R3<=0.5)
                result = (size4s_t) R1;
            else if(R3>0.5 && R3<1.5)
                result =  (size4s_t) round(R1);
            else if(R3>=1.5)
                result =  (size4s_t) R1+1;
        }
        else
            result = (size4s_t)round(R1);
    }

#ifdef DEBUG_MMVEC_QF
    printf("[RND_conv_w_qf32] sig:%lf, exp:%d, R1:%10.20f, R2:%10.20f, R3:%10.20f, result:%x(%d)\n",sig, exp, R1, R2, R3, result, result);
#endif

    return result;
}

size4u_t rnd_sat_uw(int exp, double sig)
{
    size4u_t result=0;
    size4u_t W_MAX = 0xffffffff;

    double R1=0.0; 
    double R2=0.0; 
    double R3=0.0; 
    if(sig<0.0)
        result = 0;
    else if(exp > 31)
    {
        result = W_MAX;
    }
    else
    {
        R1 = ldexp(sig, exp);
        R2 = floor(R1/2.0)*2;
        R3 = R1 - R2;
        if(R3<=0.5)
            result = (size4s_t) R1;
        else if(R3>0.5 && R3<1.5)
            result =  (size4s_t) round(R1);
        else if(R3>=1.5)
            result =  (size4s_t) R1+1;
    }

#ifdef DEBUG_MMVEC_QF
    printf("[RND_conv_uw_qf32] sig:%lf, exp:%d, R1:%10.20f, R2:%10.20f, R3:%10.20f, result:%x(%d)\n",sig, exp, R1, R2, R3, result, result);
#endif

    return result;
}

size2s_t rnd_sat_h(int exp, double sig)
{
    size2s_t result=0;
    size2s_t W_MAX = 0x7fff;
    size2s_t W_MIN = 0x8000;

    int sign = (sig>=0.0)? 0: 1;

    double R1=0.0; 
    double R2=0.0; 
    double R3=0.0; 
    if(exp > 14)
    {
        result = (sign)? W_MIN:W_MAX;
        result = (sign <<15) | result;
    }
    else
    {
        R1 = ldexp(sig, exp);
        R2 = floor(R1/2.0)*2;
        R3 = R1 - R2;
        if(sign==0)
        {
            if(R3<=0.5)
                result = (size2s_t) R1;
            else if(R3>0.5 && R3<1.5)
                result =  (size2s_t) round(R1);
            else if(R3>=1.5)
                result =  (size2s_t) R1+1;
        }
        else
        {
            if(R3<=0.5 && R3 !=0.0)
                result = (size2s_t)R1 -1;
            else if(R3>0.5 && R3<1.5)
                result = (size2s_t)round(R1);
            else// if(R3>=1.5)
                result = (size2s_t)R1;
        }
    }

#ifdef DEBUG_MMVEC_QF
    printf("[RND_conv_h_qf16] sig:%lf, exp:%d, R1:%10.20f, R2:%10.20f, R3:%10.20f, result:%x(%d)\n",sig, exp, R1, R2, R3, result, result);
#endif

    return result;
}

size2u_t rnd_sat_uh(int exp, double sig)
{
    size2u_t result=0;
    size2u_t W_MAX = 0xffff;

    double R1=0.0; 
    double R2=0.0; 
    double R3=0.0; 
    if(sig<0.0)
        result = 0;
    else if(exp > 15)
    {
        result = W_MAX;
    }
    else
    {
        R1 = ldexp(sig, exp);
        R2 = floor(R1/2.0)*2;
        R3 = R1 - R2;
        if(R3<=0.5)
            result = (size2s_t) R1;
        else if(R3>0.5 && R3<1.5)
            result =  (size2s_t) round(R1);
        else if(R3>=1.5)
            result =  (size2s_t) R1+1;
    }

#ifdef DEBUG_MMVEC_QF
    printf("[RND_conv_uh_qf16] sig:%lf, exp:%d, R1:%10.20f, R2:%10.20f, R3:%10.20f, result:%x(%d)\n",sig, exp, R1, R2, R3, result, result);
#endif

    return result;
}

size1s_t rnd_sat_b(int exp, double sig)
{
    size1s_t result=0;
    size1s_t W_MAX = 0x7f;
    size1s_t W_MIN = 0x80;

    int sign = (sig>=0.0)? 0: 1;

    double R1=0.0; 
    double R2=0.0; 
    double R3=0.0; 
    if(exp > 6)
    {
        result = (sign)? W_MIN:W_MAX;
        result = (sign <<7) | result;
    }
    else
    {
        R1 = ldexp(sig, exp);
        R2 = floor(R1/2.0)*2;
        R3 = R1 - R2;
        if(sign==0)
        {
            if(R3<=0.5)
                result = (size1s_t) R1;
            else if(R3>0.5 && R3<1.5)
                result =  (size1s_t) round(R1);
            else if(R3>=1.5)
                result =  (size1s_t) R1+1;
        }
        else
        {
            if(R3<=0.5 && R3 !=0.0)
                result = (size1s_t)R1 -1;
            else if(R3>0.5 && R3<1.5)
                result = (size1s_t)round(R1);
            else// if(R3>=1.5)
                result = (size1s_t)R1;
        }
    }

#ifdef DEBUG_MMVEC_QF
    printf("[RND_conv_b_qf16] sig:%lf, exp:%d, R1:%10.20f, R2:%10.20f, R3:%10.20f, result:%x(%d)\n",sig, exp, R1, R2, R3, result, result);
#endif

    return result;
}

size1u_t rnd_sat_ub(int exp, double sig)
{
    size1u_t result=0;
    size1u_t W_MAX = 0xff;

    double R1=0.0; 
    double R2=0.0; 
    double R3=0.0; 
    if(sig<0.0)
        result = 0;
    else if(exp > 7)
    {
        result = W_MAX;
    }
    else
    {
        R1 = ldexp(sig, exp);
        R2 = floor(R1/2.0)*2;
        R3 = R1 - R2;

        if(R3<=0.5)
            result = (size1s_t) R1;
        else if(R3>0.5 && R3<1.5)
            result =  (size1s_t) round(R1);
        else if(R3>=1.5)
            result =  (size1s_t) R1+1;
    }

#ifdef DEBUG_MMVEC_QF
    printf("[RND_conv_ub_qf16] sig:%lf, exp:%d, R1:%10.20f, R2:%10.20f, R3:%10.20f, result:%x(%d)\n",sig, exp, R1, R2, R3, result, result);
#endif

    return result;
}

size4s_t conv_w_sf(size4s_t op1, bool daz_mode)
{
    sf_t input;
    size4s_t W_MAX = 0x7fffffff;
    size4s_t W_MIN = 0x80000000;
    input.raw = op1; 
    size4s_t  result;

	if(daz_mode && input.x.exp == 0x0) {
		result = 0;
	}
    else if(isNaNF32(op1) || isInfF32(op1) || (input.f >= (float)W_MAX) || (input.f <= (float)W_MIN))
    {
        if(input.x.sign == 1){
            result = W_MIN;
        }
        else{
            result = W_MAX;
        }
    }
    else{
        //convert and round to the zero
        result = (int)input.f;
    }
    
#ifdef DEBUG_MMVEC_QF
    printf("Debug : result =0x%08x\n",result);
#endif
    return result;
}

uint32_t hf_to_sf(uint16_t op) {
    uint32_t op_ext = (uint32_t) op;

    //grabbing sign, exp, and significand and converting to sf32 format
    return ((op_ext & 0x8000) << 16) + (((op_ext & 0x7c00) + 0x1c000) << 13) + ((op_ext & 0x03ff) << 13);
}

size2s_t conv_h_hf(size2s_t op1)
{
    sf_t input;
    size2s_t HW_MAX = 0x7fff;
    size2s_t HW_MIN = 0x8000;
    size2s_t  result;

    input.raw = hf_to_sf(op1);

    if(isNaNF16(op1) || isInfF16(op1) || (input.f >= (float)HW_MAX) || (input.f <= (float)HW_MIN))
    {
        if(input.x.sign == 1){
            result = HW_MIN;
        }
        else{
            result = HW_MAX;
        }
    }
    else{
        //convert and round to the zero
        result = (short)input.f;
    }
    
#ifdef DEBUG_MMVEC_QF
    printf("Debug : result =0x%08x\n",result);
#endif
    return result;
}

uint16_t conv_h_hf_rnd(uint16_t op)
{
    float a, frac;
    sf_t input;
    size2s_t HW_MAX = 0x7fff;
    size2s_t HW_MIN = 0x8000;
    size2s_t result;

    input.raw = hf_to_sf(op);

    if(isNaNF16(op) || isInfF16(op) || (input.f >= (float)HW_MAX) || (input.f <= (float)HW_MIN))
    {
        if(input.x.sign == 1){
            result = HW_MIN;
        } else {
            result = HW_MAX;
        }
    } else {
        a = input.f;

        //Get fractional of float by truncating value and subbing from original (1.5 - 1 = .5)
        frac = fabs(a - (float)((int16_t) a));

        //round to the nearest, add .5 or -5 and truncate
        result = (a > 0) ? ((int16_t) (a + 0.5)) : ((int16_t) (a - 0.5));

        //If original fraction was a tie and the result is odd, sub/add 1
        if((frac == 0.5) && (result % 2)) {
            if(a > 0) result--;
            if(a < 0) result++;
        }
    }
    
    return result;
}


/*###########################################################################################
#############################################################################################
############################## SF/HF/QF32/QF16 compare ######################################
#############################################################################################
#############################################################################################*/

//Generic greater than floating point compare with unfloats
int cmpgt_fp(unfloat a, unfloat b, bool nan_detect)
{
    int result = 0;
    double a_d, b_d;
    a_d = ldexp(a.sig, a.exp);
    b_d = ldexp(b.sig, b.exp);

    //cmpgt should result false if either input is NaN
    if((a.nan || b.nan) && nan_detect)
        result = 0;
    //Filter out +0/-0 by checking the sign
    else if(a_d > b_d)
        result = 1;

#ifdef DEBUG_MMVEC_QF
    printf("[CMPGT]a:%10.30f, b:%10.30f\n",a_d, b_d);
#endif

    return result;
}

//Generic equal floating point compare with unfloats
int cmpeq_fp(unfloat a, unfloat b)
{
    int result = 0;
    double a_d, b_d;
    a_d = ldexp(a.sig, a.exp);
    b_d = ldexp(b.sig, b.exp);

    //cmpeq should result false if either input is NaN
    if(a.nan || b.nan)
        result = 0;
    else if(a_d == b_d)
        result = 1;

#ifdef DEBUG_MMVEC_QF
    printf("[CMPEQ]a:%10.30f, b:%10.30f\n",a_d, b_d);
#endif

    return result;
}


/**********************************/
/****** CMP greater than HF/SF ****/
/**********************************/
int cmpgt_sf(size4s_t in_a, size4s_t in_b, bool nan_detect, bool daz_mode)
{

    unfloat a, b;
    a = parse_sf_daz(in_a,daz_mode);
    b = parse_sf_daz(in_b,daz_mode);

    if(a.sign)
        a.sig = (-1.0)*a.sig;
    if(b.sign)
        b.sig = (-1.0)*b.sig;

    int result=0;
    result = cmpgt_fp(a,b,nan_detect);

    return result;
}

int cmpgt_hf(size2s_t in_a, size2s_t in_b, bool nan_detect)
{

    unfloat a, b;
    a= parse_hf(in_a);
    b= parse_hf(in_b);

    if(a.sign)
        a.sig = (-1.0)*a.sig;
    if(b.sign)
        b.sig = (-1.0)*b.sig;

    int result=0;
    result = cmpgt_fp(a,b,nan_detect);

    return result;
}

/**********************************/
/********** CMP equal HF/SF *******/
/**********************************/
int cmpeq(size4s_t in_a, size4s_t in_b, f_type type, bool daz_mode)
{

    unfloat a, b;
    if(type == SF) {
        a = parse_sf_daz(in_a,daz_mode);
        b = parse_sf_daz(in_b,daz_mode);
    } else {
        a = parse_hf(in_a);
        b = parse_hf(in_b);
    }

    if(a.sign)
        a.sig = (-1.0)*a.sig;
    if(b.sign)
        b.sig = (-1.0)*b.sig;

    int result = 0;
    result = cmpeq_fp(a,b);

    return result;
}


/**********************************/
/********** Min and Max HF/SF *****/
/**********************************/

size4s_t min_sf(size4s_t in_a, size4s_t in_b) { 
	uint32_t result;
	unfloat a = parse_sf(in_a);
	unfloat b = parse_sf(in_b);
	
	//Both inputs are 0 so if b is -0, return b. if b is 0, return a which is either -0 or 0
	if (a.zero && b.zero) {
		result = (b.sign) ? in_b : in_a;
	} 
	//If one input is nan, return nan
	else if(a.nan || b.nan) {
        //For min, if one of the inputs is a -Nan, return -Nan
        if((a.nan && a.sign) || (b.nan && b.sign)) result = ieee_neg_NaN_32;
		else result = ieee_pos_NaN_32;
	}
	else {
		//Compare a > b. If a > b, return b since min
		//Two NaN inputs will have its values compared like previous min/max, will just return a NaN
		result = cmpgt_sf(in_a,in_b,false,false) ? in_b : in_a; 
	}
	
	//Compliance Check
	check_minmax_compliance(SF,false,in_a,in_b,result);

	return result;
}

size2s_t min_hf(uint16_t in_a, uint16_t in_b) { 
	uint16_t result;
	unfloat a = parse_hf(in_a);
	unfloat b = parse_hf(in_b);

	//Both inputs are 0 so if b is -0, return b. if b is 0, return a which is either -0 or 0
	if (a.zero && b.zero) {
		result = (b.sign) ? in_b : in_a;
	} 
	//If one input is nan, return nan
	else if(a.nan || b.nan) {
        //For min, if one of the inputs is a -Nan, return -Nan
        if((a.nan && a.sign) || (b.nan && b.sign)) result = ieee_neg_NaN_16;
		else result = ieee_pos_NaN_16;
	}
	else {
		//Compare a > b. If a > b, return b since min
		//Two NaN inputs will have its values compared like previous min/max, will just return a NaN
		result = cmpgt_hf(in_a, in_b,false) ? in_b : in_a; 
	}

	//Compliance Check
	check_minmax_compliance(HF,false,in_a,in_b,result);

	return result;
}
size4s_t max_sf(size4s_t in_a, size4s_t in_b) {    
	uint32_t result;
	unfloat a = parse_sf(in_a);
	unfloat b = parse_sf(in_b);
	
	//Both inputs are 0 so if a is +0, return a. If a is -0, return b which is either -0 or +0
	if (a.zero && b.zero) {
		result = (!a.sign) ? in_a : in_b;
	} 
	//If one input is nan, return nan
	else if(a.nan || b.nan) {
        //For max, if one of the inputs is a Nan, return Nan
        if((a.nan && !a.sign) || (b.nan && !b.sign)) result = ieee_pos_NaN_32;
		else result = ieee_neg_NaN_32;
	}
	else {
		//Compare b > a. If b > a, return b since max.
		//Two NaN inputs will have its values compared like previous min/max, will just return a NaN
		result = cmpgt_sf(in_b,in_a,false,false) ? in_b : in_a; 
	}

	//Compliance Check
	check_minmax_compliance(SF,true,in_a,in_b,result);

    return result;
}
size2s_t max_hf(uint16_t in_a, uint16_t in_b) 
{
	uint16_t result;
	unfloat a = parse_hf(in_a);
	unfloat b = parse_hf(in_b);
	
	//Both inputs are 0 so if a is +0, return a. If a is -0, return b which is either -0 or +0
	if (a.zero && b.zero) {
		result = (!a.sign) ? in_a : in_b;
	} 
	//If one input is nan, return nan
	else if(a.nan || b.nan) {
        //For max, if one of the inputs is a Nan, return Nan
        if((a.nan && !a.sign) || (b.nan && !b.sign)) result = ieee_pos_NaN_16;
		else result = ieee_neg_NaN_16;
	}
	else {
		//Two NaN inputs will have its values compared like previous min/max, will just return a NaN
		result = cmpgt_hf(in_b, in_a, false) ? in_b : in_a;
	}

	//Compliance Check
	check_minmax_compliance(HF,true,in_a,in_b,result);
	
	return result;
}

//#############################################################################################
//#############################################################################################
//#############################################################################################
//#############################################################################################
//############################## Old Qfloat Implementation ####################################
//######################     Needed for libnative legacy mode    ##############################
//#############################################################################################
//#############################################################################################
//#############################################################################################

//Take signed int and generate sign, exp and ***signed sig
unfloat parse_qf32(size4s_t in)
{
    unfloat out;

	out.sign = (in>>31) & 0x1;

    out.exp = (size2s_t)(0x0000 | (in & 0xFF));
    out.exp = out.exp - BIAS_QF32;

    /*implied LSB=1*/
    size4s_t signif; 
    /*take signif and sign extend, add LSB=1*/
    signif= ((size8s_t)in >> 7) | 1;

    out.sig = (double)signif * EPSILON32;

#ifdef DEBUG_MMVEC_QF
    printf("[ARCH_QF32_parse]in=%x, exp=%d, sig=%10.20f\n", in,out.exp,out.sig);
    printf("[ARCH_QF32_parse]exp_d=%d, sig_d=%10.20f\n", ilogb(out.sig),ldexp(out.sig, -ilogb(out.sig)));
#endif
    return out;
}

unfloat parse_qf16(size2s_t in)
{
    unfloat out;

    out.sign = (in>>15) & 0x1;

    out.exp = (size1s_t)(0x00 | (in & 0x1F));
    out.exp = out.exp - BIAS_QF16;

    /*implied LSB=1*/
    size2s_t signif; 
    /*take signif and sign extend, add LSB=1*/
    signif= ((size4s_t)in >> 4) | 1;

    out.sig = (double)signif * EPSILON16;

#ifdef DEBUG_MMVEC_QF
    printf("[ARCH_QF16_parse]in=%x, exp=%d, sig=%10.20f\n", in,out.exp,out.sig);
    printf("[ARCH_QF16_parse]exp_d=%d, sig_d=%10.20f\n", ilogb(out.sig),ldexp(out.sig, -ilogb(out.sig)));
#endif
    return out;
}

//size4u_t rnd_sat_qf_sig(int sign, int* exp_in, double sig, double sig_low, f_type ft)
size4s_t rnd_sat_qf_sig(int* exp_in, double sig, double sig_low, f_type ft)
{
    double scale;
    double sig_s;
    double sig_f=0.0;
    double R1, R2, R3;
    int exp_ovf=0;
    int exp_adj=0;
    int exp_undf=0;
    int exp = *exp_in;
    int sign = (sig>=0.0)? 0:1;

    int prod_ovf=0;
    if(fabs(sig)>=2.0L && sig != -2.0L)
        prod_ovf = 1;

    int E_MIN=E_MIN_QF32;
    int E_MAX=E_MAX_QF32;
    int BIAS=BIAS_QF32;
    double _epsilon=EPSILON32;
    double _units=UNITS32;
    if(ft==QF32)
    {
        E_MIN = E_MIN_QF32;
        E_MAX = E_MAX_QF32;
        BIAS = BIAS_QF32;
        _epsilon = EPSILON32;
        _units= UNITS32;
    }
    else if(ft==QF16)
    {
        E_MIN = E_MIN_QF16;
        E_MAX = E_MAX_QF16;
        BIAS = BIAS_QF16;
        _epsilon = EPSILON16;
        _units= UNITS16;
    }
    else if(ft==SF)
    {
        E_MIN = E_MIN_SF;
        E_MAX = E_MAX_SF;
        BIAS = BIAS_SF;
        _epsilon = EPSILON32;
        _units= UNITS32;
    }
    else if(ft==HF)
    {
        E_MIN = E_MIN_HF;
        E_MAX = E_MAX_HF;
        BIAS = BIAS_HF;
        _epsilon = EPSILON16;
        _units= UNITS16;
    }

    //Set scale factor
    if((exp == (E_MIN-1)) || (prod_ovf && (exp<E_MAX)))
        scale = 2.0;
    else
        scale =1.0;

    //Scale the significand
    sig_s = sig/scale;

    //Get remainder from the scaled significand
    R1 = sig_s*_units;

    //R2 = floor((R1+R_low)/4.0)*4.0;
    //R3 = (R1+R_low) - R2;
    R2 = floor(R1/4.0)*4.0;
    R3 = R1 - R2;

    //Check for exp overflow/underflow
    if(exp>=(E_MAX+1) || (prod_ovf && exp==E_MAX))
    {
        exp_ovf=1;
    }
    else if(exp<=(E_MIN-2))
    {
        exp_undf=1;
    }
    else if(exp == E_MAX)//exp=E_MAX
    {
        //if(R3-2.0)+sig_low<=0.0 
        if((R3==0.0) && (sig_low<0.0))
        {
            sig_f = sig_s + (3.0-R3-4.0)*_epsilon;
        }
        else if((R3<2.0) || (R3==2.0 && sig_low<=0.0))
        //if(R3<=2.0)
        {
            sig_f = sig_s + (1.0-R3)*_epsilon;
        }
        else
        {
            sig_f = sig_s + (3.0-R3)*_epsilon;
        }
    }
    else if(exp == (E_MIN-1))
    {
        exp_adj = 1;
        if((R3==0.0) && (sig_low<0.0))
        {
            sig_f = sig_s + (3.0-R3-4.0)*_epsilon;
        }
        else if((R3<2.0) || (R3==2.0 && sig_low<=0.0))
        //if(R3<=2.0)
        {
            sig_f = sig_s + (1.0-R3)*_epsilon;
        }
        else
        {
            sig_f = sig_s + (3.0-R3)*_epsilon;
        }
    }
    else if(prod_ovf && (exp < E_MAX))
    {
        exp_adj = 1;
        if((R3==0.0) && (sig_low<0.0))
        {
            sig_f = sig_s + (3.0-R3-4.0)*_epsilon;
        }
        else if((R3<2.0) || (R3==2.0 && sig_low<=0.0))
        //if(R3<=2.0)
        {
            sig_f = sig_s + (1.0-R3)*_epsilon;
        }
        else
        {
            sig_f = sig_s + (3.0-R3)*_epsilon;
        }
    }
    else if(!prod_ovf)
    {
        if((R3==0.0) && (sig_low<0.0))
        {
            sig_f = sig_s + (3.0-R3-4.0)*_epsilon;
        }
        else if((R3<1.5) || (R3==1.5 && sig_low<=0.0))
        //if(R3<=1.5)
        {
            sig_f = sig_s + (1.0-R3)*_epsilon;
        }
        //else if(R3<=2.5)
        else if((R3<2.5) || (R3==2.5 && sig_low<=0.0))
        {
            sig_f = (sig + (2.0-R3)*_epsilon)*0.5;
            exp_adj=1;
        }
        else
        {
            sig_f = sig_s + (3.0-R3)*_epsilon;
        }
    }
    //get the binary bits from the double-precision significand
    //Either sig is positive or negative, IEEE double sig has magnitude
    //Check for sign at the last stage and take 2's complement if negative
    uint64_t sig_64_org, sig_64;
    sig_64_org = *(uint64_t *)&sig_f;
    sig_64 = sig_64_org;
    uint32_t sig_32=0;
    int32_t sig_32_out=0;

    int exp_df;
    
    exp_df = (sig_64_org >> 52) & 0x7FF;
    exp_df = exp_df - BIAS_DF;

    if(exp_ovf)
    {
        exp=E_MAX+BIAS; 
        if(ft==QF32 || ft==SF)
            sig_32 = (sign-1) & 0x7FFFFF;
        else if(ft==QF16 || ft==HF)
            sig_32 = (sign-1) & 0x3FF;
    }
    else if(exp_undf)
    {
        exp=E_MIN+BIAS; 
        if(ft==QF32 || ft==SF)
            sig_32 = ((-1)*sign) & 0x7FFFFF;
        else if(ft==QF16 || ft==HF)
            sig_32 = ((-1)*sign) & 0x3FF;
    }
    else
    {
        exp += BIAS+exp_adj;
        //Add MSB, generates 53bits (52+1)
        sig_64 = (sig_64_org & 0xFFFFFFFFFFFFF) | 0x10000000000000;
        //Shift out exponent 11 bits
        sig_64 = sig_64<<11;
        sig_64 = (exp_df>=0)? (sig_64 << exp_df):(sig_64>>abs(exp_df));
        if(ft==QF32)
        {
        sig_64 = sig_64 >> 41;
        sig_32 = sig_64 & 0x7FFFFF;
        }
        else if(ft==QF16)
        {
        sig_64 = sig_64 >> 54;
        sig_32 = sig_64 & 0x3FF;
        }

        if(sign)
            sig_32 = ~sig_32;
    }

    sig_32_out = (sign<<23) | sig_32;

    if(ft==QF16 ||ft==HF)
        sig_32_out = (sign<<10) | sig_32;


    if( (ft ==QF16) ||  (ft==QF32)) {
        if ((sig == 0.0) && (sig_low == 0.0)) {
            exp = 0;
            //printf("Squash to zero!\n");
        }
        
    }
 

#ifdef DEBUG_MMVEC_QF
    printf("[ARCH_QF_rnd_sat]sign=%d exp_in=%d sig=%10.30f sig_low=%10.30f\n",sign, *exp_in, sig, sig_low);
    printf("[ARCH_QF_rnd_sat]sig_s=%10.30f, sig_f=%10.30f\n",sig_s, sig_f);
    printf("[ARCH_QF_rnd_sat]prod_ovf=%d exp_adj=%d exp_ovf=%d exp_undf=%d\n",prod_ovf,exp_adj, exp_ovf, exp_undf);
    printf("[ARCH_QF_rnd_sat]sig_64_org=%lx sig_64=%lx sig_32=%x exp_df=%d exp=%d\n",sig_64_org, sig_64, sig_32, exp_df, exp);
    printf("[ARCH_QF_rnd_sat]R1=%10.30f R_low=%1.128f R2=%10.30f R3=%10.30f eps=%10.30f\n",R1,R_low,R2,R3,_epsilon);

    double final = ldexp(sig_f, (exp-BIAS));
    printf("[ARCH_QF_norm] sig_f:%10.30f, exp-BIAS:%d, ldexp:%10.128f \n",sig_f, exp-BIAS, final);
    printf("[ARCH_QF_norm] sig_32_out:%x, exp:%x \n",sig_32_out, exp);
#endif

    *exp_in = exp;
    return sig_32_out;
}

//size4s_t rnd_sat_qf32(int sign, int exp, double sig, double sig_low)
size4s_t rnd_sat_qf32(int exp, double sig, double sig_low)
{

    //size4u_t sig_32=rnd_sat_qf_sig(sign, &exp, sig, sig_low, QF32);
    //size4u_t sig_32=rnd_sat_qf_sig(&exp, sig, sig_low, QF32);
    size4s_t sig_32=rnd_sat_qf_sig(&exp, sig, sig_low, QF32);

    size4s_t result;
    //result = (sign<<31) | (sig_32 <<8) | (exp & 0xFF);
    result = (sig_32 <<8) | (exp & 0xFF);

    return result;
}

//size2s_t rnd_sat_qf16(int sign, int exp_ab, double sig, double sig_low)
size2s_t rnd_sat_qf16(int exp_ab, double sig, double sig_low)
{
    int exp=exp_ab;


    //size4u_t sig_32=rnd_sat_qf_sig(&exp, sig, sig_low, QF16);
    //printf("sig low=%f sig=%f\n", sig, sig_low);
    size4s_t sig_32=rnd_sat_qf_sig(&exp, sig, sig_low, QF16);

    size2s_t result;
    result = (sig_32<<5) | (exp & 0x1F);
    //result = (sign_ab<<15) | (sig_16<<5) | (exp_ab & 0x1F);

    return result;
}

//Neg/Abs
size4s_t neg_qf32(size4s_t a)
{
    size4s_t result;
    result = negate32(a);
    return result;
}
size4s_t abs_qf32(size4s_t a)
{
    size4s_t result;
    if((a>>31) & 1)
        result = negate32(a);
    else
        result = a;
    return result;
}
size2s_t neg_qf16(size2s_t a)
{
    size2s_t result;
    result = negate16(a);
    return result;
}
size2s_t abs_qf16(size2s_t a)
{
    size2s_t result;
    if((a>>15) & 1)
        result = negate16(a);
    else
        result = a;
    return result;
}
size4s_t neg_sf(size4s_t a)
{
    size4s_t result;
    result = negate_sf(a);
    return result;
}
size4s_t abs_sf(size4s_t a)
{
    size4s_t result;
    if((a>>31) & 1)
        result = negate_sf(a);
    else
        result = a;
    return result;
}
size2s_t neg_hf(size2s_t a)
{
    size2s_t result;
    result = negate_hf(a);
    return result;
}
size2s_t abs_hf(size2s_t a)
{
    size2s_t result;
    if((a>>15) & 1)
        result = negate_hf(a);
    else
        result = a;
    return result;
}

