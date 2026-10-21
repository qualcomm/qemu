/*
 * Stubs for kvm spapr helpers
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"

#include "hw/vfio/kvm-spapr.h"

bool vfio_spapr_kvm_attach_tce(VFIOContainer *bcontainer,
                               MemoryRegionSection *section,
                               Error **errp)
{
    g_assert_not_reached();
}
