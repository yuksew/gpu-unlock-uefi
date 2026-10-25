# GPU テンソル unlock over UEFI (GA10x)

**日本語** | [English](README.md)

NVIDIA GA10x の GeForce カードで、FP16/BF16 テンソルの発行レートを本来の値に戻す UEFI アプリケーションです。対象は、プロ向けと同じシリコンながら FP32 accumulate のテンソル演算が半分のレートに落とされているカードです。Windows が起動する前に FAT32 の USB メモリから実行し、VBIOS には一切書き込まず、再起動を挟まずに Windows Boot Manager へ制御を返します。

[balaena01/rtx3080-unlock-12g](https://github.com/balaena01/rtx3080-unlock-12g) のフォークで、そのさらに上流が [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock) の UEFI 移植です。このフォークが追加したのは、GA106 / RTX 3060 12GB のボードプロファイル、RTX 3090 の探索用プロファイル、コンソールが 2 枚目の NVIDIA カードにあるホスト用プロファイル、そして実機 1 回分の実測結果です。

## ボードとホスト

| ボードプロファイル | 選択方法 | カード | 状態 |
| --- | --- | --- | --- |
| `ga102_2206` | 既定 | RTX 3080 10GB、`10de:2206` | 上流で確定・実測済み |
| `ga102_220a` | `-Board 220a` | RTX 3080 12GB、`10de:220a` | 上流で確定、トランザクション 1 回 |
| `ga102_2204` | `-Board 2204` | MSI RTX 3090、`10de:2204` | ここで確定・unlock・実測 |
| `ga106_2504` | `-Board ga106_2504` | RTX 3060 12GB、`10de:2504` | ここで確定・実測 |

| ホストプロファイル | 選択方法 | コンソール GOP | ターゲット |
| --- | --- | --- | --- |
| `intel_bus1` | 既定 | Intel iGPU `00:02.0` | `0:1:0.0` |
| `nvidia_bus5` | `-HostProfile nvidia_bus5` | 2 枚目の NVIDIA カード `01:00.0` | `0:5:0.0` |
| `amd_igpu_bus1` | `-HostProfile amd_igpu_bus1` | AMD iGPU `08:00.0` | `0:1:0.0` |
| `discovery` | `-HostProfile discovery` | 記録のみ（未確定で可） | 記録のみ（未確定で可） |

すべてのゲートは、そのカードとその PC から読み取った値と比較します。一致しない場合は書き込み前に拒否され、未確定のフィールドが 1 つでもあるプロファイルはトランザクション用イメージをビルドできません。

## 状態

| 段階 | RTX 3060 12GB（`ga106_2504`） | MSI RTX 3090（`ga102_2204`） |
| --- | --- | --- |
| 読み取り専用 preflight | 探索 3 回 + 確定 1 回、`PENDING` 行なし、`complete=1` | 同じ |
| トランザクション | 1 回、`result=0 FLR=1 clean=1`、セレクタを `0x88888888` / `0x8` に書き換え | 2 回、2 回目は両方のアダプタにモニタを接続 |
| Windows 側での効果測定 | GEMM と ComfyUI | GEMM、prefill、decode |
| 電源断での復帰 | あり: 変更前と変更後は別ブート | あり（実測）: 通常の再起動後に 72.0 TFLOPS / 35.8 TFLOPS/GHz |

## 実測結果（RTX 3060 12GB）

FP32 accumulate の GEMM を各状態 3 回、4096³、中央値の中央値:

| 経路 | 変更前 | 変更後 | 変化 | クロック当たり | 消費電力 |
| --- | --- | --- | --- | --- | --- |
| FP16 tensor, FP32 accumulate | 25.74 TFLOPS | 47.26 TFLOPS | ×1.84 | ×1.87 | 103.1 → 131.4 W |
| BF16 tensor, FP32 accumulate | 25.60 TFLOPS | 48.42 TFLOPS | ×1.89 | ×1.92 | 102.5 → 141.0 W |
| FP16 tensor, FP16 accumulate（対照） | 46.55 TFLOPS | 46.55 TFLOPS | ×1.00 | — | 119.4 → 120.1 W |
| FP32（対照） | 8.04 TFLOPS | 8.04 TFLOPS | ×1.00 | — | 140.3 → 142.0 W |

対照 2 行が要点です。動いたのは FP32 accumulate のテンソル経路だけで、したがってこの差はクロックや測定の揺れではなく、戻った発行レートそのものです。消費電力は 170 W の上限内（131 W と 141 W）でした。

ComfyUI、Anima turbo のワークフロー 1 本、832×1216、4 step と 8 step、各状態 5 回:

| 起動モード | 変更前 | 変更後 | 変化 |
| --- | --- | --- | --- |
| 既定（FP32 accumulate） | 0.761 秒/step | 0.485 秒/step | step 当たり ×1.57 高速 |
| 既定、1 枚当たり | 3.73 秒、393 J | 2.49 秒、240 J | ×1.50 高速、エネルギー ×1.64 削減 |
| `--fast fp16_accumulation` | 0.605 秒/step | 0.570 秒/step | ×0.94 |

`--fast fp16_accumulation` はサンプラーに FP16 で accumulate させますが、これはパッチ対象の経路ではないためほぼ動きません。GEMM の対照と整合する結果で、このフラグをわざわざ付けるのが間違いである理由でもあります。

## 実測結果（MSI RTX 3090）

各状態 3 回、3060 と同じハーネス・同じフラグで、4096³・FP32 accumulate:

| 経路 | 変更前 | 変更後 | 変化 | クロック当たり | 消費電力 |
| --- | --- | --- | --- | --- | --- |
| FP16 tensor, FP32 accumulate | 70.95 TFLOPS | 122.49 TFLOPS | ×1.73 | ×1.92 | 315.6 → 296.5 W |
| BF16 tensor, FP32 accumulate | 70.29 TFLOPS | 127.30 TFLOPS | ×1.81 | ×1.91 | 320.7 → 337.2 W |
| FP16 tensor, FP16 accumulate（対照） | 129.26 TFLOPS | 129.52 TFLOPS | ×1.00 | — | 298.1 → 298.8 W |
| FP32（対照） | 24.78 TFLOPS | 24.74 TFLOPS | ×1.00 | — | 285.6 → 284.9 W |

傾向は 3060 と同じで、理由も同じです。クロック当たりではパッチ対象の経路が 2 倍になり、絶対値の伸びが小さいのは、そのぶんカードが 350 W の上限に張り付いて（実測 296〜337 W）低いクロックに落ちるためです（1980 → FP16 1785 MHz、BF16 1860 MHz）。

prefill と decode は別のハーネス（LLM サーバー、各ケース 3 回）で、各状態 2 回ずつ測りました。prompt のスループットは 4K で +8.8%、16K で +13.2% で、同じ状態の 2 回は 0.5 ポイント以内で一致します。decode は測定できる変化がありませんでした。16K では同じ状態でも実行ごとに ±10% 程度動くため（パッチなしで 109.9 と 125.3 tok/s、パッチありで 102.9 と 108.0）、1 回の値で何かを主張することはできません。以前 −13.8% と読んだのは、この高い側の 1 サンプルに乗った結果です。このハーネスの生データはこのリポジトリには入っていません。

通常の再起動だけで元に戻ります。再起動後に同じ GEMM プローブを回すと 72.0 TFLOPS・35.8 TFLOPS/GHz で、ロックされた状態でした。

## 消費電力と効率

経路ごとの消費電力は上の 2 つの表の最終列です。同じ実行を 1 J あたりの仕事で見ると:

| カード | 経路 | 変更前 | 変更後 | 1 J あたりの仕事 |
| --- | --- | --- | --- | --- |
| RTX 3060 12GB | FP16 tensor, FP32 accumulate | 249.4 GFLOPS/W | 360.5 GFLOPS/W | ×1.45 |
| RTX 3060 12GB | BF16 tensor, FP32 accumulate | 249.7 GFLOPS/W | 343.4 GFLOPS/W | ×1.38 |
| RTX 3090 | FP16 tensor, FP32 accumulate | 222.8 GFLOPS/W | 413.1 GFLOPS/W | ×1.85 |
| RTX 3090 | BF16 tensor, FP32 accumulate | 219.2 GFLOPS/W | 377.5 GFLOPS/W | ×1.72 |

対照もここでは動きません。スループットの対照と同じ論拠です: FP16 accumulate は 3060 で 389.0 → 387.7 GFLOPS/W、3090 で 433.6 → 433.4、FP32 は 57.1 → 56.6 と 86.7 → 86.8。

3060 は変更後に電力を増やします。上限 170 W に対して余裕があり、131〜141 W に収まるからです。3090 はすでに上限 350 W に張り付いているので、クロック当たり 2 倍になった仕事を低いクロックで賄う形になり、電力はほとんど動きません。どちらでも 1 J あたりの仕事は 1.4〜1.9 倍になり、SM クロックはすべてのケースで下がります（3060 は 1920 → 1882/1890 MHz、3090 は 1980 → 1785/1860 MHz）。

## 速くならないもの

VRAM 帯域、FP32 のスループット、INT8 のカーネル、そしてメモリ帯域に律速されるものすべて（LLM のトークン生成を含む）。ゲームでは何も変わりません。効果があるのは、密な FP16/BF16 の行列積を FP32 accumulate で回す時間が長い処理です: 学習・ファインチューニング、prefill、attention カーネル、拡散モデル。

未解決の点が 1 つあります。参照ホストではターゲット GPU のリンクが **Gen4 x1**（1.60 GB/s）で trained していました。上の計算律速の数値には影響しませんが、そのマシンで測る帯域律速の値（モデルのロードやオフロード）は歪みます。

## 対応範囲

確定済みのカードは、それぞれ 1 つのボードプロファイルと 1 つのホストプロファイルの組です。どちらも SKU に対する汎用設定ではありません。

| | RTX 3060 12GB | MSI RTX 3090 |
| --- | --- | --- |
| ボードプロファイル | `ga106_2504`: `10de:2504`、subsystem `10de:1536`、PROM `b1608213…a4da`、ダイ `0xb76000a1`、12288 MiB | `ga102_2204`: `10de:2204`、subsystem `1462:3881`、PROM `de78e145…2dd1aad1`、ダイ `0xb72000a1`、24576 MiB |
| ホストプロファイル | `nvidia_bus5`: ターゲット `0:5:0.0`、コンソールは `01:00.0` の 2 枚目の NVIDIA カード | `amd_igpu_bus1`: ターゲット `0:1:0.0`、コンソールは `08:00.0` の AMD iGPU |
| ファームウェア | NVIDIA 610.43.03 に固定した 5 アセット | 同じ 5 アセット |
| 効果の範囲 | 1 ブート限り、VRAM 上のみ、VBIOS への書き込みなし | 同左 |

トランザクションが UEFI ドライバを解放するのは**ターゲット側**のコントローラだけで、そのあとそのカードをリセットします。コンソールは別のアダプタにあるので動き続けます。

どちらのモデルでも、別個体のカードはまず subsystem か PROM のゲートで落ちるはずです。これらの値は SKU の性質ではなく 1 枚ごとの性質だからです。これは意図した挙動で、自分のカードを対応させる手順が下の探索手順です。

## USB からの実行

1. トランザクション用イメージをビルドし、5 つのアセットを添えてパッケージします:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile nvidia_bus5
   python .\tools\package_usb.py --assets .\assets --output .\usb-package
   ```

   `unlock/assets.json` が 5 ファイルの一覧です。`package_usb.py` は一致しないアセットを含むパッケージを拒否します。ここには同梱していません（NVIDIA のファームウェアで、上流が文書化している固定入力から生成するものです）。

2. `usb-package` の**中身**を FAT32 の USB のルートにコピーします。`GPU-UNLOCK` フォルダは書き込み可能である必要があります（アプリはそこにレポートを書きます）。

3. Secure Boot を切り、CSM を切り、モニタをターゲットではないカードに 1 台だけ繋ぎ、PC の電源を完全に切ってから **UEFI USB** のエントリで起動します。

4. `READY` で **U を 1 回**押します。Esc、または 30 秒放置でキャンセルされます。

5. `UEFI transaction completed and cleaned up` が出たら **Windows Boot Manager** を選びます。その間で再起動しないでください。

`STOP / POWER OFF` が出た場合は、電源ボタンで落としてから `GPU-UNLOCK/RUNxx/REPORT.TXT` を読んでください。カードのフラッシュには何も書いていないので、電源断が完全な巻き戻しになります。

## プロファイルと、別のカードを対応させる方法

アプリが書き込み前に比較する値は、すべてコンパイル時の 2 種類のプロファイルから来ます:

- **ボードプロファイル**（`unlock/profiles/`）: 1 枚のカードを記述します（PCI 識別、VBIOS ハッシュ、ダイ ID、コールド時のセレクタ）。
- **ホストプロファイル**（`unlock/hosts/`）: PC 側を記述します（ターゲットの位置と、リセット中にコンソールを維持するファームウェアディスプレイ）。

各フィールドは `GU_BOARD_HAVE_*` / `GU_HOST_HAVE_*` フラグを持ちます。フラグが 1 になるのは、preflight がそのカード・そのホストから実際に読み取ったときだけです。未確定のフィールドが 1 つでもある間、そのプロファイルから作れるのは読み取り専用 preflight だけで、トランザクションのビルドは `#error` で止まります。preflight では未確定のゲートは次のように記録され、

```text
gate <name>: PENDING (profile value not confirmed on this card)
```

pass として扱われることはありません。自分のカードを対応させるには `-HostProfile discovery -Preflight` でビルドし、コールドブートで数回動かして、報告された値を埋めます。フィールド一覧と手順は [docs/BOARD-PROFILE.ja.md](docs/BOARD-PROFILE.ja.md) にあります。

## ビルドとテスト

clang と lld-link（LLVM 12 以降）、ホストテスト用の MSVC 環境、Python 3.9 以降が必要です。ツールチェーンのパスは `GU_LLVM_ROOT`、`GU_CLANG`、`GU_VCVARS` で上書きできます。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile nvidia_bus5 -Preflight
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile nvidia_bus5
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\test-all.ps1
```

`-Preflight` を付けると ARM/トランザクション側がリンク時に落ちます。そのイメージは config space、ファームウェアの BAR 記述子、コールド状態のレジスタ、PROM ウィンドウを読むだけで、GPU には何も書きません。`test-all.ps1` はハードウェアに触れません。

## このリポジトリのデータ

| カード | preflight | トランザクション | 測定値 |
| --- | --- | --- | --- |
| RTX 3060 12GB | `evidence/preflight-3060-01..03/`（探索）、`-04/`（確定） | `evidence/unlock-3060-01/` | `results/rtx3060/gemm/`、`results/rtx3060/comfyui/` |
| MSI RTX 3090 | `evidence/preflight-3090-01..03/`（探索）、`-04/`（確定） | `evidence/unlock-3090-01/` | `results/rtx3090/gemm/` |

preflight のレポートはアプリ自身のログで、トランザクションのレポートは `FINAL status=0 stage=transaction attempted=1 driver_release=1 clean=1` で終わります。3090 の節にある LLM の数値は別ハーネスによるもので、生データはこのリポジトリには入っていません。

## 注意点

- ComfyUI の数値は各状態 1 ブートずつ。GEMM は各 3 回ですが、ブートごとに交互に取ったものではありません。
- ドライバやファームウェアを更新するとアセットが変わり、ゲートは 610.43.03 に固定されています。
- 長時間の安定性、サスペンド/レジューム、複数 GPU の組み合わせはここでは未試験です。
- FP32 の列は対照であって、効果ではありません。

## 来歴とライセンス

移植部分と再利用したコアは **GPL-2.0-only** です。系譜は CMP 40HX のアンロッカー → [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock) → [balaena01/rtx3080-unlock-12g](https://github.com/balaena01/rtx3080-unlock-12g) → このフォーク、という順です。同梱の EDK2 ヘッダはそれぞれのライセンスを保ち、アプリが読み込む NVIDIA のファームウェアはここでは再配布していません。[LICENSE](LICENSE) と [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を参照してください。

RTX 3080 10GB / 12GB のプロファイルは変更せずに引き継いでいます。それらの測定値と結果ファイルは上の各リポジトリに属するもので、ここでは再現していません。このフォークが持つデータは RTX 3060 のものだけです。
