#pragma once
#include <windows.h>
#include <string>
#include <vector>

// ---------------------------------------------------------
// 外部モジュールから参照・操作するためのグローバル変数
// ---------------------------------------------------------
extern HWND g_hMainWnd;       // メインのコンソールウィンドウハンドル
extern bool shouldExit;       // スレッドおよびDLLの終了シグナル

extern std::vector<int> g_openConsoleKeys;  // コンソールを開くキー（仮想キーコード配列）
extern std::vector<int> g_freeCameraKeys;   // FreeCameraの有効/無効を切り替えるトグルキー配列

extern bool g_isWaitingForKeyBind;  // キーバインド入力待ちフラグ

// キーバインド変更時に、どの機能のキーを変更しようとしているかを区別する列挙型。
// 実装理由：コンソール起動キーとFreeCameraトグルキーの変更要求を共通の入力待ちループで処理するため。
enum class KeyBindTarget {
    Console,
    FreeCamera
};
extern KeyBindTarget g_keyBindTarget; // 現在待受中のキーバインド対象

extern bool g_isWaitingForSpeedInput; // 移動速度（Speed）の数値入力待ちフラグ

// ---------------------------------------------------------
// コンソール制御API
// ---------------------------------------------------------
void AddLog(const std::wstring& text);
void CreateCustomConsole(HINSTANCE hInst);
void DestroyCustomConsole(HINSTANCE hInst);

// メニュー操作用API
void MenuUp();
void MenuDown();
void MenuSelect();
void UpdateMenuDisplay(); // メニューの描画更新API。実装理由：ホットキー操作時などに別スレッドからメニュー画面のトグル状態（文字色）をリアルタイムに再描画させるため。

// 設定のロード・セーブ
void LoadKeybinds();
void SaveKeybinds(const std::vector<int>& vks);

// キー割り当て処理
void BindNewKeys(const std::vector<int>& vks);          // コンソールオープンキーのバインド確定
void BindFreeCameraKeys(const std::vector<int>& vks);   // FreeCameraトグルキーのバインド確定
