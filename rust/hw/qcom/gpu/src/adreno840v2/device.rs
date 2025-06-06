use std::{ffi::CStr, ptr::addr_of_mut};

use qemu_api::{irq::InterruptSource, memory::{hwaddr, MemoryRegion, MemoryRegionOps, MemoryRegionOpsBuilder}, qdev::{DeviceImpl, DeviceState, ResettablePhasesImpl}, qom::{Object, ObjectImpl, ObjectType, ParentField}, qom_isa, sysbus::{SysBusDevice, SysBusDeviceImpl, SysBusDeviceMethods}};

#[repr(C)]
#[derive(qemu_api_macros::Object, qemu_api_macros::offsets)]
/// The GPU
pub struct AdrenoGpuState {
    pub parent_obj: ParentField<SysBusDevice>,
    pub mmios: [MemoryRegion; 4],
    pub interrupts: [InterruptSource; 2],
}

pub struct AdrenoGpuClass {
    parent_class: <SysBusDevice as ObjectType>::Class,
}

qom_isa!(AdrenoGpuState: SysBusDevice, DeviceState, Object);

unsafe impl ObjectType for AdrenoGpuState {
    type Class = AdrenoGpuClass;

    const TYPE_NAME: &'static CStr = super::TYPE_QCOM_GPU;
}

impl AdrenoGpuClass {
    fn class_init(&mut self) {
        self.parent_class.class_init::<AdrenoGpuState>();
    }
}


impl ObjectImpl for AdrenoGpuState {
    type ParentType = SysBusDevice;

    const INSTANCE_INIT: Option<unsafe fn(&mut Self)> = Some(Self::init);
    const INSTANCE_POST_INIT: Option<fn(&Self)> = Some(Self::post_init);
    const CLASS_INIT: fn(&mut Self::Class) = Self::Class::class_init;
}

impl DeviceImpl for AdrenoGpuState {
    const REALIZE: Option<fn(&Self)> = Some(Self::realize);

    fn properties() -> &'static [qemu_api::qdev::Property] {
        &[]
    }

    fn vmsd() -> Option<&'static qemu_api::vmstate::VMStateDescription> {
        None
    }
}

impl ResettablePhasesImpl for AdrenoGpuState {
    const ENTER: Option<fn(&Self, qemu_api::qdev::ResetType)> = None;

    const HOLD: Option<fn(&Self, qemu_api::qdev::ResetType)> = None;

    const EXIT: Option<fn(&Self, qemu_api::qdev::ResetType)> = None;
}

impl SysBusDeviceImpl for AdrenoGpuState {}

impl AdrenoGpuState {
    unsafe fn init(&mut self) {
        static GPU_OPS_KGSL: MemoryRegionOps<AdrenoGpuState> = MemoryRegionOpsBuilder::<AdrenoGpuState>::new()
            .read(&AdrenoGpuState::read_kgsl)
            .write(&AdrenoGpuState::write_kgsl)
            .native_endian()
            .impl_sizes(4, 4)
            .build();
        MemoryRegion::init_io(
            unsafe { &mut *addr_of_mut!(self.mmios[0]) },
            addr_of_mut!(*self),
            &GPU_OPS_KGSL,
            "gpu_kgsl_3d0",
            0x40000,
        );

        static GPU_OPS_RSCC: MemoryRegionOps<AdrenoGpuState> = MemoryRegionOpsBuilder::<AdrenoGpuState>::new()
            .read(&AdrenoGpuState::read_rscc)
            .write(&AdrenoGpuState::write_rscc)
            .native_endian()
            .impl_sizes(4, 4)
            .build();
        MemoryRegion::init_io(
            unsafe { &mut *addr_of_mut!(self.mmios[1]) },
            addr_of_mut!(*self),
            &GPU_OPS_RSCC,
            "gpu_rscc",
            0x10000,
        );

        static GPU_OPS_DBGC: MemoryRegionOps<AdrenoGpuState> = MemoryRegionOpsBuilder::<AdrenoGpuState>::new()
            .read(&AdrenoGpuState::read_cx_dbgc)
            .write(&AdrenoGpuState::write_cx_dbgc)
            .native_endian()
            .impl_sizes(4, 4)
            .build();
        MemoryRegion::init_io(
            unsafe { &mut *addr_of_mut!(self.mmios[2]) },
            addr_of_mut!(*self),
            &GPU_OPS_DBGC,
            "gpu_cx_dbgc",
            0x3000,
        );

        static GPU_OPS_MISC: MemoryRegionOps<AdrenoGpuState> = MemoryRegionOpsBuilder::<AdrenoGpuState>::new()
            .read(&AdrenoGpuState::read_cx_misc)
            .write(&AdrenoGpuState::write_cx_misc)
            .native_endian()
            .impl_sizes(4, 4)
            .build();
        MemoryRegion::init_io(
            unsafe { &mut *addr_of_mut!(self.mmios[3]) },
            addr_of_mut!(*self),
            &GPU_OPS_MISC,
            "gpu_cx_misc",
            0x2000,
        );
    }

    fn post_init(&self) {
        for mmio in &self.mmios {
            self.init_mmio(mmio);
        }

        for irq in &self.interrupts {
            self.init_irq(irq);
        }
    }

    fn realize(&self) {}

    fn read_kgsl(&self, _offset: hwaddr, _size: u32) -> u64 {
        eprintln!("Adreno GPU KGSL Read");
        0
    }

    fn write_kgsl(&self, _offset: hwaddr, _value: u64, _size: u32) {
        eprintln!("Adreno GPU KGSL Write");
    }

    fn read_rscc(&self, _offset: hwaddr, _size: u32) -> u64 {
        eprintln!("Adreno GPU RSCC Read");
        0
    }

    fn write_rscc(&self, _offset: hwaddr, _value: u64, _size: u32) {
        eprintln!("Adreno GPU RSCC Write");
    }

    fn read_cx_dbgc(&self, _offset: hwaddr, _size: u32) -> u64 {
        eprintln!("Adreno GPU DBGC Read");
        0
    }

    fn write_cx_dbgc(&self, _offset: hwaddr, _value: u64, _size: u32) {
        eprintln!("Adreno GPU DBGC Write");
    }

    fn read_cx_misc(&self, _offset: hwaddr, _size: u32) -> u64 {
        eprintln!("Adreno GPU misc Read");
        0
    }

    fn write_cx_misc(&self, _offset: hwaddr, _value: u64, _size: u32) {
        eprintln!("Adreno GPU misc Write");
    }
}