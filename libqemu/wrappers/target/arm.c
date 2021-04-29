/*
 * libqemu
 *
 * Copyright (c) 2019 Luc Michel <luc.michel@greensocs.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "target/arm/cpu.h"
#include "target/arm/cpu-qom.h"
#include "target/arm/cpregs.h"
#include "hw/qdev-properties.h"
#include "hw/intc/armv7m_nvic.h"

#include "arm.h"

void libqemu_cpu_arm_set_cp15_cbar(Object *obj, uint64_t cbar)
{
    ARMCPU *cpu = ARM_CPU(obj);
    CPUARMState *env = &cpu->env;

    if (arm_feature(env, ARM_FEATURE_CBAR_RO)) {
        /* XXX discarding const qualifier here to avoid modifying QEMU upstream code */
        uint32_t id = ENCODE_CP_REG(15, 0, 1, 15, 0, 4, 0);
        ARMCPRegInfo *cbar_info = (ARMCPRegInfo *) get_arm_cp_reginfo(cpu->cp_regs, id);
        assert(cbar_info);
        cbar_info->resetvalue = cbar;
    } else {
        env->cp15.c15_config_base_address = cbar;
    }

#if 0
    if (arm_feature(env, ARM_FEATURE_AARCH64)) {
        /* For AArch64, also update the 32 bits view */
        uint32_t id = ENCODE_AA64_CP_REG(0, 15, 3, 3, 1, 0);
        ARMCPRegInfo *cbar_info = (ARMCPRegInfo *) get_arm_cp_reginfo(cpu->cp_regs, id);
        assert(cbar_info);
        cbar_info->resetvalue = cbar;
    }
#endif
}


void libqemu_cpu_aarch64_set_aarch64_mode(Object *obj, bool aarch64_mode)
{
    CPUARMState *cpu = &ARM_CPU(obj)->env;

    cpu->aarch64 = aarch64_mode;
}

void libqemu_cpu_arm_add_nvic_link(Object *obj)
{
    ARMCPU *cpu = ARM_CPU(obj);
    CPUARMState *env = &cpu->env;

    object_property_add_link(obj, "nvic", TYPE_NVIC, (Object **) &env->nvic,
                             qdev_prop_allow_set_link_before_realize,
                             OBJ_PROP_LINK_STRONG);
}

void libqemu_arm_nvic_add_cpu_link(Object *obj)
{
    NVICState *nvic = NVIC(obj);

    object_property_add_link(obj, "cpu", TYPE_ARM_CPU, (Object **) &nvic->cpu,
                             qdev_prop_allow_set_link_before_realize,
                             OBJ_PROP_LINK_STRONG);
}

uint64_t libqemu_cpu_arm_get_exclusive_addr(const Object *obj)
{
    const ARMCPU *cpu = ARM_CPU(obj);
    const CPUARMState *env = &cpu->env;

    return env->exclusive_addr;
}

uint64_t libqemu_cpu_arm_get_exclusive_val(const Object *obj)
{
    const ARMCPU *cpu = ARM_CPU(obj);
    const CPUARMState *env = &cpu->env;

    return env->exclusive_val;
}

void libqemu_cpu_arm_set_exclusive_val(Object *obj, uint64_t val)
{
    ARMCPU *cpu = ARM_CPU(obj);
    CPUARMState *env = &cpu->env;

    env->exclusive_val = val;
}
