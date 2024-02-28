/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include "qemu/osdep.h"
#include "migration/cpu.h"
#include "cpu.h"
#include "hex_mmu.h"

static int get_u64_ptr(QEMUFile *f, void *pv, size_t size,
                       const VMStateField *field)
{
    uint64_t *p = pv;
    *p = qemu_get_be64(f);
    return 0;
}

static int put_u64_ptr(QEMUFile *f, void *pv, size_t size,
                      const VMStateField *field, JSONWriter *vmdesc)
{
    qemu_put_be64(f, *((uint64_t *)pv));
    return 0;
}

const VMStateInfo vmstate_info_uint64_ptr = {
    .name = "uint64_t_pointer",
    .get  = get_u64_ptr,
    .put  = put_u64_ptr,
};

static int get_mmvector(QEMUFile *f, void *pv, size_t size,
                        const VMStateField *field)
{
    MMVector *v = pv;
    for (int i = 0; i < MAX_VEC_SIZE_BYTES / 8; i++) {
        v->ud[i] = qemu_get_be64(f);
    }
    return 0;
}

static int put_mmvector(QEMUFile *f, void *pv, size_t size,
                        const VMStateField *field, JSONWriter *vmdesc)
{
    MMVector *v = pv;
    for (int i = 0; i < MAX_VEC_SIZE_BYTES / 8; i++) {
        qemu_put_be64(f, v->ud[i]);
    }
    return 0;
}

static const VMStateInfo vmstate_info_mmvector = {
    .name = "mmvector",
    .get  = get_mmvector,
    .put  = put_mmvector,
};

#define VMSTATE_MMVECTOR_ARRAY(_f, _s, _n) \
    VMSTATE_SUB_ARRAY(_f, _s, 0, _n, 0, vmstate_info_mmvector, MMVector)

static int get_mmqreg(QEMUFile *f, void *pv, size_t size,
                      const VMStateField *field)
{
    MMQReg *v = pv;
    for (int i = 0; i < MAX_VEC_SIZE_BYTES / 8 / 8; i++) {
        v->ud[i] = qemu_get_be64(f);
    }
    return 0;
}

static int put_mmqreg(QEMUFile *f, void *pv, size_t size,
                      const VMStateField *field, JSONWriter *vmdesc)
{
    MMQReg *v = pv;
    for (int i = 0; i < MAX_VEC_SIZE_BYTES / 8 / 8; i++) {
        qemu_put_be64(f, v->ud[i]);
    }
    return 0;
}

static const VMStateInfo vmstate_info_mmqreg = {
    .name = "mmqreg",
    .get  = get_mmqreg,
    .put  = put_mmqreg,
};

#define VMSTATE_MMQREG_ARRAY(_f, _s, _n) \
    VMSTATE_SUB_ARRAY(_f, _s, 0, _n, 0, vmstate_info_mmqreg, MMQReg)

static int get_hex_tlb_ptr(QEMUFile *f, void *pv, size_t size,
                       const VMStateField *field)
{
    CPUHexagonTLBContext *tlb = pv;
    for (int i = 0; i < NUM_TLB_ENTRIES; i++) {
        tlb->entries[i] = qemu_get_be64(f);
    }
    return 0;
}

static int put_hex_tlb_ptr(QEMUFile *f, void *pv, size_t size,
                      const VMStateField *field, JSONWriter *vmdesc)
{
    CPUHexagonTLBContext *tlb = pv;
    for (int i = 0; i < NUM_TLB_ENTRIES; i++) {
        qemu_put_be64(f,  tlb->entries[i]);
    }
    return 0;
}

const VMStateInfo vmstate_info_hex_tlb_ptr = {
    .name = "hex_tlb_pointer",
    .get  = get_hex_tlb_ptr,
    .put  = put_hex_tlb_ptr,
};

const VMStateDescription vmstate_pmustate = {
    .name = "pmu_state",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(num_packets, PMUState),
        VMSTATE_UINT32(hvx_packets, PMUState),
        VMSTATE_VARRAY_UINT32(g_ctrs_off, PMUState, vmstate_num_ctrs, 0,
                              vmstate_info_uint32, uint32_t),
        VMSTATE_VARRAY_UINT32(g_events, PMUState, vmstate_num_ctrs, 0,
                              vmstate_info_uint16, uint16_t),
        VMSTATE_END_OF_LIST()
    }
};

const VMStateDescription vmstate_hex_exception_info = {
    .name = "hex_exception_info",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(valid, hex_exception_info),
        VMSTATE_UINT8(type, hex_exception_info),
        VMSTATE_UINT8(cause, hex_exception_info),
        VMSTATE_UINT8(bvs, hex_exception_info),
        VMSTATE_UINT8(bv0, hex_exception_info),
        VMSTATE_UINT8(bv1, hex_exception_info),
        VMSTATE_UINT32(badva0, hex_exception_info),
        VMSTATE_UINT32(badva1, hex_exception_info),
        VMSTATE_UINT32(elr, hex_exception_info),
        VMSTATE_UINT16(diag, hex_exception_info),
        VMSTATE_UINT16(de_slotmask, hex_exception_info),
        VMSTATE_END_OF_LIST()
    }
};

const VMStateDescription vmstate_mem_log = {
    .name = "mem_log",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(va, MemLog),
        VMSTATE_UINT8(width, MemLog),
        VMSTATE_UINT32(data32, MemLog),
        VMSTATE_UINT64(data64, MemLog),
        VMSTATE_END_OF_LIST()
    }
};

const VMStateDescription vmstate_hexagon_cpu = {
    .name = "cpu",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {

        VMSTATE_UINTTL_ARRAY(env.gpr, HexagonCPU, TOTAL_PER_THREAD_REGS),
        VMSTATE_UINTTL_ARRAY(env.pred, HexagonCPU, NUM_PREGS),
        VMSTATE_UINTTL_ARRAY(env.vstore_pending, HexagonCPU, VSTORES_MAX),
        VMSTATE_UINTTL_ARRAY(env.t_sreg, HexagonCPU, NUM_SREGS),
        VMSTATE_UINTTL_ARRAY(env.t_sreg_written, HexagonCPU, NUM_SREGS),
        VMSTATE_UINTTL_ARRAY(env.greg, HexagonCPU, NUM_GREGS),
        VMSTATE_UINTTL_ARRAY(env.greg_written, HexagonCPU, NUM_GREGS),
        VMSTATE_UINTTL_ARRAY(env.reg_written, HexagonCPU, TOTAL_PER_THREAD_REGS),

        VMSTATE_VARRAY_UINT32(env.g_sreg, HexagonCPU, vmstate_num_g_sreg, 0,
                              vmstate_info_uint32, uint32_t),
        VMSTATE_VARRAY_UINT32(env.g_gcycle, HexagonCPU, vmstate_num_g_gcycle, 0,
                              vmstate_info_uint32, uint32_t),

        VMSTATE_UINTTL(env.cause_code, HexagonCPU),
        VMSTATE_UINTTL(env.last_pc_dumped, HexagonCPU),
        VMSTATE_UINTTL(env.stack_start, HexagonCPU),
        VMSTATE_UINTTL(env.wait_next_pc, HexagonCPU),
        VMSTATE_UINTTL(env.new_value_usr, HexagonCPU),
        VMSTATE_UINTTL(env.llsc_addr, HexagonCPU),
        VMSTATE_UINTTL(env.llsc_val, HexagonCPU),
        VMSTATE_UINTTL(env.imprecise_exception, HexagonCPU),

        VMSTATE_UINT8(env.bq_on, HexagonCPU),
        VMSTATE_UINT8(env.slot_cancelled, HexagonCPU),

        VMSTATE_UINT16(env.nmi_threads, HexagonCPU),

        VMSTATE_UINT32(env.last_cpu, HexagonCPU),
        VMSTATE_UINT32(env.exe_arch, HexagonCPU),
        VMSTATE_UINT32(env.timing_on, HexagonCPU),
        VMSTATE_UINT32(env.threadId, HexagonCPU),
        VMSTATE_UINT32(env.tlb_lock_state, HexagonCPU),
        VMSTATE_UINT32(env.k0_lock_state, HexagonCPU),
        VMSTATE_INT32(env.k0_lock_count, HexagonCPU),
        VMSTATE_INT32(env.tlb_lock_count, HexagonCPU),
        VMSTATE_UINT32(env.systemstate, HexagonCPU),
        VMSTATE_UINT32(env.VRegs_updated_tmp, HexagonCPU),

        VMSTATE_INT32(env.status, HexagonCPU),
        VMSTATE_INT32(env.shm_fd, HexagonCPU),
        VMSTATE_INT32(env.slot, HexagonCPU),

        VMSTATE_UINT64(env.llsc_val_i64, HexagonCPU),
        VMSTATE_UINT64(env.t_cycle_count, HexagonCPU),
        VMSTATE_UINT64(env.pktid, HexagonCPU),

        VMSTATE_BOOL(env.vtcm_pending, HexagonCPU),
        VMSTATE_BOOL(env.ss_pending, HexagonCPU),
        VMSTATE_UINTTL(env.next_PC, HexagonCPU),

        VMSTATE_POINTER(env.g_pcycle_base, HexagonCPU, 0,
                        vmstate_info_uint64_ptr, uint64_t *),

        VMSTATE_MMVECTOR_ARRAY(env.VRegs, HexagonCPU, NUM_VREGS),
        VMSTATE_MMVECTOR_ARRAY(env.future_VRegs, HexagonCPU, VECTOR_TEMPS_MAX),
        VMSTATE_MMVECTOR_ARRAY(env.tmp_VRegs, HexagonCPU, VECTOR_TEMPS_MAX),

        VMSTATE_ARRAY(env.vtmp.ud, HexagonCPU, MAX_VEC_SIZE_BYTES / 8, 0,
                      vmstate_info_uint64, uint64_t),

        VMSTATE_MMQREG_ARRAY(env.QRegs, HexagonCPU, NUM_QREGS),
        VMSTATE_MMQREG_ARRAY(env.future_QRegs, HexagonCPU, NUM_QREGS),

        VMSTATE_POINTER(env.hex_tlb, HexagonCPU, 0,
                        vmstate_info_hex_tlb_ptr, CPUHexagonTLBContext *),

        VMSTATE_STRUCT(env.pmu, HexagonCPU, 0, vmstate_pmustate, PMUState),
        VMSTATE_STRUCT(env.einfo, HexagonCPU, 0, vmstate_hex_exception_info,
                       hex_exception_info),
        VMSTATE_STRUCT_ARRAY(env.mem_log_stores, HexagonCPU, STORES_MAX, 0,
                             vmstate_mem_log, MemLog),

        VMSTATE_END_OF_LIST()
    },
};
