#pragma once

// SchematicControlモジュールの有効化/無効化状態を取得します。
// 実装理由：設定の保存処理やメニュー画面の色表示の切り替えに用いるため。
bool GetSchematicControlEnabled();

// SchematicControlの有効化/無効化を設定します。
// 実装理由：ONの時のみバックグラウンドでシグネチャスキャンを走らせ、発見時にフックを適用するため。
void SetSchematicControlEnabled(bool enabled);

// スキャン用スレッドの終了同期と、フックの解除・クリーンアップを行います。
// 実装理由：DLLアンロード時にバックグラウンドスレッドが解放されずプロセスがフリーズするのを防ぎ、フックを安全に削除するため。
void CleanupSchematicControl();
