/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Host profile: the validated hosts of the 2206 and 220a boards.
 *
 * A host profile describes where the platform firmware puts things, not what
 * the card is: the PCI location of the target GPU and the one firmware
 * display (GOP) that must keep the console alive while the target is reset.
 * On both validated hosts the GPU sat behind root port 00:01.0 at 0:1:0.0 and
 * the console ran on the Intel iGPU at PciRoot/Pci(0x2,0x0); see
 * evidence/unlock-boot-09/REPORT.TXT ("target BDF=0:1:0.0", "GOP[0] ...
 * route=1").
 */
#pragma once

#define GU_HOST_PROFILE_NAME "intel_bus1 (Intel iGPU console, GPU at 0:1:0.0, confirmed)"

/* GetLocation() of the target function: segment, bus, device, function. */
#define GU_HOST_TARGET_SEGMENT 0U
#define GU_HOST_TARGET_BUS 1U
#define GU_HOST_TARGET_DEVICE 0U
#define GU_HOST_TARGET_FUNCTION 0U
#define GU_HOST_HAVE_TARGET 1

/* The display GOP's device path below PciRoot: one {device, function} pair
   per PCI node, root first. Trailing ACPI _ADR (output) nodes are allowed. */
#define GU_HOST_DISPLAY_DEPTH 1U
#define GU_HOST_DISPLAY_PCI {{0x02, 0x00}}
#define GU_HOST_HAVE_DISPLAY 1
