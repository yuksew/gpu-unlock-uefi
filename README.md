# GPU tensor unlock over UEFI (GA10x)

**English** | [日本語](README.ja.md)

A UEFI application that restores the full FP16/BF16 tensor issue rate on NVIDIA GA10x GeForce cards: the ones shipped with FP32-accumulate tensor maths at half the rate of their professional siblings. It runs from a FAT32 USB stick before Windows starts, writes nothing to the VBIOS, and hands control straight back to Windows Boot Manager without a reboot in between.

Fork of [balaena01/rtx3080-unlock-12g](https://github.com/balaena01/rtx3080-unlock-12g), which is a UEFI port of [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock). This fork adds the GA106 / RTX 3060 12GB board profile, an RTX 3090 discovery profile, a host profile for a console on a second NVIDIA card, and the measured result of one unlock on real hardware.

## Boards and hosts

| Board profile | Select with | Card | State |
| --- | --- | --- | --- |
| `ga102_2206` | default | RTX 3080 10GB, `10de:2206` | confirmed and measured upstream |
| `ga102_220a` | `-Board 220a` | RTX 3080 12GB, `10de:220a` | confirmed upstream, one transaction |
| `ga102_2204` | `-Board 2204` | MSI RTX 3090, `10de:2204` | confirmed here, no transaction yet |
| `ga106_2504` | `-Board ga106_2504` | RTX 3060 12GB, `10de:2504` | confirmed and measured here |

| Host profile | Select with | Console GOP | Target |
| --- | --- | --- | --- |
| `intel_bus1` | default | Intel iGPU at `00:02.0` | `0:1:0.0` |
| `nvidia_bus5` | `-HostProfile nvidia_bus5` | second NVIDIA card at `01:00.0` | `0:5:0.0` |
| `console_bus8` | `-HostProfile console_bus8` | adapter at `08:00.0` | `0:1:0.0` |
| `discovery` | `-HostProfile discovery` | recorded, not required | recorded, not required |

Every gate compares against values read from that card and that PC. Anything that does not match is refused before a write, and a profile with an unconfirmed field cannot build a transaction image at all.

## Status (RTX 3060 12GB, the only card measured here)

| Step | State |
| --- | --- |
| Read-only preflight, 3 discovery boots + 1 confirmation boot | pass, no `PENDING` line, `complete=1` |
| Transaction, 1 run | `result=0 FLR=1 clean=1`, selectors rewritten |
| Effect measured in Windows | yes, see below |
| Cold-boot persistence | not tested: the patch lives in VRAM and is gone after a power cycle |

## Measured result (RTX 3060 12GB)

Three GEMM runs per state with FP32 accumulation, 4096³, medians of medians:

| Path | Before | After | Change | Per clock |
| --- | --- | --- | --- | --- |
| FP16 tensor, FP32 accumulate | 25.74 TFLOPS | 47.26 TFLOPS | ×1.84 | ×1.87 |
| BF16 tensor, FP32 accumulate | 25.60 TFLOPS | 48.42 TFLOPS | ×1.89 | ×1.92 |
| FP16 tensor, FP16 accumulate (control) | 46.55 TFLOPS | 46.55 TFLOPS | ×1.00 | — |
| FP32 (control) | 8.04 TFLOPS | 8.04 TFLOPS | ×1.00 | — |

The two controls are the point: only the FP32-accumulate tensor path moves, so the gain is the restored issue rate and not a clock or measurement artifact. Power stayed inside the 170 W limit (131 W and 141 W).

ComfyUI, one Anima turbo workflow at 832×1216, four and eight steps, five runs per state:

| Launch mode | Before | After | Change |
| --- | --- | --- | --- |
| default (FP32 accumulate) | 0.761 s/step | 0.485 s/step | ×1.57 faster per step |
| default, per image | 3.73 s, 393 J | 2.49 s, 240 J | ×1.50 faster, ×1.64 less energy |
| `--fast fp16_accumulation` | 0.605 s/step | 0.570 s/step | ×0.94 |

`--fast fp16_accumulation` makes the sampler accumulate in FP16, which is not the patched path, so it barely moves. That is consistent with the GEMM controls and it is why forcing that flag is the wrong way to use this.

## What does not get faster

VRAM bandwidth, FP32 throughput, INT8 kernels, and anything bound by memory bandwidth, including LLM token generation. Expect nothing in games. The gain is confined to workloads that spend their time in dense FP16/BF16 matmuls with FP32 accumulation: training and fine-tuning, prefill, attention kernels, diffusion.

One open question: on the reference host the target GPU trained at **Gen4 x1** (1.60 GB/s). That does not affect the compute-bound numbers above, but it distorts anything bandwidth-bound measured on that machine, including model loading and offload.

## Supported scope

- Board: RTX 3060 12GB, PCI `10de:2504`, subsystem `10de:1536`, VBIOS PROM SHA256 `b1608213…a4da`, die ID `0xb76000a1`, 12288 MiB.
- Host: target at `0:5:0.0`, console GOP on a second NVIDIA card at `01:00.0`. The transaction releases the *target's* UEFI driver and resets that card while the console keeps running on the other one.
- Firmware: the five runtime assets are pinned to NVIDIA 610.43.03 (`unlock/assets.json` holds names, sizes and SHA256).
- Scope of effect: one boot. A power cycle removes it; there is no persistence and no VBIOS write.

Another RTX 3060 12GB will almost certainly fail the subsystem or PROM gate: those values are per card, not per SKU. That is the intended behaviour, and the discovery procedure below is how you get your own. The same applies to the RTX 3090 profile, which ships unconfirmed with only the device ID and the 24576 MiB frame buffer asserted.

## Running it from USB

1. Build the transaction image and package it with the five assets:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile nvidia_bus5
   python .\tools\package_usb.py --assets .\assets --output .\usb-package
   ```

   `unlock/assets.json` lists the five files; `package_usb.py` refuses a package whose assets do not match. They are not shipped here: they are NVIDIA firmware, generated from the pinned driver inputs the upstream project documents.

2. Copy the **contents** of `usb-package` to the root of a FAT32 stick. The `GPU-UNLOCK` folder must be writable; the application writes its report there.

3. Secure Boot off, CSM off, one monitor on the non-target card, then power the PC off completely and boot the **UEFI USB** entry.

4. At `READY`, press **U once**. Esc, or 30 seconds of nothing, cancels.

5. When `UEFI transaction completed and cleaned up` appears, choose **Windows Boot Manager**. Do not reboot in between.

If `STOP / POWER OFF` appears, turn the PC off with the power button and read `GPU-UNLOCK/RUNxx/REPORT.TXT` before trying again. Nothing is written to the card's flash: a power cycle is a full revert.

## Profiles, and how another card gets supported

Every value the application compares before writing comes from one of two compile-time profiles:

- a **board profile** (`unlock/profiles/`) describes one card: PCI identity, VBIOS hash, die ID, cold speed selectors;
- a **host profile** (`unlock/hosts/`) describes the PC: where the target sits and which firmware display keeps the console alive during the reset.

Each field carries a `GU_BOARD_HAVE_*` / `GU_HOST_HAVE_*` flag. A field is confirmed only when the preflight read it off that card or that host. While any field is unconfirmed, the only image the profile can build is the read-only preflight; the transaction build stops on an `#error`. In the preflight an unconfirmed gate is recorded as

```text
gate <name>: PENDING (profile value not confirmed on this card)
```

and never as a pass. To support your own card, build `-HostProfile discovery -Preflight`, boot it cold a few times, and fill in the reported values; [docs/BOARD-PROFILE.md](docs/BOARD-PROFILE.md) has the field list and the procedure.

## Build and test

Needs clang and lld-link (LLVM 12 or newer), an MSVC environment for the host tests, and Python 3.9+. `GU_LLVM_ROOT`, `GU_CLANG` and `GU_VCVARS` override the toolchain paths.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile nvidia_bus5 -Preflight
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile nvidia_bus5
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\test-all.ps1
```

`-Preflight` drops the ARM/transaction half at link time: that image reads config space, the firmware BAR descriptor, the cold-state registers and the PROM window, and writes nothing to the GPU. `test-all.ps1` does not touch hardware.

## Data in this repository

- `evidence/preflight-3060-01..03/` — the three discovery boots that supplied the profile values.
- `evidence/preflight-3060-04/` — the confirmation boot: every gate a real comparison, `complete=1`.
- `evidence/unlock-3060-01/` — the transaction: 305 lines, ends `result=0 … clean=1`.
- `results/rtx3060/gemm/`, `results/rtx3060/comfyui/` — the before and after measurements above.

## Caveats

- One boot per state for the ComfyUI numbers; the GEMM sets have three runs each but were not interleaved boot-by-boot.
- A driver or firmware update changes the assets, and the gates are pinned to 610.43.03.
- Long-run stability, suspend/resume and multi-GPU combinations are untested here.
- The FP32 columns are controls, not effects.

## Provenance and license

The port and the reused core are **GPL-2.0-only**. The chain is the CMP 40HX unlocker, then [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock), then [balaena01/rtx3080-unlock-12g](https://github.com/balaena01/rtx3080-unlock-12g), then this fork. Bundled EDK2 headers keep their own license, and the NVIDIA firmware the application loads is not redistributed here. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The RTX 3080 10GB and 12GB profiles are inherited unchanged. Their measurements and result files belong to the repositories above and are not reproduced here; this fork carries only its own RTX 3060 data.
