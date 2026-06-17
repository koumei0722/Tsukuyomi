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
extern std::byte* g_addrBuildBlock; // buildBlockのアドレス。実装理由：FastBlockPlacementモジュールからbuildBlockを呼び出すため。
extern std::byte* g_addrPlayerPositionUpdate; // プレイヤー座標更新のアドレス。実装理由：プレイヤーの現在座標を取得するため。
extern std::byte* g_addrGetDestroySpeed; // 採掘速度計算関数のアドレス。実装理由：AutoToolモジュールで採掘時間をフックしログ出力するため。
extern std::byte* g_addrSetSelectedSlot; // ホットバー選択スロット操作関数のアドレス。実装理由：AutoToolモジュール等で現在選択されているスロット(0~8)を追跡するため。

// シグネチャスキャンを実行します。
// 実装理由：起動時に必要な関数のメモリアドレスをLibHatシグネチャスキャンにより特定するため。
bool PerformSignatureScans();

