/*
 *  Copyright(c) 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <hexagon_types.h>
#include <hexagon_protos.h>
#include <hexagon_standalone.h>

#include "cfgtable.h"
#include "vtcm_common.h"
#include "mmu.h"
#include "dma.h"

int err;
bool window_miss_seen;
uint32_t err_badva;
#define HEX_CAUSE_VWCTRL_WINDOW_MISS 0x29
#define HEX_CAUSE_PRIV_USER_NO_SINS 0x1b
#define HEX_CAUSE_PRIV_USER_NO_GINS 0x1a

void my_err_handler_helper(uint32_t ssr)
{
    uint32_t cause = GET_FIELD(ssr, SSR_CAUSE);
    switch (cause) {
    case HEX_CAUSE_VWCTRL_WINDOW_MISS:
        window_miss_seen = true;
        asm volatile("%0 = badva\n" : "=r"(err_badva));
        inc_elr(4);
        break;
    case HEX_CAUSE_PRIV_USER_NO_SINS:
    case HEX_CAUSE_PRIV_USER_NO_GINS:
        enter_kernel_mode();
        break;
    default:
        do_coredump();
        break;
    }
}

MY_EVENT_HANDLE(my_event_handle_error, my_err_handler_helper)

DEFAULT_EVENT_HANDLE(my_event_handle_nmi,         HANDLE_NMI_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissrw,   HANDLE_TLBMISSRW_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissx,    HANDLE_TLBMISSX_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_reset,       HANDLE_RESET_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_rsvd,        HANDLE_RSVD_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap0,       HANDLE_TRAP0_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap1,       HANDLE_TRAP1_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_int,         HANDLE_INT_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_fperror,     HANDLE_FPERROR_OFFSET)

#define BUFFER_SIZE 64
void test_scatter(uintptr_t scatter_addr)
{
    const unsigned int region_len = 1;
    unsigned short offsets[BUFFER_SIZE];
    unsigned short values[BUFFER_SIZE];
    Q6_vscatter_RMVhV(scatter_addr, region_len,
                      *(HVX_Vector *)offsets, *(HVX_Vector *)values);
}

bool is_vwctrl_enabled(void)
{
    uint32_t vwctrl;
    asm volatile("%0 = vwctrl\n" : "=r"(vwctrl));
    return vwctrl >> 31;
}

void get_vwctrl(bool *enable, uint32_t *vwctrl_lo, uint32_t *vwctrl_hi)
{
    uint32_t vwctrl;
    asm volatile("%0 = vwctrl\n" : "=r"(vwctrl));
    *enable = (vwctrl >> 31);
    *vwctrl_lo = vwctrl & 0xfff;
    *vwctrl_hi = (vwctrl >> 16) & 0xfff;
}

void set_vwctrl(bool enable, uint32_t vwctrl_lo, uint32_t vwctrl_hi)
{
    uint32_t vwctrl = (vwctrl_lo & 0xfff) |
                      ((vwctrl_hi & 0xfff) << 16) | (enable << 31);
    asm volatile("vwctrl = %0\n" : : "r"(vwctrl));
}

void test_vtcm_access(uint32_t access_addr, uint32_t wmin, uint32_t wmax,
                      bool expect_failure)
{
    printf("Trying to access 0x%08"PRIx32"; vwctrl is %s; "
           "VTCM window is [0x%08"PRIx32", 0x%08"PRIx32"]\n",
           access_addr, is_vwctrl_enabled() ? "enabled" : "DISABLED", wmin, wmax);
    printf("    expect %s: ", expect_failure ? "failure" : "success");

    enter_user_mode();
    test_scatter(access_addr);
    check32(window_miss_seen, expect_failure);
    printf("ok\n");
}

void test_coproc_vtcm_access(uint32_t access_addr, uint32_t wmin, uint32_t wmax,
                             bool expect_failure)
{
    printf("[Coproc] Trying to access 0x%08"PRIx32"; vwctrl is %s; "
           "VTCM window is [0x%08"PRIx32", 0x%08"PRIx32"]\n",
           access_addr, is_vwctrl_enabled() ? "enabled" : "DISABLED", wmin, wmax);
    printf("    expect %s: ", expect_failure ? "failure" : "success");

    enter_user_mode();
    Q6_mxmem2_bias_A((void *)access_addr);
    check32(window_miss_seen, expect_failure);
    printf("ok\n");
}

void calc_win_boundaries(uint32_t vtcm_base_addr, uint32_t vwctrl_lo,
                         uint32_t vwctrl_hi, uint32_t *wmin, uint32_t *wmax)
{
    /*
     * As per v79 sys spec:
     * Upper legal address is calculated as vtcm_base_addr + ((HI+1)*4KB)-1
     * Lower legal address is calculated as vtcm_base_addr + (LOW*4KB)
     */
    *wmin = vtcm_base_addr + (vwctrl_lo * 4 * 1024);
    *wmax = vtcm_base_addr + ((vwctrl_hi + 1) * 4 * 1024) - 1;
}

hexagon_udma_descriptor_type0_t *alloc_descriptor()
{
    uint8_t *ptr = aligned_alloc(DESC_ALIGN, DESC_ALIGN * 2);
    printf("desc0_1 at 0x%p\n", ptr);
    return (hexagon_udma_descriptor_type0_t *)ptr;
}

void test_vtcm_dma(uint32_t access_addr, uint32_t wmin, uint32_t wmax,
                   bool expect_failure)
{
    printf("Trying to copy vtcm via dma at 0x%08"PRIx32"; vwctrl is %s; "
           "VTCM window is [0x%08"PRIx32", 0x%08"PRIx32"]\n",
           access_addr, is_vwctrl_enabled() ? "enabled" : "DISABLED", wmin, wmax);

    const int alloc_size = 1024;
    unsigned char *memory = (unsigned char *)(access_addr + ALIGN);
    memory = (unsigned char *)((uintptr_t)memory & (~(ALIGN - 1)));

    unsigned char *src = memory;
    unsigned char *dst = memory + (alloc_size / 2);

    /* now allocate and init descriptor */
    hexagon_udma_descriptor_type0_t *desc = alloc_descriptor();
    if (!desc) {
        printf("FAIL\n");
        printf("out of memory: descriptors\n");
        exit(-2);
    }
    *desc = fill_descriptor0(src, dst, DMA_XFER_SIZE(alloc_size), NULL);

    printf("    expect %s: ", expect_failure ? "failure" : "success");
    enter_user_mode();
    /* kick off dma */
    do_dmastart(desc);
    check32(window_miss_seen, expect_failure);
    printf("ok\n");

    free(desc);
}

int main()
{
    uint32_t vwctrl_lo, vwctrl_hi, wmin, wmax, vtcm_base_addr;
    bool enable;

    install_my_event_vectors();
    setup_default_vtcm();
    vtcm_base_addr = get_vtcm_base();
    printf("VTCM base is 0x%08"PRIx32"\n", vtcm_base_addr);

    /* acquire coproc */
    asm volatile("R6=SSR\n"
                 "R6=setbit(R6, #26)\n"
                 "SSR = R6\n"
                 "{ nop; }\n"
                 "{ nop; }\n"
                 "isync;\n"
                 :
                 :
                 : "r6");

    get_vwctrl(&enable, &vwctrl_lo, &vwctrl_hi);
    calc_win_boundaries(vtcm_base_addr, vwctrl_lo, vwctrl_hi, &wmin, &wmax);

    /* normal access */
    test_vtcm_access(wmin, wmin, wmax, false);
    test_coproc_vtcm_access(wmin, wmin, wmax, false);
    test_vtcm_dma(wmin, wmin, wmax, false);

    /* vwctrl disabled */
    set_vwctrl(false, vwctrl_lo, vwctrl_hi);
    test_vtcm_access(wmin, wmin, wmax, true);
    test_coproc_vtcm_access(wmin, wmin, wmax, true);
    test_vtcm_dma(wmin, wmin, wmax, true);

    /* out of bounds access */
    calc_win_boundaries(vtcm_base_addr, vwctrl_lo + 1, vwctrl_hi, &wmin, &wmax);
    set_vwctrl(true, vwctrl_lo + 1, vwctrl_hi);
    test_vtcm_access(wmin, wmin, wmax, true);
    test_coproc_vtcm_access(wmin, wmin, wmax, true);
    test_vtcm_dma(wmin, wmin, wmax, true);

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
