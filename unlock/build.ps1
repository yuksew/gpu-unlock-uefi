param(
    # Board profile (unlock/profiles/) and host profile (unlock/hosts/); see docs/BOARD-PROFILE.md.
    [ValidateSet('2206','220a','2204','ga106_2504')][string]$Board = '2206',
    [ValidateSet('intel_bus1','nvidia_bus5','amd_igpu_bus1','discovery')][string]$HostProfile = 'intel_bus1',
    # Read-only image. An unconfirmed board or host profile builds only with this.
    [switch]$Preflight
)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
# GU_LLVM_ROOT overrides the toolchain for other installs.
$llvmRoot = if ($env:GU_LLVM_ROOT) { $env:GU_LLVM_ROOT } else { 'C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Tools/Llvm/x64/bin' }
New-Item -ItemType Directory -Path 'build' -Force | Out-Null
$profileFlags = @(switch ($Board) { '220a' { '-DGU_BOARD_220A' } '2204' { '-DGU_BOARD_2204' } 'ga106_2504' { '-DGU_BOARD_GA106_2504' } })
if ($HostProfile -eq 'discovery') { $profileFlags += '-DGU_HOST_DISCOVERY' }
if ($HostProfile -eq 'nvidia_bus5') { $profileFlags += '-DGU_HOST_NVIDIA_BUS5' }
if ($HostProfile -eq 'amd_igpu_bus1') { $profileFlags += '-DGU_HOST_AMD_IGPU_BUS1' }
if ($Preflight) { $profileFlags += '-DGU_PREFLIGHT' }
$includes = @('-I','compat','-I','../deps/edk2/MdePkg/Include','-I','../deps/edk2/MdePkg/Include/X64')
# Ask port.h itself what it selected: the manifest must not carry a second,
# hand-kept copy of the target ID. An incomplete profile without -Preflight
# stops here on port.h's #error.
$probe = "#include `"port.h`"`nGU_PROFILE_PROBE GU_TARGET_PCI_ID GU_BOARD_COMPLETE GU_HOST_COMPLETE GU_BOARD_PROFILE_NAME GU_HOST_PROFILE_NAME`n"
$expanded = @($probe | & "$llvmRoot/clang.exe" -E -P -x c -D__KERNEL__ -DGU_UEFI_PORT @profileFlags @includes -)
if ($LASTEXITCODE -ne 0) { throw "Profile selection refused: Board=$Board Host=$HostProfile Preflight=$Preflight" }
$line = $expanded | Where-Object { $_ -like 'GU_PROFILE_PROBE *' } | Select-Object -Last 1
if ($line -notmatch '^GU_PROFILE_PROBE 0x([0-9a-fA-F]{8})U ([01]) ([01]) "([^"]*)" "([^"]*)"$') { throw "Unexpected profile expansion: $line" }
$targetPci = $Matches[1].ToLower(); $complete = ($Matches[2] -eq '1' -and $Matches[3] -eq '1')
$boardProfile = $Matches[4]; $hostName = $Matches[5]
$sources = @('app','app_io','ownership','format','runtime','sha256','checks','port','compat',
    'vendor/unlock_610','vendor/prep_610','vendor/layout_610','vendor/vbios_610','vendor/gfw_eval')
$objects = @()
foreach ($source in $sources) {
    $object = 'build/' + ($source -replace '/','_') + '.obj'
    & "$llvmRoot/clang.exe" -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter -D__KERNEL__ -DGU_UEFI_PORT @profileFlags @includes -c "$source.c" -o $object
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
    $objects += $object
}
& "$llvmRoot/lld-link.exe" /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf /map:build/BOOTX64.map /out:build/BOOTX64.EFI @objects
if ($LASTEXITCODE -ne 0) { throw 'EFI link failed' }
& python verify_pe.py build/BOOTX64.EFI build/BOOTX64.map --output build/pe-verification.json
if ($LASTEXITCODE -ne 0) { throw 'PE verification failed' }
# Includes unlock/profiles/*.local.h and unlock/hosts/*.local.h when present:
# they are not tracked, so this manifest is their only record.
$inputs = @(Get-ChildItem -LiteralPath . -Recurse -File | Where-Object {
    $_.Extension -in '.c','.h','.cmd','.ps1','.py','.psm1','.json' -and $_.FullName -notlike '*\build\*' -and $_.FullName -notlike '*\releases\*'
} | Sort-Object FullName | ForEach-Object {
    [ordered]@{ Path=$_.FullName.Substring($PSScriptRoot.Length+1); Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
})
[ordered]@{
    BuiltUtc=[DateTime]::UtcNow.ToString('o'); Board=$Board; Host=$HostProfile; Preflight=[bool]$Preflight;
    BoardProfile=$boardProfile; HostProfileName=$hostName; ProfileComplete=$complete; TargetPci=$targetPci;
    SourceManifestSha256=(Get-FileHash -LiteralPath '../SOURCE-MANIFEST.json' -Algorithm SHA256).Hash;
    EfiSha256=(Get-FileHash -LiteralPath 'build/BOOTX64.EFI' -Algorithm SHA256).Hash;
    PeVerificationSha256=(Get-FileHash -LiteralPath 'build/pe-verification.json' -Algorithm SHA256).Hash;
    Edk2Commit='c1415771ac592ad8509351c46cd3cda4f38d7d52'; Sources=$inputs;
    HardwareTested=$false
} | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath 'build/build-manifest.json'
$kind = if ($Preflight) { 'read-only preflight' } else { 'transaction' }
Write-Output "Built and inspected locally: $kind image, $boardProfile, host $hostName, pci $targetPci."
Write-Output 'Run test-all.ps1, then tools/package_usb.py to prepare a local USB directory.'
