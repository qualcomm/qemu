/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "hexagon_protos.h"
#include <hexagon_types.h>

#include "ieee_ref_output_dat.h"

union ui32f {
    int32_t i;
    float f;
};
union ui16f16 {
    int16_t i;
    __fp16 f16;
};


/* 128 byte vectors */
#define VSIZE_BYTES 128
#define VSIZE_WORDS (VSIZE_BYTES / 4)

/*
 * Create vectors
 */


/* create a vector of single floats from a float */
static HVX_Vector create_sfv_from_sf(float value)
{
    union ui32f cvt;
    cvt.f = value;
    HVX_Vector tmp = Q6_V_vsplat_R(cvt.i);
    return tmp;
}

/* create a vector of half floats from a float */
static HVX_Vector create_hfv_from_sf(float value)
{
    __fp16 hf = value;
    union ui16f16 cvt;
    cvt.f16 = hf;
    HVX_Vector tmp = Q6_Vh_vsplat_R(cvt.i);
    return tmp;
}

/* create a vector of halfs from a short */
static HVX_Vector create_hv_from_short(short value)
{
    HVX_Vector tmp = Q6_Vh_vsplat_R(value);
    return tmp;
}

/* create a vector of halfs from an unsiged short */
static HVX_Vector create_hv_from_ushort(unsigned short value)
{
    HVX_Vector tmp = Q6_Vh_vsplat_R(value);
    return tmp;
}

/*
 * Extraction routines
 */


/* get lowest float from a vector of floats */
static float get_flt0_from_fltv(HVX_Vector vect)
{
    union ui32f cvt;
    cvt.i = vect[0];
    return cvt.f;
}


/* get lowest float from a vector of halfs */
static float get_flt0_from_halfv(HVX_Vector vect)
{
    union ui16f16 cvt;
    cvt.i = (vect[0] & 0xffff);
    return (float)cvt.f16;
}


//
static float get_flt0_from_vp(HVX_VectorPair vect)
{
    union ui16f16 cvt;
    HVX_Vector tmp = HEXAGON_HVX_GET_V0(vect);
    cvt.i = tmp[0];
    return cvt.f16;
}

static float get_sf0_from_vp(HVX_VectorPair vect)
{
    union ui32f cvt;
    HVX_Vector tmp = HEXAGON_HVX_GET_V0(vect);
    cvt.i = tmp[0];
    return cvt.f;
}

static char get_b0_from_vb(HVX_Vector vect)
{
    return vect[0];
}

static short get_h0_from_vh(HVX_Vector vect)
{
    return vect[0];
}

int main(int argc, char **argv)
{
    int err = 0;

    FILE *test_file = fopen("result.txt", "w+");
    if (test_file == NULL) {
        printf("FAIL\n");
        puts("fopen fail");
        exit(-1);
    }

    /* create half float vectors from a float */
    HVX_Vector hfv2 = create_hfv_from_sf(2.0);
    HVX_Vector hfv4 = create_hfv_from_sf(4.0);
    HVX_Vector hfvn6 = create_hfv_from_sf(-6.0);

    /* create single float vectors from a float */
    HVX_Vector sfvp5 = create_sfv_from_sf(0.5);
    HVX_Vector sfvp25 = create_sfv_from_sf(0.25);
    HVX_Vector sfvnp25 = create_sfv_from_sf(-0.25);

    /* create a half vector from a short */
    HVX_Vector hvn3 = create_hv_from_short(-3);

    /* create a half vector from a unsigned short */
    HVX_Vector hv32k = create_hv_from_ushort(0x8000);

    fprintf(test_file, "\nConversion intructions\n\n");


    HVX_Vector vhf = Q6_Vhf_vcvt_Vh(hvn3);

    fprintf(test_file, "half -3 converted to vhf = %.2f\n",
            get_flt0_from_halfv(vhf));

    HVX_Vector vhf3 = Q6_Vhf_vcvt_Vuh(hv32k);

    fprintf(test_file, "uhalf 32k converted to vhf = %.2f\n",
            get_flt0_from_halfv(vhf3));

    HVX_Vector vhf2 = Q6_Vhf_vcvt_VsfVsf(sfvp5, sfvp5);

    fprintf(test_file, "sf 0.5 converted to vhf = %.2f\n",
            get_flt0_from_halfv(vhf2));

    HVX_Vector vub = Q6_Vub_vcvt_VhfVhf(hfv4, hfv4);

    fprintf(test_file, "vhf 4.0 conveted to ubyte = %d\n", get_b0_from_vb(vub));

    HVX_Vector vuh = Q6_Vuh_vcvt_Vhf(hfv2);

    fprintf(test_file, "vhf 2.0 conveted to uhalf = %d\n", get_h0_from_vh(vuh));

    HVX_VectorPair vp = Q6_Whf_vcvt_Vb(vub);

    fprintf(test_file, "byte 4 conveted to hf = %.2f\n", get_flt0_from_vp(vp));

    HVX_VectorPair vphf = Q6_Whf_vcvt_Vub(vub);

    fprintf(test_file, "ubyte 4 conveted to hf = %.2f\n",
            get_flt0_from_vp(vphf));

    HVX_VectorPair vpsf = Q6_Wsf_vcvt_Vhf(vhf);

    fprintf(test_file, "hf -3 conveted to sf = %f\n", get_sf0_from_vp(vpsf));

    HVX_Vector vb = Q6_Vb_vcvt_VhfVhf(hfv4, hfv4);

    fprintf(test_file, "vhf 4.0 conveted to byte = %d\n", get_b0_from_vb(vb));

    HVX_Vector vh = Q6_Vh_vcvt_Vhf(hfv4);

    fprintf(test_file, "vhf 4.0 conveted to half = %d\n", get_h0_from_vh(vh));

    HVX_Vector hfm = Q6_Vhf_vfmax_VhfVhf(hfv2, hfv4);


    fprintf(test_file, "\nMin/Max instructions\n\n");

    fprintf(test_file, "max of hf 2.0 and hf 4.0 = %.2f\n",
            get_flt0_from_halfv(hfm));

    HVX_Vector hfm2 = Q6_Vhf_vfmin_VhfVhf(hfv2, hfv4);

    fprintf(test_file, "min of hf 2.0 and hf 4.0 = %.2f\n",
            get_flt0_from_halfv(hfm2));

    HVX_Vector sf = Q6_Vsf_vfmax_VsfVsf(sfvp5, sfvp25);

    fprintf(test_file, "max of sf 0.5 and sf 0.25 = %f\n",
            get_flt0_from_fltv(sf));

    HVX_Vector sf2 = Q6_Vsf_vfmin_VsfVsf(sfvp5, sfvp25);

    fprintf(test_file, "min of sf 0.5 and sf 0.25 = %f\n",
            get_flt0_from_fltv(sf2));


    fprintf(test_file, "\nabs/neg instructions\n\n");

    HVX_Vector hfn = Q6_Vhf_vfneg_Vhf(hfv4);

    fprintf(test_file, "negate of hf 4.0 = %.2f\n", get_flt0_from_halfv(hfn));

    HVX_Vector ahfvn6 = Q6_Vhf_vabs_Vhf(hfvn6);

    fprintf(test_file, "abs of hf -6.0 = %.2f\n", get_flt0_from_halfv(ahfvn6));

    HVX_Vector sfn = Q6_Vsf_vfneg_Vsf(sfvp5);

    fprintf(test_file, "negate of sf 0.5 = %f\n", get_flt0_from_fltv(sfn));

    HVX_Vector asfvnp25 = Q6_Vsf_vabs_Vsf(sfvnp25);

    fprintf(test_file, "abs of sf -0.25 = %f\n", get_flt0_from_fltv(asfvnp25));


    fprintf(test_file, "\nadd/sub instructions\n\n");

    HVX_Vector vaddhf = Q6_Vhf_vadd_VhfVhf(hfv4, hfvn6);

    fprintf(test_file, "hf add of 4.0 and -6.0  = %.2f\n",
            get_flt0_from_halfv(vaddhf));

    HVX_Vector vsubhf = Q6_Vhf_vsub_VhfVhf(hfv4, hfvn6);

    fprintf(test_file, "hf sub of 4.0 and -6.0  = %.2f\n",
            get_flt0_from_halfv(vsubhf));


    HVX_Vector vaddsf = Q6_Vsf_vadd_VsfVsf(sfvp5, sfvnp25);

    fprintf(test_file, "sf add of 0.5 and -0.25  = %f\n",
            get_flt0_from_fltv(vaddsf));

    HVX_Vector vsubsf = Q6_Vsf_vsub_VsfVsf(sfvp5, sfvnp25);

    fprintf(test_file, "sf sub of 0.5 and -0.25  = %f\n",
            get_flt0_from_fltv(vsubsf));


    HVX_VectorPair vpsfadd = Q6_Wsf_vadd_VhfVhf(hfv4, hfvn6);

    fprintf(test_file, "sf add of hf 4.0 and hf -6.0  = %f\n",
            get_sf0_from_vp(vpsfadd));

    HVX_VectorPair vpsfsub = Q6_Wsf_vsub_VhfVhf(hfv4, hfvn6);

    fprintf(test_file, "sf sub of hf 4.0 and hf -6.0  = %f\n",
            get_sf0_from_vp(vpsfsub));


    fprintf(test_file, "\nmultiply instructions\n\n");

    HVX_Vector hfmpy = Q6_Vhf_vmpy_VhfVhf(hfv4, hfvn6);

    fprintf(test_file, "hf mpy of 4.0 and -6.0  = %.2f\n",
            get_flt0_from_halfv(hfmpy));

    HVX_Vector acchfmpy = Q6_Vhf_vmpyacc_VhfVhfVhf(hfmpy, hfv4, hfvn6);

    fprintf(test_file, "hf accmpy of 4.0 and -6.0  = %.2f\n",
            get_flt0_from_halfv(acchfmpy));

    HVX_VectorPair sfvpmpy = Q6_Wsf_vmpy_VhfVhf(hfv4, hfvn6);

    fprintf(test_file, "sf mpy of hf 4.0 and hf -6.0  = %f\n",
            get_sf0_from_vp(sfvpmpy));

    HVX_VectorPair sfvpaccmpy = Q6_Wsf_vmpyacc_WsfVhfVhf(sfvpmpy, hfv4, hfvn6);

    fprintf(test_file, "sf accmpy of hf 4.0 and hf -6.0  = %f\n",
            get_sf0_from_vp(sfvpaccmpy));

    HVX_Vector sfmpy = Q6_Vsf_vmpy_VsfVsf(sfvp5, sfvnp25);

    fprintf(test_file, "sf mpy of 0.5 and -0.25  = %f\n",
            get_flt0_from_fltv(sfmpy));


    fprintf(test_file, "\ncopy instruction\n\n");

    HVX_Vector vfmv = Q6_Vw_vfmv_Vw(sfvp5);

    fprintf(test_file, "w copy from sf 0.5 = %f\n", get_flt0_from_fltv(vfmv));

    long int pos = ftell(test_file);
    unsigned char *buf = (unsigned char *)malloc((size_t)pos);
    if (!buf) {
        printf("FAIL\n");
        fclose(test_file);
        exit(-1);
    }

    rewind(test_file);
    size_t len = fread(buf, 1, (size_t)pos, test_file);
    fclose(test_file);
    if (len != (size_t)pos) {
        printf("FAIL\n");
        exit(-1);
    }

    err = memcmp(buf, &ieee_ref_output_dat[0], sizeof(ieee_ref_output_dat));
    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
