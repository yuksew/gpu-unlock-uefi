/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * NVIDIA RTX 3060 12GB (GA106-300), PCI 10de:2504, 12288 MiB.
 *
 * This is the discovery profile. The device ID and the frame-buffer size are
 * properties of the SKU; everything else is a property of one physical card -
 * its AIB subsystem ID, its VBIOS (PROM hash and capability layout), its die
 * ID and its cold speed-selector bits - and is therefore left unconfirmed
 * until the read-only preflight has read it off the card:
 *
 *   1. build the preflight image for this profile
 *   2. boot it once (twice more if SS0 differs between boots)
 *   3. copy the reported values into ga106_2504.local.h, all HAVE_* = 1,
 *      as docs/BOARD-PROFILE.md describes
 *   4. rebuild; the transaction image is only buildable from a confirmed profile
 *
 * Until step 3 creates ga106_2504.local.h, this header keeps every unconfirmed
 * field flagged 0 and the transaction build refuses to compile. GU_BOARD_CHIP
 * carries the GA10x-family candidate for GA106 so the probe can report
 * match/mismatch, not so it can be trusted: the die ID still has to be read.
 *
 * The five firmware assets are family-wide: the upstream module loads the same
 * gsp_ga10x.bin / *_ga102.bin set for GA102, GA104 and GA106, and the RTX 3060
 * 12GB is the one GA106 board reported working there. Only this profile changes.
 */
#pragma once

/* GU_PROFILE_DISCOVERY forces the fallback below even when a local
   profile exists, so the host tests can prove that an unconfirmed profile
   still refuses to build a write-capable image. */
#if __has_include("ga106_2504.local.h") && !defined(GU_PROFILE_DISCOVERY)
/* Filled in from this card's own preflight reports (docs/BOARD-PROFILE.md). */
#include "ga106_2504.local.h"
#else

#define GU_BOARD_PROFILE_NAME "ga106_2504 (NVIDIA RTX 3060 12GB, unconfirmed)"
#define GU_BOARD_LABEL "NVIDIA RTX 3060 12GB"

#define GU_BOARD_ID 0x250410deU

/* Read from config 0x2c by the preflight: AIB-specific. */
#define GU_BOARD_SUBSYSTEM 0x00000000U
#define GU_BOARD_HAVE_SUBSYSTEM 0

/* GA10x parts report 0x030000a1; the preflight prints what this one reports. */
#define GU_BOARD_CLASS_REV 0x030000a1U
#define GU_BOARD_HAVE_CLASS 0

/* GA10x-family candidate for the GA106 die ID at BAR0+0. Not confirmed. */
#define GU_BOARD_CHIP 0xb76000a1U
#define GU_BOARD_HAVE_CHIP 0

/* Live BAR0+0x300000: one VBIOS per board model, so this is per card. */
#define GU_BOARD_PROM_SHA256 ""
#define GU_BOARD_HAVE_PROM 0

/* 12288 MiB at FEAT 0x001183a4. This one is a SKU property, so it is a live
   gate even in the preflight: a card that reports anything else is not the
   board this profile describes, and its report cannot confirm the profile. */
#define GU_BOARD_FB_MB 0x3000U
#define GU_BOARD_HAVE_FB 1

/* Cold speed selectors: observed values, not assumptions. */
#define GU_BOARD_SS0_MASK 0xffffffffU
#define GU_BOARD_SS0_VALUE 0x00000000U
#define GU_BOARD_HAVE_SS0 0

#define GU_BOARD_SS1_BITS 0x00000000U
#define GU_BOARD_HAVE_SS1 0

#define GU_BOARD_BAR0_SIZE 0x1000000U
#define GU_BOARD_PROM_OFFSET 0x300000U
#define GU_BOARD_PROM_SIZE 0x100000U

/* VBIOS-placed capability dword; the preflight dumps all 256 config bytes. */
#define GU_BOARD_CAP_OFFSET 0x0c8U
#define GU_BOARD_CAP_VALUE 0x00000000U
#define GU_BOARD_HAVE_CAP 0

#endif
