#pragma once
#include <windows.h>

// ホットキーモニターの初期化処理を行います。
// 実装理由：キーバインド待受状態への遷移時に、確実に前回の残存キー情報をクリアするため。
void InitializeHotkeyMonitor();

// ホットキーおよびキーバインド監視の毎フレームの更新処理を実行します。
// 実装理由：dllmainのMainThreadから呼び出され、待受中・ゲーム中・デバッグ時などの
// 各状態に応じたキー監視処理を適切に振り分けて実行するため。
void UpdateHotkeyMonitor();

// DLLを終了させるべきシグナル（例：ENDキーが押されたか）を検知して返します。
// 実装理由：モジュール終了判定のロジックをメインスレッドの外部にカプセル化するため。
bool ShouldExitModule();
