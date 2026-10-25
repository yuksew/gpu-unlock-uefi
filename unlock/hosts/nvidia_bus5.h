/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Host profile: console on a second NVIDIA card, target GPU at 0:5:0.0.
 *
 * Confirmed on hardware 2026-10-25, first by the read-only preflight and then
 * by a transaction: three discovery boots supplied the paths below
 * (evidence/preflight-3060-01..03), a fourth matched them as a real comparison
 * (evidence/preflight-3060-04, GOP[0] route=1), and the transaction of the same
 * day released the target's UEFI driver and reset the target while the console
 * stayed alive on the other card (evidence/unlock-3060-01).
 *
 * That last part is the configuration the inherited notes leave untested: the
 * transaction calls DisconnectController on the NVIDIA UEFI driver instance
 * that owns the *target* controller, while the same driver image keeps serving
 * the console on a different card. It works here because the two cards are
 * separate PciIo controllers, and every recheck of the display after the
 * release passed rather than assuming it.
 *
 *   target BDF=0:5:0.0
 *   target pci_path=Pci(0x2,0x1)/Pci(0x0,0x0)/Pci(0x0,0x0)/Pci(0x0,0x0)
 *   console GOP pci_path=Pci(0x1,0x1)/Pci(0x0,0x0)      (01:00.0)
 *
 * The console therefore sits two PCI nodes below the root, behind a root port
 * at 00:01.1, so the match is exact on depth and on every node. Keep the
 * monitor on that card: a GOP on the target itself is never a console
 * candidate, and the console has to survive the target's FLR.
 */
#pragma once

#define GU_HOST_PROFILE_NAME "nvidia_bus5 (console on a second NVIDIA card at 01:00.0, target at 0:5:0.0, confirmed)"

/* GetLocation() of the target function: segment, bus, device, function. */
#define GU_HOST_TARGET_SEGMENT 0U
#define GU_HOST_TARGET_BUS 5U
#define GU_HOST_TARGET_DEVICE 0U
#define GU_HOST_TARGET_FUNCTION 0U
#define GU_HOST_HAVE_TARGET 1

/* The display GOP's device path below PciRoot: one {device, function} pair
   per PCI node, root first. Trailing ACPI _ADR (output) nodes are allowed. */
#define GU_HOST_DISPLAY_DEPTH 2U
#define GU_HOST_DISPLAY_PCI {{0x01, 0x01}, {0x00, 0x00}}
#define GU_HOST_HAVE_DISPLAY 1
