# USB preparation and booting

This guide targets the card and host listed in the README. The run in `evidence/unlock-3060-01/` booted from a freshly prepared FAT32 stick, but other sticks and other PCs remain your own test.

## Preparation

- An x64 UEFI environment with Secure Boot disabled and CSM off, and the monitor on the card that is **not** the target (the console has to survive the target's reset).
- An empty FAT32 USB stick. If it needs formatting, back up the files already on the stick first and confirm which device you are about to write.
- The built `BOOTX64.EFI`, or the provided USB template.
- For a transaction run, the five assets whose names, sizes and SHA256 values match `unlock/assets.json`. A preflight run needs none. For how to prepare the assets, see the [firmware documentation of the upstream project](https://github.com/Zagitta/gpu-unlock/tree/0ebdb1c6308b6a2072be5bf04513472942541956/firmware).

On a PC that uses Windows device encryption / BitLocker, check your recovery key before changing firmware settings.

## Building the package locally

Run this from the repository root. Build the EFI first, then place the five assets in a local `assets` directory. The commands below create a new local folder; they do not format a USB stick and do not select one automatically.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1
python .\tools\package_usb.py --assets .\assets --output .\usb-package
```

If you omit `--assets`, the template is produced without the runtime assets. `ASSETS/REQUIRED-ASSETS.json` inside the template is the list of required files, not the assets themselves, so the five files must be prepared separately.

Copy the **contents** of the finished `usb-package` to the root of the USB stick. Do not nest the `usb-package` folder one level down.

```text
USB:/
  EFI/BOOT/BOOTX64.EFI
  GPU-UNLOCK/
    ASSETS/
      booter_load_ga102.bin
      v67-ga102-610.43.03.bin
      gsp_ga10x.bin
      gsp_boot_ga102.bin
      gsp_boot_desc_ga102.bin
    LICENSES/
    SOURCE.zip
  USB-README.md
  PACKAGE.json
  SHA256SUMS.txt
```

The `GPU-UNLOCK` folder must be writable so the application can record logs. Copying only `BOOTX64.EFI` is not enough to run: the application re-checks the file contents and the board conditions.

## Running

1. Save your Windows work, power the PC off completely, then power it on. If the GPU is in a warm state left over from the driver, the checks may refuse to run.
2. In the motherboard boot menu, select the **UEFI USB** entry. F8 was used on the validated ASUS board.
3. Once the pre-checks finish and `READY` is shown, press **U once**. Esc, or waiting out the 30-second timeout without pressing U, cancels.
4. Do not unplug the USB stick or reboot while the run is in progress.
5. When `UEFI transaction completed and cleaned up` appears and control returns to the firmware, select **Windows Boot Manager**. Rebooting in between may not keep the applied state.

If the USB application opens again, leave it with Esc. Do not press U a second time in the same boot.

If `STOP / POWER OFF` appears, turn the PC off with the power button. Do not continue into Windows from that screen. Inspect `GPU-UNLOCK/RUNxx/REPORT.TXT` before trying again.

## RTX 3060 12GB

The RTX 3060 12GB (`10de:2504`) profile is confirmed only for the card and host whose values it records (see the README table). On that machine both images build; on any other 3060 the profile has to be discovered first, because the subsystem, VBIOS hash and cold selectors are per card, not per SKU. The procedure is in [BOARD-PROFILE.md](BOARD-PROFILE.md).

The confirmed machine, all three commands:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile nvidia_bus5 -Preflight
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile nvidia_bus5
python .\tools\package_usb.py --assets .\assets --output .\usb-package-3060
```

A different 3060 starts with `-HostProfile discovery -Preflight` and a package without `--assets`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile discovery -Preflight
python .\tools\package_usb.py --output .\usb-package-3060
```

The preflight reads config space, the firmware BAR descriptor, the cold-state registers and the PROM window, writes nothing to the GPU, loads no assets and never asks for U. On Linux, `./preflight-linux.sh usb-3060` stages the image as `unlock/build/usb-preflight-3060/EFI/BOOT/BOOTX64.EFI`; create an empty, writable `GPU-UNLOCK` folder on the USB root yourself.

1. The folder keeps the name `GPU-UNLOCK` for every board: the application writes its reports only there.
2. Connect the monitor to a display that is **not** the RTX 3060. The preflight rejects a console GOP on the target GPU, and it needs exactly one physical GOP, so leave every other output disconnected. On the confirmed machine that is a second NVIDIA card at `01:00.0`, and a transaction ran in exactly that configuration; a console on the target itself is always refused.
3. Power the PC off completely, boot the **UEFI USB** entry, and let the preflight finish. It returns to the firmware by itself; on a confirmed profile the report has no `PENDING` line and ends with `complete=1`.
4. Repeat from a cold start at least twice more while discovering. Each run adds `GPU-UNLOCK/RUNxx/REPORT.TXT`.
5. A transaction run needs the five assets in `GPU-UNLOCK/ASSETS`; the preflight does not. `unlock/assets.json` lists their names, sizes and SHA256, and `package_usb.py` refuses a package whose assets do not match.

## Scope

A successful UEFI run does not guarantee better performance in Windows or long-run stability. Check the device state first and compare performance in your own workload. Whether the effect survives the next cold boot has not been confirmed.

You do not need to change the bootloader of your existing Windows disk. To stop using the USB route, remove the stick and boot Windows Boot Manager as usual.
