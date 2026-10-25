# ボードプロファイルとホストプロファイル

**日本語** | [English](BOARD-PROFILE.md)

UEFI アプリが GPU に書き込む前に比較する値は、すべてコンパイル時の 2 種類のプロファイルから来ます:

- **ボードプロファイル**（`unlock/profiles/`）: 1 枚の物理カードを記述します（PCI 識別、VBIOS、ダイ ID、コールド状態のレジスタ）。
- **ホストプロファイル**（`unlock/hosts/`）: プラットフォームファームウェアがどこに何を置くかを記述します（ターゲット GPU の PCI 位置と、ターゲットのリセット中にコンソールを維持するファームウェアディスプレイ（GOP））。

各フィールドは `GU_BOARD_HAVE_*` / `GU_HOST_HAVE_*` フラグを持ちます。フィールドが*確定*（フラグ 1）になるのは、preflight がそのカード・そのホストから実際に読み取ったときだけで、他のボードからのコピーや SKU からの推測では確定しません。

| プロファイル | ファイル | 選択方法 | 状態 |
|---|---|---|---|
| MSI RTX 3080 10GB、`10de:2206` | `profiles/ga102_2206.h` | 既定（または `-DGU_BOARD_2206`） | 確定 |
| MSI RTX 3080 12GB、`10de:220a` | `profiles/ga102_220a.h` | `-DGU_BOARD_220A` | 確定 |
| RTX 3060 12GB、`10de:2504` | `profiles/ga106_2504.h` | `-DGU_BOARD_GA106_2504` | 2026-10-25 に確定、トランザクション 1 回 |
| MSI RTX 3090、`10de:2204` | `profiles/ga102_2204.h` | `-DGU_BOARD_2204` | 2026-10-25 に確定、トランザクション 1 回 |
| テスト用フィクスチャ（未確定） | `profiles/unconfirmed.h` | `-DGU_BOARD_UNCONFIRMED` | ボードではない |
| Intel iGPU がコンソール、GPU は `0:1:0.0` | `hosts/intel_bus1.h` | 既定 | 確定 |
| 2 枚目の NVIDIA カードがコンソール `01:00.0`、ターゲット `0:5:0.0` | `hosts/nvidia_bus5.h` | `-DGU_HOST_NVIDIA_BUS5` | 2026-10-25 に確定 |
| コンソール `08:00.0`、ターゲット `0:1:0.0` | `hosts/console_bus8.h` | `-DGU_HOST_CONSOLE_BUS8` | 2026-10-25 に確定 |
| それ以外のホスト | `hosts/discovery.h` | `-DGU_HOST_DISCOVERY` | 探索用: 未確定 |

`unlock/build.ps1` は `-Board 2206|220a|2204|ga106_2504`、`-HostProfile intel_bus1|nvidia_bus5|console_bus8|discovery`、`-Preflight` を取り、3 つとも `build/build-manifest.json` に記録します。

## 未確定のプロファイルがトランザクション用イメージをビルドできない理由

`port.h` は `GU_BOARD_COMPLETE` と `GU_HOST_COMPLETE` を、それぞれの `HAVE_*` フラグの AND として計算します。プロファイルが定義し忘れたフラグは 0 として読まれるので、書き忘れは「信頼される」のではなく「未確定」になります。`GU_PREFLIGHT` が定義されていない場合、未確定のボードやホストはコンパイルを `#error` で止めます:

```text
port.h: error: "This board profile is not confirmed on a card yet. ..."
port.h: error: "This host profile is not confirmed yet. ..."
```

ゲートの目的は、トランザクションが「書かれた相手のカード」に「検証した状態で」しか書き込まないことです。比較対象が確定していないゲートにはそれができないため、未確定プロファイルが作れるイメージは読み取り専用 preflight だけになります。そのイメージでは未確定フィールドは「通す」のではなく「飛ばす」扱いで、レポートに次のように残ります:

```text
gate <name>: PENDING (profile value not confirmed on this card)
```

サマリは `PREFLIGHT profile=... host=... complete=0` になります。この規則の唯一の定義は `port.h` の `GU_PROFILE_GATE()` で、preflight 以外では `#error` の後ろの最後の砦として拒否します。

FLR をまたいでベンダーのトランザクションが再確認する ID、`GU_TARGET_PCI_ID` は `GU_BOARD_ID` から導出されます。別のボードを指す `-DGU_TARGET_PCI_ID` を渡すと `#error` になり、ボードプロファイルを 2 つ選んでも同じです。

## ボードのフィールド

| フィールド | ゲート | `REPORT.TXT` のどこから読むか |
|---|---|---|
| `GU_BOARD_ID` | config `0x00`（`vendor \| device << 16`）。ターゲットの発見にも使うので pending にならない | `identity: pci=` |
| `GU_BOARD_SUBSYSTEM` / `HAVE_SUBSYSTEM` | config `0x2c`、AIB 自身の ID | `identity: subsystem=` |
| `GU_BOARD_CLASS_REV` / `HAVE_CLASS` | config `0x08`、クラスとリビジョン | `identity: class_rev=` |
| `GU_BOARD_CAP_OFFSET`、`GU_BOARD_CAP_VALUE` / `HAVE_CAP` | VBIOS が置くケイパビリティ dword（GA102 では `0xc8`） | `config[c0]=` 行の 3 番目の dword が `0xc8` |
| `GU_BOARD_BAR0_SIZE` | config space 上の BAR0 のアラインメントと、ファームウェアの BAR 記述子のサイズ | `bar0 descriptor: length=` |
| `GU_BOARD_CHIP` / `HAVE_CHIP` | BAR0+0 のダイ ID | `chip: actual=` |
| `GU_BOARD_PROM_OFFSET`、`GU_BOARD_PROM_SIZE` | ハッシュ対象の PROM ウィンドウ（BAR0+`0x300000`、1 MiB） | — |
| `GU_BOARD_PROM_SHA256` / `HAVE_PROM` | そのウィンドウの SHA-256、つまりカードの VBIOS | `live PROM hash sha256=` |
| `GU_BOARD_FB_MB` / `HAVE_FB` | FEAT `0x001183a4` のフレームバッファ MiB | `cold state: ... fb_mb=` |
| `GU_BOARD_SS0_MASK`、`GU_BOARD_SS0_VALUE` / `HAVE_SS0` | コールド時の SS0（`0x0082381c`）: `(SS0 & MASK) == VALUE` | `cold state: ... SS0=` |
| `GU_BOARD_SS1_BITS` / `HAVE_SS1` | コールド時の SS1（`0x00823820`）: `SS1` 番目のビットが立っていること | `cold state: ... SS1=` |
| `GU_BOARD_LABEL`、`GU_BOARD_PROFILE_NAME` | テキストのみ | — |

ファミリ共通のコールドゲート（GFW ブート状態、FEAT PLM `0xffffff8f`、WPR2 の高位）は GA10x の全ボードで同じなので、プロファイルには含めません。

**ブート間の SS0。** SS0 はコールドブートごとに変わることがあります。220a のカードでは 3 回のブートで `0x01521103`、`0x01521013`、`0x01521003` を観測しました。preflight は最低 3 回ブートしてください。すべてのレポートで SS0 が同一なら `MASK = 0xffffffffU` とその値を使います。そうでなければ、220a のプロファイルと同じく、観測値すべての AND を `MASK` と `VALUE` の両方に使い、揮発ビットでカードが弾かれないようにします。AND の結果が 0 になる場合、それは何もゲートしていないので、そのプロファイルを確定してはいけません。

**SS1** は許容値の集合です。観測された `SS1=n` ごとに `n` 番目のビットを立てます（2206 のプロファイルは 2 と 3 を許容し `0x0000000cU`）。

## ホストのフィールド

| フィールド | ゲート | `REPORT.TXT` のどこから読むか |
|---|---|---|
| `GU_HOST_TARGET_SEGMENT/BUS/DEVICE/FUNCTION` / `HAVE_TARGET` | ターゲット関数の `GetLocation()` | `target BDF=<seg>:<bus>:<dev>.<fn>` |
| `GU_HOST_DISPLAY_DEPTH`、`GU_HOST_DISPLAY_PCI` / `HAVE_DISPLAY` | `PciRoot` より下のコンソール GOP のデバイスパス。`Pci()` ノードごとに `{device, function}` を、根から順に | 採用された GOP の `GOP[n] pci_path=` |

表示チェックは引き続き、物理 GOP がちょうど 1 つ、有効なモード、転送されたコンソールモードの一致を要求します。ホストが未確定のときは加えて `target pci_path=` を記録し、ターゲット GPU 上またはその配下の GOP を拒否するので、トランザクションがリセットするカードがコンソールになることはありません。

Windows のバス番号は手がかりにすぎません。意味を持つのは、UEFI が preflight で報告する値です。

**検証済みのホスト:** `intel_bus1`（Intel iGPU コンソール）は GA102 の 2 枚を動かしました。`nvidia_bus5`（2 枚目の NVIDIA カード `01:00.0` がコンソール、ターゲット `0:5:0.0`）は 2026-10-25 に RTX 3060 のトランザクションを実行し、ターゲットの UEFI ドライバを解放してカードをリセットしつつ、コンソールはもう 1 枚の上で動き続けました。`console_bus8` は同じ PC の逆配置で、RTX 3090 の preflight とそのトランザクションで確認済みです（3090 がターゲット `0:1:0.0`、コンソールは `08:00.0`）。どちらも重要なのは、トランザクションが**ターゲット**コントローラを所有するドライバインスタンスに対して `DisconnectController` を呼び、同じドライバイメージが別のカードでコンソールを提供し続けるからです。

## プロファイルを確定する: preflight、その後に再ビルド

1. **preflight をビルドします**（新しいボードを、探索用ホストで）:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\unlock\build.ps1 -Board ga106_2504 -HostProfile discovery -Preflight
   ```

   または `./preflight-linux.sh usb-3060`。[USB-BOOT.ja.md](USB-BOOT.ja.md) の手順で USB に入れます。
2. **コールドスタートで起動します**（一度電源を完全に切ってから）。最低 3 回。各実行は `\GPU-UNLOCK\RUNxx\REPORT.TXT` を書きます。使ってよいレポートは `PREFLIGHT result=pass` と `FINAL status=0` で終わるものだけです。ゲートが失敗したレポートは、カードがその値の示す状態になかったことを意味します。
3. **レポートが一致することを確認します。** `identity:`（PCI、subsystem、class/revision）、`chip: actual=`、`live PROM hash sha256=` がすべてのレポートで同一である必要があります。違うなら別のカードか、何かが変わっています。何も確定してはいけません。`fb_mb` も同一である必要があります。SS0 と SS1 は上に書いた方法でまとめます。
4. **プロファイルを書きます。** そのボードに公開済みのプロファイルがあるなら、報告された値をそのプロファイルに入れ、先頭のコメントに `RUNxx` のレポート名を書き、`GU_BOARD_HAVE_*` をすべて 1 にします。2026-10-25 に確定した `profiles/ga106_2504.h` がその形です。公開済みプロファイルが無いカードは、探索用のフォールバックブロックを `unlock/profiles/<board>.local.h` にコピーして同じことをします。ローカルファイルは同梱のものを上書きします。どちらでも、レポートに無いフィールドは 0 のままにしてください。未確定は安全な状態です。ホストも同様に（`GU_HOST_PROFILE_NAME`、BDF、表示パス、両方の `HAVE_*` = 1）、`hosts/nvidia_bus5.h` と同じ形で書きます。
5. **preflight を再ビルドして、もう 1 回起動します。** 確定プロファイルが入ると全ゲートが実比較になります。レポートに `PENDING` 行が無く、`complete=1`、`PREFLIGHT result=pass` でなければなりません。ブート間で変わるコールド状態の値を捕まえられるのもこの 1 回だけです。ここまでの実行はマスクを作るためで、この 1 回がマスクを試す番です。
6. **その後で初めて**トランザクション用イメージをビルドします（`-Preflight` なし）。`build-manifest.json` はプロファイル名と、`*.local.h` プロファイルの場合はそのファイルのハッシュを記録します（Git 管理外なので）。

`*.local.h` が Git の管理外なのは意図的です。1 枚のカードと 1 台の PC を記述するものだからです。確定したボードを公開するなら、そのレポートと一緒に `unlock/profiles/` 配下の独立したプロファイルとして追加します（2206、220a、2504、2204 がその例です）。
