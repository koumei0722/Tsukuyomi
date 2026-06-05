#pragma once
#include <windows.h>
#include <cstddef>

// シグネチャスキャンで特定されたゲーム内関数のアドレスを保持するグローバル変数。
// 実装理由：スキャンモジュール以外のフックモジュールや個別機能（Freecam）からアドレスを直接参照できるようにするため。
extern std::byte* g_addrCameraUpdate;
extern std::byte* g_addrYawUpdate;
extern std::byte* g_addrPacketSend;
extern std::byte* g_addrCreativeNoClip; // creativeNoClipのアドレス。実装理由：creativeNoClipモジュールからアドレスを直接参照できるようにするため。
extern std::byte* g_addrAntiDarkness; // AntiDarknessのアドレス。実装理由：AntiDarknessモジュールからメモリ書き換え対象のアドレスを直接参照できるようにするため。

// シグネチャスキャンを実行します。
// 実装理由：起動時に必要な関数のメモリアドレスをLibHatシグネチャスキャンにより特定するため。
bool PerformSignatureScans();
