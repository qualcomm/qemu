/*
 * Stubs for target info QOM (user mode only runs a single target)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/target-info.h"
#include "qemu/target-info-qom.h"

size_t target_info_qom_num_targets(void)
{
    return 1;
}

const TargetInfo *target_info_qom_get_target(size_t idx)
{
    g_assert(idx == 0);
    return target_info();
}

bool target_heterogeneous(void)
{
    return false;
}
