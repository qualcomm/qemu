/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (c) 2024 MemVerge Inc.
 *
 */

#include <sys/file.h>
#include "qemu/osdep.h"
#include "qemu/bitmap.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "hw/cxl/cxl.h"
#include "hw/cxl/cxl_mailbox.h"
#include "hw/cxl/cxl_device.h"
#include "hw/pci/pcie.h"
#include "hw/pci/pcie_port.h"
#include "system/hostmem.h"
#include "mhsld.h"

#define TYPE_CXL_MHSLD "cxl-mhsld"
OBJECT_DECLARE_TYPE(CXLMHSLDState, CXLMHSLDClass, CXL_MHSLD)

/*
 * CXL r3.0 section 7.6.7.5.1 - Get Multi-Headed Info (Opcode 5500h)
 *
 * This command retrieves the number of heads, number of supported LDs,
 * and Head-to-LD mapping of a Multi-Headed device.
 */
static CXLRetCode cmd_mhd_get_info(const struct cxl_cmd *cmd,
                                   uint8_t *payload_in, size_t len_in,
                                   uint8_t *payload_out, size_t *len_out,
                                   CXLCCI * cci)
{
    CXLMHSLDState *s = CXL_MHSLD(cci->d);
    MHDGetInfoInput *input = (void *)payload_in;
    MHDGetInfoOutput *output = (void *)payload_out;

    uint8_t start_ld = input->start_ld;
    uint8_t ldmap_len = input->ldmap_len;
    uint8_t i;

    if (start_ld >= s->mhd_state->nr_lds) {
        return CXL_MBOX_INVALID_INPUT;
    }

    output->nr_lds = s->mhd_state->nr_lds;
    output->nr_heads = s->mhd_state->nr_heads;
    output->resv1 = 0;
    output->start_ld = start_ld;
    output->resv2 = 0;

    for (i = 0; i < ldmap_len && (start_ld + i) < output->nr_lds; i++) {
        output->ldmap[i] = s->mhd_state->ldmap[start_ld + i];
    }
    output->ldmap_len = i;

    *len_out = sizeof(*output) + output->ldmap_len;
    return CXL_MBOX_SUCCESS;
}

static const struct cxl_cmd cxl_cmd_set_mhsld[256][256] = {
    [MHSLD_MHD][GET_MHD_INFO] = {"GET_MULTI_HEADED_INFO",
        cmd_mhd_get_info, 2, 0},
};

static const Property cxl_mhsld_props[] = {
    DEFINE_PROP_UINT32("mhd-head", CXLMHSLDState, mhd_head, ~(0)),
    DEFINE_PROP_STRING("mhd-state_file", CXLMHSLDState, mhd_state_file),
    DEFINE_PROP_BOOL("mhd-init", CXLMHSLDState, mhd_init, false),
};

static int cxl_mhsld_state_open(const char *filename, int flags)
{
    char name[128];
    snprintf(name, sizeof(name), "/%s", filename);
    return shm_open(name, flags, 0666);
}

static int cxl_mhsld_state_unlink(const char *filename)
{
    char name[128];
    snprintf(name, sizeof(name), "/%s", filename);
    return shm_unlink(name);
}

static int cxl_mhsld_state_create(const char *filename, size_t size)
{
    int fd, rc;

    fd = cxl_mhsld_state_open(filename, O_RDWR | O_CREAT);
    if (fd == -1) {
        return -1;
    }

    rc = ftruncate(fd, size);

    if (rc) {
        close(fd);
        return -1;
    }

    return fd;
}

static bool cxl_mhsld_state_set(CXLMHSLDState *s, size_t block_start,
                                size_t block_count)
{
    uint8_t prev, val, *block;
    size_t i;

    val = (1 << s->mhd_head);

    /*
     * Try to claim all extents from start -> start + count;
     * break early if a claimed extent is encountered
     */
    for (i = 0; i < block_count; ++i) {
        block = &s->mhd_state->blocks[block_start + i];
        prev = __sync_val_compare_and_swap(block, 0, val);
        if (prev != 0) {
            break;
        }
    }

    if (prev == 0) {
        return true;
    }

    /* Roll back incomplete claims */
    for (;; --i) {
        block = &s->mhd_state->blocks[block_start + i];
        __sync_fetch_and_and(block, ~(1u << s->mhd_head));
        if (i == 0) {
            break;
        }
    }

    return false;
}

static void cxl_mhsld_state_clear(CXLMHSLDState *s, size_t block_start,
                                  size_t block_count)
{
    size_t i;
    uint8_t *block;

    for (i = 0; i < block_count; ++i) {
        block = &s->mhd_state->blocks[block_start + i];
        __sync_fetch_and_and(block, ~(1u << s->mhd_head));
    }
}

static void cxl_mhsld_state_initialize(CXLMHSLDState *s, size_t dc_size)
{
    if (!s->mhd_init) {
        cxl_mhsld_state_clear(s, 0, dc_size / MHSLD_BLOCK_SZ);
        return;
    }

    memset(s->mhd_state, 0, s->mhd_state_size);
    s->mhd_state->nr_heads = MHSLD_HEADS;
    s->mhd_state->nr_lds = MHSLD_HEADS;
    s->mhd_state->nr_blocks = dc_size / MHSLD_BLOCK_SZ;
}

/* Returns starting index of region in MHD map. */
/*static inline size_t cxl_mhsld_find_dc_region_start(PCIDevice *d,
                                                    CXLDCRegion *r)
{
    CXLType3Dev *dcd = CXL_TYPE3(d);
    size_t start = 0;
    uint8_t rid;

    for (rid = 0; rid < dcd->dc.num_regions; ++rid) {
        if (&dcd->dc.regions[rid] == r) {
            break;
        }
        start += dcd->dc.regions[rid].len / dcd->dc.regions[rid].block_size;
    }

    return start;
}*/

static MHSLDSharedState *cxl_mhsld_state_map(CXLMHSLDState *s)
{
    void *map;
    size_t size = s->mhd_state_size;
    int fd = s->mhd_state_fd;

    if (fd < 0) {
        return NULL;
    }

    map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        return NULL;
    }

    return (MHSLDSharedState *)map;
}

/*
 * Triggered during an add_capacity command to a CXL device:
 * takes a list of extent records and preallocates them,
 * in anticipation of a "dcd accept" response from the host.
 *
 * Extents that are not accepted by the host will be rolled
 * back later.
 */
static bool cxl_mhsld_reserve_extents(PCIDevice *d,
                                      CxlDynamicCapacityExtentList *records,
                                      uint8_t rid)
{
    uint64_t len, dpa;
    bool rc;

    CXLMHSLDState *s = CXL_MHSLD(d);
    CxlDynamicCapacityExtentList *list = records, *rollback = NULL;

    CXLType3Dev *ct3d = CXL_TYPE3(d);
    CXLDCRegion *region = &ct3d->dc.regions[rid];

    for (; list; list = list->next) {
        len = list->value->len / MHSLD_BLOCK_SZ;
        dpa = (list->value->offset + region->base) / MHSLD_BLOCK_SZ;

        rc = cxl_mhsld_state_set(s, dpa, len);

        if (!rc) {
            rollback = records;
            break;
        }
    }

    /* Setting the mhd state failed. Roll back the extents that were added */
    for (; rollback; rollback = rollback->next) {
        len = rollback->value->len / MHSLD_BLOCK_SZ;
        dpa = (list->value->offset + region->base) / MHSLD_BLOCK_SZ;

        cxl_mhsld_state_clear(s, dpa, len);

        if (rollback == list) {
            return false;
        }
    }

    return true;
}

static bool cxl_mhsld_reclaim_extents(PCIDevice *d,
                                      CXLDCExtentGroupList *ext_groups,
                                      CXLUpdateDCExtentListInPl *in)
{
    CXLMHSLDState *s = CXL_MHSLD(d);
    CXLType3Dev *ct3d = CXL_TYPE3(d);
    CXLDCExtentGroup *ext_group = QTAILQ_FIRST(ext_groups);
    CXLDCExtent *ent;
    CXLDCRegion *region;
    g_autofree unsigned long *blk_bitmap = NULL;
    uint64_t dpa, off, len, size, i;

    /* Get the DCD region via the first requested extent */
    ent = QTAILQ_FIRST(&ext_group->list);
    dpa = ent->start_dpa;
    len = ent->len;
    region = cxl_find_dc_region(ct3d, dpa, len);
    size = region->len / MHSLD_BLOCK_SZ;
    blk_bitmap = bitmap_new(size);

    /* Set all requested extents to 1 in a bitmap */
    QTAILQ_FOREACH(ent, &ext_group->list, node) {
        off = ent->start_dpa - region->base;
        len = ent->len;
        bitmap_set(blk_bitmap, off / MHSLD_BLOCK_SZ, len / MHSLD_BLOCK_SZ);
    }

    /* Clear bits associated with accepted extents */
    for (i = 0; i < in->num_entries_updated; i++) {
        off = in->updated_entries[i].start_dpa - region->base;
        len = in->updated_entries[i].len;
        bitmap_clear(blk_bitmap, off / MHSLD_BLOCK_SZ, len / MHSLD_BLOCK_SZ);
    }

    /*
     * Reclaim only the extents that belong to unaccepted extents,
     * i.e. those whose bits are still raised in blk_bitmap
     */
    for (off = find_first_bit(blk_bitmap, size); off < size;) {
        len = find_next_zero_bit(blk_bitmap, size, off) - off;
        cxl_mhsld_state_clear(s, off, len);
        off = find_next_bit(blk_bitmap, size, off + len);
    }

    return true;
}

static bool cxl_mhsld_release_extent(PCIDevice *d, uint64_t dpa, uint64_t len)
{
    cxl_mhsld_state_clear(CXL_MHSLD(d), dpa / MHSLD_BLOCK_SZ,
        len / MHSLD_BLOCK_SZ);
    return true;
}

static bool cxl_mhsld_access_valid(PCIDevice *d, uint64_t addr,
                                   unsigned int size)
{
    CXLType3Dev *ct3d = CXL_TYPE3(d);
    CXLMHSLDState *s = CXL_MHSLD(d);
    CXLDCRegion *r = cxl_find_dc_region(ct3d, addr, size);
    size_t i;

    addr = addr / r->block_size;
    size = size / r->block_size;

    for (i = 0; i < size; ++i) {
        if (s->mhd_state->blocks[addr + i] != (1 << s->mhd_head)) {
            return false;
        }
    }

    return true;
}

static void cxl_mhsld_realize(PCIDevice *pci_dev, Error **errp)
{
    CXLMHSLDState *s = CXL_MHSLD(pci_dev);
    MemoryRegion *mr;
    int fd = -1;
    size_t dc_size;

    ct3_realize(pci_dev, errp);

    /* Get number of blocks from dcd size */
    mr = host_memory_backend_get_memory(s->ct3d.dc.host_dc);
    if (!mr) {
        return;
    }
    dc_size = memory_region_size(mr);
    if (!dc_size) {
        error_setg(errp, "MHSLD does not have dynamic capacity to manage");
        return;
    }

    s->mhd_state_size = (dc_size / MHSLD_BLOCK_SZ) + sizeof(MHSLDSharedState);

    /* Sanity check the head idx */
    if (s->mhd_head >= MHSLD_HEADS) {
        error_setg(errp, "MHD Head ID must be between 0-7");
        return;
    }

    /* Create the state file if this is the 'mhd_init' instance */
    if (s->mhd_init) {
        fd = cxl_mhsld_state_create(s->mhd_state_file, s->mhd_state_size);
    } else {
        fd = cxl_mhsld_state_open(s->mhd_state_file, O_RDWR);
    }

    if (fd < 0) {
        error_setg(errp, "failed to open mhsld state errno %d", errno);
        return;
    }

    s->mhd_state_fd = fd;

    /* Map the state and initialize it as needed */
    s->mhd_state = cxl_mhsld_state_map(s);
    if (!s->mhd_state) {
        error_setg(errp, "Failed to mmap mhd state file");
        close(fd);
        cxl_mhsld_state_unlink(s->mhd_state_file);
        return;
    }

    cxl_mhsld_state_initialize(s, dc_size);

    /* Set the LD ownership for this head to this system */
    s->mhd_state->ldmap[s->mhd_head] = s->mhd_head;
    return;
}


static void cxl_mhsld_exit(PCIDevice *pci_dev)
{
    CXLMHSLDState *s = CXL_MHSLD(pci_dev);

    ct3_exit(pci_dev);

    if (s->mhd_state_fd) {
        munmap(s->mhd_state, s->mhd_state_size);
        close(s->mhd_state_fd);
        cxl_mhsld_state_unlink(s->mhd_state_file);
        s->mhd_state = NULL;
    }
}

static void cxl_mhsld_reset(DeviceState *d)
{
    CXLMHSLDState *s = CXL_MHSLD(d);

    ct3d_reset(d);
    cxl_add_cci_commands(&s->ct3d.cci, cxl_cmd_set_mhsld, 512);

    cxl_mhsld_state_clear(s, 0, s->mhd_state->nr_blocks);
}

/*
 * Example: DCD-add events need to validate that the requested extent
 *          does not already have a mapping (or, if it does, it is
 *          a shared extent with the right tagging).
 *
 * Since this operates on the shared state, we will need to serialize
 * these callbacks across QEMU instances via a mutex in shared state.
 */

static void cxl_mhsld_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->realize = cxl_mhsld_realize;
    pc->exit = cxl_mhsld_exit;
    device_class_set_legacy_reset(dc, cxl_mhsld_reset);
    device_class_set_props(dc, cxl_mhsld_props);

    CXLType3Class *cvc = CXL_TYPE3_CLASS(klass);
    cvc->mhd_get_info = cmd_mhd_get_info;
    cvc->mhd_access_valid = cxl_mhsld_access_valid;
    cvc->mhd_reserve_extents = cxl_mhsld_reserve_extents;
    cvc->mhd_reclaim_extents = cxl_mhsld_reclaim_extents;
    cvc->mhd_release_extent = cxl_mhsld_release_extent;
}

static const TypeInfo cxl_mhsld_info = {
    .name = TYPE_CXL_MHSLD,
    .parent = TYPE_CXL_TYPE3,
    .class_size = sizeof(struct CXLMHSLDClass),
    .class_init = cxl_mhsld_class_init,
    .instance_size = sizeof(CXLMHSLDState),
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CXL_DEVICE },
        { INTERFACE_PCIE_DEVICE },
        {}
    },
};

static void cxl_mhsld_register_types(void)
{
    type_register_static(&cxl_mhsld_info);
}

type_init(cxl_mhsld_register_types)
