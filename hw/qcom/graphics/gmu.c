#include "qemu/osdep.h"
#include "qapi/error.h"
#include "system/dma.h"
#include "exec/memattrs.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus-of.h"

#include "hw/qcom/smmu.h"
#include "hw/qcom/graphics.h"
#include "hw/qcom/graphics/gen8_reg.h"
#include "hw/qcom/graphics/hfi.h"
#include "hw/qcom/graphics/gmu.h"

#define GX_GDSC_POWER_OFF	BIT(0)
#define GX_CLK_OFF		BIT(1)
#define is_on(val)		(!(val & (GX_GDSC_POWER_OFF | GX_CLK_OFF)))

#define CLK_OFF (GX_GDSC_POWER_OFF | GX_CLK_OFF)


/*
 * OOB requests values. These range from 0 to 7 and then
 * the BIT() offset into the actual value is calculated
 * later based on the request. This keeps the math clean
 * and easy to ensure not reaching over/under the range
 * of 8 bits.
 */
enum oob_request {
	oob_gpu = 0,
	oob_perfcntr = 1,
	oob_boot_slumber = 6, /* reserved special case */
	oob_dcvs = 7, /* reserved special case */
	oob_max,
};

bool qcom_gmu_gpumem_read(QcomGMUState* s, uint32_t vmid, gpuaddr addr, char* buf, gpuaddr size) {
    gpuaddr buf_offset = 0;
    while(size > 0) {
        const DMAMap* map = qcom_smmu_iova2paddr(s->smmu, vmid, addr, 1);

        // printf("\t\tfetching map with vmid %d @addr 0x%lx with size 1: %p\n", vmid, addr, map);

        if (!map) {
            return false;
        }

        gpuaddr to_read = MIN(map->size, size);
        assert(addr >= map->iova);
        hwaddr host_addr = map->translated_addr + (addr - map->iova);
        // printf("\t\tReading @addr 0x%lx 0x%lx bytes\n", host_addr, to_read);
        dma_memory_read(&address_space_memory, host_addr, buf + buf_offset, to_read, MEMTXATTRS_UNSPECIFIED);

        buf_offset += to_read;
        addr += to_read;

        size -= to_read;
    }

    return true;
}

bool qcom_gmu_gpumem_write(QcomGMUState* s, uint32_t vmid, gpuaddr addr, char* buf, gpuaddr size) {
    gpuaddr buf_offset = 0;
    while(size > 0) {
        const DMAMap* map = qcom_smmu_iova2paddr(s->smmu, vmid, addr, 1);

        // printf("\t\tfetching map with vmid %d @addr 0x%lx with size 1: %p\n", vmid, addr, map);

        if (!map) {
            return false;
        }

        gpuaddr to_write = MIN(map->size, size);
        assert(addr >= map->iova);
        hwaddr host_addr = map->translated_addr + (addr - map->iova);
        // printf("\t\tReading @addr 0x%lx 0x%lx bytes\n", host_addr, to_read);
        dma_memory_write(&address_space_memory, host_addr, buf + buf_offset, to_write, MEMTXATTRS_UNSPECIFIED);

        buf_offset += to_write;
        addr += to_write;

        size -= to_write;
    }

    return true;
}

static bool is_rsc_addr(hwaddr addr)
{
    return addr >= 0x50000 && addr < 0x50000 + 0x10000;
}

static uint64_t qcom_global_gmu_read(QcomGMUState* s, hwaddr addr, unsigned size)
{
    OfSysBusDevice* of = OF_SYS_BUS_DEVICE(s);
    hwaddr addr_idx = qcom_graphics_decode_addr(addr);

    printf("[%s] read detected @addr 0x%lx (addr_idx = 0x%lx)\n", of->name, addr, addr_idx);

    if (is_rsc_addr(addr)) {
        printf("Is RSC address\n");

        addr_idx = qcom_graphics_decode_addr(addr - 0x50000);

        switch (addr_idx) {
            case GEN8_RSCC_TCS0_DRV0_STATUS:
            case GEN8_RSCC_TCS1_DRV0_STATUS:
            case GEN8_RSCC_TCS2_DRV0_STATUS:
            case GEN8_RSCC_TCS3_DRV0_STATUS:
            case GEN8_RSCC_TCS4_DRV0_STATUS:
            case GEN8_RSCC_TCS5_DRV0_STATUS:
            case GEN8_RSCC_TCS6_DRV0_STATUS:
            case GEN8_RSCC_TCS7_DRV0_STATUS:
            case GEN8_RSCC_TCS8_DRV0_STATUS:
            case GEN8_RSCC_TCS9_DRV0_STATUS: {
                printf("\tTCS read\n");
                return 1;
            }
            case GEN8_GPU_RSCC_RSC_STATUS0_DRV0: {
                return BIT(30);
            }
            default: {
                printf("\t(RSC) Unknown addr\n");
            }
        }
    } else {
        switch (addr_idx) {
            case GEN8_GMUCX_GFX_PWR_CLK_STATUS: {
                uint32_t read_val = s->regs[addr_idx];
                printf("\tpwr clk status: 0x%x\n", read_val);
                return read_val;
            }
            case GEN8_GMUCX_CM3_FW_INIT_RESULT: {
                /* completely undocumented register */
                return s->regs[GEN8_GMUCX_CM3_FW_INIT_RESULT];
            }
            case GEN8_GMUCX_HFI_CTRL_STATUS: {
                return s->regs[GEN8_GMUCX_HFI_CTRL_STATUS];
            }
            case GEN8_GMUCX_GMU2HOST_INTR_INFO: {
                return s->regs[GEN8_GMUCX_GMU2HOST_INTR_INFO];
            }
            case GEN8_GMUCX_RPMH_POWER_STATE: {
                s->regs[GEN8_GMUCX_GFX_PWR_CLK_STATUS] |= CLK_OFF;
                // return s->regs[GEN8_GMUCX_RPMH_POWER_STATE];
                return GPU_HW_MINBW;
            }
            case GEN8_GMUAO_RSCC_CONTROL_ACK:
                return s->regs[GEN8_GMUAO_RSCC_CONTROL_ACK];
            // set by tz normally
            case GEN8_GPU_CX_MISC_SLICE_ENABLE_TEST: {
                return 7;
            }
            case GEN8_GPU_CX_MISC_SLICE_ENABLE_FINAL: {
                return 7;
            }
            default: {
                printf("\tUnknown addr\n");
            }
        }
    }

    return 0;
}

static void handle_oob_request(QcomGMUState* s, enum oob_request req)
{
    printf("\t\tReceived OOB request: %u\n", req);
}

#define OOB_SET_MASK(req) (BIT(30 - (req) * 2))
#define OOB_CHECK_MASK(req) (BIT(31 - (req)))

static void qcom_global_gmu_write(QcomGMUState* s, hwaddr addr, uint64_t value, unsigned size)
{
    OfSysBusDevice* of = OF_SYS_BUS_DEVICE(s);
    hwaddr addr_idx = qcom_graphics_decode_addr(addr);

    printf("[%s] write detected @addr 0x%lx (addr_idx = 0x%lx) of value 0x%lx\n", of->name, addr, addr_idx, value);

    switch(addr_idx) {
        case GEN8_GMUAO_RSCC_CONTROL_REQ: {
            if (value & BIT(0)) {
                s->regs[GEN8_GMUCX_GFX_PWR_CLK_STATUS] |= CLK_OFF;
                s->regs[GEN8_GMUAO_RSCC_CONTROL_ACK] |= BIT(0);
            }

            if (value & BIT(1)) {
                s->regs[GEN8_GMUCX_GFX_PWR_CLK_STATUS] &= ~CLK_OFF;
                s->regs[GEN8_GMUAO_RSCC_CONTROL_ACK] |= BIT(1);
            }

            printf("\tpwr rscc control req: 0x%lx\n", value);

            break;
        }
        case GEN8_GMUCX_CM3_FW_INIT_RESULT:
            s->regs[GEN8_GMUCX_CM3_FW_INIT_RESULT] = value;
            break;
        case GEN8_GMUCX_HFI_CTRL_INIT: {
            if (value & BIT(0)) {
                s->regs[GEN8_GMUCX_HFI_CTRL_STATUS] = BIT(0);
            }
            break;
        }
        case GEN8_GMUCX_CM3_SYSRESET: {
            uint32_t is_off = GX_GDSC_POWER_OFF | GX_CLK_OFF;

            if (value & BIT(0)) {
                s->regs[GEN8_GMUCX_GFX_PWR_CLK_STATUS] |= is_off;
            } else {
                s->regs[GEN8_GMUCX_GFX_PWR_CLK_STATUS] &= ~is_off;
            }

            s->regs[GEN8_GMUCX_CM3_FW_INIT_RESULT] = BIT(8);

            printf("\tcm3 sysreset\n");

            break;
        }
        case GEN8_GMUCX_HFI_QTBL_INFO:
            s->regs[GEN8_GMUCX_HFI_QTBL_INFO] = value;
            break;
        case GEN8_GMUCX_HFI_QTBL_ADDR:
            s->regs[GEN8_GMUCX_HFI_QTBL_ADDR] = value;
            break;
        case GEN8_GMUCX_HOST2GMU_INTR_SET: {
            printf("\tGMU interruped\n");

            if (value & BIT(0)) {
                qcom_hfi_handle(s);\
            } else if (value & OOB_SET_MASK(oob_gpu)) {
                handle_oob_request(s, oob_gpu);
                s->regs[GEN8_GMUCX_GMU2HOST_INTR_INFO] |= OOB_CHECK_MASK(oob_gpu);
            } else if (value & OOB_SET_MASK(oob_perfcntr)) {
                handle_oob_request(s, oob_perfcntr);
                s->regs[GEN8_GMUCX_GMU2HOST_INTR_INFO] |= OOB_CHECK_MASK(oob_perfcntr);
            } else if (value & OOB_SET_MASK(oob_boot_slumber)) {
                handle_oob_request(s, oob_boot_slumber);
                s->regs[GEN8_GMUCX_GMU2HOST_INTR_INFO] |= OOB_CHECK_MASK(oob_boot_slumber);
            } else if (value & OOB_SET_MASK(oob_dcvs)) {
                handle_oob_request(s, oob_dcvs);
                s->regs[GEN8_GMUCX_GMU2HOST_INTR_INFO] |= OOB_CHECK_MASK(oob_dcvs);
            } else {
                printf("\t\t-> Unknown GMU interrupt value: 0x%lx\n", value);
            }

            break;
        }
        case GEN8_GMUCX_GMU2HOST_INTR_CLR: {
            s->regs[GEN8_GMUCX_GMU2HOST_INTR_INFO] &= ~value;
            break;
        }
        default: {
            printf("\tUnknown addr\n");
            break;
        }
    }
}

static uint64_t qcom_gmu_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomGMUState* s = QCOM_GMU(opaque);

    return qcom_global_gmu_read(s, addr + s->gpu_offset, size);
}

static void qcom_gmu_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomGMUState* s = QCOM_GMU(opaque);

    qcom_global_gmu_write(s, addr + s->gpu_offset, value, size);
}

static uint64_t qcom_gmu_ao_blk_dec0_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomGMUState* s = QCOM_GMU(opaque);

    return qcom_global_gmu_read(s, addr + s->gpu_offset_ao_blk, size);
}

static void qcom_gmu_ao_blk_dec0_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomGMUState* s = QCOM_GMU(opaque);

    qcom_global_gmu_write(s, addr + s->gpu_offset_ao_blk, value, size);
}

static void qcom_gmu_init(Object* obj)
{
}

static const MemoryRegionOps qcom_gmu_ops = {
    .read = qcom_gmu_read,
    .write = qcom_gmu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static const MemoryRegionOps qcom_gmu_ao_blk_dec0_ops = {
    .read = qcom_gmu_ao_blk_dec0_read,
    .write = qcom_gmu_ao_blk_dec0_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_gmu_realize(OfSysBusDevice* of, Error **errp)
{
    QcomGMUState *s = QCOM_GMU(of);
    SysBusDevice* sbd = SYS_BUS_DEVICE(of);

    memory_region_init_io(&s->iomem, OBJECT(of), &qcom_gmu_ops, s, TYPE_QCOM_GMU, of->regs[0].size);
    sysbus_init_mmio(sbd, &s->iomem);

    memory_region_init_io(&s->iomem_ao_blk, OBJECT(of), &qcom_gmu_ao_blk_dec0_ops, s, TYPE_QCOM_GMU "-ao-blk", of->regs[1].size);
    sysbus_init_mmio(sbd, &s->iomem_ao_blk);

    GArray* props = qemu_fdt_collect_phandle_props(of->fdt, of->node_path, errp);
    for (size_t i = 0; i < props->len; ++i) {
        struct fdt_phandle_prop_data* prop_data = ((struct fdt_phandle_prop_data*) props->data) + i;
        if (prop_data->kind == FDT_PROP_IOMMU) {
            assert(!s->smmu);

            s->smmu = QCOM_SMMU(of_sysbus_find_by_phandle(prop_data->phandle));
            s->vmid = ((uint32_t*) prop_data->params->data)[1];
        }
    }
    assert(s->smmu);

    qcom_hfi_init(&s->hfi);
}

static void qcom_gmu_class_init(ObjectClass* oc, void* data)
{
    OfSysBusDeviceClass* klass = OF_SYS_BUS_DEVICE_CLASS(oc);

    klass->realize = qcom_gmu_realize;
}

static const TypeInfo qcom_gmu_info = {
    .name = TYPE_QCOM_GMU,
    .parent = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomGMUState),
    .instance_init = qcom_gmu_init,
    .class_init = qcom_gmu_class_init,
};

static void qcom_gmu_register_types(void)
{
    type_register_static(&qcom_gmu_info);
}

type_init(qcom_gmu_register_types);

