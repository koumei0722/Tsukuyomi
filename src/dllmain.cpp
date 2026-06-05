#include <windows.h>
#include <thread>
#include "console/console.h"
#include "hotkey/hotkey_monitor.h"
#include "scans/scans.h"
#include "hooks/hooks.h"
#include "modules/creativeNoClip.h"
#include "modules/AntiDarkness.h"
#include "modules/SchematicControl.h"

bool shouldExit = false;

// バックグラウンドでメッセージループとホットキー監視を行うスレッド関数
DWORD WINAPI MainThread(LPVOID lpParam) {
    HINSTANCE hInst = (HINSTANCE)lpParam;
    
    // カスタムコンソールウィンドウを作成・表示します。
    CreateCustomConsole(hInst);

    MSG msg;
    InitializeHotkeyMonitor();

    // シグネチャスキャンとフックの初期化を実行
    // 実装理由：ゲーム内の各種関数アドレスを特定し、カメラ位置やパケット送信の横取りフックを適用するため。
    if (PerformSignatureScans()) {
        InitializeHooks();
        // シグネチャスキャン完了後、ロードされた設定をメモリに適用します。
        // 実装理由：設定ロード時にはまだ関数のアドレス特定が完了していないため、スキャン完了後にパッチを再適用する必要があるため。
        SetCreativeNoClipEnabled(GetCreativeNoClipEnabled());
        SetAntiDarknessEnabled(GetAntiDarknessEnabled());
        SetSchematicControlEnabled(GetSchematicControlEnabled());
    }

    while (!shouldExit) {
        // ウィンドウメッセージの処理（非ブロッキング）
        // 実装理由：メッセージを毎フレーム処理しないと、コンソールウィンドウが「応答なし」状態になりフリーズするため。
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                shouldExit = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // ホットキーおよびキーバインドの監視・更新処理を実行します。
        // 実装理由：キー入力監視と入力ポーリングの責務を MainThread から分離し、
        // メンテナンス性と可読性を向上させるため。
        UpdateHotkeyMonitor();

        // 外部からDLLアンロードのシグナルが届いたかチェック
        if (ShouldExitModule()) {
            shouldExit = true;
        }

        // CPU負荷を下げるため、また入力監視周期を確保するために 10ms の適切なウェイトを置きます。
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // スキャン用スレッドの終了同期とクリーンアップを実行します。
    // 実装理由：DLLアンロード時にSchematicControlの非同期スキャン用スレッドを安全にjoinさせ、フックを解除するため。
    CleanupSchematicControl();

    // フックの解除処理を実行
    // 実装理由：DLLアンロード時にゲーム側のフックコードを安全に復元するため。
    CleanupHooks();

    // クリーンアップ処理を実行
    DestroyCustomConsole(hInst);
    
    // スレッドの安全な終了とDLLライブラリの自動解放処理
    FreeLibraryAndExitThread(hInst, 0);
    return 0;
}

static HMODULE g_hRichEditDll = NULL;

// DLLエントリーポイント
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        // RichEditコントロールを使用可能にするため、DLLアタッチ時に riched20.dll をロードします。
        // 実装理由：ログ表示の一部にカラーテキスト（Enabled/Disabledの色分け等）を適用するため。
        g_hRichEditDll = LoadLibraryW(L"riched20.dll");

        // ローダーロックを回避するため、ゲーム起動中のメイン処理は別スレッドに逃がします。
        DisableThreadLibraryCalls(hModule);
        if (HANDLE hThread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr)) {
            CloseHandle(hThread);
        }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        if (g_hRichEditDll) {
            FreeLibrary(g_hRichEditDll);
            g_hRichEditDll = NULL;
        }
        break;
    }
    return TRUE;
}
