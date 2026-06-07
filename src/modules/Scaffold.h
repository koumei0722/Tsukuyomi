#pragma once

// Scaffold（足元への自動足場設置）モジュールの有効/無効状態を取得します。
// 実装理由：設定値の保持やメニュー上の表示色切り替えに使用するため。
bool GetScaffoldEnabled();

// Scaffold（足元への自動足場設置）モジュールの有効/無効状態を設定します。
// 実装理由：メニューからの操作や起動時の設定ロード時に状態を同期させるため。
void SetScaffoldEnabled(bool enabled);

// プレイヤーの座標更新フックから毎フレーム呼ばれ、CapsLockがONのときに足元に自動設置を行います。
// 実装理由：Scaffold/dllmain.cppと同等のタイミング（位置更新時）で設置判定とbuildBlockの実行をトリガーするため。
void UpdateScaffold(void* gameMode);
