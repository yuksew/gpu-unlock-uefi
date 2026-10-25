/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * NVIDIA RTX 3060 12GB (GA106-300), PCI 10de:2504, subsystem 10de:1536,
 * 12288 MiB.
 *
 * Confirmed on hardware 2026-10-25 from three read-only preflight boots
 * (evidence/preflight-3060-01..03/REPORT.TXT). Each value and the line it was
 * read from:
 *
 *   subsystem 0x153610de  identity: pci=0x250410de subsystem=0x153610de ...
 *   class/rev 0x030000a1  identity: ... class_rev=0x30000a1
 *   chip      0xb76000a1  chip: actual=0xb76000a1 (GA106 candidate confirmed)
 *   PROM      b1608213... live PROM hash sha256=b1608213...
 *   SS0       0x12210024 (RUN00) / 0x12011024 (RUN01, RUN02)
 *   SS1       3 in every boot
 *   cap       0xc8 = 0x00050011
 *   fb        0x3000 = 12288 MiB at FEAT 0x001183a4
 *   BDF       0:5:0.0, behind Pci(0x2,0x1)/Pci(0x0,0x0)/Pci(0x0,0x0)
 *
 * SS0 is boot-unstable on this card, exactly like the 12GB GA102 board: RUN00
 * differed from RUN01/RUN02 in bits 0x00200000 and 0x00001000. Only the bits
 * common to every observed cold boot are a gate (0x12010024), so a re-boot
 * cannot reject the card for a volatile bit. If a later cold boot differs in
 * yet another bit, re-merge the mask from all reports rather than widening it
 * by guesswork.
 *
 * SS1 is a set of accepted cold values, not an equality: only 3 has been
 * observed on this card. The two GA102 boards accept 2 or 3, so widen
 * GU_BOARD_SS1_BITS to 0x0000000c if a cold boot ever reports 2.
 *
 * Another 3060 12GB - different AIB, different VBIOS - must not reuse these
 * values. Such a card runs the discovery profile and writes its own
 * ga106_2504.local.h, which overrides this file; see docs/BOARD-PROFILE.md.
 */
#pragma once

#if __has_include("ga106_2504.local.h")
#include "ga106_2504.local.h"
#else

#define GU_BOARD_PROFILE_NAME "ga106_2504 (RTX 3060 12GB, confirmed on hardware)"
#define GU_BOARD_LABEL "RTX 3060 12GB"

#define GU_BOARD_ID 0x250410deU
#define GU_BOARD_SUBSYSTEM 0x153610deU
#define GU_BOARD_CLASS_REV 0x030000a1U

#define GU_BOARD_CHIP 0xb76000a1U

#define GU_BOARD_PROM_SHA256 \
  "b16082136f37930db1a7eace9d30a3473679b966ff3ab4253d08f1cf6eefa4da"

#define GU_BOARD_FB_MB 0x3000U

/* Only the bits common to every observed cold boot are a gate. */
#define GU_BOARD_SS0_MASK 0x12010024U
#define GU_BOARD_SS0_VALUE 0x12010024U

/* SS1 < 32 is an index into this mask; 3 is the only value seen so far. */
#define GU_BOARD_SS1_BITS 0x00000008U

#define GU_BOARD_BAR0_SIZE 0x1000000U
#define GU_BOARD_PROM_OFFSET 0x300000U
#define GU_BOARD_PROM_SIZE 0x100000U

#define GU_BOARD_CAP_OFFSET 0x0c8U
#define GU_BOARD_CAP_VALUE 0x00050011U

#define GU_BOARD_HAVE_CHIP 1
#define GU_BOARD_HAVE_SUBSYSTEM 1
#define GU_BOARD_HAVE_PROM 1
#define GU_BOARD_HAVE_SS0 1
#define GU_BOARD_HAVE_SS1 1
#define GU_BOARD_HAVE_CAP 1
#define GU_BOARD_HAVE_CLASS 1
#define GU_BOARD_HAVE_FB 1

#endif
