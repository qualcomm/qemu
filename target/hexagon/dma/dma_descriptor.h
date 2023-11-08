/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
 * User-DMA Descriptor Definitions.
 *
 */

#ifndef _DMA_DESCRIPTOR_H_
#define _DMA_DESCRIPTOR_H_

typedef uint32_t va_t;
#define VA_FMT PRIx32
typedef uint64_t pa_t;
#define PA_FMT PRIx64

#define DM0_STATUS_IDLE             0x00000000
#define DM0_STATUS_RUN              0x00000001
#define DM0_STATUS_ERROR            0x00000002

/* DMA descriptor contents */
#define DESC_NEXT_MASK              0xFFFFFFFF
#define DESC_NEXT_SHIFT             0

#define DESC_DSTATE_MASK            0x80000000
#define DESC_DSTATE_SHIFT           31
#define DESC_DSTATE_INCOMPLETE      0
#define DESC_DSTATE_COMPLETE        1

#define DESC_ORDER_MASK             0x40000000
#define DESC_ORDER_SHIFT            30
#define DESC_ORDER_NOORDER          0
#define DESC_ORDER_ORDER            1

#define DESC_BYPASSSRC_MASK         0x20000000
#define DESC_BYPASSSRC_SHIFT        29
#define DESC_BYPASSDST_MASK         0x10000000
#define DESC_BYPASSDST_SHIFT        28
#define DESC_BYPASS_OFF             0
#define DESC_BYPASS_ON              1

#define DESC_SRCCOMP_MASK           0x08000000
#define DESC_SRCCOMP_SHIFT          27
#define DESC_DSTCOMP_MASK           0x04000000
#define DESC_DSTCOMP_SHIFT          26
#define DESC_COMP_NONE              0
#define DESC_COMP_DLBC              1


#define DESC_DESCTYPE_MASK          0x03000000
#define DESC_DESCTYPE_SHIFT         24
#define DESC_DESCTYPE_TYPE0         0
#define DESC_DESCTYPE_TYPE1         1

#define DESC_LENGTH_MASK            0x00FFFFFF
#define DESC_LENGTH_SHIFT           0
#define DESC_SRC_MASK               0xFFFFFFFF
#define DESC_SRC_SHIFT              0
#define DESC_DST_MASK               0xFFFFFFFF
#define DESC_DST_SHIFT              0
#define DESC_ROIWIDTH_MASK          0x0000FFFF
#define DESC_ROIWIDTH_SHIFT         0
#define DESC_ROIHEIGHT_MASK         0xFFFF0000
#define DESC_ROIHEIGHT_SHIFT        16
#define DESC_SRCSTRIDE_MASK         0x0000FFFF
#define DESC_SRCSTRIDE_SHIFT        0
#define DESC_DSTSTRIDE_MASK         0xFFFF0000
#define DESC_DSTSTRIDE_SHIFT        16
#define DESC_SRCWIDTHOFFSET_MASK    0x0000FFFF
#define DESC_SRCWIDTHOFFSET_SHIFT   0
#define DESC_DSTWIDTHOFFSET_MASK    0xFFFF0000
#define DESC_DSTWIDTHOFFSET_SHIFT   16

#define DESC_PADDING_MASK           0xF0000000
#define DESC_PADDING_SHIFT          28

#define DESC_RDALLOC_FORGET         0x7
typedef uint32_t vaddr_t; // this is already in global types


typedef struct dma_descriptor_type0_t
{
    uint32_t next;
    uint32_t dstate_order_bypass_comp_desctype_length;
    uint32_t src;
    uint32_t dst;
} dma_descriptor_type0_t;

typedef struct dma_descriptor_type1_t
{
    uint32_t next;
    uint32_t dstate_order_bypass_comp_desctype_length;
    uint32_t src;
    uint32_t dst;
    uint32_t allocation_padding;
    uint32_t roiheight_roiwidth;
    uint32_t dststride_srcstride;
    uint32_t dstwidthoffset_srcwidthoffset;
} dma_descriptor_type1_t;

typedef enum dma_transform_t {
    DMA_XFORM_NONE              = 0,
    DMA_XFORM_EXPAND_UPPER      = 1,
    DMA_XFORM_EXPAND_LOWER      = 2,
    DMA_XFORM_COMPRESS_UPPER    = 3,
    DMA_XFORM_COMPRESS_LOWER    = 4,
    DMA_XFORM_SWAP              = 5
} dma_transform_t;


typedef enum dma_descriptor_state_t {
    DMA_DESC_NOT_DONE              = 0,
    DMA_DESC_DONE      = 1,
    DMA_DESC_EXCEPT_RUNNING      = 2,
    DMA_DESC_EXCEPT_ERROR    = 3,
    DMA_DESC_DONE_PAUSE = 4
} dma_descriptor_state_t;


typedef enum dma_2d_desc_type {
   DMA_DESC_32BIT_VA_TYPE               = 0,
   DMA_DESC_38BIT_VA_TYPE               = 2,
   DMA_DESC_32BIT_VA_L2FETCH_TYPE       = 3,
   DMA_DESC_32BIT_VA_GATHER_TYPE        = 4,
   DMA_DESC_38BIT_VA_GATHER_TYPE        = 5,
   DMA_DESC_32BIT_VA_EXPANSION_TYPE     = 6,
   DMA_DESC_32BIT_VA_COMPRESSION_TYPE   = 7,
   DMA_DESC_32BIT_VA_CONSTANT_FILL_TYPE = 8,
   DMA_DESC_32BIT_VA_WIDE_TYPE          = 9,
   DMA_DESC_38BIT_VA_WIDE_TYPE          = 10
} dma_2d_desc_type_t;

// Mapping common fields of all descriptor types.
// This is solely for determining the descriptorSize
// WARNING: Never allocate this type cause it's unaligned, etc.
typedef union {
    struct {
        vaddr_t            nextDescPointer;
        struct {
            uint32_t    undefined_w1_b0     : 24;
            uint32_t    descSize            :  2;
            uint32_t    dstDlbc             :  1;
            uint32_t    srcDlbc             :  1;
            uint32_t    dstBypass           :  1;
            uint32_t    srcBypass           :  1;
            uint32_t    order               :  1;
            uint32_t    done                :  1;
        };
        vaddr_t         srcAddress;
        vaddr_t         dstAddress;
    };
    uint32_t word[16/sizeof(uint32_t)];
} HEXAGON_DmaDescriptorCommon_t;

typedef union {
    struct {
        vaddr_t            nextDescPointer;
        struct {
            uint32_t       length              : 24;
            uint32_t       descSize            :  2;   // == 2'b00
            uint32_t       dstDlbc             :  1;
            uint32_t       srcDlbc             :  1;
            uint32_t       dstBypass           :  1;
            uint32_t       srcBypass           :  1;
            uint32_t       order               :  1;
            uint32_t       done                :  1;
        };
        vaddr_t            srcAddress;
        vaddr_t            dstAddress;
    };
    uint32_t word[16/sizeof(uint32_t)];
} __attribute__ ((aligned (16))) HEXAGON_DmaDescriptorLinear_t;




typedef union {
    struct {
        vaddr_t            nextDescPointer;
        union {
        struct {
            uint32_t       curBlockOffset      :  8;
            uint32_t       startBlockOffest    :  8;
            uint32_t       undefined_w1_b16    :  8;
            uint32_t       descSize            :  2;   // == 2'b01
            uint32_t       dstDlbc             :  1;
            uint32_t       srcDlbc             :  1;
            uint32_t       dstBypass           :  1;
            uint32_t       srcBypass           :  1;
            uint32_t       order               :  1;
            uint32_t       done                :  1;
        };
            struct {
                uint32_t       dstStride_wide      : 24;    // Wide Descriptor type
                uint32_t       undefined_w1_b24    : 8;
            };
        };
        vaddr_t            srcAddress;
        vaddr_t            dstAddress;


        // Word 4, bytes 19:16
        uint8_t             type                   ;
        union {
            struct {
                uint8_t     srcUpperAddr           : 6; // 38-bit VA
                uint8_t     srcUpperAddr_reserved  : 2;
            };
            uint8_t         blockDelta             ; // Expansion/Compression
            uint8_t         fillValue              ; // Constant Fill
        };
        union {
            struct {
                uint8_t     dstUpperAddr           : 6; // 38-bit VA
                uint8_t     dstUpperAddr_reserved  : 2;
            };
            uint8_t         blockSize              ; // Expansion/Compression
        };
		uint8_t wr_alloc  : 1;
		uint8_t rd_alloc  : 3;
        uint8_t         transform                 : 4;


        // Words 5,6,7
        union {
            struct {
                uint16_t    width                  ;
                uint16_t    height                 ;
            };
			union {
            struct {
                uint32_t    width_wide             : 24;
                uint32_t    height_lo              : 8;
            };
				uint8_t width_wide_height_lo[4];
			};
        };
        union {
            struct {
                uint16_t    srcStride              ;
                uint16_t    dstStride              ;
            };
			union {
            struct {
                    uint32_t    height_hi          : 8;
					uint32_t srcStride_wide : 24;
				};
				uint8_t height_hi_srcStride_wide[4];
            };
        };
        union {
            struct {
                uint16_t    srcWidthOffset         ;
                uint16_t    dstWidthOffset         ;
            };
            struct {
				uint32_t width_offset    : 24;
                uint32_t    undefined_w7_24        : 8;
            };
        };
        // NOTE: WidthOffset's are architecturally ambiguous. HW just needs to be consistent (eg. pause/resume)
        // Current implementation seems to define these as post-padding
    };
    uint32_t word[32/sizeof(uint32_t)];
} __attribute__ ((aligned (32))) HEXAGON_DmaDescriptor2D_t;

#define MAX_DESC_SIZE (sizeof(HEXAGON_DmaDescriptor2D_t))

typedef union HEXAGON_DmaDescriptor {
    struct {
        HEXAGON_DmaDescriptorCommon_t   common;
        uint8_t                         fill_type_c[MAX_DESC_SIZE-sizeof(HEXAGON_DmaDescriptorCommon_t)];
    };
    struct {
        HEXAGON_DmaDescriptorLinear_t   type0;
        uint8_t                         fill_type_0[MAX_DESC_SIZE-sizeof(HEXAGON_DmaDescriptorLinear_t)];
    };
    HEXAGON_DmaDescriptor2D_t           type1;
    uint8_t     byte[MAX_DESC_SIZE];
    uint32_t    word[MAX_DESC_SIZE/sizeof(uint32_t)];
} HEXAGON_DmaDescriptor_t;

enum {
    UWBCA_NEW,
    UWBCA_IN,
    UWBCA_OUT
};

typedef struct dma_decoded_descriptor {
    HEXAGON_DmaDescriptor_t desc;
    uint32_t pc;
    uint32_t va;
    uint64_t pa;

    uint64_t previous_src_va;
    uint64_t previous_dst_va;

    // Some of these might be redundant and maybe we can eliminate them
    uint32_t bytes_to_transfer;
    uint32_t lines_to_transfer;
    uint32_t max_transfer_size;

    uint32_t bytes_to_read;
    uint32_t bytes_to_write;
    uint32_t padding_scale_factor_num;
    uint32_t padding_scale_factor_den;
    uint32_t src_roi_width;
    uint32_t dst_roi_width;
    uint32_t num_lines_read;
    uint32_t num_lines_write;
    uint32_t max_transfer_size_read;
    uint32_t max_transfer_size_write;
    uint32_t no_transfer; // for verif

    uint32_t gather;
    uint32_t l2fetch;
    uint32_t constant_fill;
    uint32_t expansion;
    uint32_t expansion_va;
    uint32_t compression;

    uint32_t gather_addr;   // just for debug
	uint32_t valid;
    uint32_t extended_va;
	uint32_t wide;
    uint32_t exception;
    uint32_t pause;
    uint32_t state;
    uint32_t restart;
    uint32_t id;
    uint32_t uwbc_a_state;
} dma_decoded_descriptor_t;




uint32_t get_dma_desc_next(void *d);
void set_dma_desc_next(void *d, uint32_t v);
uint32_t get_dma_desc_dstate(void *d);
void set_dma_desc_dstate(void *d, uint32_t v);
uint32_t get_dma_desc_order(void *d);
void set_dma_desc_order(void *d, uint32_t v);
uint32_t get_dma_desc_bypasssrc(void *d);
void set_dma_desc_bypasssrc(void *d, uint32_t v);
uint32_t get_dma_desc_bypassdst(void *d);
void set_dma_desc_bypassdst(void *d, uint32_t v);
uint32_t get_dma_desc_srccomp(void *d);
void set_dma_desc_srccomp(void *d, uint32_t v);
uint32_t get_dma_desc_dstcomp(void *d);
void set_dma_desc_dstcomp(void *d, uint32_t v);
uint32_t get_dma_desc_desctype(void *d);
void set_dma_desc_desctype(void *d, uint32_t v);
uint32_t get_dma_desc_length(void *d);
void set_dma_desc_length(void *d, uint32_t v);
uint32_t get_dma_desc_src(void *d);
void set_dma_desc_src(void *d, uint32_t v);
uint32_t get_dma_desc_dst(void *d);
void set_dma_desc_dst(void *d, uint32_t v);
uint32_t get_dma_desc_roiwidth(void *d);
void set_dma_desc_roiwidth(void *d, uint32_t v);
uint32_t get_dma_desc_roiheight(void *d);
void set_dma_desc_roiheight(void *d, uint32_t v);
uint32_t get_dma_desc_srcstride(void *d);
void set_dma_desc_srcstride(void *d, uint32_t v);
uint32_t get_dma_desc_dststride(void *d);
void set_dma_desc_dststride(void *d, uint32_t v);
uint32_t get_dma_desc_srcwidthoffset(void *d);
void set_dma_desc_srcwidthoffset(void *d, uint32_t v);
uint32_t get_dma_desc_dstwidthoffset(void *d);
void set_dma_desc_dstwidthoffset(void *d, uint32_t v);
uint32_t get_dma_desc_padding(void *d);
void set_dma_desc_padding(void *d, uint32_t v);



/* DM2 Support*/
#define DM2CFG_GUEST_MODE_STALL_MASK            0x00000001
#define DM2CFG_GUEST_MODE_STALL_SHIFT           0
#define DM2CFG_GUEST_MODE_STALL_YES             0
#define DM2CFG_GUEST_MODE_STALL_NO              1

#define DM2CFG_MONITOR_MODE_STALL_MASK          0x00000002
#define DM2CFG_MONITOR_MODE_STALL_SHIFT         1
#define DM2CFG_MONITOR_MODE_STALL_YES           0
#define DM2CFG_MONITOR_MODE_STALL_NO            1

#define DM2CFG_EXCEPTION_MODE_STALL_MASK        0x00000008
#define DM2CFG_EXCEPTION_MODE_STALL_SHIFT       3
#define DM2CFG_EXCEPTION_MODE_STALL_NO          0
#define DM2CFG_EXCEPTION_MODE_STALL_YES         1

#define DM2CFG_DEBUG_MODE_STALL_MASK            0x00000010
#define DM2CFG_DEBUG_MODE_STALL_SHIFT           4
#define DM2CFG_DEBUG_MODE_STALL_YES             0
#define DM2CFG_DEBUG_MODE_STALL_NO              1

#define DM2CFG_DLBC_ENABLE_MASK                 0x00000080
#define DM2CFG_DLBC_ENABLE_SHIFT                7
#define DM2CFG_DLBC_DISABLE                     0
#define DM2CFG_DLBC_ENABLE                      1

#define DM2CFG_OOO_WRITE_MASK                   0x00000100
#define DM2CFG_OOO_WRITE_SHIFT                  8
#define DM2CFG_OOO_WRITE_DISABLE                1
#define DM2CFG_OOO_WRITE_ENABLE                 0

#define DM2CFG_ERROR_EXCEPTION_MASK             0x00000200
#define DM2CFG_ERROR_EXCEPTION_SHIFT            9
#define DM2CFG_ERROR_EXCEPTION_GENERATE_NO      0
#define DM2CFG_ERROR_EXCEPTION_GENERATE_YES     1

#define DM2CFG_OUTSTANDING_READ_MASK            0x00FF0000
#define DM2CFG_OUTSTANDING_READ_SHIFT           16

#define DM2CFG_OUTSTANDING_WRITE_MASK           0xFF000000
#define DM2CFG_OUTSTANDING_WRITE_SHIFT          24

uint32_t get_dma_dm2_cfg_guest_mode_stall(uint32_t cfg);
void set_dma_dm2_cfg_guest_mode_stall(uint32_t *cfg, uint32_t v);
uint32_t get_dma_dm2_cfg_monitor_mode_stall(uint32_t cfg);
void set_dma_dm2_cfg_monitor_mode_stall(uint32_t *cfg, uint32_t v);
uint32_t get_dma_dm2_cfg_exception_stall(uint32_t cfg);
void set_dma_dm2_cfg_exception_stall(uint32_t *cfg, uint32_t v);
uint32_t get_dma_dm2_cfg_debug_stall(uint32_t cfg);
void set_dma_dm2_cfg_debug_stall(uint32_t *cfg, uint32_t v);
uint32_t get_dma_dm2_cfg_dlbc_enable(uint32_t cfg);
void set_dma_dm2_cfg_dlbc_enable(uint32_t *cfg, uint32_t v);
uint32_t get_dma_dm2_cfg_error_exception_ctrl(uint32_t cfg);
void set_dma_dm2_cfg_error_exception_ctrl(uint32_t *cfg, uint32_t v);
uint32_t get_dma_dm2_cfg_ooo_write_ctrl(uint32_t cfg);
void set_dma_dm2_cfg_ooo_write_ctrl(uint32_t *cfg, uint32_t v);
uint32_t get_dma_dm2_cfg_outstanding_transactions_read(uint32_t cfg);
void set_dma_dm2_cfg_outstanding_transactions_read(uint32_t *cfg, uint32_t v);
uint32_t get_dma_dm2_cfg_outstanding_transactions_write(uint32_t cfg);
void set_dma_dm2_cfg_outstanding_transactions_write(uint32_t *cfg, uint32_t v);

// Descriptor size in bytes
uint32_t HEXAGON_DmaDescriptorSize(uint32_t descSize);

void HEXAGON_DmaDescriptor_printList(const char *prefix, HEXAGON_DmaDescriptor_t *descP);
int  HEXAGON_DmaDescriptor_toStr(char *s, const HEXAGON_DmaDescriptor_t *descP);
int  HEXAGON_DmaDescriptorLinear_toStr(char *s, const HEXAGON_DmaDescriptorLinear_t *p);
int  HEXAGON_DmaDescriptor2D_toStr(char *s, const HEXAGON_DmaDescriptor2D_t *p);




#endif /*_DMA_DESCRIPTOR_H_ */
