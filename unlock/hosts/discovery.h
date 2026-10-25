/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Host discovery profile (-DGU_HOST_DISCOVERY).
 *
 * For a host whose topology has not been read by the preflight yet: the
 * target GPU's PCI location and the console GOP's device path are both left
 * unconfirmed. Windows' bus numbers are only a hint; the values that count
 * are the ones UEFI reports, so the preflight logs them:
 *
 *   target BDF=<seg>:<bus>:<dev>.<fn> ...
 *   target pci_path=Pci(0x..,0x..)/...
 *   GOP[n] pci_path=Pci(0x..,0x..)/...
 *
 * While a field is unconfirmed the preflight lets it through - the display
 * only if its path is well formed and does not lead to the target GPU - and
 * port.h refuses to build a write-capable image. To confirm the host, copy
 * the logged values into hosts/discovery.local.h (same macros, HAVE_* = 1),
 * keeping the comment that names the report they came from, and rebuild.
 */
#pragma once

/* GU_PROFILE_DISCOVERY forces the fallback, as for the board profiles. */
#if __has_include("discovery.local.h") && !defined(GU_PROFILE_DISCOVERY)
#include "discovery.local.h"
#else

#define GU_HOST_PROFILE_NAME "discovery (host topology unconfirmed)"

#define GU_HOST_TARGET_SEGMENT 0U
#define GU_HOST_TARGET_BUS 0U
#define GU_HOST_TARGET_DEVICE 0U
#define GU_HOST_TARGET_FUNCTION 0U
#define GU_HOST_HAVE_TARGET 0

#define GU_HOST_DISPLAY_DEPTH 0U
#define GU_HOST_DISPLAY_PCI {{0x00, 0x00}}
#define GU_HOST_HAVE_DISPLAY 0

#endif
