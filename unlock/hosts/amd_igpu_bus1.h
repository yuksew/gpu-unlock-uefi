/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Host profile: console on the platform's integrated GPU, target GPU at
 * 0:1:0.0.
 *
 * Confirmed on hardware 2026-10-25 by three read-only discovery boots and a
 * transaction on the host where the RTX 3090 is the target
 * (evidence/preflight-3090-01..03 and evidence/unlock-3090-01). The preflight
 * records both paths, and all three discovery boots agreed on them:
 *
 *   target BDF=0:1:0.0
 *   target pci_path=Pci(0x1,0x1)/Pci(0x0,0x0)
 *   console GOP pci_path=Pci(0x8,0x1)/Pci(0x0,0x0)      (08:00.0)
 *
 * The console is the AMD integrated GPU: the monitor is on the motherboard's
 * output, and the firmware reports that adapter at 08:00.0 behind root port
 * 00:08.1. This is the configuration to prefer, because the console is not
 * sharing a driver or a card with the target: the transaction only touches the
 * 3090 at 01:00.0.
 *
 * The profile was first shipped as `console_bus8`, before the adapter behind
 * 08:00.0 was identified; evidence/preflight-3090-04/REPORT.TXT still carries
 * that name because a log is not edited after the fact. The values and gates
 * are unchanged.
 *
 * Both paths are two PCI nodes below the root, so the match is exact on depth
 * and on every node. The profile gates the console path, not the model behind
 * it: what the gate has to guarantee is that the console is not on the target
 * and that it survives the target's FLR.
 */
#pragma once

#define GU_HOST_PROFILE_NAME "amd_igpu_bus1 (AMD iGPU console at 08:00.0, target at 0:1:0.0, confirmed)"

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
