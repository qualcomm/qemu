/*
 * QEMU target info QOM types
 *
 * Copyright (c) Qualcomm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QEMU_TARGET_INFO_QOM_H
#define QEMU_TARGET_INFO_QOM_H

#include "qemu/target-info-def.h"
#include "qom/object.h"

#define TYPE_TARGET_INFO "target-info"

typedef struct TargetInfoQom {
    Object parent_obj;
} TargetInfoQom;

typedef struct TargetInfoQomClass {
    ObjectClass parent_class;
    const TargetInfo *target_info;
} TargetInfoQomClass;

OBJECT_DECLARE_TYPE(TargetInfoQom, TargetInfoQomClass, TARGET_INFO)

void target_info_qom_set_target(void);
void target_info_qom_set_target_info(const TargetInfo *ti);
void target_info_qom_set_target_from_name(const char *name);
void target_info_qom_set_target_from_argv0(const char *argv0);

/**
 * target_info_qom_num_targets:
 *
 * Returns: number of targets selected (more than one for heterogeneous
 * emulation). The first one is the primary target, returned by target_info().
 */
size_t target_info_qom_num_targets(void);

/**
 * target_info_qom_get_target:
 * @idx: index of target, less than target_info_qom_num_targets()
 *
 * Returns: TargetInfo of selected target at index @idx.
 */
const TargetInfo *target_info_qom_get_target(size_t idx);

/**
 * target_info_qom_find_target:
 * @name: target name
 *
 * Returns: TargetInfo of selected target named @name, or NULL if this target
 * was not selected.
 */
const TargetInfo *target_info_qom_find_target(const char *name);

#endif /* QEMU_TARGET_INFO_QOM_H */
