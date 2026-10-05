# isoforge

[![License](https://img.shields.io/github/license/tmkyd/isoforge)](LICENSE)
[![Release](https://img.shields.io/github/v/release/tmkyd/isoforge)](https://github.com/tmkyd/isoforge/releases/latest)
![Platform](https://img.shields.io/badge/platform-Windows%20x64%20%7C%20x86-0078D4)

[English](README.md) | 日本語

isoforgeは、光学ドライブ上のデータディスク（CD、DVD、Blu-ray）をISOファイルに保存するWindows用のツールです。

PC用の光学ドライブは搭載されることが少なくなり、入手もしにくくなっています。これまでCD、DVD、Blu-rayに保存してきたデータは、ドライブが使えるうちに別の記憶装置へ移す必要があります。ディスクからファイル単位でコピーすると、特に小さなファイルが多い場合に時間がかかります。isoforgeは、ディスクを先頭から末尾まで一度に読み、ISOファイルにします。ISOファイルはそのまま保管できるほか、仮想ドライブとしてマウントし（Windowsではダブルクリック、または右クリックの［マウント］）、そこからファイルをコピーすることもできます。

> [!IMPORTANT]
> isoforgeは、ファイルからのISOの作成（再オーサリング）、コピーガードの解除、音楽CDや映像ディスクのリッピング、媒体への書き込みを**行いません**。

## 特徴

- CD、DVD、Blu-rayのデータディスクを、1行のコマンドでISOファイルに保存できます。読み取りの前に媒体を確認し、保存できない媒体であれば理由を表示して停止します。
- ISOのSHA-256を常に計算し、`sha256sum`形式のハッシュファイルに書き出します。`isoforge verify`で後からISOをハッシュファイルと照合でき、`create --verify`では媒体をもう一度読んで保存した内容を確認できます。
- ドライブ・媒体の情報、進捗、エラーの詳細をログファイルに出力できます（`--log`）。保存に失敗したときの原因の調査に役立ちます。
- 読めなかったセクタを再試行します（既定は3回、`--retries`）。一時的な読み取りエラーで保存が止まりません。

## 動作環境

- Windows 10以降
- 内蔵または外付け光学ドライブ

## インストール

[最新のリリース](https://github.com/tmkyd/isoforge/releases/latest)から、お使いのWindowsに合うzipをダウンロードして展開し、ターミナル（PowerShell、コマンドプロンプト、Windows Terminal）から`isoforge.exe`を実行します。

- `isoforge-<version>-win-x64.zip`：64ビット版のWindows（ほとんどのPC）
- `isoforge-<version>-win-x86.zip`：32ビット版のWindows 10（64ビット版のWindowsでも動作します）

`isoforge.exe`は依存ファイルのない単一の実行ファイルで、通常は管理者権限も不要です。

## 使い方

最も単純な使い方は、ドライブZ:の媒体を現在のフォルダーの`disc.iso`に保存する例です。

```powershell
isoforge create Z: -o disc.iso
```

ISOのSHA-256は、同じフォルダーのハッシュファイル`disc.iso.sha256`に書き出されます。後からISOをハッシュファイルと照合するには、次のように実行します。

```powershell
isoforge verify disc.iso
```

### コマンド

```text
isoforge list [--json]
isoforge info <drive> [--json] [--wait <seconds>]
isoforge create <drive> -o <file.iso> [options]
isoforge verify <file.iso> [--hash <sha256 | file>] [-q] [--no-progress]
isoforge help [<command>]
isoforge -h, --help | -V, --version
```

`isoforge`だけ、または`info`・`create`・`verify`を引数なしで実行すると、ヘルプを表示します。各コマンドのオプションは`isoforge help <command>`で確認できます。

### 例

保存できる媒体かどうかは、`info`で事前に確認できます。次は、ドライブZ:のDVD-ROMを確認した例です。

```console
> isoforge info Z:
Device:         \\.\Z: (CdRom0)
Model:          PIONEER BD-RW BDR-209MIO 1.54
Disc type:      DVD-ROM (profile 0010h)
Capacity:       1968880 blocks of 2048 bytes
Disc status:    complete, last session complete, 1 session(s), tracks 1-1
Track 1:        start 0, 1968880 sectors, session 1
Media range:    LBA 0-1968879 (1968880 sectors, 4032266240 bytes)
ISO 9660:       1968880 sectors
UDF:            1968880 sectors
Adopted range:  LBA 0-1968879 (1968880 sectors, 4032266240 bytes)
Supported:      yes
```

その他の例：

```powershell
# 光学ドライブの一覧と、媒体の有無・媒体種別
isoforge list

# 媒体の準備を待たずに同じ確認をする
isoforge info Z: --wait 0

# 媒体を保存し、2回目の読み取りで照合し、ログを残す
isoforge create Z: -o disc.iso --verify --log isoforge.log

# 読めなかったセクタごとに最大10回再試行する
isoforge create Z: -o disc.iso --retries 10

# 媒体を保存し、結果をスクリプト向けのJSONで出力する
isoforge create Z: -o disc.iso --json
```

### ドライブの指定

| 形式 | 意味 |
| --- | --- |
| `Z`、`Z:`、`Z:\`、`\\.\Z:` | ドライブ文字 |
| `\\.\CdRom0`、`CdRom0` | デバイス名（ドライブ文字のないドライブ向け） |

`CdRom0`はGit Bashなどのシェルでも引用符なしで指定できます。デバイスの番号はUSBドライブの抜き差しで変わることがあるので、`isoforge list`で現在の対応を確認してください。

### `create`のオプション

| オプション | 既定値 | 説明 |
| --- | --- | --- |
| `-o, --output <path>` | 必須 | 出力するISOファイル |
| `--overwrite` | オフ | 既存のISOファイル・ハッシュファイルを上書きする |
| `--keep-partial` | オフ | 失敗・中断したときに一時ファイルを残す |
| `--hash-file <path>` | `<output>.sha256` | SHA-256のハッシュファイルの出力先 |
| `--no-hash-file` | オフ | ハッシュファイルを書かない（ハッシュは表示する） |
| `--verify` | オフ | 書き込み後に媒体をもう一度読み、SHA-256を照合する |
| `--retries <n>` | 3 | 読めなかったセクタごとの再試行の回数（500ミリ秒間隔） |
| `--eject` | オフ | 成功したら媒体を取り出す |
| `--log <path>` | なし | 詳細なログを追記する（10%ごとの進捗を含む） |
| `--wait <seconds>` | 30 | 媒体の準備を待つ最大の秒数。0は待たない |
| `--json` | オフ | 結果をJSONで標準出力に出す。失敗時も出す（`-v`・`-q`とは併用不可） |
| `-v, --verbose` / `-q, --quiet` | | 出力を増やす／減らす |
| `--no-progress` | オフ | 進捗表示を出さない |

`info`は`--json`と`--wait`、`list`は`--json`も受け付けます。`verify`は`-q`（結果の行だけを出力）と`--no-progress`を受け付け、ハッシュファイルに別のファイル名が書かれている場合は警告します。

## `create`の処理

1. 媒体を読む前に、出力ファイルとハッシュファイルが存在しないこと（`--overwrite`を除く）、媒体が対象で範囲を確定できること、出力先に十分な空き容量があり、ファイルサイズの上限（FAT32など）に収まることを確認します。
2. 媒体を`<output>.partial`に読み込み、`--verify`を含めてすべて成功してから`<output>`に名前を変えます。失敗したときやCtrl+Cで中断したときは、一時ファイルを削除します。
3. SHA-256は常に表示し、`--no-hash-file`を指定しなければ`sha256sum`形式（`<hash> *<file name>`）でハッシュファイルに書きます。`sha256sum -c`で照合できます。

再試行しても読めないセクタがあると、保存は失敗します（終了コード3）。唯一の例外は、CDのトラック末尾の1〜2セクタがファイルシステムの範囲外にある場合です（トラックアットワンスで書き込んだCD-Rでは読めないことが多い）。この場合は警告を表示して除外します。

## 対応する媒体

- 2048バイトのセクタと、ISO 9660またはUDFのファイルシステムを持つ、1セッション・1トラックのデータディスク。ファイナライズ済みか、閉じたセッションが1つの追記可能な媒体：
  - CD-ROM、CD-R、CD-RW（Mode 1のデータトラックが1つ）
  - DVD-ROM、DVD±R、DVD±RW、DVD±R DL
  - BD-ROM、BD-R、BD-RE（BDXLの3層・4層を含む）
- 対象外：音楽CD、Mixed Mode CD、Mode 2（CD-ROM XA）、マルチセッション、最終セッションが閉じていない媒体、パケットライトの媒体、DVD-RAM、HD DVD、コピーガードのある媒体。映像ディスク（DVD-Video、BD-Video）は判別しません。暗号化されたセクタは読み取りに失敗します。

対象外の理由は`isoforge info`で確認できます。

## 終了コード

| コード | 意味 |
| --- | --- |
| 0 | 成功 |
| 1 | 引数の誤り |
| 2 | 対象外の媒体・構成、媒体なし、または光学ドライブでない |
| 3 | 読み取りエラー |
| 4 | 出力ファイルのエラー（`--overwrite`なしで既存のファイルがある、空き容量不足を含む） |
| 5 | ハッシュの不一致（`create --verify`、`verify`） |
| 6 | 権限不足 |
| 7 | その他のエラー |
| 130 | 中断（Ctrl+C） |

## ソースからのビルド

Visual Studio 2026（「C++によるデスクトップ開発」ワークロード）が必要です。

```bat
scripts\build.cmd              rem x64とx86のDebug・Releaseの構成・ビルド
scripts\build.cmd x86-release  rem 指定したプリセットだけ
scripts\build.cmd --package    rem x64とx86の配布用zipもout\package\に作成する
```

このスクリプトは、アーキテクチャごとのVisual Studioの開発者環境を自分で読み込みます。CMakeのプリセット（`x64-debug`、`x64-release`、`x86-debug`、`x86-release`）は、同じアーキテクチャの開発者コマンドプロンプト（`VsDevCmd -arch=amd64`または`-arch=x86`）から直接使うこともできます。

```bat
cmake --preset x64-release
cmake --build --preset x64-release
cpack --preset x64-release
```

## 不具合の報告

不具合や、対応しているはずなのに対象外になる媒体は、[GitHub Issues](https://github.com/tmkyd/isoforge/issues)で報告してください。`isoforge info <drive>`（または`--json`）の出力と、失敗した`create`の`--log`のファイルがあると、原因の調査に役立ちます。

## ライセンス

Copyright 2026 tmkyd。Apache License 2.0のもとで提供しています。[LICENSE](LICENSE)を参照してください。
