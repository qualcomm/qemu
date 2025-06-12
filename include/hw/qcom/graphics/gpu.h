#pragma once

#include "hw/sysbus.h"
#include "qom/object.h"

#include "kgsl.h"
#include "rscc.h"
#include "cx_dbgc.h"
#include "cx_misc.h"

#define TYPE_QCOM_GPU "qcom_gpu"
OBJECT_DECLARE_SIMPLE_TYPE(QcomGpuState, QCOM_GPU)

#define QCOM_GPU_MEMORY_BASE 0x3D00000
#define QCOM_GPU_MEMORY_SIZE 0x100000

typedef struct QcomGpuState QcomGpuState;
typedef struct QcomGpuClass QcomGpuClass;

struct QcomGpuState {
    SysBusDevice parent;

    MemoryRegion container;

    KgslState kgsl;
    MemoryRegion kgsl_iomem;

    RsccState rscc;
    MemoryRegion rscc_iomem;

    CxDbgcState cx_dbgc;
    MemoryRegion cx_dbgc_iomem;

    CxMiscState cx_misc;
    MemoryRegion cx_misc_iomem;
};

struct QcomGpuClass {
    /*< private >*/
    SysBusDeviceClass parent_class;

    /*< public >*/
};
