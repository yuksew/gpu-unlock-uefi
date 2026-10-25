/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Host profile: Gigabyte B850 AORUS ELITE WIFI7 ICE with an RTX 3060 12GB.
 *
 * Confirmed on hardware 2026-10-25: three read-only discovery boots supplied
 * the paths below, and a fourth boot with this profile matched them as a real
 * comparison (evidence/preflight-3060-01..04/REPORT.TXT; GOP[0] route=1). This
 * host is not the one the two
 * GA102 boards were validated on, and it differs in both things a host profile
 * owns:
 *
 *   target BDF=0:5:0.0
 *   target pci_path=Pci(0x2,0x1)/Pci(0x0,0x0)/Pci(0x0,0x0)/Pci(0x0,0x0)
 *   GOP[0] pci_path=Pci(0x1,0x1)/Pci(0x0,0x0)   (AMD iGPU, 1024x768)
 *
 * The display is the platform's integrated GPU at 01:00.0, reached through
 * root port Pci(0x1,0x1) - not the Intel Pci(0x2,0x0) of intel_bus1, and not
 * device 2 of the old hard-coded route. Two PCI nodes, so the match is exact
 * on depth and on every node.
 *
 * The monitor must stay on the motherboard output: the console GOP has to
 * survive the target GPU's FLR, and a GOP on the target itself is never a
 * console candidate.
 */
#pragma once

#define GU_HOST_PROFILE_NAME "amd_bus5 (AMD iGPU console at 01:00.0, GPU at 0:5:0.0, confirmed)"

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
