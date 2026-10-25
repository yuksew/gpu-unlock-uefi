/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * MSI RTX 3090, PCI 10de:2204, subsystem 1462:3881, 24576 MiB.
 *
 * Confirmed on hardware 2026-10-25: three read-only discovery boots
 * (evidence/preflight-3090-01..03/REPORT.TXT) supplied the values below, and a
 * fourth boot with this profile (evidence/preflight-3090-04/REPORT.TXT) passed
 * every gate as a real comparison. Each value and the line it was read from:
 *
 *   subsystem 0x38811462  identity: pci=0x220410de subsystem=0x38811462 ...
 *   class/rev 0x030000a1  identity: ... class_rev=0x30000a1
 *   chip      0xb72000a1  chip: actual=0xb72000a1 (GA102)
 *   PROM      de78e145... live PROM hash sha256=de78e145...
 *   SS0       0x50004016  in the three discovery boots, 0x50004006 in a later one
 *   SS1       6           in all three discovery boots
 *   cap       0xc8 = 0x00050011
 *   fb        0x6000 = 24576 MiB at FEAT 0x001183a4
 *
 * SS0 looked stable across the three discovery boots and was gated exactly,
 * but a later cold boot reported 0x50004006 instead of 0x50004016: bit 0x10 is
 * volatile after all. The gate is the AND of every observed value, so neither
 * state rejects the card (the 220a profile needed the same treatment).
 *
 * SS1 is a set of accepted cold values, not an equality: only 6 has been seen.
 *
 * This profile is the MSI card's, not the SKU's. Another RTX 3090 has its own
 * subsystem and VBIOS: run the discovery preflight and write its own
 * ga102_2204.local.h, which overrides this file.
 */
#pragma once

#if __has_include("ga102_2204.local.h")
#include "ga102_2204.local.h"
#else

#define GU_BOARD_PROFILE_NAME "ga102_2204 (MSI RTX 3090, confirmed on hardware)"
#define GU_BOARD_LABEL "MSI RTX 3090"

#define GU_BOARD_ID 0x220410deU
#define GU_BOARD_SUBSYSTEM 0x38811462U
#define GU_BOARD_CLASS_REV 0x030000a1U

#define GU_BOARD_CHIP 0xb72000a1U

#define GU_BOARD_PROM_SHA256 \
  "de78e145e527059faec711a9a320b6dca6a88dc4de0fed882d1688ba2dd1aad1"

#define GU_BOARD_FB_MB 0x6000U

/* Only the bits common to every observed cold boot (0x50004016, 0x50004006). */
#define GU_BOARD_SS0_MASK 0x50004006U
#define GU_BOARD_SS0_VALUE 0x50004006U

/* SS1 < 32 is an index into this mask; 6 is the only value seen so far. */
#define GU_BOARD_SS1_BITS 0x00000040U

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
