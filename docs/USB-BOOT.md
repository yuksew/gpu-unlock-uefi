# USB preparation and booting

**English** | [日本語](USB-BOOT.ja.md) | [한국어](USB-BOOT.ko.md)

This guide targets the MSI RTX 3080 experimental environment stated in the README. Existing runs are confirmed, but booting from a freshly prepared FAT32 USB stick has not been validated yet.

## Preparation

- An x64 UEFI environment, Secure Boot disabled, and the display connected to the Intel integrated graphics.
- An empty FAT32 USB stick. If it needs formatting, back up the files already on the stick first and confirm which device you are about to write.
- The built `BOOTX64.EFI`, or the provided USB template.
- The five assets whose names, sizes and SHA256 values match `unlock/assets.json`. For how to prepare the assets, see the [firmware documentation of the upstream project](https://github.com/Zagitta/gpu-unlock/tree/0ebdb1c6308b6a2072be5bf04513472942541956/firmware).

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
  RTX3080-UNLOCK/
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

The `RTX3080-UNLOCK` folder must be writable so the application can record logs. Copying only `BOOTX64.EFI` is not enough to run: the application re-checks the file contents and the board conditions.

## Running

1. Save your Windows work, power the PC off completely, then power it on. If the GPU is in a warm state left over from the driver, the checks may refuse to run.
2. In the motherboard boot menu, select the **UEFI USB** entry. F8 was used on the validated ASUS board.
3. Once the pre-checks finish and `READY` is shown, press **U once**. Esc, or waiting out the 30-second timeout without pressing U, cancels.
4. Do not unplug the USB stick or reboot while the run is in progress.
5. When `UEFI transaction completed and cleaned up` appears and control returns to the firmware, select **Windows Boot Manager**. Rebooting in between may not keep the applied state.

If the USB application opens again, leave it with Esc. Do not press U a second time in the same boot.

If `STOP / POWER OFF` appears, turn the PC off with the power button. Do not continue into Windows from that screen. Inspect `RTX3080-UNLOCK/RUNxx/REPORT.TXT` before trying again.

## RTX 3060 12GB: read-only preflight first

The RTX 3060 12GB (`10de:2504`) profile is unvalidated and starts unconfirmed, so the only image it builds is the read-only preflight. That image reads config space, the firmware BAR descriptor, the cold-state registers and the PROM window, writes nothing to the GPU, loads no assets and never asks for U. The procedure that turns its reports into a confirmed profile is in [BOARD-PROFILE.md](BOARD-PROFILE.md).

1. Build the preflight for the 3060 on a host whose topology is not yet confirmed:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile discovery -Preflight
   python .\tools\package_usb.py --output .\usb-package-3060
   ```

   The assets are not needed for the preflight, so `--assets` can be omitted. On Linux, `./preflight-linux.sh usb-3060` stages the image as `unlock/build/usb-preflight-3060/EFI/BOOT/BOOTX64.EFI`; then create an empty, writable `RTX3080-UNLOCK` folder on the USB root yourself.
2. The folder keeps the name `RTX3080-UNLOCK` for every board: the application writes its reports only there.
3. Connect the monitor to a display that is **not** the RTX 3060. The preflight rejects a console GOP on the target GPU, and it needs exactly one physical GOP, so leave every other output disconnected. The validated configuration is the Intel iGPU; a console on another NVIDIA card is accepted by the preflight but has never been used for a transaction.
4. Power the PC off completely, boot the **UEFI USB** entry, and let the preflight finish. It returns to the firmware by itself; `PREFLIGHT passed` means every confirmed gate passed and the unconfirmed ones were recorded as `PENDING`.
5. Repeat from a cold start at least twice more. Each run adds `RTX3080-UNLOCK/RUNxx/REPORT.TXT`.
6. Copy the reports to the PC and confirm the profiles as described in [BOARD-PROFILE.md](BOARD-PROFILE.md). Until both `ga106_2504.local.h` and `hosts/discovery.local.h` exist with every field confirmed, `build.ps1` without `-Preflight` stops on an `#error` and no transaction image exists.

## Scope

A successful UEFI run does not guarantee better performance in Windows or long-run stability. Check the device state first and compare performance in your own workload. Whether the effect survives the next cold boot has not been confirmed.

You do not need to change the bootloader of your existing Windows disk. To stop using the USB route, remove the stick and boot Windows Boot Manager as usual.
