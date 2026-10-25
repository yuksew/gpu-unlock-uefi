#pragma once
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/DevicePath.h>
#include <stddef.h>

/* Board profile. One profile per physical board, selected at compile time;
   every expected value in it was read from that board's own firmware dump and
   cold-state probe, so two profiles must never be mixed. The default is the
   first card validated end to end (MSI RTX 3080 10GB).
     -DGU_BOARD_2206          MSI RTX 3080 10GB  (10de:2206 / 1462:3890)
     -DGU_BOARD_220A          MSI RTX 3080 12GB  (10de:220a / 1462:3896)
     -DGU_BOARD_GA106_2504    RTX 3060 12GB      (10de:2504)
     -DGU_BOARD_2204          RTX 3090           (10de:2204, unconfirmed)
     -DGU_BOARD_UNCONFIRMED   test fixture only, never a board
   Profiles live in unlock/profiles/; docs/BOARD-PROFILE.md lists the fields. */
#if defined(GU_BOARD_2206) + defined(GU_BOARD_220A) + defined(GU_BOARD_GA106_2504) + \
    defined(GU_BOARD_2204) + defined(GU_BOARD_UNCONFIRMED) > 1
#error "Select exactly one GU_BOARD_* profile"
#endif
#if defined(GU_BOARD_UNCONFIRMED)
#include "profiles/unconfirmed.h"
#elif defined(GU_BOARD_2204)
#include "profiles/ga102_2204.h"
#elif defined(GU_BOARD_GA106_2504)
#include "profiles/ga106_2504.h"
#elif defined(GU_BOARD_220A)
#include "profiles/ga102_220a.h"
#else
#include "profiles/ga102_2206.h"
#endif

/* GU_TARGET_PCI_ID is the dword at config 0x00 (vendor | device << 16) that
   the retained vendor transaction re-checks during the FLR. Deriving it from
   the profile removes the second place a board could be named wrongly. */
#ifndef GU_TARGET_PCI_ID
#define GU_TARGET_PCI_ID GU_BOARD_ID
#elif GU_TARGET_PCI_ID != GU_BOARD_ID
#error "GU_TARGET_PCI_ID and GU_BOARD_ID name different boards"
#endif

/* A profile is complete when every gate value has been read off the card
   itself, not copied from another board or assumed from the SKU. Only a
   complete profile may be built into a write-capable image: the read-only
   preflight exists to turn an incomplete profile into a complete one. A
   HAVE flag the profile forgot to define reads as 0 here, so an omission
   makes the profile incomplete rather than silently trusted. */
#if GU_BOARD_HAVE_CHIP && GU_BOARD_HAVE_SUBSYSTEM && GU_BOARD_HAVE_PROM && \
    GU_BOARD_HAVE_SS0 && GU_BOARD_HAVE_SS1 && GU_BOARD_HAVE_CAP && \
    GU_BOARD_HAVE_CLASS && GU_BOARD_HAVE_FB
#define GU_BOARD_COMPLETE 1
#else
#define GU_BOARD_COMPLETE 0
#endif

/* Host profile, selected the same way. It owns what the platform firmware
   decides rather than the card: where the target GPU sits and which GOP
   keeps the console while the target is reset. The default is the host the
   two GA102 boards were validated on.
     (default)                Intel iGPU console, GPU at 0:1:0.0
     -DGU_HOST_NVIDIA_BUS5    second NVIDIA card holds the console, target 0:5:0.0
     -DGU_HOST_CONSOLE_BUS8   console at 08:00.0, target 0:1:0.0
     -DGU_HOST_DISCOVERY      unconfirmed; preflight only
   Host profiles live in unlock/hosts/. */
#if defined(GU_HOST_NVIDIA_BUS5) + defined(GU_HOST_CONSOLE_BUS8) + defined(GU_HOST_DISCOVERY) > 1
#error "Select exactly one GU_HOST_* profile"
#endif
#if defined(GU_HOST_NVIDIA_BUS5)
#include "hosts/nvidia_bus5.h"
#elif defined(GU_HOST_CONSOLE_BUS8)
#include "hosts/console_bus8.h"
#elif defined(GU_HOST_DISCOVERY)
#include "hosts/discovery.h"
#else
#include "hosts/intel_bus1.h"
#endif
#if GU_HOST_HAVE_TARGET && GU_HOST_HAVE_DISPLAY
#define GU_HOST_COMPLETE 1
#else
#define GU_HOST_COMPLETE 0
#endif

#define GU_PROFILE_COMPLETE (GU_BOARD_COMPLETE && GU_HOST_COMPLETE)
#if !GU_BOARD_COMPLETE && !defined(GU_PREFLIGHT)
#error "This board profile is not confirmed on a card yet. Build the GU_PREFLIGHT image, confirm the profile from its REPORT.TXT (docs/BOARD-PROFILE.md), then rebuild."
#endif
#if !GU_HOST_COMPLETE && !defined(GU_PREFLIGHT)
#error "This host profile is not confirmed yet. Build the GU_PREFLIGHT image, confirm the host from its REPORT.TXT (docs/BOARD-PROFILE.md), then rebuild."
#endif

/* GU_PROFILE_GATE(Have, Match) is the value of one board or host gate. A
   confirmed field must match. An unconfirmed field cannot be a gate - there
   is nothing to compare against - so the preflight lets it through to record
   the card's or host's own value; anywhere else it rejects. The #errors above
   make that branch unreachable outside the preflight; the 0 is the backstop. */
#if defined(GU_PREFLIGHT)
#define GU_PROFILE_PENDING_OK 1
#else
#define GU_PROFILE_PENDING_OK 0
#endif
#define GU_PROFILE_GATE(Have, Match) ((Have) ? (Match) : GU_PROFILE_PENDING_OK)
/* The target function's GetLocation() against the host profile. */
#define GU_HOST_TARGET_AT(S, B, D, F)                                       \
  GU_PROFILE_GATE(GU_HOST_HAVE_TARGET,                                      \
    (S) == GU_HOST_TARGET_SEGMENT && (B) == GU_HOST_TARGET_BUS &&            \
    (D) == GU_HOST_TARGET_DEVICE && (F) == GU_HOST_TARGET_FUNCTION)

typedef struct GU_DMA GU_DMA;
struct GU_DMA {
  GU_DMA *next;
  VOID *cpu, *mapping;
  UINT64 dma;
  UINTN bytes, pages;
  BOOLEAN mapped;
};
typedef struct {
  EFI_BOOT_SERVICES *bs;
  EFI_PCI_IO_PROTOCOL *pci;
  EFI_HANDLE image, handle;
  UINT64 bar_base, bar_size, ticks;
  UINT16 pm_cap, pcie_cap, msi_cap, msix_cap;
  UINT32 saved_config[64];
  UINT16 saved_devctl, saved_devctl2;
  UINT8 pcie_version;
  BOOLEAN fault, retained, started, quiesced, armed, owned, display_ok, saved;
  BOOLEAN master_attributes_pending;
  UINT64 master_original_attributes;
  UINT16 master_original_command;
  EFI_PCI_IO_PROTOCOL *master_pci;
  /* Containment may run at TPL_NOTIFY. Capture diagnostics without file IO;
     preserve first errors even if a later containment retry succeeds. */
  EFI_STATUS master_attribute_status, master_hardware_status;
  UINT64 master_last_attributes;
  UINT16 master_last_command;
  UINT32 flrs, mmio_writes, dma_count;
  GU_DMA *allocations;
  BOOLEAN (*display_check)(VOID);
} GU_PLATFORM;
extern GU_PLATFORM Gu;

VOID GuInit(EFI_BOOT_SERVICES *Bs, EFI_PCI_IO_PROTOCOL *Pci);
BOOLEAN GuTargetConfig(const UINT32 *Config, UINTN Segment, UINTN Bus, UINTN Device, UINTN Function);
BOOLEAN GuSafeToReturn(VOID);
EFI_STATUS GuDmaAllocate(UINTN Bytes, VOID **Cpu, UINT64 *Dma);
EFI_STATUS GuDmaFree(VOID *Cpu, UINT64 Dma);
UINT32 GuRead32(UINT32 Offset);
VOID GuWrite32(UINT32 Offset, UINT32 Value);
EFI_STATUS GuSavePci(VOID);
EFI_STATUS GuRestorePci(VOID);
EFI_STATUS GuFlr(VOID);
EFI_STATUS GuClearMaster(VOID);
EFI_STATUS GuEnableMaster(VOID);
EFI_STATUS GuConfigRead(EFI_PCI_IO_PROTOCOL_WIDTH Width, UINT32 Offset, VOID *Value);
EFI_STATUS GuConfigWrite(EFI_PCI_IO_PROTOCOL_WIDTH Width, UINT32 Offset, VOID *Value);
VOID GuStarted(VOID);
VOID GuQuiesced(VOID);
VOID GuLog(const char *Format, ...);
VOID GuLogText(const char *Text);
VOID GuCopyFromIo(VOID *Destination, const VOID *Source, size_t Bytes);
VOID *GuAlloc(size_t Bytes);
VOID GuFree(VOID *Memory);
VOID GuStall(UINTN Microseconds);
