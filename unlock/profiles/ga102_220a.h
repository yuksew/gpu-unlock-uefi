/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * MSI RTX 3080 12GB, PCI 10de:220a, subsystem 1462:3896.
 *
 * Values come from this card's own firmware dump (PROM SHA256 799d147b...),
 * its read-only preflight and one transaction run. SS0 is boot-unstable on
 * this board - 0x01521103, 0x01521013 and 0x01521003 were observed over three
 * boots - so the gate masks down to the bits common to all three and a re-boot
 * cannot reject the card for a volatile bit.
 */
#pragma once

#define GU_BOARD_PROFILE_NAME "ga102_220a (MSI RTX 3080 12GB, confirmed)"
#define GU_BOARD_LABEL "MSI RTX 3080 12GB"

#define GU_BOARD_ID 0x220a10deU
#define GU_BOARD_SUBSYSTEM 0x38961462U
#define GU_BOARD_CLASS_REV 0x030000a1U
#define GU_BOARD_HAVE_CLASS 1

/* GA102 die ID at BAR0+0; 0xb72000a1 in every probe boot of this card. */
#define GU_BOARD_CHIP 0xb72000a1U

/* Live BAR0+0x300000, byte-identical over three consecutive probe boots. */
#define GU_BOARD_PROM_SHA256 \
  "799d147bfc7ef2c094668b5197c0425eb6692c6b623435a2803b15f9aee80a97"

/* 12288 MiB, read back from FEAT 0x001183a4. */
#define GU_BOARD_FB_MB 0x3000U

/* SS0 (0x0082381c) is boot-unstable: only the common bits are a gate. */
#define GU_BOARD_SS0_MASK 0x01521003U
#define GU_BOARD_SS0_VALUE 0x01521003U

/* SS1 (0x00823820) was 5 in every probe boot; the 10GB profile accepts 2 or 3. */
#define GU_BOARD_SS1_BITS 0x00000020U

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
#define GU_BOARD_HAVE_FB 1
