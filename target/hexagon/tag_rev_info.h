/*
 *  Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
 */

#ifndef HEXAGON_TAG_ARCH_TABLE_H
#define HEXAGON_TAG_ARCH_TABLE_H

struct tag_rev_info { int introduced, removed; };

const struct tag_rev_info tag_rev_info[XX_LAST_OPCODE] = {
    [A7_clip] = { .introduced = 0x67, .removed = 0x0 },
    [A7_croundd_ri] = { .introduced = 0x67, .removed = 0x0 },
    [A7_croundd_rr] = { .introduced = 0x67, .removed = 0x0 },
    [A7_vclip] = { .introduced = 0x67, .removed = 0x0 },
    [F2_dfmax] = { .introduced = 0x67, .removed = 0x0 },
    [F2_dfmin] = { .introduced = 0x67, .removed = 0x0 },
    [F2_dfmpyfix] = { .introduced = 0x67, .removed = 0x0 },
    [F2_dfmpyhh] = { .introduced = 0x67, .removed = 0x0 },
    [F2_dfmpylh] = { .introduced = 0x67, .removed = 0x0 },
    [F2_dfmpyll] = { .introduced = 0x67, .removed = 0x0 },
    [J2_callrh] = { .introduced = 0x73, .removed = 0x0 },
    [J2_jumprh] = { .introduced = 0x73, .removed = 0x0 },
    [J2_unpause] = { .introduced = 0x73, .removed = 0x0 },
    [L2_loadw_aq] = { .introduced = 0x68, .removed = 0x0 },
    [L4_loadd_aq] = { .introduced = 0x68, .removed = 0x0 },
    [L6_memcpy] = { .introduced = 0x0, .removed = 0x75 },
    /* [LX_L16b_tmp] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_S16b_new] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_absdiffl_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_absdiffl_uh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_absdiffl_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_absdiffl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_absl_uh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_absl_uh_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_absl_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_absl_uw_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_add_wh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_add_wuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_add_wwh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_add_wwuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_addh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_addhsat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_addl_uw_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_addl_uw_w_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_addl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_addl_w_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_adduhhsat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_adduhsat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_and] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_andl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_andn] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_andnl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_assign] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_assign_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_avgl_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_avgl_h_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_avgl_uh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_avgl_uh_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_avgl_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_avgl_uw_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_avgl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_avgl_w_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_eq_and_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_eq_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_eq_or_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_eq_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_gt_and_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_gt_and_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_gt_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_gt_or_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_gt_or_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_gt_uh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_gt_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_cmp_gt_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_conv_uw_uh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_conv_w_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_lut32r_huhh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_lut4r_huhh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_maxh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_maxl_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_maxl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_maxuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_minh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_minl_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_minl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_minuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpa_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpa_h_rt] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpa_h_rt_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpa_h_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpa_uh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpa_uh_rt] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpa_uh_rt_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpa_uh_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpal_uwuh_lo] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpal_wh_lo] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpal_wuhh_lo] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_huhw_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_hw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_uhuw_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_uwhw_si] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_uwuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_uwuh_hi] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_uwuh_lo] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_uwuhuw_si] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_wh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_wh_hi] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_wh_lo] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_whuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_whuh_hi] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_whuh_lo] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_whw_si] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_wuhh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_wuhh_hi] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_wuhh_lo] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpar_wuhw_si] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mparh_uhhw_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mps_uh_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_huhw_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_hw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_uhuw_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_uwhw_si] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_uwuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_uwuh_hi] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_uwuh_lo] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_uwuhuw_si] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_wh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_wh_hi] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_wh_lo] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_whuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_whuh_hi] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_whuh_lo] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_whw_si] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_wuhh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_wuhh_hi] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_wuhh_lo] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsr_wuhw_si] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpsrh_uhhw_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpy_narrow_l] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpy_narrow_rt] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpy_unarrow_l] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpy_unarrow_rt] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpy_uwide_l] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpy_uwide_rt] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpy_wide_l] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpy_wide_l_mix] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpy_wide_rt] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpy_wide_rt_mix] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyl_h_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyl_h_sh_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyl_h_sh_rnd_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyl_h_sh_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyl_huh_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyl_uh_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyr_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyr_h_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyr_h_sh_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyr_h_sh_rnd_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyr_h_sh_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyr_huh_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyr_l] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_mpyr_uh_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_muxl_hwuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_muxl_uh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_navgl_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_navgl_uh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_navgl_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_navgl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_navgsl_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_navgsl_uh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_navgsl_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_navgsl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyl_h_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyl_h_sh_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyl_huh_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyl_huh_sh_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyl_wh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyl_whuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyr_h_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyr_h_sh_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyr_huh_sh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyr_huh_sh_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyr_wh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_nmpyr_whuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_not] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_not_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_or] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_orl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_orn] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_ornl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_rsplat_tmp] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_rsplat_wide_tmp] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shra_l_h_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shra_l_uh_uh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shra_l_uw_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shra_l_w_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shra_r_h_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shra_r_uh_uh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shra_r_uw_uw] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shra_r_w_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shra_w_w_rnd_sat_s1] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shrl_h] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shrl_hwh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shrl_hwh_rnd_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shrl_hwh_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shrl_uhuhh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shrl_uhuwh_rnd_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shrl_uhuwh_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shrl_uhwh_rnd_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shrl_uhwh_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shrl_uwuwh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_shrl_wwh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_ssplat_tmp] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_sub_wh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_sub_wuh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_subh] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_subhsat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_subl_uw_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_subl_uw_w_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_subl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_subl_w_sat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_subuhhsat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_subuhsat] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_usplat_tmp] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_xor] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [LX_xorl_w] = { .introduced = 0x81, .removed = 0x0 }, */
    [M7_dcmpyiw] = { .introduced = 0x67, .removed = 0x0 },
    [M7_dcmpyiw_acc] = { .introduced = 0x67, .removed = 0x0 },
    [M7_dcmpyiwc] = { .introduced = 0x67, .removed = 0x0 },
    [M7_dcmpyiwc_acc] = { .introduced = 0x67, .removed = 0x0 },
    [M7_dcmpyrw] = { .introduced = 0x67, .removed = 0x0 },
    [M7_dcmpyrw_acc] = { .introduced = 0x67, .removed = 0x0 },
    [M7_dcmpyrwc] = { .introduced = 0x67, .removed = 0x0 },
    [M7_dcmpyrwc_acc] = { .introduced = 0x67, .removed = 0x0 },
    [M7_wcmpyiw] = { .introduced = 0x67, .removed = 0x0 },
    [M7_wcmpyiw_rnd] = { .introduced = 0x67, .removed = 0x0 },
    [M7_wcmpyiwc] = { .introduced = 0x67, .removed = 0x0 },
    [M7_wcmpyiwc_rnd] = { .introduced = 0x67, .removed = 0x0 },
    [M7_wcmpyrw] = { .introduced = 0x67, .removed = 0x0 },
    [M7_wcmpyrw_rnd] = { .introduced = 0x67, .removed = 0x0 },
    [M7_wcmpyrwc] = { .introduced = 0x67, .removed = 0x0 },
    [M7_wcmpyrwc_rnd] = { .introduced = 0x67, .removed = 0x0 },
    /* [M8_cvt_rs_f8] = { .introduced = 0x81, .removed = 0x0 }, */
    [M8_cvt_rs_hf] = { .introduced = 0x73, .removed = 0x0 },
    [M8_cvt_rs_ub] = { .introduced = 0x73, .removed = 0x0 },
    [M8_cvt_rs_ub_sc0] = { .introduced = 0x73, .removed = 0x0 },
    [M8_cvt_rs_ub_sc1] = { .introduced = 0x73, .removed = 0x0 },
    [M8_cvt_rs_uh_2x1] = { .introduced = 0x73, .removed = 0x0 },
    [M8_cvt_rs_uh_2x2] = { .introduced = 0x73, .removed = 0x0 },
    [M8_mxaccshl] = { .introduced = 0x68, .removed = 0x79 },
    [M8_mxclracc] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxclracc_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvta_sat_uh] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvta_sat_uh2x2] = { .introduced = 0x69, .removed = 0x0 },
    [M8_mxcvta_sat_uh2x2_r] = { .introduced = 0x69, .removed = 0x0 },
    [M8_mxcvta_sat_uh_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvta_uh] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvta_uh2x2] = { .introduced = 0x69, .removed = 0x0 },
    [M8_mxcvta_uh2x2_r] = { .introduced = 0x69, .removed = 0x0 },
    [M8_mxcvta_uh_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtb_sat_uh] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtb_sat_uh2x2] = { .introduced = 0x69, .removed = 0x0 },
    [M8_mxcvtb_sat_uh2x2_r] = { .introduced = 0x69, .removed = 0x0 },
    [M8_mxcvtb_sat_uh_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtb_uh] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtb_uh2x2] = { .introduced = 0x69, .removed = 0x0 },
    [M8_mxcvtb_uh2x2_r] = { .introduced = 0x69, .removed = 0x0 },
    [M8_mxcvtb_uh_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_dm_sat_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_dm_sat_ub_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_dm_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_dm_ub_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_sat_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_sat_hf_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_sat_pos_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_sat_pos_hf_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_sat_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_sat_ub_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtl_ub_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_dm_sat_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_dm_sat_ub_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_dm_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_dm_ub_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_sat_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_sat_hf_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_sat_pos_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_sat_pos_hf_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_sat_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_sat_ub_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxcvtr_ub_r] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem] = { .introduced = 0x73, .removed = 0x0 },
    [M8_mxmem2_bias] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem2_st_bias] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_2x2] = { .introduced = 0x73, .removed = 0x0 },
    [M8_mxmem_bias] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_blk_dm_act_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_blk_sm_act_f8] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmem_blk_sm_act_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_blk_sm_act_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_cm] = { .introduced = 0x73, .removed = 0x0 },
    [M8_mxmem_cm_deep] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmem_deep] = { .introduced = 0x79, .removed = 0x0 },
    /* [M8_mxmem_deep_f8] = { .introduced = 0x81, .removed = 0x0 }, */
    [M8_mxmem_dm_act_ub] = { .introduced = 0x68, .removed = 0x0 },
    /* [M8_mxmem_f8] = { .introduced = 0x81, .removed = 0x0 }, */
    [M8_mxmem_sm_act_f8] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmem_sm_act_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_sm_act_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_st_bias] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_wei_b] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_wei_b1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_wei_c] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_wei_f8] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmem_wei_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_wei_n] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_wei_n_2x] = { .introduced = 0x73, .removed = 0x0 },
    [M8_mxmem_wei_sb1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_wei_sc] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmem_wei_sm] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmema_wei_b] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmema_wei_b1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmema_wei_c] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmema_wei_f8] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmema_wei_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmema_wei_n] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmema_wei_n_2x] = { .introduced = 0x73, .removed = 0x0 },
    [M8_mxmema_wei_sb1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmema_wei_sc] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmema_wei_sm] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemd_blk_dm_act_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemd_blk_sm_act_f8] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmemd_blk_sm_act_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemd_blk_sm_act_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdi_wei_b] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdi_wei_b1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdi_wei_c] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdi_wei_f8] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmemdi_wei_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdi_wei_n] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdi_wei_n_2x] = { .introduced = 0x73, .removed = 0x0 },
    [M8_mxmemdi_wei_sb1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdi_wei_sc] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdi_wei_sm] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdp_wei_b] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdp_wei_b1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdp_wei_c] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdp_wei_f8] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmemdp_wei_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdp_wei_n] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdp_wei_n_2x] = { .introduced = 0x73, .removed = 0x0 },
    [M8_mxmemdp_wei_sb1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdp_wei_sc] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdp_wei_sm] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdr_wei_b] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdr_wei_b1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdr_wei_c] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdr_wei_f8] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmemdr_wei_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdr_wei_n] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdr_wei_n_2x] = { .introduced = 0x73, .removed = 0x0 },
    [M8_mxmemdr_wei_sb1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdr_wei_sc] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemdr_wei_sm] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmems_blk_dm_act_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmems_blk_sm_act_f8] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmems_blk_sm_act_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmems_blk_sm_act_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmems_wei_b] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmems_wei_b1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmems_wei_c] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmems_wei_f8] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmems_wei_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmems_wei_n] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmems_wei_n_2x] = { .introduced = 0x73, .removed = 0x0 },
    [M8_mxmems_wei_sb1] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmems_wei_sc] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmems_wei_sm] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemu_blk_dm_act_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemu_blk_sm_act_f8] = { .introduced = 0x79, .removed = 0x0 },
    [M8_mxmemu_blk_sm_act_hf] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxmemu_blk_sm_act_ub] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxswap] = { .introduced = 0x68, .removed = 0x0 },
    [M8_mxswap_hf] = { .introduced = 0x68, .removed = 0x0 },
    [R6_release_at_vi] = { .introduced = 0x68, .removed = 0x0 },
    [R6_release_st_vi] = { .introduced = 0x68, .removed = 0x0 },
    [S2_pstorerbf_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_pstorerbt_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_pstorerdf_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_pstorerdt_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_pstorerhf_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_pstorerht_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_pstorerif_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_pstorerit_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_storerb_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_storerd_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_storerh_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_storeri_pi_nt] = { .introduced = 0x79, .removed = 0x0 },
    [S2_storew_rl_at_vi] = { .introduced = 0x68, .removed = 0x0 },
    [S2_storew_rl_st_vi] = { .introduced = 0x68, .removed = 0x0 },
    [S4_stored_rl_at_vi] = { .introduced = 0x68, .removed = 0x0 },
    [S4_stored_rl_st_vi] = { .introduced = 0x68, .removed = 0x0 },
    [V6_get_qfext] = { .introduced = 0x79, .removed = 0x0 },
    [V6_get_qfext_oracc] = { .introduced = 0x79, .removed = 0x0 },
    [V6_set_qfext] = { .introduced = 0x79, .removed = 0x0 },
    [V6_v6mpyhubs10] = { .introduced = 0x68, .removed = 0x0 },
    [V6_v6mpyhubs10_vxx] = { .introduced = 0x68, .removed = 0x0 },
    [V6_v6mpyvubs10] = { .introduced = 0x68, .removed = 0x0 },
    [V6_v6mpyvubs10_vxx] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vabs_f8] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vabs_hf] = { .introduced = 0x68, .removed = 0x0 },
    /* [V6_vabs_qf16_hf] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vabs_qf16_qf16] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vabs_qf32_qf32] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vabs_qf32_sf] = { .introduced = 0x81, .removed = 0x0 }, */
    [V6_vabs_sf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vadd_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vadd_hf_f8] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vadd_hf_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vadd_qf16] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vadd_qf16_mix] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vadd_qf32] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vadd_qf32_mix] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vadd_sf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vadd_sf_bf] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vadd_sf_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vadd_sf_sf] = { .introduced = 0x68, .removed = 0x0 },
    /* [V6_valign4] = { .introduced = 0x81, .removed = 0x0 }, */
    [V6_vasrvuhubrndsat] = { .introduced = 0x69, .removed = 0x0 },
    [V6_vasrvuhubsat] = { .introduced = 0x69, .removed = 0x0 },
    [V6_vasrvwuhrndsat] = { .introduced = 0x69, .removed = 0x0 },
    [V6_vasrvwuhsat] = { .introduced = 0x69, .removed = 0x0 },
    [V6_vassign_fp] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vassign_tmp] = { .introduced = 0x69, .removed = 0x0 },
    [V6_vcombine_tmp] = { .introduced = 0x69, .removed = 0x0 },
    /* [V6_vconv_bf_qf32] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vconv_f8_qf16] = { .introduced = 0x81, .removed = 0x0 }, */
    [V6_vconv_h_hf] = { .introduced = 0x73, .removed = 0x0 },
    /* [V6_vconv_h_hf_rnd] = { .introduced = 0x81, .removed = 0x0 }, */
    [V6_vconv_hf_h] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vconv_hf_qf16] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vconv_hf_qf32] = { .introduced = 0x68, .removed = 0x0 },
    /* [V6_vconv_qf16_f8] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vconv_qf16_hf] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vconv_qf16_qf16] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vconv_qf32_qf32] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vconv_qf32_sf] = { .introduced = 0x81, .removed = 0x0 }, */
    [V6_vconv_sf_qf32] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vconv_sf_w] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vconv_w_sf] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vcvt2_b_hf] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vcvt2_hf_b] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vcvt2_hf_ub] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vcvt2_ub_hf] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vcvt_b_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vcvt_bf_sf] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vcvt_f8_hf] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vcvt_h_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vcvt_hf_b] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vcvt_hf_f8] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vcvt_hf_h] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vcvt_hf_sf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vcvt_hf_ub] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vcvt_hf_uh] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vcvt_sf_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vcvt_ub_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vcvt_uh_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vdmpy_sf_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vdmpy_sf_hf_acc] = { .introduced = 0x68, .removed = 0x0 },
    /* [V6_veqhf] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_veqhf_and] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_veqhf_or] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_veqhf_xor] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_veqsf] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_veqsf_and] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_veqsf_or] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_veqsf_xor] = { .introduced = 0x81, .removed = 0x0 }, */
    [V6_vfmax_f8] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vfmax_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vfmax_sf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vfmin_f8] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vfmin_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vfmin_sf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vfneg_f8] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vfneg_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vfneg_sf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vgtbf] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vgtbf_and] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vgtbf_or] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vgtbf_xor] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vgthf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vgthf_and] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vgthf_or] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vgthf_xor] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vgtsf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vgtsf_and] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vgtsf_or] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vgtsf_xor] = { .introduced = 0x68, .removed = 0x0 },
    /* [V6_vilog2_hf] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vilog2_qf16] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vilog2_qf32] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vilog2_sf] = { .introduced = 0x81, .removed = 0x0 }, */
    [V6_vmax_bf] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vmax_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmax_sf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmerge_qf] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vmin_bf] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vmin_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmin_sf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_hf_f8] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vmpy_hf_f8_acc] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vmpy_hf_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_hf_hf_acc] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_qf16] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_qf16_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_qf16_mix_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_qf32] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_qf32_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_qf32_mix_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_qf32_qf16] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_qf32_sf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_rt_hf] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vmpy_rt_qf16] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vmpy_rt_sf] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vmpy_sf_bf] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vmpy_sf_bf_acc] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vmpy_sf_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_sf_hf_acc] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpy_sf_sf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vmpyuhvs] = { .introduced = 0x69, .removed = 0x0 },
    /* [V6_vneg_qf16_hf] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vneg_qf16_qf16] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vneg_qf32_qf32] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vneg_qf32_sf] = { .introduced = 0x81, .removed = 0x0 }, */
    /* [V6_vrmpyzbb_rt] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_vrmpyzbb_rt_acc] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_vrmpyzbb_rx] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_vrmpyzbb_rx_acc] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_vrmpyzbub_rt] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_vrmpyzbub_rt_acc] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_vrmpyzbub_rx] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_vrmpyzbub_rx_acc] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_vrmpyzcb_rt] = { .introduced = 0x0, .removed = 0x68 }, */
    /* [V6_vrmpyzcb_rt_acc] = { .introduced = 0x0, .removed = 0x68 }, */
    /* [V6_vrmpyzcb_rx] = { .introduced = 0x0, .removed = 0x68 }, */
    /* [V6_vrmpyzcb_rx_acc] = { .introduced = 0x0, .removed = 0x68 }, */
    /* [V6_vrmpyzcbs_rt] = { .introduced = 0x0, .removed = 0x68 }, */
    /* [V6_vrmpyzcbs_rt_acc] = { .introduced = 0x0, .removed = 0x68 }, */
    /* [V6_vrmpyzcbs_rx] = { .introduced = 0x0, .removed = 0x68 }, */
    /* [V6_vrmpyzcbs_rx_acc] = { .introduced = 0x0, .removed = 0x68 }, */
    /* [V6_vrmpyznb_rt] = { .introduced = 0x0, .removed = 0x68 }, */
    /* [V6_vrmpyznb_rt_acc] = { .introduced = 0x0, .removed = 0x68 }, */
    /* [V6_vrmpyznb_rx] = { .introduced = 0x0, .removed = 0x68 }, */
    /* [V6_vrmpyznb_rx_acc] = { .introduced = 0x0, .removed = 0x68 }, */
    [V6_vsub_hf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vsub_hf_f8] = { .introduced = 0x79, .removed = 0x0 },
    [V6_vsub_hf_hf] = { .introduced = 0x68, .removed = 0x0 },
    /* [V6_vsub_hf_mix] = { .introduced = 0x81, .removed = 0x0 }, */
    [V6_vsub_qf16] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vsub_qf16_mix] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vsub_qf32] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vsub_qf32_mix] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vsub_sf] = { .introduced = 0x68, .removed = 0x0 },
    [V6_vsub_sf_bf] = { .introduced = 0x73, .removed = 0x0 },
    [V6_vsub_sf_hf] = { .introduced = 0x68, .removed = 0x0 },
    /* [V6_vsub_sf_mix] = { .introduced = 0x81, .removed = 0x0 }, */
    [V6_vsub_sf_sf] = { .introduced = 0x68, .removed = 0x0 },
    /* [V6_zLd_ai] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_zLd_pi] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_zLd_ppu] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_zLd_pred_ai] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_zLd_pred_pi] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_zLd_pred_ppu] = { .introduced = 0x0, .removed = 0x75 }, */
    /* [V6_zextract] = { .introduced = 0x0, .removed = 0x75 }, */
    [Y2_dcfetchbo_nt] = { .introduced = 0x79, .removed = 0x0 },
    [Y2_dczeroa_nt] = { .introduced = 0x79, .removed = 0x0 },
    /* [Y2_tlbpp] = { .introduced = 0x81, .removed = 0x0 }, */
    [Y6_diag] = { .introduced = 0x67, .removed = 0x0 },
    [Y6_diag0] = { .introduced = 0x67, .removed = 0x0 },
    [Y6_diag1] = { .introduced = 0x67, .removed = 0x0 },
    [Y6_dmcfgrd] = { .introduced = 0x68, .removed = 0x0 },
    [Y6_dmcfgwr] = { .introduced = 0x68, .removed = 0x0 },
    [Y6_dmlink] = { .introduced = 0x68, .removed = 0x0 },
    [Y6_dmpause] = { .introduced = 0x68, .removed = 0x0 },
    [Y6_dmpoll] = { .introduced = 0x68, .removed = 0x0 },
    [Y6_dmresume] = { .introduced = 0x68, .removed = 0x0 },
    [Y6_dmstart] = { .introduced = 0x68, .removed = 0x0 },
    [Y6_dmsyncht] = { .introduced = 0x68, .removed = 0x0 },
    [Y6_dmtlbsynch] = { .introduced = 0x68, .removed = 0x0 },
    [Y6_dmwait] = { .introduced = 0x68, .removed = 0x0 },
};

#endif /* HEXAGON_TAG_ARCH_TABLE_H */
