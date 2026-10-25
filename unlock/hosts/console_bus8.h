/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Host profile: console on the adapter the firmware reports at 08:00.0,
 * target GPU at 0:1:0.0.
 *
 * Confirmed on hardware 2026-10-25 by three read-only discovery boots on the
 * host where the RTX 3090 is the target (evidence/preflight-3090-01..03). The
 * same three reports agreed on both paths, which the preflight records as:
 *
 *   target BDF=0:1:0.0
 *   target pci_path=Pci(0x1,0x1)/Pci(0x0,0x0)
 *   console GOP pci_path=Pci(0x8,0x1)/Pci(0x0,0x0)      (08:00.0)
 *
 * Both are two PCI nodes below the root, behind root ports 00:01.1 and 00:08.1,
 * so the match is exact on depth and on every node. The profile gates the
 * console *path*, not the model behind it: what the gate has to guarantee is
 * that the console is not on the target and that it survives the target's FLR.
 * The adapter at 08:00.0 is whichever output the monitor was moved to for the
 * run that produced these reports.
 *
 * This is not the inverse of hosts/nvidia_bus5.h and the two must not be
 * confused: there the 3090 held the console and the 3060 was the target, here
 * the 3090 is the target. Keeping both lets either card be unlocked on this PC
 * without re-deriving the paths.
 */
#pragma once

#define GU_HOST_PROFILE_NAME "console_bus8 (console at 08:00.0, target at 0:1:0.0, confirmed)"

/* GetLocation() of the target function: segment, bus, device, function. */
#define GU_HOST_TARGET_SEGMENT 0U
#define GU_HOST_TARGET_BUS 1U
#define GU_HOST_TARGET_DEVICE 0U
#define GU_HOST_TARGET_FUNCTION 0U
#define GU_HOST_HAVE_TARGET 1

/* The display GOP's device path below PciRoot: one {device, function} pair
   per PCI node, root first. Trailing ACPI _ADR (output) nodes are allowed. */
#define GU_HOST_DISPLAY_DEPTH 2U
#define GU_HOST_DISPLAY_PCI {{0x08, 0x01}, {0x00, 0x00}}
#define GU_HOST_HAVE_DISPLAY 1
