# RTX 3080 Unlock

**English** | [日本語](README.ja.md) | [한국어](README.ko.md)

A UEFI compute-unlock experiment for Windows, validated on an MSI RTX 3080 10GB and, in the 12GB (`220a`) profile, on a 12GB card of the same family. A UEFI application runs from a USB stick and then hands control straight back to Windows Boot Manager, without a reboot in between.

**The supported scope is only the experimental environment listed below.** This is not a general-purpose tool for RTX 3080 cards from other vendors, nor for the RTX 3090. If the device, the display path or the firmware checks do not match, the application refuses to run.

## What gets faster, and what does not

The unlock restores the FP16 tensor path. It does not change VRAM bandwidth, FP32 throughput or the quantized kernels, so the gain depends entirely on how much of a workload goes through that path.

| Workload | Change | Why |
| --- | ---: | --- |
| Dense BF16 / FP16 GEMM | +70–86% | Tensor path doubles per clock |
| TF32 GEMM | +80% | Same path |
| llama.cpp prefill, IQ2 model | +8–11% | Most matmuls run through the quantized kernels, which are not patched |
| llama.cpp generation | 0% | Bound by memory bandwidth, which the unlock does not change |
| FP32 GEMM | −0.6% to +7% | Not a patched path; the spread is clock and harness variation |

Measurements and their caveats are in the sections below.

## Running it from USB

1. Prepare a FAT32 USB stick and the required files as described in [USB preparation and booting](docs/USB-BOOT.md).
2. Select the USB stick in the UEFI boot menu.
3. When the read-only checks finish and `READY` is shown, press **U once**.
4. After success, control returns to the firmware menu: choose **Windows Boot Manager**. Do not reboot in between.

The USB template contains the EFI application, the sources, usage notes and licenses. The five runtime assets must be added locally; `unlock/assets.json` lists their file names, sizes and SHA256 values. A template copied as-is cannot run.

## Tested environment

- MSI RTX 3080 10GB: PCI `10de:2206`, subsystem `1462:3890`, VBIOS `94.02.59.00.aa`.
- MSI RTX 3080 12GB: PCI `10de:220a`, subsystem `1462:3896`, 12288 MiB, PROM SHA256 `799d147b…`. Preflight and one transaction run only.
- ASUS TUF GAMING B660M-PLUS D4, Intel i5-13600K / UHD 770 for display output.
- Windows 11 Pro build 26200, NVIDIA 610.60 driver.
- v0.10 real run: UEFI transaction and cleanup completed, then CUDA measurements on the Windows side.

The current code also checks BDF `0000:01:00.0`, BARs, the board PROM and the Intel display path. The same product name on a different PC has not been validated. Booting from a freshly built FAT32 USB stick is still a separate validation item.

## Measured results

| Measurement | Before (TFLOPS) | After (TFLOPS) | Change |
| --- | ---: | ---: | ---: |
| FP32 pedantic | 22.057145 | 21.922047 | −0.61% |
| FP16 Tensor / FP32 accumulate | 60.874030 | 109.217226 | +79.42% |
| BF16 Tensor / FP32 accumulate | 60.910719 | 113.090926 | +85.67% |

These are 4096×4096 GEMM measurements taken with the same executable and the same CUDA/cuBLAS environment. The baseline is a single process; the post-unlock value is the median of three process medians. They do not imply game performance, the speedup of every AI workload, long-run stability, or persistence across a cold boot. [Measurement data](results/benchmark.json) and the [real UEFI run log](evidence/unlock-boot-09/REPORT.TXT) are included. Personal paths and device UUIDs were removed from the measurement data.

## PyTorch measurements (matmul, FP32 accumulate)

| Kind | Before | After | Change | SM clock (before → after) |
| --- | ---: | ---: | ---: | --- |
| BF16 4096 | 58.6 | 101.0 | +72% | 1815 → 1530 MHz |
| FP16 4096 | 57.4 | 97.7 | +70% | 1800 → 1485 MHz |
| BF16 8192 | 61.4 | 112.7 | +84% | 1905 → 1643 MHz |
| FP16 8192 | 60.5 | 109.6 | +81% | 1875 → 1605 MHz |
| TF32 4096 | 28.3 | 51.1 | +80% | 1905 → 1635 MHz |
| FP32 4096 (reference) | 17.8 | 19.1 | +7% | 1515 → 1485 MHz |

TFLOPS, PyTorch matmul with FP32 accumulation, same card and the same harness. The before column was measured on this card before the unlock, in an earlier session. Raw values are in [results/pytorch-matrix.json](results/pytorch-matrix.json).

Normalising by the SM clock makes the mechanism visible:

- BF16, FP16 and TF32 reach **×2.0–2.1 TFLOPS per clock** after the unlock (32.3 → 66.0, 31.9 → 65.8, 32.3 → 68.6, 32.3 → 68.3, 14.9 → 31.3 TFLOPS/GHz). All tensor paths double per clock, which is what restoring a half-rate tensor path looks like.
- The absolute gain is only 70–84% because the unlocked runs settle at lower clocks (1815 → 1530 MHz, and so on). The per-clock throughput doubled and the clock gave part of it back. The reason for the lower clocks has not been measured directly; the power budget is the plausible explanation.
- FP32 at 4096 moves ×1.09 per clock. FP32 does not use the patched path, so read it as clock and measurement variation rather than an unlock effect. It also disagrees with the cuBLAS `fp32_pedantic` run above (−0.61%), which is a different harness.

## llama.cpp prefill (Saluki 27B IQ2)

| Prompt length | Prefill before | Prefill after | Change | Generation |
| --- | ---: | ---: | ---: | --- |
| 512 tokens | 849 tok/s | 945 tok/s | +11% | unchanged |
| 2048 tokens | 959 tok/s | 1042 tok/s | +9% | unchanged |
| 8192 tokens | 1006 tok/s | 1086 tok/s | +8% | unchanged |

`llama-server` (llama.cpp), Saluki 27B at IQ2, prefill throughput. Same card before and after the unlock. Data: [results/llm-prefill.json](results/llm-prefill.json).

- The gain is about a tenth, not a doubling. An IQ2 model runs most of its matmuls through the quantized kernels, which do not use the patched FP16 tensor path. What improves is the part that still runs in FP16, such as parts of attention.
- Generation speed does not move, which is what the memory-bandwidth limit predicts: decoding one token at a time is bandwidth-bound, and the unlock does not change VRAM bandwidth.
- No per-kernel profile was taken, so the split above is read off the numbers rather than measured.

## Build and test

Windows development environment: LLVM/clang 12 + lld-link (VS2019 BuildTools), an MSVC environment (VS2022 Community), Python 3.9 or newer. The default tool paths in the scripts assume this installation.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\test-all.ps1
```

The build produces `unlock/build/BOOTX64.EFI` and checks the PE format. `build.ps1` takes `-Board 2206|220a|ga106_2504`, `-HostProfile intel_bus1|discovery` and `-Preflight` (see [Adding another board](#adding-another-board)) and records them in `unlock/build/build-manifest.json`; `GU_LLVM_ROOT` overrides the LLVM path, and `GU_CLANG` / `GU_VCVARS` do the same for `test-port.cmd`. The test commands do not touch the GPU. For the full simulated test that needs extra fixtures, and for packaging, see [TESTING.md](TESTING.md).

## 12GB (220a) profile and the read-only preflight

`unlock/port.h` selects the board profile at compile time. The default is the 10GB card validated above (`10de:2206` / `1462:3890`); `-DGU_BOARD_220A` selects the 12GB card (`10de:220a` / `1462:3896`, 12288 MiB, PROM SHA256 `799d147b…`). Every expected value in both profiles comes from that card's own firmware dump, so the two must never be mixed.

```bash
./preflight-linux.sh preflight     # build/preflight-12gb/BOOTX64.EFI
./preflight-linux.sh transaction   # build/transaction-12gb/BOOTX64.EFI
./preflight-linux.sh all           # both of the above + the validated 10GB transaction
./preflight-linux.sh usb           # build/usb-preflight/EFI/BOOT/BOOTX64.EFI
```

The `preflight` build passes `-DGU_PREFLIGHT` and drops the ARM/transaction code at link time. It only reads config space, firmware BAR descriptors and the PROM window over MMIO, writes nothing to the GPU, and closes the exclusive PciIo open as soon as each check is done. On failure, the gate that stopped it is printed on the console and recorded in `REPORT.TXT`. Gate expectations for the 12GB card, the offline replay tool (`analysis/r769/preflight_gate_replay.py`) and the run procedure are documented in `analysis/r769/preflight-12gb.md` in the workspace.

Hardware status of the 12GB profile: the read-only preflight passes all ten gates, and one transaction run completed with `status=0 stage=transaction attempted=1 driver_release=1 clean=1` (`FLR=1`). After that run FP16 Tensor GEMM measured 109.5 TFLOPS, against 60.5–61.4 TFLOPS measured on this same card before the unlock in an earlier session. The tables above are not interleaved runs, so read the ratios as an estimate. The patch lives only in VRAM for the current boot and is gone after a reboot.

## Adding another board

Every board-specific expectation lives in a board profile (`unlock/profiles/`), and everything the PC decides — where the GPU sits on PCI and which firmware display keeps the console — lives in a host profile (`unlock/hosts/`). [docs/BOARD-PROFILE.md](docs/BOARD-PROFILE.md) lists the fields and the procedure: build the read-only preflight from a discovery profile, boot it from cold a few times, copy the values it reports into `*.local.h`, and rebuild. A profile with any field still unconfirmed builds only the preflight; the transaction image refuses to compile.

The five firmware assets do not change per board. They are GA10x-family files: the upstream module (`kernel/gpu_unlock/unlock_610.c`) loads the same `gsp_ga10x.bin` and `*_ga102.bin` set for every GA10x card it supports, and an RTX 3060 12GB (`10de:2504`, GA106) has been reported working with it there. `unlock/assets.json` and the asset hashes stay as they are.

**RTX 3060 12GB (`ga106_2504`): preflight confirmed on hardware, no transaction run yet.** Confirmed on 2026-10-25 from three read-only cold boots on a Gigabyte B850 AORUS ELITE WIFI7 ICE:

| Field | Value | Source line in `REPORT.TXT` |
| --- | --- | --- |
| subsystem | `0x153610de` | `identity: pci=0x250410de subsystem=0x153610de` |
| class/revision | `0x030000a1` | `identity: ... class_rev=0x30000a1` |
| die ID | `0xb76000a1` | `chip: actual=0xb76000a1` |
| PROM SHA256 | `b1608213…` | `live PROM hash sha256=b1608213…` |
| cold SS0 | `0x12210024` / `0x12011024` | `cold state: … SS0=…` |
| cold SS1 | `3` | `cold state: … SS1=0x3` |
| capability dword | `0xc8 = 0x00050011` | `config[c0]=… 00050011 …` |
| frame buffer | `0x3000` = 12288 MiB | `cold state: … fb_mb=0x3000` |

SS0 is boot-unstable on this card, exactly as on the 12GB GA102 board, so only the bits common to every observed cold boot are a gate (`0x12010024`). The card sits at `0:5:0.0`, which is not the validated host's bus, and the console runs on the AMD iGPU at `Pci(0x1,0x1)/Pci(0x0,0x0)` — hence the `amd_bus5` host profile. No transaction has been run on a 3060: the write-capable image builds and every gate is confirmed, but nothing has been written to the card yet.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile amd_bus5 -Preflight
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile amd_bus5
```

```bash
./preflight-linux.sh preflight-3060     # build/preflight-3060/BOOTX64.EFI
./preflight-linux.sh usb-3060           # build/usb-preflight-3060/EFI/BOOT/BOOTX64.EFI
./preflight-linux.sh transaction-3060   # build/transaction-3060/BOOTX64.EFI
```

Another 3060 12GB (different AIB, different VBIOS) must not reuse those values: it runs `-HostProfile discovery -Preflight`, and its own `ga106_2504.local.h` / `hosts/discovery.local.h` override the shipped files. `-DGU_BOARD_UNCONFIRMED` is a test fixture, never a board.

## Provenance and license

The port and the reused core of this project are **GPL-2.0-only**. The core code comes from [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock/tree/0ebdb1c6308b6a2072be5bf04513472942541956). The bundled EDK2 headers and related notices keep their own licenses.

See [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md).
