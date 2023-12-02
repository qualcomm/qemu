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

/**
 * @brief User-DMA timing model
 * http://qwiki.qualcomm.com/hexagon-architecture/Dma_model
 *
 */

#ifndef _UARCH_DMA_H_
#define _UARCH_DMA_H_

typedef struct uarch_dma_engine_inst_t {
	dma_t				*dma;				///< pointer back to dma engine instance
} uarch_dma_engine_inst_t;

#define uarch_dma_tlbw(...)
#define uarch_dma_tlbinvasid(...)
#define uarch_dma_engine_step(...)
#define uarch_dma_engine_free(...)
#define uarch_dma_flush_timing(...)
#define uarch_dma_log_to_uarch(...)
#define uarch_dma_log_word_store(...)
#define uarch_dma_snaphot_flush(...)
#define uarch_dma_engine_init(...) (void *)NULL

#endif
