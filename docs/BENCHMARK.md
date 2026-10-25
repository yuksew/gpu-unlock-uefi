# Measuring speed and power before and after the unlock

Two harnesses, both read-only with respect to the GPU (no clock, power-limit or driver change), both choosing the card by PCI bus ID so a second NVIDIA GPU in the PC is never measured by mistake:

| Harness | What it measures |
|---|---|
| `analysis/power-benchmark.cu` + `run_power_benchmark.py` | Sustained cuBLAS GEMM in four precisions, board power, clocks, PCIe link and host-to-device bandwidth |
| `analysis/bench_comfyui.py` | Image generation through a running ComfyUI: seconds per image and per sampling step, power and PCIe traffic during generation |

`compare_power.py` and `bench_comfyui.py compare` turn two labelled sets into a Markdown table with ratios. Power is the board power NVML reports, not wall power.

## The unlock is per boot

The patch lives in VRAM for one boot, so "before" and "after" can never be measured in the same session. Alternate them over separate cold boots — before, after, before, after — and give every set its own label (`before`, `after`, or with the mode, `before-fp16acc`). Use `--note` for anything that differs between boots (ambient temperature, which monitor output was used, background load).

## GEMM: what moves and what must not

`power-benchmark.exe` runs each precision back to back for `--seconds` (default 15) after a warmup, so the card reaches the clock it can hold at its power limit:

- `fp16_tensor_fp32_accumulate`, `bf16_tensor_fp32_accumulate` — the paths the GeForce half-rate limit applies to, and the paths the unlock targets;
- `fp16_tensor_fp16_accumulate_control` — already full rate on GeForce, so its ratio should stay near 1.00; if it moves, something other than the unlock changed;
- `fp32_pedantic` — no tensor cores, also expected to stay.

Baseline on the RTX 3060 12GB in this repository's test PC (normal boot, ComfyUI resident and idle, short 3 s windows): FP16/FP32-accumulate 25.6 TFLOPS at 98 W, the FP16-accumulate control 46.9 TFLOPS. A working unlock should move the first towards the second. Expect it to reach the 170 W power limit and lose clock when it does, which is why the table also shows TFLOPS/GHz and GFLOPS/W.

```powershell
analysis\build-power-benchmark.cmd     # GU_VCVARS / GU_NVCC override the toolchain paths
python -I analysis\run_power_benchmark.py --pci 0000:05:00.0 --label before --runs 3 --out results\rtx3060\gemm
python -I analysis\compare_power.py results\rtx3060\gemm --base before --test after
```

The runner refuses to start while another process holds a compute context on the target GPU (for example ComfyUI); stop it for the GEMM runs.

## ComfyUI

`bench_comfyui.py run` talks to the ComfyUI already running at `--server` (default `http://127.0.0.1:8188`) and checks that it runs on the card at `--pci`. The workflow is the latest one in ComfyUI's history, or `--workflow` (API format), sanitised: neutral fixed prompts, LoRA loaders bypassed, `SaveImage` replaced by `PreviewImage`. The sanitised graph is saved as `workflow-<hash>.json` in the output directory and reused by later sessions there, and `compare` refuses sessions with different workflows.

Each measured prompt changes only the seed, so models and text encodings stay cached, and alternates between the workflow's step count S and 2S. `(t(2S) - t(S)) / S` is the time per sampling step — the part the unlock can speed up — and the remainder is the fixed cost per image (VAE decode, preview).

```powershell
python -I analysis\bench_comfyui.py run --pci 0000:05:00.0 --label before-fp16acc --out results\rtx3060\comfyui
python -I analysis\bench_comfyui.py compare results\rtx3060\comfyui --base before-fp16acc --test after-fp16acc
```

**Launch mode matters.** `--fast fp16_accumulation` makes PyTorch's FP16 matrix multiplies accumulate in FP16, which GeForce already runs at full rate, so the unlock has little to remove there. ComfyUI's default (FP32 accumulation, and BF16 models in any mode) is where the half-rate limit applies. Measure both modes, restarting ComfyUI between them; the record stores the launch arguments and `compare` prints them.

## PCIe x1

A card in an x1 slot moves about 1.6 GB/s to and from the host (measured: Gen4 x1, pinned 256 MiB copies), against 20+ GB/s at x16. That does not limit either harness, and both record evidence of it:

- the GEMM operands stay in VRAM, and each GEMM window records NVML's PCIe counters (`median_pcie_rx_kbs`, `median_pcie_tx_kbs`) so a result can show it;
- ComfyUI keeps the model in VRAM between prompts; the baseline workflow above moved about 34 MB/s over the link while generating, about 2% of what x1 carries.

What x1 does slow down is everything that streams over the link: model loading (excluded — warmup prompts load the model and are not measured), offloading layers to host memory, and splitting one model across two GPUs. Keep those out of a before/after comparison: one GPU, the whole model resident (`-ngl 99` for llama.cpp), `CUDA_VISIBLE_DEVICES` limited to the card under test. If `median_pcie_rx_mbs` during generation climbs towards 1,000 MB/s, the run is link-bound and will not show the unlock.
