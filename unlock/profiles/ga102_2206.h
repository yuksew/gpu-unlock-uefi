/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * MSI RTX 3080 10GB, PCI 10de:2206, subsystem 1462:3890.
 *
 * Every expected value below was read from this card's own firmware dump and
 * cold-state probe in the repository this fork came from
 * (balaena01/rtx3080-unlock-12g, evidence/boot-03 and unlock-boot-09).
 * GU_BOARD_HAVE_* is 1 for every field because each one has been confirmed on
 * the card itself, which is what allows a write-capable (non-preflight) image
 * to be built from this profile. The measurements for this board are in that
 * repository too; this fork did not reproduce them.
 */
#pragma once

#define GU_BOARD_PROFILE_NAME "ga102_2206 (MSI RTX 3080 10GB, confirmed)"
#define GU_BOARD_LABEL "MSI RTX 3080 10GB"

/* PCI config 0x00 and 0x2c, dword order, plus class/revision at 0x08. */
#define GU_BOARD_ID 0x220610deU
#define GU_BOARD_SUBSYSTEM 0x38901462U
#define GU_BOARD_CLASS_REV 0x030000a1U
#define GU_BOARD_HAVE_CLASS 1

/* GA102 die ID at BAR0+0; 0xb72000a1 in every probe boot of this card. */
#define GU_BOARD_CHIP 0xb72000a1U

/* Live BAR0+0x300000, byte-identical over three consecutive probe boots. */
#define GU_BOARD_PROM_SHA256 \
  "f3dc8f07b0b8c9856d192df172dfa7f7f84ac17ef6edfdb448ad7fa94efd2789"

/* 10240 MiB, read back from FEAT 0x001183a4. */
#define GU_BOARD_FB_MB 0x2800U

/* Cold SS0 (0x0082381c) is stable on this card, so the gate is exact. */
#define GU_BOARD_SS0_MASK 0xffffffffU
#define GU_BOARD_SS0_VALUE 0x46254040U

/* Cold SS1 (0x00823820) is 2 or 3; both select the same cold state. */
#define GU_BOARD_SS1_BITS 0x0000000cU

/* BAR0 is 16 MiB; the board PROM window is the 1 MiB at BAR0+0x300000. */
#define GU_BOARD_BAR0_SIZE 0x1000000U
#define GU_BOARD_PROM_OFFSET 0x300000U
#define GU_BOARD_PROM_SIZE 0x100000U

/* VBIOS-placed capability dword the 10GB host reports at config 0xc8. */
#define GU_BOARD_CAP_OFFSET 0x0c8U
#define GU_BOARD_CAP_VALUE 0x00050011U

#define GU_BOARD_HAVE_CHIP 1
#define GU_BOARD_HAVE_SUBSYSTEM 1
#define GU_BOARD_HAVE_PROM 1
#define GU_BOARD_HAVE_SS0 1
#define GU_BOARD_HAVE_SS1 1
#define GU_BOARD_HAVE_CAP 1
#define GU_BOARD_HAVE_FB 1
