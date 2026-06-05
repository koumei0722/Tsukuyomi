#pragma once
#include <windows.h>

// ゲームがフォーカスされているか（コンソールウィンドウを除く）を判定します。
// 実装理由：ゲーム外部（Alt+Tab時など）やコンソール画面の操作中に意図せずキー入力を処理してしまうのを防ぐため。
bool IsGameFocused();
