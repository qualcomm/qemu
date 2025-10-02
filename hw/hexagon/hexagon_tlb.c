/*
 * Hexagon TLB QOM Object
 *
 * Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/hexagon/hexagon_tlb.h"
#include "hw/qdev-properties.h"
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
#include "trace.h"

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
    tlb_pgsize_t size =
        ctz64(entry) + (GET_TLB_FIELD(entry, PTE_HSV39) ? 4 : 0);
    g_assert(size < NUM_PGSIZE_TYPES);
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

static inline uint64_t hex_tlb_virt_addr(uint64_t entry)
{
    int shift = GET_TLB_FIELD(entry, PTE_HSV39) ? 20 : TARGET_PAGE_BITS;
    return (uint64_t)GET_TLB_FIELD(entry, PTE_VPN) << shift;
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

static inline void hex_tlb_entry_get_perm(void *env_ptr, uint64_t entry,
                                          MMUAccessType access_type,
                                          int mmu_idx, int *prot,
                                          int32_t *excp)
{
    CPUHexagonState *env = (CPUHexagonState *)env_ptr;
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
            env->cause_code = HEX_CAUSE_FETCH_NO_UPAGE;
        } else if (!perm_x) {
            *excp = HEX_EVENT_PRECISE;
            env->cause_code = HEX_CAUSE_FETCH_NO_XPAGE;
        }
        break;
    case MMU_DATA_LOAD:
        if (user_idx && !perm_u) {
            *excp = HEX_EVENT_PRECISE;
            env->cause_code = HEX_CAUSE_PRIV_NO_UREAD;
        } else if (!perm_r) {
            *excp = HEX_EVENT_PRECISE;
            env->cause_code = HEX_CAUSE_PRIV_NO_READ;
        }
        break;
    case MMU_DATA_STORE:
        if (user_idx && !perm_u) {
            *excp = HEX_EVENT_PRECISE;
            env->cause_code = HEX_CAUSE_PRIV_NO_UWRITE;
        } else if (!perm_w) {
            *excp = HEX_EVENT_PRECISE;
            env->cause_code = HEX_CAUSE_PRIV_NO_WRITE;
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

static inline bool hex_tlb_entry_match(void *env, uint64_t entry,
                                       uint8_t asid, target_ulong VA,
                                       MMUAccessType access_type, hwaddr *PA,
                                       int *prot, uint64_t *size, int32_t *excp,
                                       int mmu_idx)
{
    if (hex_tlb_entry_match_noperm(entry, asid, VA)) {
        hex_tlb_entry_get_perm(env, entry, access_type, mmu_idx, prot, excp);
        *PA = hex_tlb_phys_addr(entry);
        *size = hex_tlb_page_size_bytes(entry);
        return true;
    }
    return false;
}

static bool hex_tlb_is_match(void *env,
                             uint64_t entry1, uint64_t entry2,
                             bool consider_gbit)
{
    bool valid1 = GET_TLB_FIELD(entry1, PTE_V);
    bool valid2 = GET_TLB_FIELD(entry2, PTE_V);
    uint64_t size1 = hex_tlb_page_size_bytes(entry1);
    uint64_t vaddr1 = ROUND_DOWN(hex_tlb_virt_addr(entry1), size1);
    uint64_t size2 = hex_tlb_page_size_bytes(entry2);
    uint64_t vaddr2 = ROUND_DOWN(hex_tlb_virt_addr(entry2), size2);
    int asid1 = GET_TLB_FIELD(entry1, PTE_ASID);
    int asid2 = GET_TLB_FIELD(entry2, PTE_ASID);
    bool gbit1 = GET_TLB_FIELD(entry1, PTE_G);
    bool gbit2 = GET_TLB_FIELD(entry2, PTE_G);

    if (!valid1 || !valid2) {
        return false;
    }

    if (((vaddr1 <= vaddr2) && (vaddr2 < (vaddr1 + size1))) ||
        ((vaddr2 <= vaddr1) && (vaddr1 < (vaddr2 + size2)))) {
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

static const Property hexagon_tlb_properties[] = {
    DEFINE_PROP_UINT32("num-entries", HexagonTLBState, num_entries,
                       MAX_TLB_ENTRIES),
};

static void hexagon_tlb_init(Object *obj)
{
    HexagonTLBState *s = HEXAGON_TLB(obj);
    /* Initialize fields to safe defaults */
    s->num_entries = 0;
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

    /* Allocate TLB entries array */
    s->entries = g_new0(uint64_t, s->num_entries);
    qemu_log("HexagonTLB: allocated %u entries (%u bytes)\n",
             s->num_entries, (unsigned)(s->num_entries * sizeof(uint64_t)));
}

static void hexagon_tlb_reset_hold(Object *obj, ResetType type)
{
    HexagonTLBState *s = HEXAGON_TLB(obj);
    int i;

    /* Only reset if entries have been allocated */
    if (!s->entries) {
        return;
    }

    /* Reset all TLB entries to 0 */
    for (i = 0; i < s->num_entries; i++) {
        s->entries[i] = 0;
    }
}

static const VMStateDescription vmstate_hexagon_tlb = {
    .name = "hexagon_tlb",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(num_entries, HexagonTLBState),
        VMSTATE_VARRAY_UINT32(entries, HexagonTLBState, num_entries, 0,
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

uint64_t hexagon_tlb_read(HexagonTLBState *tlb, void *env_ptr, uint32_t index)
{
    CPUHexagonState *env = (CPUHexagonState *)env_ptr;
    if (!tlb) {
        qemu_log_mask(LOG_GUEST_ERROR, "TLB read with NULL TLB state\n");
        return 0;
    }

    uint32_t myidx = TLB_WRAP_INDEX(index);

    /* Bounds check */
    if (myidx >= tlb->num_entries) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "TLB read index %u (wrapped %u) exceeds entries %u\n",
                      index, myidx, tlb->num_entries);
        return 0;
    }

    return tlb->entries[myidx];
}

void hexagon_tlb_write(HexagonTLBState *tlb, void *env_ptr,
                       uint32_t index, uint64_t value)
{
    CPUHexagonState *env = (CPUHexagonState *)env_ptr;
    if (!tlb) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "TLB write attempted but TLB not initialized\n");
        return;
    }

    uint32_t myidx = TLB_WRAP_INDEX(index);

    /* Ensure index is within bounds of this TLB */
    if (myidx >= tlb->num_entries) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "TLB write index %u out of bounds (num_entries=%u)\n",
                      myidx, tlb->num_entries);
        return;
    }

    bool old_entry_valid = GET_TLB_FIELD(tlb->entries[myidx], PTE_V);
    if (old_entry_valid && hexagon_cpu_mmu_enabled(env)) {
        /* FIXME - Do we have to invalidate everything here? */
        CPUState *cs = env_cpu(env);
        tlb_flush(cs);
    }
    uint64_t VA = hex_tlb_virt_addr(value);
    uint64_t PA = hex_tlb_phys_addr(value);
    trace_hexagon_tlbw(env->threadId, myidx, VA, PA);
    tlb->entries[myidx] = value;
}

bool hexagon_tlb_find_match(HexagonTLBState *tlb, void *env_ptr,
                            target_ulong VA, MMUAccessType access_type,
                            hwaddr *PA, int *prot, uint64_t *size,
                            int32_t *excp, int mmu_idx)
{
    CPUHexagonState *env = (CPUHexagonState *)env_ptr;
    *PA = 0;
    *prot = 0;
    *size = 0;
    *excp = 0;

    if (!tlb) {
        /* No TLB - return miss */
        return false;
    }

    uint32_t ssr = arch_get_system_reg(env, HEX_SREG_SSR);
    uint8_t asid = GET_SSR_FIELD(SSR_ASID, ssr);
    int i;
    HexagonCPU *cpu = env_archcpu(env);

    /* Search through available TLB entries */
    uint32_t search_limit = MIN(cpu->num_tlbs, tlb->num_entries);
    for (i = 0; i < search_limit; i++) {
        uint64_t entry = tlb->entries[i];
        if (hex_tlb_entry_match(env, entry, asid, VA, access_type, PA, prot,
                                size, excp, mmu_idx)) {
            return true;
        }
    }
    return false;
}

static uint32_t hex_tlb_lookup_by_asid(HexagonTLBState *tlb,
                                       void *env_ptr, uint32_t asid,
                                       uint64_t VA, bool extended)
{
    CPUHexagonState *env = (CPUHexagonState *)env_ptr;
    uint32_t not_found = 0x80000000;
    uint32_t idx = not_found;

    if (!tlb) {
        /* No TLB - return not found */
        return not_found;
    }

    HexagonCPU *cpu = env_archcpu(env);
    uint32_t init_tlb_reg = extended ? DMA_TLB_OFFSET : 0;
    uint32_t max_tlb_reg = extended
        ? DMA_TLB_OFFSET + cpu->dma_jtlb_entries
        : cpu->jtlb_entries;

    /* Ensure we don't go beyond allocated entries */
    max_tlb_reg = MIN(max_tlb_reg, tlb->num_entries);

    env->imprecise_exception = 0;
    for (uint32_t i = init_tlb_reg; i < max_tlb_reg; i++) {
        uint64_t entry = tlb->entries[i];
        if (hex_tlb_entry_match_noperm(entry, asid, VA)) {
            if (idx != not_found) {
                env->imprecise_exception = HEX_EVENT_IMPRECISE;
                env->cause_code = HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH;
                break;
            }
            idx = i;
        }
    }

    return idx;
}

uint32_t hexagon_tlb_lookup(HexagonTLBState *tlb, void *env_ptr,
                            uint32_t ssr, uint32_t VA)
{
    return hex_tlb_lookup_by_asid(tlb, env_ptr, GET_SSR_FIELD(SSR_ASID, ssr),
                                  VA, false);
}

uint32_t hexagon_tlb_lookup_extended(HexagonTLBState *tlb, void *env_ptr,
                                     uint32_t ssr, uint64_t VA)
{
    return hex_tlb_lookup_by_asid(tlb, env_ptr, GET_SSR_FIELD(SSR_ASID, ssr),
                                  VA, true);
}

/*
 * Return codes:
 * 0 or positive             index of match
 * -1                        multiple matches
 * -2                        no match
 */
int hexagon_tlb_check_overlap(HexagonTLBState *tlb, void *env_ptr,
                              uint64_t entry, uint64_t index)
{
    CPUHexagonState *env = (CPUHexagonState *)env_ptr;
    int matches = 0;
    int last_match = 0;
    int i;

    if (!tlb) {
        /* No TLB - no overlap */
        return 0;
    }

    HexagonCPU *cpu = env_archcpu(env);
    uint32_t search_limit = MIN(cpu->num_tlbs, tlb->num_entries);

    for (i = 0; i < search_limit; i++) {
        if (hex_tlb_is_match(env, entry, tlb->entries[i], false)) {
            matches++;
            last_match = i;
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

void hexagon_tlb_lock(HexagonTLBState *tlb, void *env_ptr)
{
    /* Lock operation is now handled in the CPU layer */
}

void hexagon_tlb_unlock(HexagonTLBState *tlb, void *env_ptr)
{
    /* Unlock operation is now handled in the CPU layer */
}

void hexagon_tlb_dump(HexagonTLBState *tlb, void *env_ptr)
{
    CPUHexagonState *env = (CPUHexagonState *)env_ptr;
    int i;

    if (!tlb) {
        return;
    }

    HexagonCPU *cpu = env_archcpu(env);
    uint32_t dump_limit = MIN(cpu->num_tlbs, tlb->num_entries);

    for (i = 0; i < dump_limit; i++) {
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
