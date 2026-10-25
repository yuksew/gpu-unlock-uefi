#pragma once

#include "port.h"

typedef struct {
  UINT16 pm, pcie, msi, msix;
  UINT8 pcie_version;
} GU_CAPS;

/* Config is a complete, CPU-owned 256-byte snapshot. Out is cleared on failure. */
BOOLEAN GuCheckConfig(const UINT32 Config[64], UINTN Segment, UINTN Bus,
                      UINTN Device, UINTN Function, GU_CAPS *Out);

/* Most PCI nodes a parsed path may hold: root port, switch ports, endpoint. */
#define GU_PATH_MAX_PCI 8U

/* Path must point to readable memory through its terminator, up to MaxBytes.
   MaxBytes is a read bound, not necessarily the exact allocation length.
   Accepts exactly PciRoot (PNP0A03/PNP0A08, UID 0), then 1..GU_PATH_MAX_PCI
   Pci() nodes, then any ACPI _ADR nodes, then a single-instance end. Writes
   one {device, function} pair per Pci() node, root first, and returns their
   count; returns 0 for any other shape, leaving Nodes unspecified. */
UINTN GuPathPci(const EFI_DEVICE_PATH_PROTOCOL *Path, UINTN MaxBytes,
                UINT8 Nodes[GU_PATH_MAX_PCI][2]);

/* Same bounds as GuPathPci. Returns 1 when the PCI chain is the host
   profile's confirmed display (GU_HOST_DISPLAY_PCI), 2 for the GA102 boards'
   validated RTX route Pci(0x1,0x0)/Pci(0x0,0x0), and 0 otherwise - always 0
   for route 1 while the host's display is unconfirmed. */
int GuDisplayPath(const EFI_DEVICE_PATH_PROTOCOL *Path, UINTN MaxBytes);
