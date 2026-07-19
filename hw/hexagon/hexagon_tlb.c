/*
 * Hexagon TLB QOM Object
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/hexagon/hexagon_tlb.h"
#include "hw/core/qdev-properties.h"
#include "qapi/error.h"
#include "qemu/qemu-print.h"
#include "qemu/log.h"
#include "target/hexagon/cpu.h"
#include "target/hexagon/internal.h"
#include "target/hexagon/macros.h"
#include "target/hexagon/sys_macros.h"
#include "target/hexagon/reg_fields.h"
#include "target/hexagon/hex_mmu.h"
#include "migration/vmstate.h"

#define GET_TLB_FIELD(ENTRY, FIELD)                               \
    ((uint64_t)fEXTRACTU_BITS(ENTRY, reg_field_info[FIELD].width, \
                              reg_field_info[FIELD].offset))

/* PPD (physical page descriptor) */
static inline uint64_t GET_PPD(uint64_t entry)
{
    if (GET_TLB_FIELD(entry, PTE_HSV39)) {
        int PA4543_shift = reg_field_info[PTE_PPD].width;
        int PA4544_shift = PA4543_shift + reg_field_info[PTE_PA43].width;
        return GET_TLB_FIELD(entry, PTE_PPD) |
               (GET_TLB_FIELD(entry, PTE_PA43) << PA4543_shift) |
               (GET_TLB_FIELD(entry, PTE_PA4544) << PA4544_shift);
    } else {
        return GET_TLB_FIELD(entry, PTE_PPD) |
               (GET_TLB_FIELD(entry, PTE_PA35) <<
                reg_field_info[PTE_PPD].width);
    }
}

#define NO_ASID      (1 << 8)

typedef enum {
    PGSIZE_4K,
    PGSIZE_16K,
    PGSIZE_64K,
    PGSIZE_256K,
    PGSIZE_1M,
    PGSIZE_4M,
    PGSIZE_16M,
    PGSIZE_64M,
    PGSIZE_256M,
    PGSIZE_1G,
    PGSIZE_4G,
    PGSIZE_16G,
    PGSIZE_64G,
    NUM_PGSIZE_TYPES
} tlb_pgsize_t;

static const char *pgsize_str[NUM_PGSIZE_TYPES] = {
    "4K",
    "16K",
    "64K",
    "256K",
    "1M",
    "4M",
    "16M",
    "64M",
    "256M",
    "1G",
    "4G",
    "16G",
    "64G",
};

#define INVALID_MASK 0xffffffffLL

static const uint64_t encmask_2_mask[] = {
    0x0fffLL,                           /* 4k,   0000 */
    0x3fffLL,                           /* 16k,  0001 */
    0xffffLL,                           /* 64k,  0010 */
    0x3ffffLL,                          /* 256k, 0011 */
    0xfffffLL,                          /* 1m,   0100 */
    0x3fffffLL,                         /* 4m,   0101 */
    0xffffffLL,                         /* 16m,  0110 */
    0x3ffffffLL,                        /* 64m,  0111 */
    0xfffffffLL,                        /* 256m, 1000 */
    0x3fffffffLL,                       /* 1g,   1001 */
    0xffffffffLL,                      /* 4g,   1010 */
    0x3ffffffffLL,                     /* 16g,  1011 */
    0xfffffffffLL,                     /* 64g,  1100 */
    INVALID_MASK,                      /* RSVD, 0111 */
};

/*
 * @return the page size type from @a entry.
 */
static inline tlb_pgsize_t hex_tlb_pgsize_type(uint64_t entry)
{
    if (entry == 0) {
        return 0;
    }
    /*
     * The page size is encoded as the position of the lowest set bit of the
     * PPD field.  Restrict the scan to that field: the bits above it belong
     * to other fields (C, permissions, VPN, ASID...), and letting them pick
     * the size turns an entry whose PPD happens to be zero into an absurdly
     * large page that then appears to overlap everything.
     */
    uint64_t ppd = GET_TLB_FIELD(entry, PTE_PPD);
    unsigned size = (ppd ? ctz64(ppd) : 0) +
                    (GET_TLB_FIELD(entry, PTE_HSV39) ? 4 : 0);
    if (size >= NUM_PGSIZE_TYPES) {
        /*
         * The guest can park any value in a TLB slot, so this field is not
         * trustworthy.  Saturate rather than aborting the emulator.
         */
        qemu_log_mask(LOG_GUEST_ERROR,
                      "hexagon TLB: entry 0x%016" PRIx64 " encodes page size "
                      "%u, clamping to %s\n",
                      entry, size, pgsize_str[NUM_PGSIZE_TYPES - 1]);
        size = NUM_PGSIZE_TYPES - 1;
    }
    return size;
}

/*
 * @return the page size of @a entry, in bytes.
 */
static inline uint64_t hex_tlb_page_size_bytes(uint64_t entry)
{
    return 1ull << (TARGET_PAGE_BITS + 2 * hex_tlb_pgsize_type(entry));
}

static inline uint64_t hex_tlb_phys_page_num(uint64_t entry)
{
    uint32_t ppd = GET_PPD(entry);
    return ppd >> 1;
}

static inline uint64_t hex_tlb_phys_addr(uint64_t entry)
{
    uint64_t pagemask = encmask_2_mask[hex_tlb_pgsize_type(entry)];
    uint64_t pagenum = hex_tlb_phys_page_num(entry);
    uint64_t PA = (pagenum << TARGET_PAGE_BITS) & (~pagemask);
    return PA;
}

static inline vaddr hex_tlb_virt_addr(uint64_t entry)
{
    int shift = GET_TLB_FIELD(entry, PTE_HSV39) ? 20 : TARGET_PAGE_BITS;
    return (vaddr)GET_TLB_FIELD(entry, PTE_VPN) << shift;
}

static inline bool hex_tlb_entry_match_noperm(uint64_t entry, uint32_t asid,
                                              uint64_t VA)
{
    if (GET_TLB_FIELD(entry, PTE_V)) {
        if (GET_TLB_FIELD(entry, PTE_G)) {
            /* Global entry - ignore ASID */
        } else if (asid != NO_ASID) {
            uint32_t tlb_asid = GET_TLB_FIELD(entry, PTE_ASID);
            if (tlb_asid != asid) {
                return false;
            }
        }

        uint64_t page_size = hex_tlb_page_size_bytes(entry);
        uint64_t page_start =
            ROUND_DOWN(hex_tlb_virt_addr(entry), page_size);
        if (page_start <= VA && VA < page_start + page_size) {
            /* FIXME - Anything else we need to check? */
            return true;
        }
    }
    return false;
}

static inline void hex_tlb_entry_get_perm(uint64_t entry,
                                          MMUAccessType access_type,
                                          int mmu_idx, int *prot,
                                          int32_t *excp, int32_t *cause_code)
{
    bool perm_x = GET_TLB_FIELD(entry, PTE_X);
    bool perm_w = GET_TLB_FIELD(entry, PTE_W);
    bool perm_r = GET_TLB_FIELD(entry, PTE_R);
    bool perm_u = GET_TLB_FIELD(entry, PTE_U);
    bool user_idx = mmu_idx == MMU_USER_IDX;

    if (mmu_idx == MMU_KERNEL_IDX) {
        *prot = PAGE_VALID | PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return;
    }

    *prot = PAGE_VALID;
    switch (access_type) {
    case MMU_INST_FETCH:
        if (user_idx && !perm_u) {
            *excp = HEX_EVENT_PRECISE;
            *cause_code = HEX_CAUSE_FETCH_NO_UPAGE;
        } else if (!perm_x) {
            *excp = HEX_EVENT_PRECISE;
            *cause_code = HEX_CAUSE_FETCH_NO_XPAGE;
        }
        break;
    case MMU_DATA_LOAD:
        if (user_idx && !perm_u) {
            *excp = HEX_EVENT_PRECISE;
            *cause_code = HEX_CAUSE_PRIV_NO_UREAD;
        } else if (!perm_r) {
            *excp = HEX_EVENT_PRECISE;
            *cause_code = HEX_CAUSE_PRIV_NO_READ;
        }
        break;
    case MMU_DATA_STORE:
        if (user_idx && !perm_u) {
            *excp = HEX_EVENT_PRECISE;
            *cause_code = HEX_CAUSE_PRIV_NO_UWRITE;
        } else if (!perm_w) {
            *excp = HEX_EVENT_PRECISE;
            *cause_code = HEX_CAUSE_PRIV_NO_WRITE;
        }
        break;
    }

    if (!user_idx || perm_u) {
        if (perm_x) {
            *prot |= PAGE_EXEC;
        }
        if (perm_r) {
            *prot |= PAGE_READ;
        }
        if (perm_w) {
            *prot |= PAGE_WRITE;
        }
    }
}

static inline bool hex_tlb_entry_match(uint64_t entry, uint8_t asid,
                                       target_ulong VA,
                                       MMUAccessType access_type,
                                       hwaddr *PA, int *prot, uint64_t *size,
                                       int32_t *excp, int32_t *cause_code,
                                       int mmu_idx)
{
    if (hex_tlb_entry_match_noperm(entry, asid, VA)) {
        hex_tlb_entry_get_perm(entry, access_type, mmu_idx, prot, excp,
                               cause_code);
        *PA = hex_tlb_phys_addr(entry);
        *size = hex_tlb_page_size_bytes(entry);
        return true;
    }
    return false;
}

static bool hex_tlb_is_match(uint64_t entry1, uint64_t entry2,
                             bool consider_gbit)
{
    /*
     * Bail out before decoding anything else: an invalid entry can hold an
     * arbitrary value the guest parked there, so its page-size field is not
     * meaningful.
     */
    if (!GET_TLB_FIELD(entry1, PTE_V) || !GET_TLB_FIELD(entry2, PTE_V)) {
        return false;
    }

    Range range1;
    uint64_t size1 = hex_tlb_page_size_bytes(entry1);
    range_init_nofail(&range1, ROUND_DOWN(hex_tlb_virt_addr(entry1), size1),
                      size1);
    Range range2;
    uint64_t size2 = hex_tlb_page_size_bytes(entry2);
    range_init_nofail(&range2, ROUND_DOWN(hex_tlb_virt_addr(entry2), size2),
                      size2);
    int asid1 = GET_TLB_FIELD(entry1, PTE_ASID);
    int asid2 = GET_TLB_FIELD(entry2, PTE_ASID);
    bool gbit1 = GET_TLB_FIELD(entry1, PTE_G);
    bool gbit2 = GET_TLB_FIELD(entry2, PTE_G);

    if (range_overlaps_range(&range1, &range2)) {
        if (asid1 == asid2) {
            return true;
        }
        if ((consider_gbit && gbit1) || gbit2) {
            return true;
        }
    }
    return false;
}

/* TLB Object implementation */

static inline uint32_t get_total_size_elts(HexagonTLBState *s)
{
    g_assert(s->num_entries <= DMA_TLB_OFFSET);
    return DMA_TLB_OFFSET + s->dma_entries;
}

static const Property hexagon_tlb_properties[] = {
    DEFINE_PROP_UINT32("num-entries", HexagonTLBState, num_entries,
                       MAX_TLB_ENTRIES),
    DEFINE_PROP_UINT32("dma-entries", HexagonTLBState, dma_entries, 0),
};

static void hexagon_tlb_init(Object *obj)
{
    HexagonTLBState *s = HEXAGON_TLB(obj);
    /* Initialize fields to safe defaults */
    s->num_entries = 0;
    s->dma_entries = 0;
    s->entries = NULL;
}

static void hexagon_tlb_finalize(Object *obj)
{
    HexagonTLBState *s = HEXAGON_TLB(obj);
    g_free(s->entries);
}

static void hexagon_tlb_realize(DeviceState *dev, Error **errp)
{
    HexagonTLBState *s = HEXAGON_TLB(dev);

    if (s->num_entries == 0) {
        error_setg(errp, "num-entries must be greater than 0");
        return;
    }

    /* Calculate allocation size as max of regular entries and DMA range */
    uint32_t total_size = get_total_size_elts(s);
    s->entries = g_new0(uint64_t, total_size);
}

static void hexagon_tlb_reset_hold(Object *obj, ResetType type)
{
    HexagonTLBState *s = HEXAGON_TLB(obj);

    /* Reset entire TLB array to 0 */
    uint32_t total_size = get_total_size_elts(s);
    memset(s->entries, 0, total_size * sizeof(uint64_t));
}

static int hexagon_tlb_pre_save(void *opaque)
{
    HexagonTLBState *s = opaque;
    s->save_size = get_total_size_elts(s);
    return 0;
}

static const VMStateDescription vmstate_hexagon_tlb = {
    .name = "hexagon_tlb",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_save = hexagon_tlb_pre_save,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(num_entries, HexagonTLBState),
        VMSTATE_UINT32(dma_entries, HexagonTLBState),
        VMSTATE_UINT32(save_size, HexagonTLBState),
        VMSTATE_VARRAY_UINT32_ALLOC(entries, HexagonTLBState,
                                    save_size, 0,
                                    vmstate_info_uint64, uint64_t),
        VMSTATE_END_OF_LIST()
    }
};

static void hexagon_tlb_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = hexagon_tlb_realize;
    rc->phases.hold = hexagon_tlb_reset_hold;
    device_class_set_props(dc, hexagon_tlb_properties);
    dc->vmsd = &vmstate_hexagon_tlb;
    dc->desc = "Hexagon TLB";
    dc->user_creatable = false;
}

static const TypeInfo hexagon_tlb_info = {
    .name          = TYPE_HEXAGON_TLB,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HexagonTLBState),
    .instance_init = hexagon_tlb_init,
    .instance_finalize = hexagon_tlb_finalize,
    .class_init    = hexagon_tlb_class_init,
};

static void hexagon_tlb_register_types(void)
{
    type_register_static(&hexagon_tlb_info);
}

type_init(hexagon_tlb_register_types)

/* TLB interface functions */

uint64_t hexagon_tlb_read(HexagonTLBState *tlb, uint32_t index)
{
    if (!tlb) {
        qemu_log_mask(LOG_GUEST_ERROR, "TLB read with NULL TLB state\n");
        return 0;
    }

    /* Check bounds for regular vs DMA access */
    if (index >= DMA_TLB_OFFSET) {
        uint32_t dma_index = index - DMA_TLB_OFFSET;
        if (dma_index >= tlb->dma_entries) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "DMA TLB read index %u (dma_index %u) exceeds "
                          "dma_entries %u\n",
                          index, dma_index, tlb->dma_entries);
            return 0;
        }
    } else if (index >= tlb->num_entries) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "Regular TLB read index %u exceeds num_entries %u\n",
                      index, tlb->num_entries);
        return 0;
    }

    return tlb->entries[index];
}

void hexagon_tlb_write(HexagonTLBState *tlb, uint32_t index, uint64_t value,
                       bool old_entry_valid, bool mmu_enabled,
                       uint32_t threadId, uint32_t wrapped_index)
{
    if (!tlb) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "TLB write attempted but TLB not initialized\n");
        return;
    }

    /* Check bounds for regular vs DMA access */
    if (wrapped_index >= DMA_TLB_OFFSET) {
        uint32_t dma_index = wrapped_index - DMA_TLB_OFFSET;
        if (dma_index >= tlb->dma_entries) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "DMA TLB write index %u (dma_index %u) out of bounds "
                          "(dma_entries=%u)\n",
                          wrapped_index, dma_index, tlb->dma_entries);
            return;
        }
    } else if (wrapped_index >= tlb->num_entries) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "Regular TLB write index %u exceeds num_entries %u\n",
                      wrapped_index, tlb->num_entries);
        return;
    }

    tlb->entries[wrapped_index] = value;
}

bool hexagon_tlb_find_match(HexagonTLBState *tlb, uint8_t asid,
                            target_ulong VA, MMUAccessType access_type,
                            hwaddr *PA, int *prot, uint64_t *size,
                            int32_t *excp, int32_t *cause_code, int mmu_idx)
{
    *PA = 0;
    *prot = 0;
    *size = 0;
    *excp = 0;
    *cause_code = 0;

    if (!tlb) {
        /* No TLB - return miss */
        return false;
    }

    int i;
    /* Search through regular TLB entries */
    for (i = 0; i < tlb->num_entries; i++) {
        uint64_t entry = tlb->entries[i];
        if (hex_tlb_entry_match(entry, asid, VA, access_type, PA, prot,
                                size, excp, cause_code, mmu_idx)) {
            return true;
        }
    }

    /* Search through DMA TLB entries */
    for (i = DMA_TLB_OFFSET; i < DMA_TLB_OFFSET + tlb->dma_entries; i++) {
        uint64_t entry = tlb->entries[i];
        if (hex_tlb_entry_match(entry, asid, VA, access_type, PA, prot,
                                size, excp, cause_code, mmu_idx)) {
            return true;
        }
    }
    return false;
}

static uint32_t hex_tlb_lookup_by_asid(HexagonTLBState *tlb, uint32_t asid,
                                       uint64_t VA, bool extended,
                                       uint32_t *imprecise_exception,
                                       int32_t *cause_code)
{
    uint32_t not_found = 0x80000000;
    uint32_t idx = not_found;

    if (!tlb) {
        /* No TLB - return not found */
        return not_found;
    }

    *imprecise_exception = 0;

    if (extended) {
        /* Search DMA TLB entries */
        for (uint32_t i = DMA_TLB_OFFSET;
             i < DMA_TLB_OFFSET + tlb->dma_entries; i++) {
            uint64_t entry = tlb->entries[i];
            if (hex_tlb_entry_match_noperm(entry, asid, VA)) {
                if (idx != not_found) {
                    *imprecise_exception = HEX_EVENT_IMPRECISE;
                    *cause_code = HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH;
                    break;
                }
                idx = i;  /* Return actual index */
            }
        }
    } else {
        /* Search regular TLB entries */
        for (uint32_t i = 0; i < tlb->num_entries; i++) {
            uint64_t entry = tlb->entries[i];
            if (hex_tlb_entry_match_noperm(entry, asid, VA)) {
                if (idx != not_found) {
                    *imprecise_exception = HEX_EVENT_IMPRECISE;
                    *cause_code = HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH;
                    break;
                }
                idx = i;
            }
        }
    }

    return idx;
}

uint32_t hexagon_tlb_lookup(HexagonTLBState *tlb, uint8_t asid, uint32_t VA,
                            uint32_t *imprecise_exception, int32_t *cause_code)
{
    return hex_tlb_lookup_by_asid(tlb, asid, VA, false,
                                  imprecise_exception, cause_code);
}

uint32_t hexagon_tlb_lookup_extended(HexagonTLBState *tlb, uint8_t asid,
                                     uint64_t VA, uint32_t *imprecise_exception,
                                     int32_t *cause_code)
{
    return hex_tlb_lookup_by_asid(tlb, asid, VA, true,
                                  imprecise_exception, cause_code);
}

/*
 * Return codes:
 * 0 or positive             index of match
 * -1                        multiple matches
 * -2                        no match
 */
int hexagon_tlb_check_overlap(HexagonTLBState *tlb, uint64_t entry,
                              uint64_t index)
{
    int matches = 0;
    int last_match = 0;
    int i;

    if (!tlb) {
        /* No TLB - no overlap */
        return 0;
    }

    /* Check regular TLB entries */
    for (i = 0; i < tlb->num_entries; i++) {
        if (hex_tlb_is_match(entry, tlb->entries[i], false)) {
            matches++;
            last_match = i;
        }
    }

    /* Check DMA TLB entries */
    for (i = DMA_TLB_OFFSET; i < DMA_TLB_OFFSET + tlb->dma_entries; i++) {
        if (hex_tlb_is_match(entry, tlb->entries[i], false)) {
            matches++;
            last_match = i;  /* Return actual index */
        }
    }

    if (matches == 1) {
        return last_match;
    }
    if (matches == 0) {
        return -2;
    }
    return -1;
}

void hexagon_tlb_dump(HexagonTLBState *tlb)
{
    int i;

    if (!tlb) {
        return;
    }

    for (i = 0; i < tlb->num_entries; i++) {
        uint64_t entry = tlb->entries[i];
        if (GET_TLB_FIELD(entry, PTE_V)) {
            qemu_printf("0x%016" PRIx64 ": ", entry);
            uint64_t PA = hex_tlb_phys_addr(entry);
            uint64_t VA = hex_tlb_virt_addr(entry);
            qemu_printf(
                "V:%" PRId64 " G:%" PRId64 " A1:%" PRId64 " A0:%" PRId64,
                GET_TLB_FIELD(entry, PTE_V), GET_TLB_FIELD(entry, PTE_G),
                GET_TLB_FIELD(entry, PTE_ATR1), GET_TLB_FIELD(entry, PTE_ATR0));
            qemu_printf(" ASID:0x%02" PRIx64 " VA:0x%08" PRIx64,
                        GET_TLB_FIELD(entry, PTE_ASID), VA);
            qemu_printf(
                " X:%" PRId64 " W:%" PRId64 " R:%" PRId64 " U:%" PRId64
                " C:%" PRId64,
                GET_TLB_FIELD(entry, PTE_X), GET_TLB_FIELD(entry, PTE_W),
                GET_TLB_FIELD(entry, PTE_R), GET_TLB_FIELD(entry, PTE_U),
                GET_TLB_FIELD(entry, PTE_C));
            qemu_printf(" PA:0x%09" PRIx64 " SZ:%s (0x%" PRIx64 ")\n", PA,
                        pgsize_str[hex_tlb_pgsize_type(entry)],
                        hex_tlb_page_size_bytes(entry));
        }
    }
}

/* TLB accessor functions */
uint32_t hexagon_tlb_get_num_entries(HexagonTLBState *tlb)
{
    return tlb ? tlb->num_entries : 0;
}

uint32_t hexagon_tlb_get_dma_entries(HexagonTLBState *tlb)
{
    return tlb ? tlb->dma_entries : 0;
}

uint32_t hexagon_tlb_get_total_entries(HexagonTLBState *tlb)
{
    if (!tlb) {
        return 0;
    }
    return get_total_size_elts(tlb);
}
