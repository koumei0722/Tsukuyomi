#include "scans.h"
#include <libhat/scanner.hpp>
#include <libhat/signature.hpp>
#include "console/console.h"
#include <cstddef>

std::byte* g_addrCameraUpdate = nullptr;
std::byte* g_addrYawUpdate = nullptr;
std::byte* g_addrPacketSend = nullptr;
std::byte* g_addrCreativeNoClip = nullptr; // creativeNoClipのアドレス
std::byte* g_addrAntiDarkness = nullptr; // AntiDarknessのアドレス
std::byte* g_addrBuildBlock = nullptr; // buildBlockのアドレス
std::byte* g_addrPlayerPositionUpdate = nullptr; // プレイヤー座標更新のアドレス
std::byte* g_addrGetDestroySpeed = nullptr; // 採掘速度計算関数のアドレス。実装理由：AutoToolモジュールで採掘時間をフックしログ出力するため。
std::byte* g_addrSetSelectedSlot = nullptr; // ホットバー選択スロット操作関数のアドレス。実装理由：AutoToolモジュール等で現在選択されているスロット(0~8)を追跡するため。

bool PerformSignatureScans() {
    AddLog(L"[Tsukuyomi] Starting signature scans...");

    // 1. CameraUpdateのシグネチャスキャン
    // 実装理由：カメラの座標を書き換える命令（mov [rcx+...], eax）であり、同時にこの関数の先頭（プロローグのないリーフ関数）であるため、このアドレスをフック先として直接特定します。
    constexpr auto camsignature = hat::compile_signature<"8B 42 40 89 41 40 8B 42 44 89 41 44 8B 42 48 89 41 48 8B 42 3C 89 41 3C">();
    hat::scan_result CamScanResult = hat::find_pattern(camsignature, ".text");
    g_addrCameraUpdate = reinterpret_cast<std::byte*>(CamScanResult.get());
    if (!g_addrCameraUpdate) {
        AddLog(L"[Error] Failed to find CameraUpdate signature.");
        return false;
    }

    // 2. YawUpdateのシグネチャスキャン
    // 実装理由：カメラ移動方向の計算に必要なプレイヤーの視線（Yaw）へのポインタを取得するためのフック先特定。
    constexpr auto yawsignature = hat::compile_signature<"40 53 48 81 EC 00 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 F0 00 00 00 F3 0F 10 52 10">();
    hat::scan_result YawScanResult = hat::find_pattern(yawsignature, ".text");
    g_addrYawUpdate = reinterpret_cast<std::byte*>(YawScanResult.get());
    if (!g_addrYawUpdate) {
        AddLog(L"[Error] Failed to find YawUpdate signature.");
        return false;
    }

    // 3. PacketSendのシグネチャスキャン
    // 実装理由：フリーカメラ中に自キャラクターの移動パケットの送信を一時的にブロックし、サーバー側との位置ずれ同期を防ぐため。
    constexpr auto packetSendSig = hat::compile_signature<"48 83 EC ? 48 0F ? ? ? 48 83 C0 ? 74 ? 48 83 F8 ? 48 8B">();
    hat::scan_result PacketSendResult = hat::find_pattern(packetSendSig, ".text");
    g_addrPacketSend = reinterpret_cast<std::byte*>(PacketSendResult.get());
    if (!g_addrPacketSend) {
        AddLog(L"[Error] Failed to find PacketSend signature.");
        return false;
    }

    // 4. creativeNoClipのシグネチャスキャン
    // 実装理由：プレイヤーのNoClip（壁抜け）挙動を制御するフラグのアドレスを特定し、パッチを適用するため。
    constexpr auto creativeNoClipSig = hat::compile_signature<"80 BB C0 01 00 00 01 75 0E C6 83 C0 01 00 00 02 C6 83 C4 01 00 00 00 C6 83 C4 01 00 00 00 41 C7 43 18 00 00 00 00 48 8B 4D 08 BA ?? ?? ?? ?? 8B 5D 10 E8 ?? ?? ?? ?? 4C 8B D0">();
    hat::scan_result CreativeNoClipResult = hat::find_pattern(creativeNoClipSig, ".text");
    std::byte* creativeNoClipBase = reinterpret_cast<std::byte*>(CreativeNoClipResult.get());
    if (!creativeNoClipBase) {
        AddLog(L"[Error] Failed to find creativeNoClip signature.");
        return false;
    }
    g_addrCreativeNoClip = creativeNoClipBase + 0x1D;

    // 5. AntiDarknessのシグネチャスキャン
    // 実装理由：暗闇エフェクト処理が行われるコード領域の先頭アドレスを特定し、パッチを適用するため。
    constexpr auto antidarknessSig = hat::compile_signature<"8B 02 83 F8 25 77 15 48 8B 14 C3 48 85 D2 74 0C 48 81 C2 80 01 00 00 E8 ? ? ? ? C6 86 81 00 00 00 01 8B 05 ? ? ? ? 39 06 74 35 48 8D 4F 08 E8 ? ? ? ? 84 C0 74 28 8B 06 83 F8 26 73 21">();
    hat::scan_result AntiDarknessResult = hat::find_pattern(antidarknessSig, ".text");
    g_addrAntiDarkness = reinterpret_cast<std::byte*>(AntiDarknessResult.get());
    if (!g_addrAntiDarkness) {
        AddLog(L"[Error] Failed to find AntiDarkness signature.");
        return false;
    }

    // 6. buildBlockのシグネチャスキャン
    // 実装理由：ブロックの高速設置を呼び出すため。
    constexpr auto buildBlockSig = hat::compile_signature<"48 89 5C 24 10 48 89 74 24 18 55 57 41 54 41 56 41 57 48 8D 6C 24 90 48 81 EC 70 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 60 41 0F B6 F1 45 0F B6 F0 4C 8B FA 48 8B F9">();
    hat::scan_result BuildBlockResult = hat::find_pattern(buildBlockSig, ".text");
    g_addrBuildBlock = reinterpret_cast<std::byte*>(BuildBlockResult.get());
    if (!g_addrBuildBlock) {
        AddLog(L"[Error] Failed to find buildBlock signature.");
        return false;
    }

    // 7. PlayerPositionUpdateのシグネチャスキャン
    // 実装理由：プレイヤーの現在座標を取得するため。
    constexpr auto playerPosSig = hat::compile_signature<"F2 41 0F 10 00 33 C9 F2 0F 11 02 41 8B 40 08 89 42 08 F2 41 0F 10 40 0C F2 0F 11 42 0C 41 8B 40 14 89 42 14 41 8B 40 18 89 42 18">();
    hat::scan_result PlayerPosResult = hat::find_pattern(playerPosSig, ".text");
    g_addrPlayerPositionUpdate = reinterpret_cast<std::byte*>(PlayerPosResult.get());
    if (!g_addrPlayerPositionUpdate) {
        AddLog(L"[Error] Failed to find PlayerPositionUpdate signature.");
        return false;
    }
    // 8. GetDestroySpeedのシグネチャスキャン
    // 実装理由：ブロックの採掘時間/速度を計算する関数を特定し、フックして計算値をログ出力するため。
    constexpr auto getDestroySpeedSig = hat::compile_signature<"48 8B C4 48 89 58 10 48 89 70 18 48 89 78 20 55 41 54 41 55 41 56 41 57 48 8D 68 A1 48 81 EC ? ? ? ? 0F 29 70 C8 0F 29 78 B8 48 8B F9 33 F6 44 8B E6">();
    hat::scan_result GetDestroySpeedResult = hat::find_pattern(getDestroySpeedSig, ".text");
    g_addrGetDestroySpeed = reinterpret_cast<std::byte*>(GetDestroySpeedResult.get());
    if (!g_addrGetDestroySpeed) {
        AddLog(L"[Error] Failed to find GetDestroySpeed signature.");
        return false;
    }

    // 9. SetSelectedSlotのシグネチャスキャン
    // 実装理由：ホットバーの選択スロット操作関数を特定し、選択スロットインデックス(0~8)を追跡するため。
    constexpr auto setSelectedSlotSig = hat::compile_signature<"48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 80 FD FF FF 48 81 EC 80 03 00 00 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 70 02 00 00 44 8B F2">();
    hat::scan_result SetSelectedSlotResult = hat::find_pattern(setSelectedSlotSig, ".text");
    g_addrSetSelectedSlot = reinterpret_cast<std::byte*>(SetSelectedSlotResult.get());
    if (!g_addrSetSelectedSlot) {
        AddLog(L"[Error] Failed to find SetSelectedSlot signature.");
        return false;
    }


    AddLog(L"[Tsukuyomi] All signatures scanned successfully.");
    return true;
}
