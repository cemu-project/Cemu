# CemuExtend

CemuExtendは、Wii Uエミュレーター[Cemu](https://github.com/cemu-project/Cemu)をベースに、`.cemod`パッケージとCemuExtend ABI 2（CEX2）を追加したフォークです。通常のCemuと同じWii Uタイトル・Homebrew・ASM Graphic Packを実行できます。

`.cemod`には次の2つの実行モードがあります。

| モード | 用途 | 信頼境界 |
| --- | --- | --- |
| `isolated` | CEX2サービスだけを使う第三者Mod | Mod専用PPCアドレス空間、W^X、CPU・memory quota |
| `trusted_native` | Minecraft hook、GX2/Cafe API、ゲームmemoryを使うnative Mod | sandboxなし。同一タイトルのnative Modは相互に信頼 |

どちらも`package_version: 1`、`api_version: 2`、`execution_mode`を持つ`manifest.json`が必須です。旧共有memory ABI 1は受け付けません。

## CEX2

CEX2は`CEX2Query`、`CEX2Open`、`CEX2Submit`、`CEX2Poll`、`CEX2Cancel`、`CEX2Close`で構成されます。requestは呼び出し時にCemu所有memoryへコピーされ、responseはPoll時に検査済みguest bufferへコピーされます。共有Header、Ring、State Page、Bulk Poolはありません。1 messageの上限は64 KiBです。

提供サービスはCore、Input、Logging、Configuration、File、Clipboard、Window、Capture、Diagnosticsです。FileとConfigurationはownerごとのnamespaceを使用し、path・quota・型・pagination・async completionをhost側で検証します。

`isolated` ownerはMod principal単位です。`trusted_native` ownerは`trusted-title:<titleId>`であり、同じタイトルの承認済みnative ModはCEX2 session、権限、File／Configuration namespaceを共有します。

## `.cemod` package

`.cemod`はZIP containerで、`manifest.json`、PPC32 big-endian ELF (`mod.elf`)、任意のEd25519公開鍵と署名を格納します。署名対象は各entryの名前・長さ・SHA-256を固定順に連結したdigestです。

署名済みprincipalはpublisher fingerprintとMod ID、未署名principalはpackage SHA-256です。未署名packageは内容が変わると再承認が必要です。署名済み更新も要求権限が増えた場合は再承認が必要です。署名だけで権限が増えることはありません。

packageはユーザーデータの`cemuextend/mods/`へ配置します。`Options` → `General settings` → `CemuExtend`で実行モード、署名状態、要求権限を確認し、明示的に承認してください。変更は次回タイトル起動時に反映されます。

### `isolated`

`isolated` ELFは`cemod_init`、`cemod_tick`、`cemod_event`、`cemod_shutdown`を公開します。codeはRX、data／stackはRW-NXです。ゲームmemory、任意Cafe API、任意hookは公開されません。InterpreterとJITの双方でaddress spaceと権限を検査します。

### `trusted_native`

`trusted_native`はPPC32 big-endian ET_DYNです。undefined symbolとW+X segmentを拒否し、対応relocationは`R_PPC_ADDR32`、`ADDR16_LO`、`ADDR16_HI`、`ADDR16_HA`、`REL24`、`REL32`、`RELATIVE`だけです。Cafe、GX2、CEX2はMod自身が`OSDynLoad`で解決します。

最初のbranchだけをELFの`.cemod.bootstrap` sectionにCMB1 tableとして記録します。Cemuはmodule hash、対象命令とmask、handlerの実行segment、REL24範囲、patch競合を検証してから一括適用し、JIT cacheを無効化します。それ以降のhookはMod側C++と`libhookevent`で設置できます。

trusted ELFはASM適用後に`mod_id`順で共有4 MiB codecaveへ配置されます。検証失敗、競合、容量不足では該当Modをロードせず、部分patchを残しません。タイトル終了時は元命令を復元してJITを再無効化し、codecaveとCEX2 sessionを解放します。

> [!WARNING]
> `trusted_native`はsandboxではありません。ゲームmemory、Minecraft allocator、GX2/Cafe API、他のnative Modへ無制限にアクセスできます。内容とpublisherを信頼できるpackageだけを承認してください。

## SDK

ゲストSDKは[libcemuextend](https://github.com/PinkDiamondTeam/libcemuextend)です。CEX2 Client APIとwire schemaは両モード共通です。`isolated`は検査済みHLE stub、`trusted_native`は`ConfigureTrustedCafePlatform`で設定する`OSDynLoad` resolverを使用します。

Minecraft向けの参照実装は隣接する`mcwiiu-client-template`にあり、描画hook、Minecraft allocator、ImGui、GX2 backend、`libhookevent`、CEX2 Clientを組み合わせた署名可能なtrusted `.cemod`をDockerで生成します。

## Docker build

サブモジュールを取得後、次を実行します。

```sh
git submodule update --init --recursive
./docker-build.sh
```

scriptはRelease buildとCTestを実行し、両方が成功した場合だけ最終実行ファイルを次の固定先へ原子的に配置します。

```text
/home/umi/Workspace/wiiu/CemuExtend/result/bin/Cemu_release
```

一般的な依存関係と非Docker buildは[BUILD.md](BUILD.md)を参照してください。

## 主な実装

| パス | 内容 |
| --- | --- |
| `src/Cafe/HW/Espresso/CemodPackage*` | ZIP、manifest、署名、ELF検証 |
| `src/Cafe/HW/Espresso/ModExecutionContext*` | isolated memory・CPU・principal |
| `src/Cafe/HW/Espresso/TrustedCemodRuntime*` | ET_DYN relocation、CMB1 bootstrap、codecave lifecycle |
| `src/Cafe/OS/libs/cemuextend/` | CEX2 owner、session、service、storage |
| `src/gui/wxgui/GeneralSettings2.cpp` | package承認、実行モード・署名・権限表示 |
| `dependencies/libcemuextend/` | ABI 2 SDKとwire schema |

## License

CemuExtendはupstream Cemuと同じ[Mozilla Public License 2.0](LICENSE.txt)でライセンスされます。`dependencies/`と一部のsourceには各ファイル記載の別licenseが適用されます。
