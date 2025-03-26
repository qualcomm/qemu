/*
 * Virtio video device
 *
 * Copyright Red Hat
 * Copyright 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#define TYPE_VIRTIO_GPU_QNN_PCI "virtio-gpu-qnn-pci"
typedef struct VirtIOGPUQNNPCI VirtIOGPUQNNPCI;
DECLARE_INSTANCE_CHECKER(VirtIOGPUQNNPCI, VIRTIO_GPU_QNN_PCI,
                         TYPE_VIRTIO_GPU_QNN_PCI)

struct VirtIOGPUQNNPCI {
    VirtIOGPUPCIBase parent_obj;
    VirtIOGPUQNN vdev;
};

static void virtio_gpu_qnn_initfn(Object *obj)
{
    VirtIOGPUQNNPCI *dev = VIRTIO_GPU_QNN_PCI(obj);

    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev),
                                TYPE_VIRTIO_GPU_QNN);
    VIRTIO_GPU_PCI_BASE(obj)->vgpu = VIRTIO_GPU_BASE(&dev->vdev);
}

static const VirtioPCIDeviceTypeInfo virtio_gpu_qnn_pci_info = {
    .generic_name = TYPE_VIRTIO_GPU_QNN_PCI,
    .parent = TYPE_VIRTIO_GPU_PCI_BASE,
    .instance_size = sizeof(VirtIOGPUQNNPCI),
    .instance_init = virtio_gpu_qnn_initfn,
};
module_obj(TYPE_VIRTIO_GPU_QNN_PCI);
module_kconfig(VIRTIO_PCI);

static void virtio_gpu_qnn_pci_register_types(void)
{
    virtio_pci_types_register(&virtio_gpu_qnn_pci_info);
}

type_init(virtio_gpu_qnn_pci_register_types)

module_dep("hw-display-virtio-gpu-pci");
