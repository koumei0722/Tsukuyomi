#include "hotkey_monitor.h"
#include "console/console.h"
#include "modules/FreeCamera.h"
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>

// スレッド終了判定用フラグの外部参照（dllmain.cppで定義）
extern bool shouldExit;

// キーバインド待受対象の外部参照（console.cppで定義）
extern KeyBindTarget g_keyBindTarget;

// キーバインド待受開始時に一度全てのキーが離されたか管理するフラグ
static bool g_hasClearedInitialKeys = false;

// キーボード上の有効なキーのみを判定するヘルパー関数
// 実装理由：特定の環境で常にONと判定されるゴーストキーをスキャン対象から除外するため。
static bool IsValidKeyboardKey(int vk) {
    if (vk == VK_BACK || vk == VK_TAB || vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_SPACE) return true;
    if (vk >= VK_PRIOR && vk <= VK_DOWN) return true; // PageUp, PageDown, End, Home, Left, Up, Right, Down
    if (vk == VK_INSERT || vk == VK_DELETE) return true;
    if (vk >= '0' && vk <= '9') return true;
    if (vk >= 'A' && vk <= 'Z') return true;
    if (vk >= VK_NUMPAD0 && vk <= VK_DIVIDE) return true; // テンキー 0-9, *, +, -, ., /
    if (vk >= VK_F1 && vk <= VK_F24) return true;
    if (vk >= VK_LSHIFT && vk <= VK_RMENU) return true; // LSHIFT, RSHIFT, LCONTROL, RCONTROL, LMENU, RMENU
    if (vk >= 0xBA && vk <= 0xC0) return true; // 記号キー (; = , - . / `)
    if (vk >= 0xDB && vk <= 0xDF) return true; // 記号キー ([ \ ] ' など)
    if (vk == 0xE2) return true; // _ (ろ) キーなど
    return false;
}

void InitializeHotkeyMonitor() {
    g_hasClearedInitialKeys = false;
}

// キーバインド待受時のキーポーリング処理
// 実装理由：キーバインド設定画面（[ Press any key to bind... ]）において、
// 押されたキーが完全に離されるまでの入力コンボ（同時押し）を検知し、適切にバインドさせるため。
static void HandleKeyBindWaiting() {
    if (!g_hasClearedInitialKeys) {
        // 待受開始時に一度すべてのキーが離されるのを待ちます。
        // 実装理由：メニュー選択時に押されたSpace/Enterキーの指が離れる前の状態を
        // 新しいバインドキーとして誤登録してしまうのを完全に防ぐため。
        bool anyPressed = false;
        for (int k = 8; k < 256; ++k) {
            if (!IsValidKeyboardKey(k)) continue;
            if (GetAsyncKeyState(k) & 0x8000) {
                anyPressed = true;
                break;
            }
        }
        if (!anyPressed) {
            g_hasClearedInitialKeys = true;
        }
        return;
    }

    std::vector<int> pressedKeys;
    int firstKey = 0;
    
    // 1. 最初に入力されたキーを検出
    for (int k = 8; k < 256; ++k) {
        if (!IsValidKeyboardKey(k)) continue;
        if (GetAsyncKeyState(k) & 0x8000) {
            // 左右の修飾キー（Shift, Control, Alt）を一般的な仮想キーコードに正規化します。
            // 実装理由：左右個別のキーコードで登録してしまうと、ゲームプレイ中の同時押し判定や
            // メニュー上の文字列表記（"CTRL"など）との整合性が崩れ、バグを引き起こしやすくなるため。
            int normalizedVk = k;
            if (k == VK_LCONTROL || k == VK_RCONTROL) normalizedVk = VK_CONTROL;
            else if (k == VK_LSHIFT || k == VK_RSHIFT) normalizedVk = VK_SHIFT;
            else if (k == VK_LMENU || k == VK_RMENU) normalizedVk = VK_MENU;

            firstKey = normalizedVk;
            pressedKeys.push_back(normalizedVk);
            break;
        }
    }

    if (firstKey != 0) {
        // 2. キーが押されている間、追加で押されたキーもすべて収集しながら、
        //    最終的に「すべてのキーが完全に離される」までループ監視します。
        while (true) {
            bool anyKeyPressed = false;
            for (int k = 8; k < 256; ++k) {
                if (!IsValidKeyboardKey(k)) continue;
                if (GetAsyncKeyState(k) & 0x8000) {
                    anyKeyPressed = true;
                    
                    // 左右の修飾キーを一般的な仮想キーコードに正規化
                    int normalizedVk = k;
                    if (k == VK_LCONTROL || k == VK_RCONTROL) normalizedVk = VK_CONTROL;
                    else if (k == VK_LSHIFT || k == VK_RSHIFT) normalizedVk = VK_SHIFT;
                    else if (k == VK_LMENU || k == VK_RMENU) normalizedVk = VK_MENU;

                    // 重複を避けて同時押し配列に追加
                    // 実装理由：ユーザーが順番に押し下げる際に、同一のキーコードが
                    // 配列に多重に登録されてキーバインド設定が汚れてしまうのを防ぐため。
                    if (std::find(pressedKeys.begin(), pressedKeys.end(), normalizedVk) == pressedKeys.end()) {
                        pressedKeys.push_back(normalizedVk);
                    }
                }
            }
            if (!anyKeyPressed) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 3. 確定されたバインドキーのリストをコンソールモジュールに送信
        if (!pressedKeys.empty()) {
            // 待受対象（Console / FreeCamera）に応じて割り当て関数を振り分けます。
            // 実装理由：1つのバインド待受ロジックから、選択したメニュー項目の対象に正しく割り当てるため。
            if (g_keyBindTarget == KeyBindTarget::Console) {
                BindNewKeys(pressedKeys);
            } else {
                BindFreeCameraKeys(pressedKeys);
            }
            g_hasClearedInitialKeys = false; // 次回のバインド待受に備えてフラグをリセットします。
        }
    }
}

// ゲームアクティブ時のホットキー判定処理
// 実装理由：ゲーム中に登録された同時押しキー（Consoleオープンキー、またはFreeCameraトグルキー）が
// 押された場合に、それぞれの機能をトグル・起動させるため。
static void HandleGameHotkey() {
    HWND activeWindow = GetForegroundWindow();
    if (!activeWindow || activeWindow == g_hMainWnd) return;

    DWORD activeProcId;
    GetWindowThreadProcessId(activeWindow, &activeProcId);
    
    // UWPアプリ特有の親ウィンドウ（ApplicationFrameHost.exe）対策
    if (activeProcId != GetCurrentProcessId()) {
        HWND childHwnd = FindWindowExA(activeWindow, NULL, "Windows.UI.Core.CoreWindow", NULL);
        if (childHwnd) {
            GetWindowThreadProcessId(childHwnd, &activeProcId);
        }
    }

    if (activeProcId == GetCurrentProcessId()) {
        // --- 1. Consoleオープンキーの判定 ---
        bool allPressed = true;
        for (int vk : g_openConsoleKeys) {
            if (!(GetAsyncKeyState(vk) & 0x8000)) {
                allPressed = false;
                break;
            }
        }

        if (allPressed && !g_openConsoleKeys.empty()) {
            ShowWindow(g_hMainWnd, SW_RESTORE);
            ShowWindow(g_hMainWnd, SW_SHOW);
            SetWindowPos(g_hMainWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetForegroundWindow(g_hMainWnd);

            // すべてのキーが離されるまで待機（チャタリング防止）
            while (true) {
                bool anyPressed = false;
                for (int vk : g_openConsoleKeys) {
                    if (GetAsyncKeyState(vk) & 0x8000) {
                        anyPressed = true;
                        break;
                    }
                }
                if (!anyPressed) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return; // 1フレームに複数のホットキーが多重処理されるのを防ぐため早期リターンします
        }

        // --- 2. FreeCameraトグルキーの判定 ---
        // 実装理由：ゲーム内でFreeCameraトグルキー（同時押し対応）が押された際、コンソール有効/無効をトグルし、
        // かつチャタリング（連続したON/OFFのブレ）を防ぐため。
        bool freeCamPressed = true;
        for (int vk : g_freeCameraKeys) {
            if (!(GetAsyncKeyState(vk) & 0x8000)) {
                freeCamPressed = false;
                break;
            }
        }

        if (freeCamPressed && !g_freeCameraKeys.empty()) {
            bool nextState = !GetFreeCameraEnabled();
            SetFreeCameraEnabled(nextState);
            AddLog(nextState ? L"[FreeCamera] Enabled (via hotkey)" : L"[FreeCamera] Disabled (via hotkey)");
            UpdateMenuDisplay(); // ホットキーによるトグル状態の変化をリアルタイムにコンソールメニューに反映させます。

            // トグルキーが完全に離されるまで待機（チャタリング防止）
            while (true) {
                bool anyPressed = false;
                for (int vk : g_freeCameraKeys) {
                    if (GetAsyncKeyState(vk) & 0x8000) {
                        anyPressed = true;
                        break;
                    }
                }
                if (!anyPressed) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
}

void UpdateHotkeyMonitor() {
    HWND activeWindow = GetForegroundWindow();
    if (!activeWindow) return;

    if (g_isWaitingForKeyBind) {
        if (activeWindow == g_hMainWnd) {
            HandleKeyBindWaiting();
        }
    } else {
        g_hasClearedInitialKeys = false;
        HandleGameHotkey();
    }

    // INSERTキーでコンソールを再オープン（手動デバッグ・フォールバック用）
    if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
        ShowWindow(g_hMainWnd, SW_RESTORE);
        ShowWindow(g_hMainWnd, SW_SHOW);
        SetForegroundWindow(g_hMainWnd);
        while (GetAsyncKeyState(VK_INSERT) & 0x8000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

bool ShouldExitModule() {
    // ENDキーが押されたらアンロードシグナルを返します。
    if (GetAsyncKeyState(VK_END) & 0x8000) {
        return true;
    }
    return false;
}
