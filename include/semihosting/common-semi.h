/*
 *  Semihosting support for systems modeled on the Arm "Angel"
 *  semihosting syscalls design. This includes Arm and RISC-V processors
 *
 *  Copyright (c) 2005, 2007 CodeSourcery.
 *  Copyright (c) 2019 Linaro
 *  Written by Paul Brook.
 *
 *  Copyright © 2020 by Keith Packard <keithp@keithp.com>
 *  Adapted for systems other than ARM, including RISC-V, by Keith Packard
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  ARM Semihosting is documented in:
 *     Semihosting for AArch32 and AArch64 Release 2.0
 *     https://static.docs.arm.com/100863/0200/semihosting.pdf
 *
 *  RISC-V Semihosting is documented in:
 *     RISC-V Semihosting
 *     https://github.com/riscv/riscv-semihosting-spec/blob/main/riscv-semihosting-spec.adoc
 */

#ifndef COMMON_SEMI_H
#define COMMON_SEMI_H

/*
 * Target-specific operations needed by the Arm-compatible semihosting
 * implementation.  Each target architecture supporting it provides one
 * instance, reached at runtime through CPUClass::semihosting_ops, so that
 * several targets can be linked into a single binary.
 */
typedef struct SemihostingCPUOps {
    /* Return semihosting argument register @argno. */
    uint64_t (*arg)(CPUState *cs, int argno);
    /* Set the register holding the semihosting return value. */
    void (*set_ret)(CPUState *cs, uint64_t ret);
    /* Set the register holding the semihosting error value, if any. */
    void (*set_err)(CPUState *cs, int err);
    /* Return %true if the guest is using the 64-bit semihosting ABI. */
    bool (*is_64bit)(CPUState *cs);
    /* Return %true if SYS_EXIT takes an extended parameter block. */
    bool (*sys_exit_is_extended)(CPUState *cs);
    /* Return the address just above the semihosting scratch area. */
    uint64_t (*stack_bottom)(CPUState *cs);
    /* Return %true if SYS_SYNCCACHE is implemented. */
    bool (*has_synccache)(CPUState *cs);
} SemihostingCPUOps;

void common_semi_cb(CPUState *cs, uint64_t ret, int err);
void do_common_semihosting(CPUState *cs);

#endif /* COMMON_SEMI_H */
