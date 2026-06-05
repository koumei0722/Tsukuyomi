# Tsukuyomi - Minecraft Native Mod DLL

`Tsukuyomi`（ツクヨミ）は、Windows 10/11版 Minecraft (1.21.13X) 向けのC++製ネイティブMod DLLです。

---

## 🌟 主な機能 (Features)

- **FreeCamera**
- **CreativeNoClip**
- **AntiDarkness**
- **SchematicControl**

---

## 💻 ゲーム内コンソールメニュー (In-Game Menu)

ゲーム内で登録されたキー（デフォルト：`Y`）を押すと、`RichEdit` を用いた専用のコンソールメニューが重ね合わせウィンドウとして展開されます。

- **メニュー操作**:
  - `W` / `S` または `Up` / `Down`: 選択項目の移動
  - `Space`: 決定（トグル切り替え、またはサブメニュー展開）
  - `Esc`: メニューウィンドウの非表示化
  - `Insert`: コンソール強制再表示（緊急フォールバック用）
  - `End`: DLLの安全なアンロード（フック解除とクリーンアップ）
- **キーバインド変更**:
  - コンソール展開キーやFreeCameraトグルキーは、メニューからいつでも変更可能です。
  - 同時押し（キーコンボ）にも対応しています（例：`Ctrl + Alt + Y` など）。
- **自動保存と移行 (Migration)**:
  - 各種設定やキーバインドは、ゲーム実行ファイルと同階層の `Tsukuyomi/` フォルダへ自動保存されます。
  - 設定ファイル形式の変更があった場合、自動的に旧設定から移行（マイグレーション）が行われます。

---

## 🚀 ビルド方法 (How to Build)

依存パッケージの管理には [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) を使用しているため、ビルド時に手動で依存ライブラリをインストールする必要はありません。

### 前提条件
- Windows OS
- CMake (バージョン 3.20 以上)
- C++20 に対応したコンパイラ（MSBuild / Visual Studio 2022 を推奨）

### ビルド手順

1. コマンドプロンプトまたは PowerShell を開きます。
2. プロジェクトのルートディレクトリに移動し、以下のコマンドを実行します。

```bash
# ビルドディレクトリの作成と構成
cmake -B build -D CMAKE_BUILD_TYPE=Release

# ビルドの実行
cmake --build build --config Release
```

ビルドが完了すると、`build/Release/Tsukuyomi.dll` が生成されます。

---

## 💉 使用方法 (Usage)

1. Minecraft（1.21.13X）を起動します。
2. FateInjector などのDLLインジェクターを使用し、生成された `Tsukuyomi.dll` を `Minecraft.Windows.exe` プロセスにインジェクトします。
3. ゲーム内でデフォルトのコンソールキー（`Y`）を押すとメニューが開き、初期ログが表示されます。
