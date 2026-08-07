/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Test Purpose:
 * A packet may straddle a page boundary.  When the page it continues into
 * has no translation, the fetch raises TLBMISSX with cause 0x61
 * (HEX_CAUSE_TLBMISSX_CAUSE_NEXTPAGE) and ELR must point at the straddling
 * packet itself -- not at some earlier packet in the same page.
 *
 * This matters because miss handlers derive the address to map from ELR:
 * the H2 hypervisor, for instance, fills the TLB for ELR + 16 on cause 0x61
 * (16 bytes being the maximum packet size, so ELR + 16 always lands in the
 * page that is missing).  An ELR pointing at an earlier packet makes the
 * handler map the page that is already present, and the fetch faults again
 * forever.
 *
 * The straddling packet is placed at page0 + 0xff8 and is preceded by a run
 * of packets in the same page, so that QEMU has translated several packets
 * before it reaches it.
 */

#define PAGE0           0x9b800000
#define PAGE1           0x9b801000
#define CROSSING_PKT    (PAGE0 + 0xff8)

/* V=1, G=1, ASID=0, VPN=0x9b800 */
#define PAGE0_HI        0xc009b800
/* X=W=R=U=1, C=7, PPD=(0x9b800 << 1) | 1 (the trailing 1 selects 4KB) */
#define PAGE0_LO        0xf7137001

/* Exit codes */
#define PASS            0
#define FAIL_ELR        1
#define FAIL_BADVA      2
#define FAIL_CAUSE      3
#define FAIL_TOO_MANY   4
#define FAIL_RESULT     5
#define FAIL_EVENT      6

#define count           r10
#define tlb_index       r11

.global start
start:
    r0 = ##evb
    evb = r0
    r0 = #0
    ssr = r0
    jump #setup

.org 0x100
evb:
    jump #handle_event          /* 0: reset */
    jump #handle_event          /* 1: nmi */
    jump #handle_event          /* 2: error */
    jump #handle_event          /* 3: rsvd */
    jump #handle_tlbmissx       /* 4: tlbmissx */
    jump #handle_event          /* 5: rsvd */
    jump #handle_event          /* 6: tlbmissrw */

setup:
    /* Identity-map the 4KB page holding this code.  Leave PAGE1 unmapped. */
    {
        r1 = ##PAGE0_HI
        r0 = ##PAGE0_LO
    }
    tlb_index = #8
    tlbw(r1:0, tlb_index)

    /* Enable the MMU */
    r2 = ##0x0085a07f
    syscfg = r2

    count = #0
    {
        r24 = #0
        r25 = #0
        r26 = #0
    }
    jump #run_up

handle_event:
    r2 = #FAIL_EVENT
    stop(r0)

handle_tlbmissx:
    /*
     * Without a correct ELR the handler below maps a page that is already
     * present and we come straight back here.  Bail out rather than spin.
     */
    count = add(count, #1)
    {
        p0 = cmp.gtu(count, #3); if (p0.new) jump:nt .Lfail_too_many
    }

    /* ELR must be the packet that straddles the boundary */
    r0 = elr
    r1 = ##CROSSING_PKT
    p0 = cmp.eq(r0, r1)
    if (!p0) jump #.Lfail_elr

    /* BADVA must name the page we could not fetch from */
    r0 = badva
    r1 = ##0xfffff000
    r0 = and(r0, r1)
    r1 = ##PAGE1
    p0 = cmp.eq(r0, r1)
    if (!p0) jump #.Lfail_badva

    /* SSR.CAUSE must be "next page", not the plain fetch miss */
    r0 = ssr
    r0 = and(r0, #0xff)
    p0 = cmp.eq(r0, #0x61)
    if (!p0) jump #.Lfail_cause

    /*
     * Fill the missing page the way a real miss handler does: from ELR + 16,
     * identity mapped, 4KB.
     */
    r0 = elr
    r0 = add(r0, #16)
    r0 = lsr(r0, #12)               /* VPN */
    r3 = ##0xc0000000
    r1 = or(r3, r0)                 /* V=1, G=1, VPN */
    r0 = asl(r0, #1)
    r0 = or(r0, #1)                 /* PPD, 4KB */
    r3 = ##0xf7000000
    r0 = or(r0, r3)                 /* X=W=R=U=1, C=7 */
    tlb_index = add(tlb_index, #1)
    tlbw(r1:0, tlb_index)
    rte

.Lfail_elr:
    r2 = #FAIL_ELR
    stop(r0)
.Lfail_badva:
    r2 = #FAIL_BADVA
    stop(r0)
.Lfail_cause:
    r2 = #FAIL_CAUSE
    stop(r0)
.Lfail_too_many:
    r2 = #FAIL_TOO_MANY
    stop(r0)
.Lfail_result:
    r2 = #FAIL_RESULT
    stop(r0)

/*
 * Six single-instruction packets ending at PAGE0 + 0xff8, so that the
 * straddling packet is not the first packet of its translation block.
 */
.org 0xfe0
run_up:
    nop
    nop
    nop
    nop
    nop
    nop

/*
 * PAGE0 + 0xff8: a 12-byte packet spanning 0xff8 .. 0x1003, so its last word
 * lives in the page that is not mapped yet.  r24-r26 are deliberately outside
 * the duplex register set (r0-r7, r16-r23): a duplex would fold two of these
 * into a single word and the packet would no longer reach across the boundary.
 */
    {
        r24 = #0x11
        r25 = #0x22
        r26 = #0x33
    }

/* PAGE0 + 0x1004: the straddling packet must have committed all three writes */
    p0 = cmp.eq(r24, #0x11)
    if (!p0) jump #.Lfail_result
    p0 = cmp.eq(r25, #0x22)
    if (!p0) jump #.Lfail_result
    p0 = cmp.eq(r26, #0x33)
    if (!p0) jump #.Lfail_result

    /* Exactly one TLBMISSX event should have been needed */
    p0 = cmp.eq(count, #1)
    if (!p0) jump #.Lfail_too_many

    r2 = #PASS
    stop(r0)
