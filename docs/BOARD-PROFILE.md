# Board and host profiles

**English** | [日本語](BOARD-PROFILE.ja.md)

Every value the UEFI application compares against before it writes to a GPU comes from two compile-time profiles:

- a **board profile** (`unlock/profiles/`) describes one physical card: its PCI identity, VBIOS, die ID and cold-state registers;
- a **host profile** (`unlock/hosts/`) describes where the platform firmware puts things: the target GPU's PCI location and the one firmware display (GOP) that keeps the console alive while the target is reset.

Each field has a `GU_BOARD_HAVE_*` or `GU_HOST_HAVE_*` flag. A field is *confirmed* (flag 1) only when its value was read off that card or that host by the preflight, never copied from another board or assumed from the SKU.

| Profile | File | Select with | State |
|---|---|---|---|
| MSI RTX 3080 10GB, `10de:2206` | `profiles/ga102_2206.h` | default (or `-DGU_BOARD_2206`) | confirmed |
| MSI RTX 3080 12GB, `10de:220a` | `profiles/ga102_220a.h` | `-DGU_BOARD_220A` | confirmed |
| RTX 3060 12GB, `10de:2504` | `profiles/ga106_2504.h` | `-DGU_BOARD_GA106_2504` | confirmed 2026-10-25, one transaction |
| MSI RTX 3090, `10de:2204` | `profiles/ga102_2204.h` | `-DGU_BOARD_2204` | confirmed 2026-10-25, one transaction |
| Test fixture, unconfirmed | `profiles/unconfirmed.h` | `-DGU_BOARD_UNCONFIRMED` | never a board |
| Intel iGPU console, GPU at `0:1:0.0` | `hosts/intel_bus1.h` | default | confirmed |
| Second NVIDIA card holds the console at `01:00.0`, target at `0:5:0.0` | `hosts/nvidia_bus5.h` | `-DGU_HOST_NVIDIA_BUS5` | confirmed 2026-10-25 |
| AMD iGPU console at `08:00.0`, target at `0:1:0.0` | `hosts/amd_igpu_bus1.h` | `-DGU_HOST_AMD_IGPU_BUS1` | confirmed 2026-10-25 |
| Any other host | `hosts/discovery.h` | `-DGU_HOST_DISCOVERY` | discovery: unconfirmed |

`unlock/build.ps1` takes `-Board 2206|220a|2204|ga106_2504`, `-HostProfile intel_bus1|nvidia_bus5|amd_igpu_bus1|discovery` and `-Preflight`, and records all three in `build/build-manifest.json`.

## Why an unconfirmed profile cannot build a transaction image

`port.h` computes `GU_BOARD_COMPLETE` and `GU_HOST_COMPLETE` as the AND of their `HAVE_*` flags. A flag the profile forgot to define reads as 0, so an omission makes a profile incomplete rather than trusted. Unless `GU_PREFLIGHT` is defined, an incomplete board or host stops the compiler with an `#error`:

```text
port.h: error: "This board profile is not confirmed on a card yet. ..."
port.h: error: "This host profile is not confirmed yet. ..."
```

The gates exist so the transaction only writes to the card it was written for, in the state it was validated in. A gate with nothing confirmed to compare against cannot do that, so the only image an unconfirmed profile can produce is the read-only preflight. In that image an unconfirmed field is skipped, not passed: the report logs

```text
gate <name>: PENDING (profile value not confirmed on this card)
```

and the summary says `PREFLIGHT profile=... host=... complete=0`. `GU_PROFILE_GATE()` in `port.h` is the single definition of this rule; outside the preflight it rejects, as a backstop behind the `#error`.

`GU_TARGET_PCI_ID`, the ID the vendor transaction re-checks across the FLR, is derived from `GU_BOARD_ID`. Passing a `-DGU_TARGET_PCI_ID` that names another board is an `#error`, and so is selecting two board profiles.

## Board fields

| Field | Gate | Read from in `REPORT.TXT` |
|---|---|---|
| `GU_BOARD_ID` | config `0x00` (`vendor \| device << 16`); also how the target is found, never pending | `identity: pci=` |
| `GU_BOARD_SUBSYSTEM` / `HAVE_SUBSYSTEM` | config `0x2c`, the AIB's own ID | `identity: subsystem=` |
| `GU_BOARD_CLASS_REV` / `HAVE_CLASS` | config `0x08`, class and revision | `identity: class_rev=` |
| `GU_BOARD_CAP_OFFSET`, `GU_BOARD_CAP_VALUE` / `HAVE_CAP` | VBIOS-placed capability dword (`0xc8` on GA102) | `config[c0]=` line, third dword for `0xc8` |
| `GU_BOARD_BAR0_SIZE` | BAR0 alignment in config space and size in the firmware BAR descriptor | `bar0 descriptor: length=` |
| `GU_BOARD_CHIP` / `HAVE_CHIP` | die ID at BAR0+0 | `chip: actual=` |
| `GU_BOARD_PROM_OFFSET`, `GU_BOARD_PROM_SIZE` | the live PROM window that is hashed (BAR0+`0x300000`, 1 MiB) | —|
| `GU_BOARD_PROM_SHA256` / `HAVE_PROM` | SHA-256 of that window: the card's VBIOS | `live PROM hash sha256=` |
| `GU_BOARD_FB_MB` / `HAVE_FB` | frame-buffer MiB at FEAT `0x001183a4` | `cold state: ... fb_mb=` |
| `GU_BOARD_SS0_MASK`, `GU_BOARD_SS0_VALUE` / `HAVE_SS0` | cold SS0 (`0x0082381c`): `(SS0 & MASK) == VALUE` | `cold state: ... SS0=` |
| `GU_BOARD_SS1_BITS` / `HAVE_SS1` | cold SS1 (`0x00823820`): bit `SS1` must be set | `cold state: ... SS1=` |
| `GU_BOARD_LABEL`, `GU_BOARD_PROFILE_NAME` | text only | —|

The family-level cold gates (GFW boot state, FEAT PLM `0xffffff8f`, WPR2 high) are the same for every GA10x board and are not part of the profile.

**SS0 across boots.** SS0 can change between cold boots: the 220a card showed `0x01521103`, `0x01521013` and `0x01521003` over three boots. Boot the preflight at least three times. If SS0 is identical in every report, use `MASK = 0xffffffffU` and that value. Otherwise use the AND of all observed values as both `MASK` and `VALUE`, as the 220a profile does, so a volatile bit cannot reject the card. An AND of 0 gates nothing; do not confirm a profile from it.

**SS1** is a set of accepted values: set bit `n` for every observed `SS1=n` (the 2206 profile accepts 2 or 3, `0x0000000cU`).

## Host fields

| Field | Gate | Read from in `REPORT.TXT` |
|---|---|---|
| `GU_HOST_TARGET_SEGMENT/BUS/DEVICE/FUNCTION` / `HAVE_TARGET` | `GetLocation()` of the target function | `target BDF=<seg>:<bus>:<dev>.<fn>` |
| `GU_HOST_DISPLAY_DEPTH`, `GU_HOST_DISPLAY_PCI` / `HAVE_DISPLAY` | the console GOP's device path below `PciRoot`, one `{device, function}` per `Pci()` node, root first | `GOP[n] pci_path=` of the accepted GOP |

The display check requires exactly one GOP that matches the console path, a valid mode on it and a matching forwarded console mode. A second GOP on another adapter is logged and ignored, because releasing the target's driver only blanks the target's own outputs. What is never allowed is the console itself sitting at or below the target GPU, which is refused whatever the profile says. With an unconfirmed host the preflight additionally logs `target pci_path=` and logs `pci_path=` for every GOP.

Windows' bus numbers are only a hint. The values that count are the ones UEFI reports in the preflight.

**Validated hosts:** `intel_bus1` (Intel iGPU console) ran the two GA102 boards. `nvidia_bus5` (console on a second NVIDIA card at `01:00.0`, target at `0:5:0.0`) ran an RTX 3060 transaction on 2026-10-25: the target's UEFI driver was released and the card reset while the console kept running on the other card. `amd_igpu_bus1` is the inverse layout on the same PC, confirmed by the RTX 3090 preflight and then by two transactions: there the 3090 is the target at `0:1:0.0` and the console is the AMD integrated GPU the firmware reports at `08:00.0`. The second transaction ran with a monitor also attached to the 3090, so the target carried a spare GOP; the display check ignored it, the run reached `clean=1`, and the target's output came back with the Windows driver. That is the configuration to prefer, since the console shares neither a driver nor a card with the target. Both cases matter because the transaction calls `DisconnectController` on the driver instance that owns the *target* controller while the same driver image keeps serving the console on a different card.

## Confirming a profile: preflight, then rebuild

1. **Build the preflight** for the new board on the discovery host:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile discovery -Preflight
   ```

   or `./preflight-linux.sh usb-3060`. Stage it as in [USB-BOOT.md](USB-BOOT.md).
2. **Boot it from a cold start** (power fully off first), at least three times. Each run writes `\GPU-UNLOCK\RUNxx\REPORT.TXT`. Use a report only if it ends with `PREFLIGHT result=pass` and `FINAL status=0`: a gate that failed means the card was not in the state the values describe.
3. **Check the reports agree.** `identity:` (PCI, subsystem, class/revision), `chip: actual=` and `live PROM hash sha256=` must be identical in every report; if they are not, the reports are from different cards or something changed, and nothing should be confirmed. `fb_mb` must also be identical. SS0 and SS1 are combined as described above.
4. **Write the profile.** If the board already has a published profile of its own, put the reported values into that profile, name the `RUNxx` reports in a comment at the top, and set every `GU_BOARD_HAVE_*` to 1 —that is what `profiles/ga106_2504.h` records for the card confirmed on 2026-10-25. For a card that has no published profile, copy the discovery fallback block into `unlock/profiles/<board>.local.h` and do the same there; the local file overrides the shipped one. Either way, leave a field at 0 if any report lacks it: an unconfirmed field is the safe state. Do the same for the host (`GU_HOST_PROFILE_NAME`, the BDF, the display path, both `HAVE_*` = 1), as `hosts/nvidia_bus5.h` records.
5. **Rebuild the preflight and boot it once more.** With the confirmed profiles in place every gate is a real comparison; the report must show no `PENDING` line, `complete=1` and `PREFLIGHT result=pass`. This is also the only way to catch a cold-state value that varies between boots: the runs so far were used to build the mask, this one tests it.
6. **Only then** build the transaction image (no `-Preflight`). `build-manifest.json` records the profile names and, for a `*.local.h` profile, that file's hash —those are not tracked by Git.

`*.local.h` files are ignored by Git on purpose: they describe one card and one PC. To publish a confirmed board, add it as its own profile under `unlock/profiles/` with the reports it came from, as the 2206 and 220a profiles do.
