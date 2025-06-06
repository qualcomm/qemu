use qemu_api::c_str;

pub mod device;
pub mod device_class;
pub mod registers;

pub const TYPE_QCOM_GPU: &::std::ffi::CStr = c_str!("qcom_gpu");