/*
 * Xqci manual helper function declarations.
 *
 * Copyright (c) 2025 rev.ng Labs Srl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.
 *
 * See the LICENSE file in the top-level directory for details.
 */

#ifdef TARGET_RISCV32
DEF_HELPER_4(xqci_swm, void, env, tl, tl, s32)
DEF_HELPER_4(xqci_lwm, void, env, tl, tl, s32)
DEF_HELPER_4(xqci_setwm, void, env, tl, tl, tl)

#ifndef CONFIG_USER_ONLY
DEF_HELPER_1(xqci_mienter, void, env)
DEF_HELPER_1(xqci_mienter_nest, void, env)
DEF_HELPER_1(xqci_mileaveret, void, env)

DEF_HELPER_3(xqci_outw, void, env, tl, tl)
DEF_HELPER_2(xqci_inw, tl, env, tl)
#endif
#endif
