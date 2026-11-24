/*
 * QEMU binary/target API (QOM types)
 *
 *  Copyright (c) Linaro
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qom/object.h"
#include "qemu/error-report.h"
#include "qemu/target-info-def.h"
#include "qemu/target-info-init.h"
#include "qemu/target-info-qom.h"
#include "qemu/target-info.h"
#include "hw/core/boards.h"

static void target_info_qom_class_init(ObjectClass *oc, const void * data)
{
    TargetInfoQomClass *klass = TARGET_INFO_CLASS(oc);
    klass->target_info = data;
}

static const TypeInfo target_info_parent_type = {
    .name = TYPE_TARGET_INFO,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(TargetInfoQom),
    .class_size = sizeof(TargetInfoQomClass),
    /* use class_base_init so children classes can set class_data accordingly */
    .class_base_init = target_info_qom_class_init,
    /* children classes will be concrete, which allows to easily query them
     * without listing this parent class also */
    .abstract = true,
};

DEFINE_TARGET_INFO_TYPE(target_info_parent_type)

static const TargetInfo *target_info_ptr;
/* All targets selected, including the primary one (first element) */
static GPtrArray *target_infos;

const TargetInfo *target_info(void)
{
    return target_info_ptr;
}

static void target_info_qom_set_targets(const TargetInfo **tis, size_t num)
{
    g_assert(num > 0);
    if (target_infos) {
        g_ptr_array_set_size(target_infos, 0);
    } else {
        target_infos = g_ptr_array_new();
    }
    for (size_t i = 0; i < num; i++) {
        g_ptr_array_add(target_infos, (gpointer)tis[i]);
    }
    target_info_ptr = tis[0];
}

void target_info_qom_set_target(void)
{
    g_autoptr(GSList) targets = object_class_get_list(TYPE_TARGET_INFO, false);

    size_t num_found = g_slist_length(targets);
    if (num_found != 1) {
        error_setg(&error_fatal, num_found == 0 ?
                                 "no target-info is available" :
                                 "more than one target-info is available");
    }
    target_info_qom_set_target_info(TARGET_INFO_CLASS(targets->data)->target_info);
}

void target_info_qom_set_target_info(const TargetInfo *ti)
{
    target_info_qom_set_targets(&ti, 1);
}

size_t target_info_qom_num_targets(void)
{
    return target_infos ? target_infos->len : 0;
}

const TargetInfo *target_info_qom_get_target(size_t idx)
{
    g_assert(target_infos && idx < target_infos->len);
    return g_ptr_array_index(target_infos, idx);
}

const TargetInfo *target_info_qom_find_target(const char *name)
{
    for (size_t i = 0; i < target_info_qom_num_targets(); i++) {
        const TargetInfo *ti = target_info_qom_get_target(i);
        if (!strcmp(ti->target_name, name)) {
            return ti;
        }
    }
    return NULL;
}

bool target_heterogeneous(void)
{
    return target_info_qom_num_targets() > 1;
}

/*
 * When doing heterogeneous emulation, a type is available if it is available
 * for any of the selected targets.
 *
 * Machines are an exception: they only depend on the primary target, as each
 * target is expected to define its own set of machines, potentially with
 * conflicting names (e.g. "virt").
 */
static bool target_info_type_is_available(const TypeInfo *info)
{
    if (info->is_available()) {
        return true;
    }

    if (!target_heterogeneous() ||
        g_str_has_suffix(info->name, "-" TYPE_MACHINE)) {
        return false;
    }

    bool available = false;
    const TargetInfo *primary = target_info_ptr;
    for (size_t i = 1; i < target_infos->len && !available; i++) {
        target_info_ptr = g_ptr_array_index(target_infos, i);
        available = info->is_available();
    }
    target_info_ptr = primary;

    return available;
}

static void list_targets_available(void)
{
    printf("List of targets available:\n");
    g_autoptr(GSList) targets = object_class_get_list_sorted(TYPE_TARGET_INFO, false);
    for (GSList *elem = targets; elem; elem = elem->next) {
        const TargetInfo *ti = TARGET_INFO_CLASS(elem->data)->target_info;
        printf("- %s\n", ti->target_name);
    }
    printf("Several targets can be selected at once for heterogeneous "
           "emulation, using a comma separated list (first one is the "
           "primary target).\n");
}

static const TargetInfo *target_info_lookup(const char *name)
{
    g_autoptr(GSList) targets = object_class_get_list(TYPE_TARGET_INFO, false);
    for (GSList *elem = targets; elem; elem = elem->next) {
        const TargetInfo *ti = TARGET_INFO_CLASS(elem->data)->target_info;
        if (!strcmp(name, ti->target_name)) {
            return ti;
        }
    }
    return NULL;
}

void target_info_qom_set_target_from_name(const char *names)
{
    if (!strcmp(names, "help")) {
        list_targets_available();
        exit(0);
    }

    g_auto(GStrv) list = g_strsplit(names, ",", -1);
    size_t num = g_strv_length(list);
    g_autofree const TargetInfo **tis = g_new0(const TargetInfo *, num + 1);

    for (size_t i = 0; i < num; i++) {
        const TargetInfo *ti = target_info_lookup(list[i]);
        if (!ti) {
            error_report("target '%s' is not available", list[i]);
            list_targets_available();
            exit(1);
        }
        for (size_t j = 0; j < i; j++) {
            if (tis[j] == ti) {
                error_report("target '%s' is selected more than once",
                             list[i]);
                exit(1);
            }
        }
        tis[i] = ti;
    }

    if (num == 0) {
        error_report("no target selected");
        list_targets_available();
        exit(1);
    }

    target_info_qom_set_targets(tis, num);
    type_set_is_available_handler(target_info_type_is_available);
}

void target_info_qom_set_target_from_argv0(const char *argv0)
{
    g_autoptr(GSList) targets = object_class_get_list(TYPE_TARGET_INFO, false);
    size_t num_targets = g_slist_length(targets);
    g_assert(num_targets > 0);

    /* if we have only one target, set it as default */
    if (num_targets == 1) {
        const TargetInfo *ti = TARGET_INFO_CLASS(targets->data)->target_info;
        target_info_qom_set_target_info(ti);
        return;
    }

    g_autofree char *basename_cstr = g_path_get_basename(argv0);
    g_autoptr(GString) basename = g_string_new(basename_cstr);

    const char *rm_common_patterns[] = {"qemu-system",
                                        "qemu-fuzz",
                                        "-target",
                                        "-unsigned",
                                        ".exe",
                                        NULL};
    for (const char **rm = rm_common_patterns; *rm; ++rm) {
        size_t len = strlen(*rm);
        char *found = strstr(basename->str, *rm);
        if (found) {
            size_t pos = found - basename->str;
            g_string_erase(basename, pos, len);
        }
    }

    /* remove ^[-.] */
    const char *rm_separator[] = {"-", ".", NULL};
    for (const char **rm = rm_separator; *rm; ++rm) {
        size_t len = strlen(*rm);
        if (!strncmp(basename->str, *rm, len)) {
            g_string_erase(basename, 0, len);
        }
    }

    /* remove [-.]{ok, ko, this-is-my-custom-binary-name, .*} */
    for (const char **rm = rm_separator; *rm; ++rm) {
        char *end = strstr(basename->str, *rm);
        if (end) {
            size_t pos = end - basename->str;
            size_t len = basename->len - pos;
            g_string_erase(basename, pos, len);
        }
    }

    if (basename->len > 0) {
        target_info_qom_set_target_from_name(basename->str);
        return;
    }

    /* we have a qemu-system single-binary and multiple targets */
    error_report("missing -target parameter");
    list_targets_available();
    exit(1);
}
