/*
 * QEMU Hypervisor.framework (HVF) support
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

/* header to be included in HVF-specific code */

#ifndef HVF_INT_H
#define HVF_INT_H

#include "qemu/queue.h"
#include "exec/vaddr.h"
#include "system/memory.h"
#include "gdbstub/enums.h"
#include "qom/object.h"
#include "accel/accel-ops.h"
#include "exec/hwaddr.h"

#ifdef __aarch64__
#include <Hypervisor/Hypervisor.h>
typedef hv_vcpu_t hvf_vcpuid;
#else
#include <Hypervisor/hv.h>
typedef hv_vcpuid_t hvf_vcpuid;
#endif

typedef struct hvf_vcpu_caps {
    uint64_t vmx_cap_pinbased;
    uint64_t vmx_cap_procbased;
    uint64_t vmx_cap_procbased2;
    uint64_t vmx_cap_entry;
    uint64_t vmx_cap_exit;
    uint64_t vmx_cap_preemption_timer;
} hvf_vcpu_caps;

struct HVFState {
    AccelState parent_obj;

    hvf_vcpu_caps *hvf_caps;
    uint64_t vtimer_offset;
    QTAILQ_HEAD(, hvf_sw_breakpoint) hvf_sw_breakpoints;
};
extern HVFState *hvf_state;

struct AccelCPUState {
    hvf_vcpuid fd;
#ifdef __aarch64__
    hv_vcpu_exit_t *exit;
    bool vtimer_masked;
    bool guest_debug_enabled;
    struct QEMUTimer *wfi_timer;
#endif
};

void assert_hvf_ok_impl(hv_return_t ret, const char *file, unsigned int line,
                        const char *exp);
#define assert_hvf_ok(EX) assert_hvf_ok_impl((EX), __FILE__, __LINE__, #EX)
const char *hvf_return_string(hv_return_t ret);
int hvf_arch_init(void);
hv_return_t hvf_arch_vm_create(MachineState *ms, uint32_t pa_range);
uint32_t hvf_arch_get_default_ipa_bit_size(void);
uint32_t hvf_arch_get_max_ipa_bit_size(void);
void hvf_kick_vcpu_thread(CPUState *cpu);

/* Must be called by the owning thread */
int hvf_arch_init_vcpu(CPUState *cpu);
/* Must be called by the owning thread */
void hvf_arch_vcpu_destroy(CPUState *cpu);
/* Must be called by the owning thread */
int hvf_arch_vcpu_exec(CPUState *);
/* Must be called by the owning thread */
int hvf_arch_put_registers(CPUState *);
/* Must be called by the owning thread */
int hvf_arch_get_registers(CPUState *);
/* Must be called by the owning thread */
void hvf_arch_update_guest_debug(CPUState *cpu);

void hvf_protect_clean_range(hwaddr addr, size_t size);
void hvf_unprotect_dirty_range(hwaddr addr, size_t size);
/* caller must hold the BQL */
bool hvf_gpa_page_is_mapped(uint64_t gpa);

struct hvf_sw_breakpoint {
    vaddr pc;
    vaddr saved_insn;
    int use_count;
    QTAILQ_ENTRY(hvf_sw_breakpoint) entry;
};

struct hvf_sw_breakpoint *hvf_find_sw_breakpoint(CPUState *cpu,
                                                 vaddr pc);
int hvf_sw_breakpoints_active(CPUState *cpu);

int hvf_arch_insert_sw_breakpoint(CPUState *cpu, struct hvf_sw_breakpoint *bp);
int hvf_arch_remove_sw_breakpoint(CPUState *cpu, struct hvf_sw_breakpoint *bp);
int hvf_arch_insert_gdbstub_hw_breakpoint(vaddr addr, vaddr len,
                                          GdbBreakpointType type);
int hvf_arch_remove_gdbstub_hw_breakpoint(vaddr addr, vaddr len,
                                          GdbBreakpointType type);
void hvf_arch_remove_all_gdbstub_hw_breakpoints(void);

/* Local-only HVF race and unmapped-RAM diagnostic hooks. */
#define HVF_TEST_TARGET_PAGE UINT64_C(0x40200000)

void hvf_test_init(void);
bool hvf_test_pa_matches(uint64_t pa);
void hvf_test_register_qmp_commands(void);
bool hvf_test_target_page_overlaps(hwaddr start, uint64_t size);
void hvf_test_trace_set_phys_mem(MemoryListener *listener,
                                 MemoryRegionSection *section, bool add,
                                 uint64_t transaction, const char *action,
                                 int ret);
void hvf_test_trace_log(MemoryListener *listener, MemoryRegionSection *section,
                        uint64_t transaction, int old, int new,
                        const char *action);
void hvf_test_trace_map(hwaddr start, uint64_t size, const char *action,
                        const char *name);
void hvf_test_trace_protect(hwaddr start, uint64_t size,
                            hv_memory_flags_t flags, const char *action);
bool hvf_test_record_abort(CPUState *cpu, uint64_t pc, uint64_t va,
                           uint64_t pa, uint64_t syndrome, uint32_t dfsc,
                           AddressSpace *as, MemoryRegion *mr,
                           bool mapping_known, bool mapped, hwaddr xlat,
                           uint8_t dirty_mask, const char *action);
bool hvf_test_gate(CPUState *cpu, bool matching, uint64_t pc, uint64_t pa);
void hvf_test_mark_done(void);

/*
 * hvf_update_guest_debug:
 * @cs: CPUState for the CPU to update
 *
 * Update guest to enable or disable debugging. Per-arch specifics will be
 * handled by calling down to hvf_arch_update_guest_debug.
 */
void hvf_update_guest_debug(CPUState *cpu);

bool hvf_arch_cpu_realize(CPUState *cpu, Error **errp);
uint32_t hvf_arch_get_default_ipa_bit_size(void);
uint32_t hvf_arch_get_max_ipa_bit_size(void);

#endif
