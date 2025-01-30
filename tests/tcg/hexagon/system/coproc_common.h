/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef COPROC_COMMON_H
#define COPROC_COMMON_H 1

#define BYTE_ALIGNMENT_REQ 2048

#define X_TILE_SIZE_DEF (8)
#define Y_TILE_SIZE_DEF (8)
#define Z_TILE_SIZE_DEF (32)

#define X_TILE_OFFSET (5)
#define Y_TILE_OFFSET (8)
#define Z_TILE_OFFSET (0)

#define SPATIAL_OFFSET (2)

#define SPATIAL_SIZE 6
#define DEPTH_SIZE 5


#define CROUTON_SIZE 2048
#define VECTOR_SIZE 128
#define BYTES_PER_LANE 4
#define Z_REMAINDER_OFFSET (11)

#define X_TILE_MASK (7)
#define Y_TILE_MASK (7)
#define Z_TILE_MASK (31)

#define INT8_BIAS 0x0
#define INT8_SHIFT_SCALE 14408
#define FP_BIAS 0x3c00

#define SET_SSR_XE2()   \
{    \
    uint32_t SSR_TEMP=0;    \
    asm( \
        "%0 = SSR\n"    \
        "%0 = setbit(%0 , #26)\n"    \
        "SSR = %0\n" \
        :: "r" (SSR_TEMP) \
		:\
    );\
};

#define SET_SSR_XE_XA(XA)   \
{    \
    uint32_t SSR_TEMP=0;    \
    uint32_t XA_TEMP = XA; \
    asm( \
        "%0 = SSR\n"    \
        "%0 = setbit(%0 , #31)\n"    \
        "%0 = insert(%1, #3, #27)\n"  \
        "SSR = %0\n" \
        :: "r" (SSR_TEMP), "r" (XA_TEMP) \
		:\
    );\
};

#define RESET_PMU() \
    __asm__ __volatile__ (\
        " r0 = #0x48 ; trap0(#0); \n" : : \
        : "r0","r1","r2","r3","r4","r5","r6","r7","memory")
#define DUMP_PMU() \
    __asm__ __volatile__ (\
        " r0 = #0x4a ; trap0(#0); \n" : : \
        : "r0","r1","r2","r3","r4","r5","r6","r7","memory")
#define cycles() ({ \
        unsigned long long _; asm volatile ("%0=s31:30" : "=r" (_)); _; \
    })


const uint32_t VECT_LEN = 128; // Set vector length for contexts
const uint32_t PRINT = 1;
const uint32_t NOPRINT = 0;

typedef struct coproc_param {
	uint32_t activations_start;
	uint32_t activations_range;
	uint32_t weights_start;
	uint32_t weights_range;
	uint8_t * output;
	uint32_t cvt_range;
	uint32_t bias;
	uint32_t x_tiles;
	uint32_t y_tiles;
	uint32_t z_tiles;
} coproc_param_t;

static uint32_t convert_mask(uint32_t v) {
	switch(v) {
		case 1:	return 1;
		case 3: return 2;
		case 7: return 3;
		case 15: return 4;
		case 31: return 5;
		case 63: return 6;
		case 127: return 7;
		case 255: return 8;
	}
	return 0;
}
static uint32_t log_2(uint32_t v) {


	switch(v) {
		case 1:	return 0;
		case 2: return 1;
		case 4: return 2;
		case 8: return 3;
		case 16: return 4;
		case 32: return 5;
		case 64: return 6;
		case 128: return 7;
		case 256: return 8;
		case 512: return 9;

	}
	return 0;
}

static int log2_depth(int v) {


	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;

	switch(v) {
		case 32: return 0;
		case 64: return 1;
		case 128: return 2;
		case 256: return 3;
		case 512: return 4;
		case 1024: return 5;
		case 2048: return 6;
		case 4096: return 7;
		case 8192: return 8;
	}
	return 0;
}


static uint32_t spatial_major_convert(uint32_t val) {
	uint32_t spatial_bits = (val >> DEPTH_SIZE) & ((1 << SPATIAL_OFFSET) - 1);
	uint32_t depth_bits = (val & (((1<<DEPTH_SIZE) - 1))) << SPATIAL_OFFSET;
	val &= ~((1<<(DEPTH_SIZE+SPATIAL_OFFSET))-1);
	//printf("\n%x %x %x\n", val, depth_bits, spatial_bits);
	return val | depth_bits | spatial_bits;
}


static void print_bin(uint32_t l, int32_t val)
{
	int i = l;
	while (i-- != 0) {
		printf("%ld",((val >> i) & 1));
	}
}


void write_output_image(const char * fname, int8_t * img, int32_t X, int32_t Y, int32_t output_depth, int32_t dy) {
	uint32_t fp;
	if((fp = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0777)) < 0) {
        printf("Error: Cannot open %s for output\n", fname);
        return;
    }
	write(fp, &X,  sizeof(uint32_t));
	write(fp, &Y,  sizeof(uint32_t));
	write(fp, &output_depth,  sizeof(uint32_t));
	write(fp, &dy,  sizeof(uint32_t));
	if(write(fp, img,  sizeof(uint8_t)*X*Y*output_depth)!=X*Y*output_depth){
		printf("Error:  Writing file: %s\n", fname);
	}
    close(fp);
}

uint8_t * read_output_image(const char * fname, int32_t * X, int32_t * Y, int32_t * output_depth, int32_t * dy) {
	uint32_t fp;
	if((fp = open(fname, O_RDONLY)) < 0 ) {
        printf("Error: Cannot open %s for output\n", fname);
		return NULL;
    }
	read(fp, X,  sizeof(uint32_t));
	read(fp, Y,  sizeof(uint32_t));
	read(fp, output_depth,  sizeof(uint32_t));
	read(fp, dy,  sizeof(uint32_t));
	int32_t s = (*X) * (*Y) * (*output_depth);
	uint8_t * img = (uint8_t *)malloc(sizeof(uint8_t)*s);

	if(read(fp, img,  sizeof(uint8_t)*s)!=s){
		printf("Error:  Writing file: %s\n", fname);
	}
	close(fp);
	return img;
}

void write_output_crouton(const char * fname, uint8_t * in) {
	uint32_t fp;
	printf("Writing to %s\n", fname);
	if((fp = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0777)) < 0) {
        printf("Error: Cannot open %s for output\n", fname);
        return;
    }
	if(write(fp, (uint8_t * ) in,  CROUTON_SIZE)!=CROUTON_SIZE){
		printf("Error:  Writing file: %s\n", fname);
	}
    close(fp);
}

#if defined(__cplusplus)
class mxmem_cvt_t {
	public:
		mxmem_cvt_t() : m_start(0), m_range(0) {};
		void ch_start(uint32_t in) {m_ch_start = in; };
		void ch_stop(uint32_t in) {m_ch_stop = in; };
		void offset(uint32_t in) {m_offset = in; };
		void spatial_mask(uint32_t in) {m_spatial_mask = in; };
		void y0(uint32_t in) {m_y0 = in; };
		void dy(uint32_t in) {m_dy = in; };
		uint32_t start() { return spatial_major_convert(m_start); };
		uint32_t range() { return spatial_major_convert(m_range); };
		uint32_t start_cm() { return m_start; };
		uint32_t range_cm() { return m_range; };
	private:
		union {
			struct {
				uint32_t m_ch_start : 5;
				uint32_t m_offset : 6;
				uint32_t m_y0 : 21;
			};
			uint32_t m_start;
		};
		union {
			struct {
				uint32_t m_ch_stop : 5;
				uint32_t m_spatial_mask : 6;
				uint32_t m_dy : 21;
			};
			uint32_t m_range;
		};
};


class mxmem_act_t {
	public:
		mxmem_act_t() : m_start(0), m_range(0) {};
		void ch_start(uint32_t in) {m_ch_start = in; };
		void ch_stop(uint32_t in) {m_ch_stop = in; };
		void filter(uint32_t in) {m_filter = in; };
		void spatial_mask(uint32_t in) {m_spatial_mask = in; };
		void y0(uint32_t in) {m_y0 = in; };
		void dy(uint32_t in) {m_dy = in; };
		uint32_t start() { return spatial_major_convert(m_start); };
		uint32_t range() { return spatial_major_convert(m_range); };
		uint32_t start_cm() { return m_start; };
		uint32_t range_cm() { return m_range; };
	private:
		union {
			struct {
				uint32_t m_ch_start : 5;
				uint32_t m_filter : 6;
				uint32_t m_y0 : 21;
			};
			uint32_t m_start;
		};
		union {
			struct {
				uint32_t m_ch_stop : 5;
				uint32_t m_spatial_mask : 6;
				uint32_t m_dy : 21;
			};
			uint32_t m_range;
		};
};
#endif
#endif
