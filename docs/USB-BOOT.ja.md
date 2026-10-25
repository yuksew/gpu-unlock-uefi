# USB の準備と起動

**日本語** | [English](USB-BOOT.md)

このガイドは README に載っているカードとホストが対象です。`evidence/unlock-3060-01/` の実行は、新しく用意した FAT32 のスティックから起動しました。ただし他のスティックや他の PC は、各自の環境でのテストになります。

## 準備

- x64 の UEFI 環境で、Secure Boot を切り、CSM を切り、モニタは**ターゲットではない**カードに接続します（コンソールはターゲットのリセットを生き延びる必要があります）。
- 空の FAT32 USB メモリ。フォーマットが必要なら、先に中のファイルを退避し、これから書くデバイスが正しいことを確認してください。
- ビルドした `BOOTX64.EFI`、または同梱の USB テンプレート。
- トランザクション実行には、名前・サイズ・SHA256 が `unlock/assets.json` と一致する 5 つのアセットが必要です。preflight 実行には不要です。アセットの作り方は[上流プロジェクトのファームウェア文書](https://github.com/Zagitta/gpu-unlock/tree/0ebdb1c6308b6a2072be5bf04513472942541956/firmware)を参照してください。

Windows のデバイス暗号化 / BitLocker を使っている PC では、ファームウェア設定を変える前に回復キーを確認してください。

## ローカルでのパッケージ作成

リポジトリのルートで実行します。先に EFI をビルドし、5 つのアセットをローカルの `assets` ディレクトリに置きます。下のコマンドは新しいローカルフォルダを作るだけで、USB のフォーマットも、デバイスの自動選択もしません。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1
python .\tools\package_usb.py --assets .\assets --output .\usb-package
```

`--assets` を省くと、実行時アセットを含まないテンプレートができます。テンプレート内の `ASSETS/REQUIRED-ASSETS.json` は必要なファイルの一覧であってアセット本体ではないので、5 つのファイルは別途用意する必要があります。

できあがった `usb-package` の**中身**を USB のルートにコピーします。`usb-package` フォルダを 1 段下に置かないでください。

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

`GPU-UNLOCK` フォルダは書き込み可能である必要があります（アプリがログを書くため）。`BOOTX64.EFI` だけをコピーしても動きません。アプリはファイルの中身とボードの条件を再検査します。

## 実行

1. Windows の作業を保存し、PC の電源を完全に切ってから入れ直します。GPU がドライバの残した暖まった状態にあると、チェックが実行を拒否することがあります。
2. マザーボードのブートメニューで **UEFI USB** のエントリを選びます。
3. 事前チェックが終わり `READY` が出たら **U を 1 回**押します。Esc、または 30 秒のタイムアウトでキャンセルされます。
4. 実行中は USB を抜かず、再起動もしないでください。
5. `UEFI transaction completed and cleaned up` が出てファームウェアに戻ったら、**Windows Boot Manager** を選びます。途中で再起動すると、適用された状態が保たれない可能性があります。

USB アプリがもう一度開いたら、Esc で閉じます。同じブートで U を 2 回押さないでください。

`STOP / POWER OFF` が出たら、電源ボタンで PC を切ってください。その画面から Windows に進まないでください。再試行の前に `GPU-UNLOCK/RUNxx/REPORT.TXT` を確認します。

## RTX 3060 12GB

RTX 3060 12GB（`10de:2504`）のプロファイルは、その値を記録したカードとホストに対してのみ確定しています（README の表を参照）。そのマシンでは両方のイメージがビルドできます。他の 3060 では、subsystem、VBIOS ハッシュ、コールド時のセレクタが SKU ではなく 1 枚ごとの性質なので、まず探索が必要です。手順は [BOARD-PROFILE.ja.md](BOARD-PROFILE.ja.md) にあります。

確定済みのマシンでの 3 コマンド:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile nvidia_bus5 -Preflight
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile nvidia_bus5
python .\tools\package_usb.py --assets .\assets --output .\usb-package-3060
```

別の 3060 は `-HostProfile discovery -Preflight` と、`--assets` なしのパッケージから始めます:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile discovery -Preflight
python .\tools\package_usb.py --output .\usb-package-3060
```

preflight は config space、ファームウェアの BAR 記述子、コールド状態のレジスタ、PROM ウィンドウを読み、GPU には何も書かず、アセットも読み込まず、U も求めません。Linux では `./preflight-linux.sh usb-3060` が `unlock/build/usb-preflight-3060/EFI/BOOT/BOOTX64.EFI` として配置します。USB のルートに空の書き込み可能な `GPU-UNLOCK` フォルダは自分で作成してください。

1. フォルダ名はどのボードでも `GPU-UNLOCK` です。アプリはそこにしかレポートを書きません。
2. モニタはターゲットでは**ない**出力に接続します。コンソールの GOP はホストプロファイルに一致する必要があり、ターゲット自身の上のコンソールは常に拒否されます。別のアダプタに 2 台目のモニタを繋いでも構いません。確定済みのマシンでは、3060 のときは `01:00.0` の 2 枚目の NVIDIA カード、3090 のときは `08:00.0` の AMD iGPU がコンソールで、どちらの構成でもトランザクションが動いています。
3. PC の電源を完全に切って **UEFI USB** のエントリで起動し、preflight を最後まで走らせます。勝手にファームウェアへ戻ります。確定プロファイルではレポートに `PENDING` 行が無く、`complete=1` で終わります。
4. 探索中はコールドスタートから最低 2 回追加で実行します。各実行が `GPU-UNLOCK/RUNxx/REPORT.TXT` を増やします。
5. トランザクション実行には `GPU-UNLOCK/ASSETS` の 5 アセットが必要で、preflight には不要です。`unlock/assets.json` が名前・サイズ・SHA256 の一覧で、`package_usb.py` は一致しないアセットを含むパッケージを拒否します。

## RTX 3090

RTX 3090 のプロファイルは、その値を記録した MSI のカードとホストに対して確定しています。ここでは 3090 が普段コンソールを握っているカードでもあるため、unlock するにはコンソールを別に移す必要があります。確定済みの配置では、モニタはマザーボード側の出力（ファームウェアが `08:00.0` と報告する AMD iGPU）にあり、3090 は `0:1:0.0` のターゲットです（`-HostProfile amd_igpu_bus1`）。モニタを 3090 に戻すとコンソールがターゲット上になり、表示ゲートが拒否します。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board 2204 -HostProfile amd_igpu_bus1 -Preflight
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board 2204 -HostProfile amd_igpu_bus1
python .\tools\package_usb.py --assets .\assets --output .\usb-package-3090
```

unlock 後は最大 350 W を引くので、電源は 1 本のデイジーチェーンではなく、別系統の PCIe ケーブルで取ってください。

## 適用範囲

UEFI での実行が成功しても、Windows 側の性能や長時間の安定性が保証されるわけではありません。まずデバイスの状態を確認し、自分のワークロードで測ってください。効果は電源を切ると消えます。`results/` の変更前・変更後は別々のブートで取っており、コールドブートは毎回ロックされた状態から始まります。サスペンド/レジュームはどちらのボードでも未試験です。

既存の Windows ディスクのブートローダを変える必要はありません。USB 経由をやめるときは、スティックを抜いて普段どおり Windows Boot Manager で起動してください。
