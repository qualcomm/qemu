/*
 * Virtio video device
 *
 * Copyright Red Hat
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * Authors:
 *  Dave Airlie
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/pci/pci.h"
#include "hw/qdev-properties.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-gpu-pci.h"
#include "qom/object.h"

#define TYPE_VIRTIO_GPU_CL_PCI "virtio-gpu-cl-pci"
typedef struct VirtIOGPUCLPCI VirtIOGPUCLPCI;
DECLARE_INSTANCE_CHECKER(VirtIOGPUCLPCI, VIRTIO_GPU_CL_PCI,
                         TYPE_VIRTIO_GPU_CL_PCI)

struct VirtIOGPUCLPCI {
    VirtIOGPUPCIBase parent_obj;
    VirtIOGPUCL vdev;
};

static void virtio_gpu_cl_initfn(Object *obj)
{
    VirtIOGPUCLPCI *dev = VIRTIO_GPU_CL_PCI(obj);

    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev),
                                TYPE_VIRTIO_GPU_CL);
    VIRTIO_GPU_PCI_BASE(obj)->vgpu = VIRTIO_GPU_BASE(&dev->vdev);
}

static const VirtioPCIDeviceTypeInfo virtio_gpu_cl_pci_info = {
    .generic_name = TYPE_VIRTIO_GPU_CL_PCI,
    .parent = TYPE_VIRTIO_GPU_PCI_BASE,
    .instance_size = sizeof(VirtIOGPUCLPCI),
    .instance_init = virtio_gpu_cl_initfn,
};
module_obj(TYPE_VIRTIO_GPU_CL_PCI);
module_kconfig(VIRTIO_PCI);

static void virtio_gpu_cl_pci_register_types(void)
{
    virtio_pci_types_register(&virtio_gpu_cl_pci_info);
}

type_init(virtio_gpu_cl_pci_register_types)

module_dep("hw-display-virtio-gpu-pci");
