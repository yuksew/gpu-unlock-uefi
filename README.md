# GPU tensor unlock over UEFI (GA10x)

**English** | [日本語](README.ja.md)

A UEFI application that restores the full FP16/BF16 tensor issue rate on NVIDIA GA10x GeForce cards: the ones shipped with FP32-accumulate tensor maths at half the rate of their professional siblings. It runs from a FAT32 USB stick before Windows starts, writes nothing to the VBIOS, and hands control straight back to Windows Boot Manager without a reboot in between.

Fork of [balaena01/rtx3080-unlock-12g](https://github.com/balaena01/rtx3080-unlock-12g), which is a UEFI port of [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock). This fork adds the GA106 / RTX 3060 12GB board profile, the RTX 3090 profile, two host profiles beyond the inherited one (console on a second NVIDIA card, console on the AMD iGPU), and the measured result of one unlock on each card.

## Boards and hosts

| Board profile | Select with | Card | State |
| --- | --- | --- | --- |
| `ga102_2206` | default | RTX 3080 10GB, `10de:2206` | confirmed and measured upstream |
| `ga102_220a` | `-Board 220a` | RTX 3080 12GB, `10de:220a` | confirmed upstream, one transaction |
| `ga102_2204` | `-Board 2204` | MSI RTX 3090, `10de:2204` | confirmed, unlocked and measured here |
| `ga106_2504` | `-Board ga106_2504` | RTX 3060 12GB, `10de:2504` | confirmed and measured here |

| Host profile | Select with | Console GOP | Target |
| --- | --- | --- | --- |
| `intel_bus1` | default | Intel iGPU at `00:02.0` | `0:1:0.0` |
| `nvidia_bus5` | `-HostProfile nvidia_bus5` | second NVIDIA card at `01:00.0` | `0:5:0.0` |
| `amd_igpu_bus1` | `-HostProfile amd_igpu_bus1` | AMD iGPU at `08:00.0` | `0:1:0.0` |
| `discovery` | `-HostProfile discovery` | recorded, not required | recorded, not required |

Every gate compares against values read from that card and that PC. Anything that does not match is refused before a write, and a profile with an unconfirmed field cannot build a transaction image at all.

## Status

| Step | RTX 3060 12GB (`ga106_2504`) | MSI RTX 3090 (`ga102_2204`) |
| --- | --- | --- |
| Read-only preflight | 3 discovery boots + 1 confirmation boot, no `PENDING` line, `complete=1` | same |
| Transaction, 1 run | `result=0 FLR=1 clean=1`, selectors rewritten to `0x88888888` / `0x8` | same |
| Effect measured in Windows | GEMM and ComfyUI | GEMM, prefill and decode |
| Gone after a power cycle | yes: the before and after sets are separate boots | yes, measured directly: 72.0 TFLOPS and 35.8 TFLOPS/GHz after a normal reboot |

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

## Measured result (MSI RTX 3090)

Three GEMM runs per state, same harness and flags as the 3060 set, 4096³ with FP32 accumulation:

| Path | Before | After | Change | Per clock |
| --- | --- | --- | --- | --- |
| FP16 tensor, FP32 accumulate | 70.95 TFLOPS | 122.49 TFLOPS | ×1.73 | ×1.92 |
| BF16 tensor, FP32 accumulate | 70.29 TFLOPS | 127.30 TFLOPS | ×1.81 | ×1.91 |
| FP16 tensor, FP16 accumulate (control) | 129.26 TFLOPS | 129.52 TFLOPS | ×1.00 | — |
| FP32 (control) | 24.78 TFLOPS | 24.74 TFLOPS | ×1.00 | — |

Same shape as the 3060 and the same reason: per clock the patched path doubles, while the absolute gain is smaller because the card then sits against its 350 W limit (296–337 W measured) and settles at a lower clock (1980 → 1785 MHz on FP16, → 1860 MHz on BF16).

Prefill and decode were measured separately with a different harness (an LLM server, 3 repetitions per case) twice per state: prompt throughput rose 8.8% at 4K and 13.2% at 16K, and each state's two runs agree to within half a point. Decode did not move measurably. The same state varies by about ±10% between runs at 16K (109.9 and 125.3 tok/s without the patch, 102.9 and 108.0 with it), so one run is not enough to claim an effect there, and the single high sample is what an earlier reading of −13.8% rested on. That harness's raw data is not in this repository.

A normal reboot is enough to undo it: after one, the same GEMM probe read 72.0 TFLOPS and 35.8 TFLOPS/GHz, the locked state.

## Power and efficiency

Same runs, sampled from NVML over the same 15 s window: average power, and TFLOPS per watt.

| Card | Path | Before | After | Work per joule |
| --- | --- | --- | --- | --- |
| RTX 3060 12GB | FP16 tensor, FP32 accumulate | 103.1 W, 249.4 GFLOPS/W | 131.4 W, 360.5 GFLOPS/W | ×1.45 |
| RTX 3060 12GB | BF16 tensor, FP32 accumulate | 102.5 W, 249.7 GFLOPS/W | 141.0 W, 343.4 GFLOPS/W | ×1.38 |
| RTX 3090 | FP16 tensor, FP32 accumulate | 315.6 W, 222.8 GFLOPS/W | 296.5 W, 413.1 GFLOPS/W | ×1.85 |
| RTX 3090 | BF16 tensor, FP32 accumulate | 320.7 W, 219.2 GFLOPS/W | 337.2 W, 377.5 GFLOPS/W | ×1.72 |

The 3060 draws more afterwards because it has the headroom: its limit is 170 W and it settles at 131–141 W. The 3090 is already at its 350 W limit, so it spends the doubled work per clock on a lower clock instead and its power barely moves. Either way the work per joule rises by 1.4–1.9×, and the SM clock falls in every case (1920 → 1882/1890 MHz on the 3060, 1980 → 1785/1860 MHz on the 3090).

## What does not get faster

VRAM bandwidth, FP32 throughput, INT8 kernels, and anything bound by memory bandwidth, including LLM token generation. Expect nothing in games. The gain is confined to workloads that spend their time in dense FP16/BF16 matmuls with FP32 accumulation: training and fine-tuning, prefill, attention kernels, diffusion.

One open question: on the reference host the target GPU trained at **Gen4 x1** (1.60 GB/s). That does not affect the compute-bound numbers above, but it distorts anything bandwidth-bound measured on that machine, including model loading and offload.

## Supported scope

Each confirmed card is one board profile plus one host profile. Neither is a general-purpose setting for the SKU.

| | RTX 3060 12GB | MSI RTX 3090 |
| --- | --- | --- |
| Board profile | `ga106_2504`: `10de:2504`, subsystem `10de:1536`, PROM `b1608213…a4da`, die `0xb76000a1`, 12288 MiB | `ga102_2204`: `10de:2204`, subsystem `1462:3881`, PROM `de78e145…2dd1aad1`, die `0xb72000a1`, 24576 MiB |
| Host profile | `nvidia_bus5`: target `0:5:0.0`, console on a second NVIDIA card at `01:00.0` | `amd_igpu_bus1`: target `0:1:0.0`, console on the AMD iGPU at `08:00.0` |
| Firmware | the five assets pinned to NVIDIA 610.43.03 | the same five assets |
| Scope of effect | one boot, VRAM only, no VBIOS write | one boot, VRAM only, no VBIOS write |

The transaction releases the UEFI driver of the *target* controller only, then resets that card; the console keeps running because it is on another adapter.

Another card of either model will almost certainly fail the subsystem or PROM gate: those values are per card, not per SKU. That is the intended behaviour, and the discovery procedure below is how you get your own.

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

| Card | Preflight | Transaction | Measurements |
| --- | --- | --- | --- |
| RTX 3060 12GB | `evidence/preflight-3060-01..03/` (discovery), `-04/` (confirmation) | `evidence/unlock-3060-01/` | `results/rtx3060/gemm/`, `results/rtx3060/comfyui/` |
| MSI RTX 3090 | `evidence/preflight-3090-01..03/` (discovery), `-04/` (confirmation) | `evidence/unlock-3090-01/` | `results/rtx3090/gemm/` |

Each preflight report is the application's own log; each transaction report ends with `FINAL status=0 stage=transaction attempted=1 driver_release=1 clean=1`. The LLM numbers in the 3090 section came from a separate harness whose raw data is not in this repository.

## Caveats

- One boot per state for the ComfyUI numbers; the GEMM sets have three runs each but were not interleaved boot-by-boot.
- A driver or firmware update changes the assets, and the gates are pinned to 610.43.03.
- Long-run stability, suspend/resume and multi-GPU combinations are untested here.
- The FP32 columns are controls, not effects.

## Provenance and license

The port and the reused core are **GPL-2.0-only**. The chain is the CMP 40HX unlocker, then [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock), then [balaena01/rtx3080-unlock-12g](https://github.com/balaena01/rtx3080-unlock-12g), then this fork. Bundled EDK2 headers keep their own license, and the NVIDIA firmware the application loads is not redistributed here. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The RTX 3080 10GB and 12GB profiles are inherited unchanged. Their measurements and result files belong to the repositories above and are not reproduced here; this fork carries only its own RTX 3060 data.
