/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * NVIDIA RTX 3090, PCI 10de:2204, 24576 MiB.
 *
 * Discovery profile. This is GA102, like the two 3080 profiles, so the die ID,
 * the class/revision and the BAR/PROM geometry are the same family values. The
 * subsystem, the VBIOS PROM hash and the cold speed selectors still belong to
 * one physical card, so they stay unconfirmed until the read-only preflight has
 * read them off that card:
 *
 *   1. build the preflight with -Board 2204 -HostProfile discovery
 *   2. boot it cold at least three times, with the monitor on a display that is
 *      NOT this card - the console has to survive the reset
 *   3. put the reported values into ga102_2204.local.h, every HAVE_* = 1
 *   4. rebuild; the transaction image only exists once the profile is confirmed
 *
 * Only this profile changes for a 3090. It takes the same five GA10x assets as
 * the 3080 profiles (gsp_ga10x.bin and the *_ga102.bin set), and the upstream
 * module lists 10de:2204 as a card it has run on.
 *
 * The frame buffer size is a property of the SKU, so it is a live gate even in
 * the preflight: a card that reports anything but 24576 MiB is not this board.
 */
#pragma once

#if __has_include("ga102_2204.local.h")
/* Filled in from this card's own preflight reports (docs/BOARD-PROFILE.md). */
#include "ga102_2204.local.h"
#else

#define GU_BOARD_PROFILE_NAME "ga102_2204 (RTX 3090, unconfirmed)"
#define GU_BOARD_LABEL "RTX 3090"

#define GU_BOARD_ID 0x220410deU

/* Read from config 0x2c by the preflight: AIB-specific. */
#define GU_BOARD_SUBSYSTEM 0x00000000U
#define GU_BOARD_HAVE_SUBSYSTEM 0

/* GA10x parts report 0x030000a1; the preflight prints what this one reports. */
#define GU_BOARD_CLASS_REV 0x030000a1U
#define GU_BOARD_HAVE_CLASS 0

/* GA102 die ID at BAR0+0, as confirmed on three GA102 cards in this repository. */
#define GU_BOARD_CHIP 0xb72000a1U
#define GU_BOARD_HAVE_CHIP 0

/* Live BAR0+0x300000: one VBIOS per board model, so this is per card. */
#define GU_BOARD_PROM_SHA256 ""
#define GU_BOARD_HAVE_PROM 0

/* 24576 MiB at FEAT 0x001183a4. */
#define GU_BOARD_FB_MB 0x6000U
#define GU_BOARD_HAVE_FB 1

/* Cold speed selectors: observed values, not assumptions. */
#define GU_BOARD_SS0_MASK 0xffffffffU
#define GU_BOARD_SS0_VALUE 0x00000000U
#define GU_BOARD_HAVE_SS0 0

#define GU_BOARD_SS1_BITS 0x00000001U
#define GU_BOARD_HAVE_SS1 0

#define GU_BOARD_BAR0_SIZE 0x1000000U
#define GU_BOARD_PROM_OFFSET 0x300000U
#define GU_BOARD_PROM_SIZE 0x100000U

/* VBIOS-placed capability dword; the preflight dumps all 256 config bytes. */
#define GU_BOARD_CAP_OFFSET 0x0c8U
#define GU_BOARD_CAP_VALUE 0x00000000U
#define GU_BOARD_HAVE_CAP 0

#endif
