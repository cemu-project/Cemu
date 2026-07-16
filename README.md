# CemuExtend

CemuExtend は、Wii U エミュレーター [Cemu](https://github.com/cemu-project/Cemu) をベースに、ゲーム内で動く PowerPC コードとホスト PC を接続する **CemuExtend Bridge** と、バイナリ Graphic Pack パッチ（CPB1）の読み込みを追加したフォークです。

通常の Cemu と同じように Wii U タイトルや Homebrew を実行できることに加え、対応するゲスト Mod からホスト側の入力、ファイル、クリップボード、画面情報などを、バージョン付きの共有メモリ ABI 経由で利用できます。

> [!IMPORTANT]
> CemuExtend 固有機能を使用するには、このフォークでゲームを起動する必要があります。通常の Cemu には `cemuextend` HLE モジュールと CPB1 ローダーは含まれていません。

## libcemuextend との関係

Bridge は、CemuExtend 側のホスト実装と、ゲスト側で利用する `libcemuextend` に分かれています。

| コンポーネント | 動作する場所 | 役割 |
| --- | --- | --- |
| **CemuExtend**（このリポジトリ） | ホスト PC | Cemu 本体、Bridge の HLE ホスト、権限管理、CPB1 ローダー |
| [libcemuextend](https://github.com/PinkDiamondTeam/libcemuextend) | 共通ヘッダー／Wii U ゲスト | Bridge の wire ABI と、ゲストから接続する C++20 SDK |

```text
ホスト PC                                      emulated Wii U / PowerPC
┌──────────────────────── CemuExtend ──────────────────────────────────┐
│ BridgeHost ── 共有メモリ ABI 1.0 ── libcemuextend::guest::Client    │
│     │                                  │                              │
│ 入力・FS・Clipboard・Capture        対応するゲスト Mod                │
│ CPB1 Graphic Pack loader                                             │
└──────────────────────────────────────────────────────────────────────┘
```

`libcemuextend` は Cemu や特定タイトルの型に依存しません。CemuExtend は `dependencies/libcemuextend` をサブモジュールとして取り込み、ホストとゲストで同じ ABI 定義を共有します。タイトル固有の状態やイベントは、各ゲスト Mod が追加サービスとして定義できます。

## CemuExtend 固有の機能

### CemuExtend Bridge

ゲストは HLE モジュール `cemuextend` の `CEXQuery`、`CEXRegister`、`CEXNotify`、`CEXUnregister` を動的に解決し、ゲスト側で確保した共有メモリ領域をホストへ登録します。既定の 256 KiB 領域には、サービスディレクトリ、ホスト／ゲスト状態ページ、要求・応答・イベント用 SPSC ring、64 KiB の bulk block 2個が含まれます。

Bridge ABI 1.0 が提供するホストサービスは次のとおりです。

| サービス | 主な用途 |
| --- | --- |
| Core | サービス列挙、Ping、バージョン、購読、統計 |
| Input | キーボード・マウス・コントローラー状態、VPAD 観測、入力注入 |
| Logging | ゲストから Cemu ログへの出力 |
| Configuration | タイトル単位の型付き Key-Value 設定 |
| File | タイトルごとに隔離されたディレクトリ内のファイル操作 |
| Clipboard | ホストのクリップボード取得・設定 |
| Window | TV/DRC サイズ、DPI、フォーカス、フルスクリーン状態 |
| Capture | TV または GamePad 画面の RGB キャプチャ |
| Diagnostics | heartbeat、ring 使用量、エラー、転送量などの統計 |

ファイル操作は Cemu のユーザーデータディレクトリにある `cemuextend/files/<title-id>/` 以下へ制限されます。絶対パス、ルート名、`..` による脱出、シンボリックリンク経由の脱出は拒否されます。Configuration の値も `cemuextend/config/<title-id>.bin` へタイトル単位で保存されます。

### タイトル単位の権限

Bridge の Read / Write / Inject 権限は、Cemu の `Options` → `General settings` → `CemuExtend` からタイトル ID ごとに設定できます。設定変更は実行中のセッションにも反映されます。

既定では、Core・Input・Configuration・File・Window・Diagnostics の Read と、Input・Logging の Write のみ許可されます。入力マッピングへ直接介入する Inject、クリップボード、画面キャプチャなどは明示的に許可してください。ゲスト側は許可されていない操作に対して `PermissionDenied` を受け取ります。

> [!WARNING]
> Write / Inject 権限は、ホストのデータやゲーム入力を変更できます。信頼できるタイトル／Mod に必要最小限の権限だけを付与してください。

### CPB1 Graphic Pack パッチ

CemuExtend は、Graphic Pack 内の `patch_*.cpb` を読み込めます。CPB1 は codecave ペイロード、パッチレコード、再配置情報をまとめたバイナリ形式で、巨大な `.asm` パッチより高速かつ確実に配布するために使用します。

形式、対応 relocation、パッケージ構成、検証方法は [GRAPHIC_PACK_CPB.md](GRAPHIC_PACK_CPB.md) を参照してください。

## ゲスト Mod を動かす

`libcemuextend` を組み込んだゲスト Mod を動かす基本手順は次のとおりです。

1. CemuExtend をビルドまたは用意して起動します。
2. ゲスト Mod を含む Graphic Pack を Cemu の `graphicPacks` 以下へ配置します。
3. `Options` → `Graphic packs` でパックを有効にし、対象タイトルを起動します。
4. ゲストが `cemuextend` HLE モジュールを検出すると、Bridge セッションが登録されます。
5. 必要に応じて `General settings` → `CemuExtend` で対象タイトルの権限を設定します。

ゲスト側の組み込み方法と Client API は [libcemuextend](https://github.com/PinkDiamondTeam/libcemuextend) を参照してください。

## ビルド

サブモジュールには `libcemuextend` を含むため、再帰的に取得してください。

```sh
git clone --recursive https://github.com/PinkDiamondTeam/CemuExtend.git
cd CemuExtend
```

すでにクローン済みの場合は次を実行します。

```sh
git submodule update --init --recursive
```

Linux で Nix を使用する場合:

```sh
nix develop
cmake -S . -B build/nix -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_VCPKG=OFF \
  -DALLOW_PORTABLE=OFF
cmake --build build/nix --parallel
./bin/Cemu_debug
```

Docker を使用する場合:

```sh
docker build -t cemu-extend:build .
docker run --rm -it cemu-extend:build
```

Windows、Linux、macOS、FreeBSD の依存パッケージや各ビルド方法は [BUILD.md](BUILD.md) を参照してください。CemuExtend は C++20 を使用し、Windows / Linux / macOS の 64-bit 環境を対象とします。

## ソースコード上の主要箇所

| パス | 内容 |
| --- | --- |
| `src/Cafe/OS/libs/cemuextend/` | HLE モジュールと Bridge ホスト実装 |
| `src/gui/wxgui/GeneralSettings2.cpp` | タイトル別 Bridge 権限の設定 UI |
| `src/Cafe/GraphicPack/GraphicPack2PatchesParser.cpp` | CPB1 Graphic Pack ローダー |
| `dependencies/libcemuextend/` | ホストとゲストで共有する wire ABI |
| `GRAPHIC_PACK_CPB.md` | CPB1 の仕様と生成・検証方法 |

## upstream Cemu について

Cemu は C/C++ で実装されたオープンソースの Wii U エミュレーターです。基本的なセットアップ、ゲーム互換性、一般的な問題については、[Cemu 公式サイト](https://cemu.info)、[互換性 Wiki](https://wiki.cemu.info/wiki/Main_Page)、[セットアップガイド](https://cemu.cfw.guide) を参照してください。

Cemu 本体に由来する問題と CemuExtend 固有機能の問題を切り分ける際は、同じタイトルを upstream Cemu でも再現できるか確認してください。

## ライセンス

CemuExtend は upstream Cemu と同じく [Mozilla Public License 2.0](LICENSE.txt) でライセンスされます。`dependencies/` および一部のソースには、それぞれのファイルに記載された別ライセンスが適用されます。
