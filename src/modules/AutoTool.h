#pragma once

// AutoToolモジュールの有効化/無効化状態を取得します。
// 実装理由：設定の保存処理やメニュー画面の色表示の切り替えに用いるため。
bool GetAutoToolEnabled();

// AutoToolの有効化/無効化を設定します。
// 実装理由：コンソールからの操作イベントを受けて、即座に状態（有効/無効）を切り替えるため。
void SetAutoToolEnabled(bool enabled);
