# Build and test

## Basic checks

Install the compilers and Python listed in the README on Windows, then run this from the repository root.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\test-all.ps1
```

The basic checks exercise probe, port / Bus Master, preflight, runtime, the PE rejection tests and the USB packaging tools. `evidence/boot-03/REPORT.TXT` is used as the preflight fixture. The GPU and the UEFI services are simulated.

To run only the Python packaging tests:

```text
python -m unittest discover -s tools -p "test_*.py" -v
```

## Additional checks with local fixtures

Prepare the following files, then pass `-WithLocalFixtures`.

- `analysis/pinned-assets/`: the five assets listed in `unlock/assets.json`.
- `evidence/boot-03/PROM.BIN`: 1MiB, SHA256 `F3DC8F07B0B8C9856D192DF172DFA7F7F84AC17EF6EDFDB448AD7FA94EFD2789`.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\test-all.ps1 -WithLocalFixtures
```

This adds the transaction, SHA256, EFI transaction, whole-application and FWSEC ordering checks. The PROM fixture is the regression input for the recorded board; replacing it with a capture from another card is not the same validation.

## Source and package hashes

`SOURCE-MANIFEST.json` lists the public source files and their SHA256 values. The USB packaging tool puts only the files in that list into `SOURCE.zip` and verifies the build inputs, the EFI and the runtime assets. Personal working files and build caches are not packaged.

If you change the sources, add the new files to Git, refresh the source manifest, then build and test again.

```powershell
git add .
python .\tools\source_manifest.py
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\test-all.ps1
```

`tools/source_manifest.py` works from the Git-tracked file set. Sources received as a ZIP can be built and packaged as-is using the manifest shipped with them.

Test results are recorded in `unlock/build/test-results.json`. Passing under the host/mock environment and booting from a real USB stick are separate results.
