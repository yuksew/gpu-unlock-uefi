/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Test fixture: a board profile with nothing confirmed on a card.
 *
 * This is not a board. It exists so the tests can prove, without hardware,
 * that a profile with unconfirmed fields cannot be built into a write-capable
 * image and that the preflight still records a card it does not know. It uses
 * the RTX 3060 12GB device ID only so it can be selected with the same switches
 * as a real profile; every field a card has to confirm is flagged 0 here.
 *
 * Never add a value to this file. A real profile is what a card's own reports
 * produce; see profiles/ga106_2504.h for that.
 */
#pragma once

#define GU_BOARD_PROFILE_NAME "unconfirmed fixture (test only)"
#define GU_BOARD_LABEL "unconfirmed fixture"

#define GU_BOARD_ID 0x250410deU
#define GU_BOARD_SUBSYSTEM 0x00000000U
#define GU_BOARD_CLASS_REV 0x030000a1U

#define GU_BOARD_CHIP 0xb76000a1U
#define GU_BOARD_PROM_SHA256 ""

#define GU_BOARD_FB_MB 0x3000U

#define GU_BOARD_SS0_MASK 0xffffffffU
#define GU_BOARD_SS0_VALUE 0x00000000U
#define GU_BOARD_SS1_BITS 0x00000001U

#define GU_BOARD_BAR0_SIZE 0x1000000U
#define GU_BOARD_PROM_OFFSET 0x300000U
#define GU_BOARD_PROM_SIZE 0x100000U

#define GU_BOARD_CAP_OFFSET 0x0c8U
#define GU_BOARD_CAP_VALUE 0x00000000U

#define GU_BOARD_HAVE_CHIP 0
#define GU_BOARD_HAVE_SUBSYSTEM 0
#define GU_BOARD_HAVE_PROM 0
#define GU_BOARD_HAVE_SS0 0
#define GU_BOARD_HAVE_SS1 0
#define GU_BOARD_HAVE_CAP 0
#define GU_BOARD_HAVE_CLASS 0
#define GU_BOARD_HAVE_FB 1
