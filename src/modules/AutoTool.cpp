#include "AutoTool.h"

// AutoToolの現在の有効/無効状態を保持するグローバル変数。
// 実装理由：フックハンドラからこの変数を参照し、ログ出力処理を行うか判定するため。
static bool g_enabled = false;

// AutoToolモジュールの有効化/無効化状態を取得します。
// 実装理由：設定の保存処理やメニュー画面の色表示の切り替えに用いるため。
bool GetAutoToolEnabled() {
    return g_enabled;
}

// AutoToolの有効化/無効化を設定します。
// 実装理由：コンソールからの操作イベントを受けて、即座に状態（有効/無効）を切り替えるため。
void SetAutoToolEnabled(bool enabled) {
    g_enabled = enabled;
}
