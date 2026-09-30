#ifndef HW_INTC_RISCV_CLIC_H
#define HW_INTC_RISCV_CLIC_H
#include "hw/core/sysbus.h"
#define TYPE_RISCV_CLIC "riscv.clic"
OBJECT_DECLARE_SIMPLE_TYPE(RiscvCLICState, RISCV_CLIC)
enum { RISCV_CLIC_MAX_SOURCES = 1024, RISCV_CLIC_MMIO_SIZE = 0x1000000,
       RISCV_CLICINTIP_OFFSET = 0x800000, RISCV_CLICINTIE_OFFSET = 0x800400,
       RISCV_CLICINTCFG_OFFSET = 0x800800, RISCV_CLICCFG_OFFSET = 0x800c00,
       RISCV_CLIC_CONTROL_OFFSET = 0x7ff000 };
struct RiscvCLICState { SysBusDevice parent_obj; MemoryRegion mmio;
    uint32_t num_sources, aperture_size, cliccfg, control;
    uint8_t pending[RISCV_CLIC_MAX_SOURCES], enable[RISCV_CLIC_MAX_SOURCES], config[RISCV_CLIC_MAX_SOURCES];
    qemu_irq output; };
#endif
