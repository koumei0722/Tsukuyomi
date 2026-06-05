#pragma once

// AntiDarknessモジュールの有効化/無効化状態を取得します。
// 実装理由：設定の保存処理やメニュー画面の色表示の切り替えに用いるため。
bool GetAntiDarknessEnabled();

// AntiDarknessの有効化/無効化を設定し、ゲーム内のメモリにパッチを適用します。
// 実装理由：ゲーム起動時やコンソールからの操作イベントを受けて、即時に対象アドレスのコード（バイト値）を書き換えるため。
void SetAntiDarknessEnabled(bool enabled);
