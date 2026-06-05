#include "window_utils.h"

extern HWND g_hMainWnd; // console.cpp で定義されているコンソールウィンドウのハンドル

// ゲームウィンドウがアクティブであり、かつチートコンソール画面が開かれていない状態か判定します。
// 実装理由：アクティブでない状態でのキー入力誤作動（別の作業中にチート機能が反応する等）を防ぐため。
bool IsGameFocused() {
    HWND activeWindow = GetForegroundWindow();
    if (!activeWindow || activeWindow == g_hMainWnd) {
        return false;
    }

    DWORD activeProcId;
    GetWindowThreadProcessId(activeWindow, &activeProcId);

    // UWPアプリ対策として、親ウィンドウから子ウィンドウのプロセスIDを取得します。
    // 実装理由：MinecraftがUWPアプリとして動作している場合、GetForegroundWindowが直接ゲームプロセスのIDを返さないことがあるため。
    if (activeProcId != GetCurrentProcessId()) {
        HWND childHwnd = FindWindowExA(activeWindow, NULL, "Windows.UI.Core.CoreWindow", NULL);
        if (childHwnd) {
            GetWindowThreadProcessId(childHwnd, &activeProcId);
        }
    }

    return activeProcId == GetCurrentProcessId();
}
