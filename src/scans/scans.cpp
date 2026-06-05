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

bool PerformSignatureScans() {
    AddLog(L"[Tsukuyomi] Starting signature scans...");

    // 1. CameraUpdateのシグネチャスキャン
    // 実装理由：ゲーム内のカメラ位置更新処理を乗っ取るフックおよびNOPパッチを適用するためのアドレス特定。
    constexpr auto camsignature = hat::compile_signature<"48 83 EC 48 8B 01 4C 8D 44 24 20 89 42 40 8B 41 04 89 42 44 8B 41 08 89 42 48">();
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

    AddLog(L"[Tsukuyomi] All signatures scanned successfully.");
    return true;
}
