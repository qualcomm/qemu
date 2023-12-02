/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef XLATE_INFO_H
#define XLATE_INFO_H 1

#include "hex_arch_types.h"

typedef union {
	size2u_t raw;
	struct {
		size2u_t device:1;
		size2u_t gatherable:1;
		size2u_t reorderable:1;
		size2u_t early_wackable:1;
		size2u_t sharability:2;
		size2u_t arch_cacheable:1;	/* EJP: need to have this in v60 for "outer cacheable" but L2$ disabled in syscfg for ll/sc */
		size2u_t vtcm:1; /* Indicate VTCM for DMA */
		size2u_t l2tcm:1; /* Indicate L2TCM for DMA */
		size2u_t ahb:1; /* Indicate AHB for DMA */
		size2u_t l2vic:1; /* Indicate AHB for DMA */
		size2u_t l2itcm:1; /* Indicate AHB for DMA */
		size2u_t l1s:1; /* Indicate AHB for DMA */
		size2u_t axi2:1; /* Indicate AHB for DMA */
		size2u_t invalid_cccc:1; /* Indicate invalid CCCC for DMA */
		size2u_t invalid_dma:1; /* Indicate invalid memory space for DMA */
	};
} sys_mem_info_t;

typedef union {
	size1u_t raw;
	struct {
		size1u_t cacheable:1;
		size1u_t write_back:1;
		size1u_t read_allocate:1;
		size1u_t write_allocate:1;
		size1u_t transient:1;
	};
} sys_cache_info_t;

typedef union {
	size2u_t raw;
	struct {
		union {
			size1u_t u_xwr;
			struct {
				size1u_t u_r:1;
				size1u_t u_w:1;
				size1u_t u_x:1;
			};
		};
		union {
			size1u_t p_xwr;
			struct {
				size1u_t p_r:1;
				size1u_t p_w:1;
				size1u_t p_x:1;
			};
		};
	};
} sys_perms_t;

enum {
	XLATE_TRANSTYPE_NONE,
	XLATE_TRANSTYPE_ARCHTLB,
	XLATE_TRANSTYPE_WALKER,
};

typedef struct {
	paddr_t pa;
	sys_perms_t perms;
	sys_cache_info_t inner;
	sys_cache_info_t outer;
	sys_mem_info_t memtype;
	size1u_t transtype;
	size1u_t size:6;
	size1u_t global:1;
	size1u_t pte_atr0:1;
	size1u_t pte_atr1:1;
	size1u_t pte_x:1;
	size1u_t pte_r:1;
	size1u_t pte_w:1;
	size1u_t pte_u:1;
	size1u_t cccc:4;
	size1u_t jtlbidx;
} xlate_info_t;

#endif
