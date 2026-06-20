#include "hooks.h"
#include <windows.h>
#include "scans/scans.h"
#include "console/console.h"
#include "modules/FreeCamera.h"
#include "modules/FastBlockPlacement.h"
#include "modules/AutoTool.h"
#include "utils/window_utils.h"
#include <MinHook.h>
#include <cmath>

// 元の関数のポインタ定義。
// フックハンドラ内からオリジナルのゲーム処理を呼び出すために使用します。
typedef void(__fastcall* CameraUpdate_t)(void* rcx, void* rdx, void* r8);
static CameraUpdate_t OrigCameraUpdate = nullptr;

typedef void(__fastcall* YawUpdate_t)(void* rcx, void* rdx, void* r8, void* r9);
static YawUpdate_t OrigYawUpdate = nullptr;

typedef void(__fastcall* PacketSend_t)(void* _this, void* packet);
static PacketSend_t OrigPacketSend = nullptr;

// buildBlock フックのオリジナルポインタ
// 実装理由：他モジュール（FastBlockPlacementなど）からフックをバイパスしてオリジナルのブロック設置処理を呼び出せるようにグローバル公開します。
typedef bool(__fastcall* BuildBlockHook_t)(void* rcx, void* rdx, unsigned char r8, unsigned char r9);
BuildBlockHook_t g_origBuildBlock = nullptr;

// プレイヤー位置更新フックのオリジナルポインタ
// 実装理由：Microsoft x64 呼出規約により、フック元が4つの引数を渡すため、シグネチャを正確に合わせてレジスタの破損を防ぎます。
typedef void(__fastcall* PlayerPositionUpdate_t)(void* rcx, void* rdx, void* r8, void* r9);
static PlayerPositionUpdate_t OrigPlayerPositionUpdate = nullptr;

// 採掘速度計算関数のオリジナルポインタ定義。
// 実装理由：Microsoft x64 呼出規約により、フック元が4つの引数を渡すため、シグネチャを正確に合わせてレジスタの破損を防ぎ、元の採掘処理を呼び出します。
typedef float(__fastcall* GetDestroySpeed_t)(void* rcx, void* rdx, void* r8, void* r9);
static GetDestroySpeed_t OrigGetDestroySpeed = nullptr;

// 選択されているスロットインデックス(0~8)を保持するグローバル変数。
// 実装理由：AutoTool機能のON/OFFに関わらず常に最新の選択スロットを追跡し、自動切り替え時の基準とするため。
static int g_currentSlot = 0;

// ホットバー選択スロット操作関数のオリジナルポインタ定義。
// 実装理由：スロット選択関数をフックしつつ、ゲーム本来のスロット選択処理を正常に継続させるため。
typedef void(__fastcall* SetSelectedSlot_t)(void* rcx, void* rdx, void* r8, void* r9);
static SetSelectedSlot_t OrigSetSelectedSlot = nullptr;



// スロットインデックスを保持するメモリアドレス（直近2種類）を保存する配列。
// 実装理由：スロットを保存しているアドレスは2種類存在し、リログ等のタイミングでアドレスが変動するため、直近2つの一意なポインタを保持して同期的に両方書き換えるため。
static void* g_slotObjects[2] = { nullptr, nullptr };

// 採掘状態および元の選択スロットを保持するグローバル変数。
// 実装理由：採掘終了時に自動切り替えされたスロットを元のスロットに正しく復元するため。
static bool g_isMining = false;
static int g_originalSlot = -1; // -1: 未退避状態
static void RestoreOriginalSlot();
static ULONGLONG g_lastDestroySpeedTick = 0; // 最後に採掘速度計算関数が呼ばれた時刻。実装理由：高速連打時の入力取りこぼし対策としてのタイムアウト復元用。
static ULONGLONG g_ignoreSlotSaveUntil = 0; // スロットの保存を無視する期限時刻（ミリ秒）。実装理由：採掘終了時の復元後、非同期に遅れて届く最速スロット変更イベントにより g_currentSlot が汚染されるのを防ぐため。
static bool g_isChangingSlot = false; // 最速スロットの計算・変更中、または元のスロットへの復元中であることを示すフラグ。実装理由：変更中に発生する SetSelectedSlot の呼び出しで g_currentSlot が上書きされるのを完全に防ぐため。

// プレイヤーの現在座標を保持するグローバル変数
// 実装理由：座標更新フックで常に最新値に保ち、高速設置時の原点座標として使用するため。
int g_playerBlockX = 0;
int g_playerBlockY = 0;
int g_playerBlockZ = 0;

// プレイヤー移動に関連する`パケットIDの列挙
enum class PacketID : int {
    MovePlayer      = 0x13,
    PlayerAuthInput = 0x90,
};

// パケットオブジェクトからIDを取得するヘルパー関数
// 実装理由：C++の仮想関数テーブル（vtable）の構造に依存して、安全にパケットIDを取り出すため。
static int getPacketId(void* packet) {
    auto vtable = *reinterpret_cast<void***>(packet);
    auto getId = reinterpret_cast<int(__fastcall*)(void*)>(vtable[1]);
    return getId(packet);
}

// ---------------------------------------------------------
// フックハンドラ
// ---------------------------------------------------------

// カメラ座標の更新処理のフック
// 実装理由：フリーカメラ有効時のみ、ゲーム内カメラの座標更新処理を割り込み、独自の自由移動座標を反映するため。
// また、AutoTool有効時、左クリックが離されたこと（採掘終了）を毎フレーム検知して、即座に元のスロットへ復元するため。
static void __fastcall hk_CameraUpdate(void* rcx, void* rdx, void* r8) {
    // 1. オリジナルのカメラ更新処理を実行します。
    // 実装理由：視点の向き（Pitch/Yawなど）の更新処理を含む、座標以外のすべてのカメラ制御を正常に動作させるため。
    OrigCameraUpdate(rcx, rdx, r8);

    // 2. FreeCameraが有効な場合、オリジナルの処理によって上書きされた座標を独自の座標で即座に書き戻します。
    // ※X座標はフック競合回避のためNOP化していませんが、呼び出し直後に書き戻すことで描画のチラつきを防ぎます。
    // 実装理由：フック破損によるクラッシュを防ぎつつ、視点変更とフリーカメラ座標の維持を安全に両立させるため。
    if (GetFreeCameraEnabled()) {
        UpdateFreeCameraPosition(rcx);
    }

    if (GetAutoToolEnabled() && g_originalSlot != -1) {
        ULONGLONG now = GetTickCount64();
        // 左クリックが離された場合、または最後の採掘速度計算から250ms以上経過した場合に復元します。
        // 実装理由：高速連打時に入力解放（VK_LBUTTON）を取りこぼした場合でも、一定時間採掘計算が走らなければ
        // 確実に元のスロットへ復元させて不整合を防ぐため。
        bool isClickReleased = !IsGameFocused() || !(GetAsyncKeyState(VK_LBUTTON) & 0x8000);
        bool isTimeout = (g_lastDestroySpeedTick != 0 && (now - g_lastDestroySpeedTick >= 250));

        if (isClickReleased || isTimeout) {
            g_isMining = false;
            g_lastDestroySpeedTick = 0; // タイマーリセット
            RestoreOriginalSlot();
        }
    }
}

// プレイヤーの視線（Yaw）更新処理のフック
// 実装理由：カメラの進行方向の算出に必要なプレイヤーの水平視線角度（Yaw）のメモリアドレスを特定・取得するため。
static void __fastcall hk_YawUpdate(void* rcx, void* rdx, void* r8, void* r9) {
    OrigYawUpdate(rcx, rdx, r8, r9);
    // rdxの指す領域にプレイヤーの視点情報（座標、Pitch、Yaw）が存在するため、
    // ポインタをPlayerView型にキャストしてモジュールへ登録します。
    // 実装理由：FreeCameraやFastBlockPlacementの計算で、視線情報と始点座標を同期的に参照するため。
    SetPlayerViewPtr(reinterpret_cast<PlayerView*>(rdx));
}

// パケット送信処理のフック
// 実装理由：フリーカメラが有効な間、プレイヤーの移動情報をサーバーに送信するのを防ぎ、
// サーバー側との同期ずれ（キャラクターが突然ワープするなどの不自然な同期）を抑止するため。
static void __fastcall hk_PacketSend(void* _this, void* packet) {
    if (GetFreeCameraEnabled()) {
        int id = getPacketId(packet);
        if (id == (int)PacketID::PlayerAuthInput || id == (int)PacketID::MovePlayer) {
            return; // 該当パケットの送信をスキップ（遮断）
        }
    }
    OrigPacketSend(_this, packet);
}

static void* g_lastGameMode = nullptr;
static ULONGLONG g_lastPositionTick = 0; // 最終座標更新時刻
bool g_isPlacingBlock = false; // グローバル再入ガードフラグ

// buildBlock のフックハンドラ
// 実装理由：設置時のGamemodeポインタを動的に取得・保存し、FastBlockPlacement有効時には
// 周囲への高速一括設置要求をキュー（遅延処理）に登録するため。
static bool __fastcall hk_BuildBlock(void* rcx, void* rdx, unsigned char r8, unsigned char r9) {
    g_lastGameMode = rcx; // Gamemodeポインタを保存します

    // 呼び出し時点での自動設置アクティブ状態を判定して保存します。
    // 実装理由：OnBlockPlaced で g_hasSavedY が true に更新されるため、
    // 手動での最初のブロック設置（g_hasSavedY が false から始まる回）が
    // 誤って自動設置の無効化ガード（(0,0,0)への書き換え）に巻き込まれないように保護するため。
    bool wasActive = IsFastPlacementActive();

    if (GetFastBlockPlacementEnabled()) {
        // 最初のブロック設置イベント時に、実際設置Y座標を特定・保存します。
        OnBlockPlaced(rcx, rdx, r8);
    }

    // 自動設置中（右クリック長押しでY高度固定設置中）である場合、
    // ゲーム本来の手動設置処理を無効化するため、オリジナル処理を呼び出さずに早期リターンします。
    // 実装理由：手動による重複設置やパケット競合を無効化し、自動設置の整合性を保つため。
    // プログラムによる自動設置は g_origBuildBlock をバイパスして直接呼び出すため、ここを通過しません。
    // したがって、IsFastPlacementRunning() による呼び出し元チェックは不要となり、wasActive のみで安全にガードできます。
    if (wasActive) {
        return false;
    }

    // 再入ガードを設定してオリジナル関数を呼び出します。
    // 実装理由：手動設置処理の実行中に、ゲーム内の位置同期によって再帰的に自動設置が
    // 呼び出されてクラッシュする（関数の再入バグ）のを防ぐため。
    g_isPlacingBlock = true;
    bool result = g_origBuildBlock(rcx, rdx, r8, r9);
    g_isPlacingBlock = false;
    return result;
}

// プレイヤー座標更新関数のフックハンドラ
// 実装理由：ゲーム内のプレイヤー現在座標(rdxが指すメモリ)からX, Y, Zの整数座標を読み出し、
// グローバル変数にリアルタイム保存しておくため。シグネチャを完全に合致させてレジスタR8, R9の破壊を防ぎます。
static void __fastcall hk_PlayerPositionUpdate(void* rcx, void* rdx, void* r8, void* r9) {
    // プレイヤーの座標取得（位置同期フック）が1秒以上動作しなかった場合のみ、保存されているGameModeポインタをクリアします。
    // 実装理由：ワールド退出時やローディング時にのみ安全にポインタをリセットし、ダングリングを防止するため。
    const ULONGLONG now = GetTickCount64();
    if (g_lastPositionTick != 0 && now - g_lastPositionTick >= 1000) {
        g_lastGameMode = nullptr;
    }
    g_lastPositionTick = now;

    if (rdx) {
        float* pPos = reinterpret_cast<float*>(rdx);
        g_playerBlockX = static_cast<int>(std::floor(pPos[0]));
        g_playerBlockY = static_cast<int>(std::floor(pPos[1]));
        g_playerBlockZ = static_cast<int>(std::floor(pPos[2]));

        // オリジナルの位置更新処理を実行する前に、自動設置のトリガーを実行します。
        // 実装理由：OrigPlayerPositionUpdateを実行するとそのフレームのTickが進み、手動設置で利用された
        // コンテキストポインタが破棄されるため、まだ有効な状態（前処理）のポインタを用いて安全に設置処理を呼び出すため。
        if (!g_isPlacingBlock) {
            // 安全なシミュレーションTickのタイミングでFastBlockPlacementの遅延自動設置を更新します。
            UpdateFastPlacement();
        }
    }

    // 最後にオリジナルの位置更新を呼び出します。
    OrigPlayerPositionUpdate(rcx, rdx, r8, r9);
}

// 元のスロットに復元するヘルパー関数
// 実装理由：採掘終了時に自動切り替えされたスロットを元のスロットに正しく復元するため。
static void RestoreOriginalSlot() {
    if (g_originalSlot != -1) {
        int slotToRestore = g_originalSlot;
        // 先にリセットして、SetSelectedSlot 内での誤書き換えを防ぐ
        g_originalSlot = -1; 

        // 復元したスロットインデックスを g_currentSlot に即座に設定します。
        // 実装理由：復元した結果が即座に g_currentSlot に反映されるようにし、
        // かつ遅れて届く古い最速スロット変更イベントを無視するための基準値とするため。
        g_currentSlot = slotToRestore;

        // 復元後、非同期に遅れて届く可能性のある古いスロット変更パケットや同期イベントから
        // g_currentSlot を守るために、250ミリ秒間の書き換え禁止クールダウンを設けます。
        g_ignoreSlotSaveUntil = GetTickCount64() + 250;

        // 変更中フラグを立てて、SetSelectedSlot をガードします。
        g_isChangingSlot = true;

        for (int i = 0; i < 2; ++i) {
            if (g_slotObjects[i]) {
                *reinterpret_cast<int*>(reinterpret_cast<char*>(g_slotObjects[i]) + 0x10) = slotToRestore;
            }
        }

        g_isChangingSlot = false;
    }
}

// 採掘速度計算関数のフックハンドラ。
// 実装理由：AutoTool有効時、全ホットバースロット（0〜8）の採掘速度をOrigGetDestroySpeedにより算出し、
// 最も速いスロット（同速時は元のスロット優先、次いでインデックス小優先）を選択中のスロットとして切り替え、ブロックを破壊するため。
static float __fastcall hk_GetDestroySpeed(void* rcx, void* rdx, void* r8, void* r9) {
    if (!GetAutoToolEnabled()) {
        return OrigGetDestroySpeed(rcx, rdx, r8, r9);
    }

    bool isRdxValid = (reinterpret_cast<uintptr_t>(rdx) == 0xB || reinterpret_cast<uintptr_t>(rdx) == 0x1);

    if (isRdxValid && rcx) {
        // 採掘速度計算関数が呼ばれたため、タイムスタンプを更新して採掘中フラグを立てます。
        // 実装理由：高速連打時に入力解放（VK_LBUTTON）を取りこぼした場合のタイムアウト復元用。
        g_lastDestroySpeedTick = GetTickCount64();
        g_isMining = true;

        // リログ等でアドレスが変動するため、g_slotObjectsから直近のアクティブなスロットインデックスを取得して offset を求めます。
        int activeSlot = g_currentSlot;
        if (g_slotObjects[0]) {
            activeSlot = *reinterpret_cast<int*>(reinterpret_cast<char*>(g_slotObjects[0]) + 0x10);
        }

        // 元の選択スロット（退避済みならそれ、未退避なら現在のゲーム内スロット）を特定します。
        int origSlot = (g_originalSlot != -1) ? g_originalSlot : activeSlot;

        if (origSlot >= 0 && origSlot <= 8) {
            uintptr_t* pVal = reinterpret_cast<uintptr_t*>(reinterpret_cast<char*>(rcx) + 0x10);
            uintptr_t originalVal = *pVal;

            if (activeSlot >= 0 && activeSlot <= 8) {
                // スロット切り替え計算・変更中フラグを立てて、SetSelectedSlot をガードします。
                g_isChangingSlot = true;

                uintptr_t offset = originalVal - 0x98 * activeSlot;

                float speeds[9];
                // 2.全スロット(0~8)のMining speedを計算
                for (int slot = 0; slot < 9; ++slot) {
                    *pVal = offset + 0x98 * slot;
                    speeds[slot] = OrigGetDestroySpeed(rcx, rdx, r8, r9);
                }

                int bestSlot = origSlot;
                float bestSpeed = speeds[origSlot];

                // 優先順位（1. 元の選択スロット、2. スロットの番号が小さいスロット）に基づき、最速スロットを算出。
                for (int slot = 0; slot < 9; ++slot) {
                    if (slot == origSlot) continue;

                    float speed = speeds[slot];
                    bool isBetter = false;
                    if (speed > bestSpeed) {
                        isBetter = true;
                    } else if (speed == bestSpeed) {
                        if (bestSlot == origSlot) {
                            isBetter = false;
                        } else {
                            if (slot == origSlot) {
                                isBetter = true;
                            } else {
                                if (slot < bestSlot) {
                                    isBetter = true;
                                }
                            }
                        }
                    }

                    if (isBetter) {
                        bestSpeed = speed;
                        bestSlot = slot;
                    }
                }

                // 最速スロットが本来の選択スロット（origSlot）と同じ場合：
                // 書き換える必要も、後から元のスロットに戻す必要もありません。
                // 実装理由：最適なツールが現在の手持ちと同じであれば、余計なスロットの変更イベントを発生させず、
                // かつ採掘終了時の復元処理（RestoreOriginalSlot）自体をスキップして不整合を防ぐため。
                if (bestSlot == origSlot) {
                    // 退避中であれば解除（戻す必要がないため）
                    g_originalSlot = -1;

                    // もし現在ゲーム内で選択されているスロット（activeSlot）が origSlot と異なるなら、origSlot に書き戻します。
                    // 実装理由：すでに前の採掘などでスロットが切り替わっていた場合のみ同期的に戻すため。
                    if (activeSlot != origSlot) {
                        for (int i = 0; i < 2; ++i) {
                            if (g_slotObjects[i]) {
                                *reinterpret_cast<int*>(reinterpret_cast<char*>(g_slotObjects[i]) + 0x10) = origSlot;
                            }
                        }
                    }

                    // ブロック破壊に使用するスロットアドレスを本来のスロットアドレスに設定します。
                    *pVal = offset + 0x98 * origSlot;
                }
                else {
                    // 最速スロットが本来の選択スロット（origSlot）と異なる場合：
                    // 最速スロットへ書き換える必要があり、後から元に戻す必要があります。

                    // 採掘が開始された（またはスロットが切り替わった）ため、前回の復元クールダウンを即座に終了します。
                    // 実装理由：クールダウンが残ったままだと、今回の採掘による正当なスロット切り替えイベントが
                    // クールダウン中の遅延パケットと誤認されて本来のスロットに書き戻されてしまうのを防ぐため。
                    g_ignoreSlotSaveUntil = 0;

                    // 初回の切り替え時（まだ退避していない場合）のみ、本来のスロットを退避します。
                    // 実装理由：採掘中の最適なツールへの自動切り替えに備えて本来のスロットを保存しておくため。
                    // 左クリックの物理キー状態（GetAsyncKeyState）を入れると、高速連打時に判定漏れが発生し、
                    // スロット退避（保存）が行われないまま切り替えのみが走るバグが起きるため、この判定は行いません。
                    if (g_originalSlot == -1) {
                        g_originalSlot = origSlot;
                    }

                    // もし現在ゲーム内で選択されているスロット（activeSlot）が最速スロット（bestSlot）と異なるなら書き換えます。
                    // 実装理由：すでに最速スロットが選択されている場合は書き込み処理をスキップして負荷を減らすため。
                    if (activeSlot != bestSlot) {
                        for (int i = 0; i < 2; ++i) {
                            if (g_slotObjects[i]) {
                                *reinterpret_cast<int*>(reinterpret_cast<char*>(g_slotObjects[i]) + 0x10) = bestSlot;
                            }
                        }
                    }

                    // ブロック破壊に使用するスロットのアドレスを最速スロットのアドレスに書き換えます。
                    *pVal = offset + 0x98 * bestSlot;
                }

                // ガードフラグを解除します
                g_isChangingSlot = false;

                return OrigGetDestroySpeed(rcx, rdx, r8, r9);
            }
        }
    }

    return OrigGetDestroySpeed(rcx, rdx, r8, r9);
}

// ホットバー選択スロット操作関数のフックハンドラ。
// 実装理由：AutoToolがOFFのときも、現在選択されているホットバースロット（0〜8）および rcx アドレス履歴を常に検知して保存するため。
static void __fastcall hk_SetSelectedSlot(void* rcx, void* rdx, void* r8, void* r9) {
    OrigSetSelectedSlot(rcx, rdx, r8, r9);
    if (rcx) {
        // 直近の2種類のrcxを保存します（アドレス重複を排除）。
        // 実装理由：スロットを保存しているアドレスは2種類存在し、リログ等でアドレスが変動するため、常に最新の2件を保持して同期させるため。
        if (g_slotObjects[0] != rcx) {
            g_slotObjects[1] = g_slotObjects[0];
            g_slotObjects[0] = rcx;
        }

        int slot = *reinterpret_cast<int*>(reinterpret_cast<char*>(rcx) + 0x10);
        if (slot >= 0 && slot <= 8) {
            // 自動切り替え(採掘中)や、復元直後の非同期パケット遅延時間中であれば、ユーザーのスロット情報を保護するため無視します。
            // 実装理由：採掘中の最速スロットへの切り替えや、復元直後に遅れて到着する古いスロット変更パケットによって
            // 手動選択スロット（g_currentSlot）が最速スロットなどの値で上書き（汚染）されるのを完全に防ぐため。
            ULONGLONG now = GetTickCount64();
            bool shouldIgnore = (g_originalSlot != -1) || g_isMining || (now < g_ignoreSlotSaveUntil);

            if (!shouldIgnore) {
                g_currentSlot = slot;
            } else {
                // クールダウン期間中に本来のスロットと異なるスロットへの変更イベントが発生した場合、
                // 遅延パケットによるメモリ汚染とみなして本来のスロットに書き戻します。
                // 実装理由：採掘終了の復元直後に遅れて到着する古い最速スロット変更イベントにより、
                // ゲーム内のメモリ値（rcx+0x10）が汚染されるのを防ぐため。
                bool isCooldown = (now < g_ignoreSlotSaveUntil);
                if (isCooldown && slot != g_currentSlot) {
                    *reinterpret_cast<int*>(reinterpret_cast<char*>(rcx) + 0x10) = g_currentSlot;
                }
            }
        }
    }
}



// ---------------------------------------------------------
// フック制御API
// ---------------------------------------------------------

bool InitializeHooks() {
    if (MH_Initialize() != MH_OK) {
        AddLog(L"[Error] Failed to initialize MinHook.");
        return false;
    }

    // 1. CameraUpdate フック作成
    if (MH_CreateHook(g_addrCameraUpdate, &hk_CameraUpdate, reinterpret_cast<void**>(&OrigCameraUpdate)) != MH_OK ||
        MH_EnableHook(g_addrCameraUpdate) != MH_OK) {
        AddLog(L"[Error] Failed to hook CameraUpdate.");
        return false;
    }

    // 2. YawUpdate フック作成
    if (MH_CreateHook(g_addrYawUpdate, &hk_YawUpdate, reinterpret_cast<void**>(&OrigYawUpdate)) != MH_OK ||
        MH_EnableHook(g_addrYawUpdate) != MH_OK) {
        AddLog(L"[Error] Failed to hook YawUpdate.");
        return false;
    }

    // 3. PacketSend フック作成
    if (MH_CreateHook(g_addrPacketSend, &hk_PacketSend, reinterpret_cast<void**>(&OrigPacketSend)) != MH_OK ||
        MH_EnableHook(g_addrPacketSend) != MH_OK) {
        AddLog(L"[Error] Failed to hook PacketSend.");
        return false;
    }

    // 4. buildBlock フック作成
    if (MH_CreateHook(g_addrBuildBlock, &hk_BuildBlock, reinterpret_cast<void**>(&g_origBuildBlock)) != MH_OK ||
        MH_EnableHook(g_addrBuildBlock) != MH_OK) {
        AddLog(L"[Error] Failed to hook buildBlock.");
        return false;
    }

    // 5. PlayerPositionUpdate フック作成
    if (MH_CreateHook(g_addrPlayerPositionUpdate, &hk_PlayerPositionUpdate, reinterpret_cast<void**>(&OrigPlayerPositionUpdate)) != MH_OK ||
        MH_EnableHook(g_addrPlayerPositionUpdate) != MH_OK) {
        AddLog(L"[Error] Failed to hook PlayerPositionUpdate.");
        return false;
    }

    // 6. GetDestroySpeed フック作成
    if (MH_CreateHook(g_addrGetDestroySpeed, &hk_GetDestroySpeed, reinterpret_cast<void**>(&OrigGetDestroySpeed)) != MH_OK ||
        MH_EnableHook(g_addrGetDestroySpeed) != MH_OK) {
        AddLog(L"[Error] Failed to hook GetDestroySpeed.");
        return false;
    }

    // 7. SetSelectedSlot フック作成
    if (MH_CreateHook(g_addrSetSelectedSlot, &hk_SetSelectedSlot, reinterpret_cast<void**>(&OrigSetSelectedSlot)) != MH_OK ||
        MH_EnableHook(g_addrSetSelectedSlot) != MH_OK) {
        AddLog(L"[Error] Failed to hook SetSelectedSlot.");
        return false;
    }



    AddLog(L"[Tsukuyomi] All hooks initialized successfully.");
    return true;
}

void CleanupHooks() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
