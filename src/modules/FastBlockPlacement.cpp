#include "FastBlockPlacement.h"
#include "FreeCamera.h"
#include <windows.h>
#include <cstddef>
#include <chrono>
#include <cmath>

static bool g_enabled = false;
extern bool g_isPlacingBlock; // hooks.cppで定義されているグローバル再入ガードフラグ
static PlacementMode g_placementMode = MODE_Y_AXIS; // 手動軸固定モード管理用（デフォルトはY軸固定）
static int g_placeDistance = 5; // 設置距離設定用（1〜5ブロック）

// buildBlockの関数定義
typedef bool(__fastcall* BuildBlockHook_t)(void* rcx, void* rdx, unsigned char r8, unsigned char r9);
extern BuildBlockHook_t g_origBuildBlock; // hooks.cppで定義されているオリジナルトランポリン
extern std::byte* g_addrBuildBlock; // scans.cppでスキャン特定されたアドレス

bool GetFastBlockPlacementEnabled() {
    return g_enabled;
}

void SetFastBlockPlacementEnabled(bool enabled) {
    g_enabled = enabled;
}

PlacementMode GetFastPlacementMode() {
    return g_placementMode;
}

void SetFastPlacementMode(PlacementMode mode) {
    g_placementMode = mode;
}

int GetFastPlacementDistance() {
    return g_placeDistance;
}

void SetFastPlacementDistance(int distance) {
    // 設置可能距離は 1〜5ブロックの範囲に制限します。
    // 実装理由：負数や過度に大きい値を設定されるのを防ぐためのバリデーション。
    if (distance < 1) {
        g_placeDistance = 1;
    } else if (distance > 5) {
        g_placeDistance = 5;
    } else {
        g_placeDistance = distance;
    }
}

// 固定軸指定用列挙型
// 実装理由：手動設置時の面に応じて、X, Y, Z軸のどの平面を固定して設置するか管理するため。
enum Axis { AXIS_X, AXIS_Y, AXIS_Z };

// 状態管理用変数
static Axis g_savedAxis = AXIS_Y;
static int g_savedX = 0;
static int g_savedY = 0;
static int g_savedZ = 0;
static bool g_hasSavedY = false; // hooks.cppとの整合性維持のため変数名はそのまま「座標保存完了フラグ」として流用します。
static void* g_savedGameMode = nullptr;
static auto g_lastPlaceTime = std::chrono::steady_clock::now();
// 連続設置のディレイ（ミリ秒）。ユーザー要望により0固定とします。
static constexpr int g_placeDelayMs = 0;

// 右クリック離下検出のチャタリング対策（デバウンス）用変数
// 実装理由：GetAsyncKeyStateはスレッドの処理や呼び出し周波数によって一瞬 false を返すチャタリングが発生するため、
// 右クリックが離されたことを判定する際に遅延（バッファ時間）を設けて判定を安定化させるために定義します。
static bool g_rButtonReleased = true;
static std::chrono::steady_clock::time_point g_rButtonReleaseTime = std::chrono::steady_clock::now();

void OnBlockPlaced(void* gameMode, void* pos, unsigned char face) {
    if (!g_enabled || !gameMode || !pos || g_isPlacingBlock) return;

    // 右クリック（設置ボタン）が押されている間、かつすでに座標が保存されている場合は、更新をロックします。
    // 実装理由：右クリックを押し続けて連続で架橋（自動設置）している最中に、
    // カーソルがブレて別の面をカチカチ右クリックしたとしても、最初の固定平面がズレないように保護するため。
    if (g_hasSavedY && (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
        return;
    }

    BlockPos* p = reinterpret_cast<BlockPos*>(pos);

    // 設定されたモード（自動判定は廃止されました）に応じて固定する軸と座標を決定します。
    // 実装理由：手動で指定された軸(X, Y, Z)を強制ロックし、叩かれた面がその軸に対応する場合は面に基づき、
    // それ以外の面が叩かれた場合は叩かれたブロック座標そのものを基準座標として決定するため。
    if (g_placementMode == MODE_Y_AXIS) {
        g_savedAxis = AXIS_Y;
        int targetY = p->y;
        if (face == 0) {
            targetY = p->y - 1;
        } else if (face == 1) {
            targetY = p->y + 1;
        }
        g_savedY = targetY;
    }
    else if (g_placementMode == MODE_X_AXIS) {
        g_savedAxis = AXIS_X;
        int targetX = p->x;
        if (face == 4) {
            targetX = p->x - 1;
        } else if (face == 5) {
            targetX = p->x + 1;
        }
        g_savedX = targetX;
    }
    else if (g_placementMode == MODE_Z_AXIS) {
        g_savedAxis = AXIS_Z;
        int targetZ = p->z;
        if (face == 2) {
            targetZ = p->z - 1;
        } else if (face == 3) {
            targetZ = p->z + 1;
        }
        g_savedZ = targetZ;
    }

    g_hasSavedY = true;
    g_savedGameMode = gameMode;
}

void UpdateFastPlacement() {
    if (!g_enabled || !g_hasSavedY || !g_savedGameMode || !g_origBuildBlock || g_isPlacingBlock) return;

    // 右クリックのチャタリング・入力ブレ対策（デバウンス）
    // 実装理由：GetAsyncKeyStateはスレッド状態やポーリング頻度によって一瞬 false を返す入力のブレがあり、
    // 即時リセットしてしまうと手動の2個目の設置がガードをすり抜けてしまうため、
    // 右クリックが離されてから 200ms が経過するまでは Y高度の固定状態を維持します。
    if (!(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) {
        if (!g_rButtonReleased) {
            g_rButtonReleased = true;
            g_rButtonReleaseTime = std::chrono::steady_clock::now();
        } else {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_rButtonReleaseTime).count();
            if (elapsed >= 200) {
                g_hasSavedY = false;
                g_rButtonReleased = false;
                g_savedGameMode = nullptr; // GameMode ポインタをクリアしてダングリングを防ぎます
            }
        }
        return;
    } else {
        g_rButtonReleased = false;
    }

    PlayerView* playerView = GetPlayerViewPtr();
    if (!playerView) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastPlaceTime).count();

    // 設定されたディレイ（ミリ秒）が経過している場合にのみ自動設置を処理します。
    if (elapsed >= g_placeDelayMs) {
        // 再入ガードを設定して自動設置処理を実行します。
        // 実装理由：自動設置の処理（fnBuildBlock）の最中に発生する位置更新イベントから
        // 再帰的に別の自動設置が走り、ゲーム内部バッファやスタックが破壊されるのを防ぐため。
        g_isPlacingBlock = true;

        // 1. 視点角度（Pitch/Yaw）から方向ベクトル(dirX, dirY, dirZ)を算出
        const float PI = 3.14159265f;
        float yawRad = playerView->yaw * (PI / 180.0f);
        float pitchRad = playerView->pitch * (PI / 180.0f);

        float dirX = -std::sin(yawRad) * std::cos(pitchRad);
        float dirY = -std::sin(pitchRad); // Pitchが正なら下方向（-Y）
        float dirZ = std::cos(yawRad) * std::cos(pitchRad);

        // Yaw（首の左右角度）のみから求める、Pitchの影響を受けない水平方向の向きベクトル
        float horizX = -std::sin(yawRad);
        float horizZ = std::cos(yawRad);

        BuildBlockHook_t fnBuildBlock = g_origBuildBlock;
        bool anySuccess = false;

        // 2. 保存された固定軸（g_savedAxis）に応じて設置処理を分岐
        if (g_savedAxis == AXIS_Y) {
            // 水平主軸の決定
            bool isXAxis = std::abs(horizX) > std::abs(horizZ);

            if (isXAxis) {
                int startX = static_cast<int>(std::floor(playerView->x));
                int startZ = static_cast<int>(std::floor(playerView->z));
                int stepX = (horizX > 0) ? 1 : -1;
                unsigned char face = (horizX > 0) ? 4 : 5; // 東なら西(4)、西なら東(5)（反転）

                // 指定された距離（g_placeDistance）の数だけ直線上に設置します。
                // 実装理由：視線角度や交点判定による制限を排除し、設定されたブロック数分確実に設置するため。
                for (int i = 1; i <= g_placeDistance; ++i) {
                    int x = startX + stepX * i;
                    BlockPos basePos = { x, g_savedY, startZ };

                    bool success = fnBuildBlock(g_savedGameMode, &basePos, face, 0);
                    if (success) {
                        anySuccess = true;
                    }
                }
            } else {
                int startX = static_cast<int>(std::floor(playerView->x));
                int startZ = static_cast<int>(std::floor(playerView->z));
                int stepZ = (horizZ > 0) ? 1 : -1;
                unsigned char face = (horizZ > 0) ? 2 : 3; // 南なら北(2)、北なら南(3)（反転）

                // 指定された距離（g_placeDistance）の数だけ直線上に設置します。
                // 実装理由：視線角度や交点判定による制限を排除し、設定されたブロック数分確実に設置するため。
                for (int i = 1; i <= g_placeDistance; ++i) {
                    int z = startZ + stepZ * i;
                    BlockPos basePos = { startX, g_savedY, z };

                    bool success = fnBuildBlock(g_savedGameMode, &basePos, face, 0);
                    if (success) {
                        anySuccess = true;
                    }
                }
            }
        }
        else if (g_savedAxis == AXIS_X) {
            // YZ平面上の主軸の決定
            bool isYAxis = std::abs(dirY) > std::abs(dirZ);

            if (isYAxis) {
                int startY = static_cast<int>(std::floor(playerView->y));
                int startZ = static_cast<int>(std::floor(playerView->z));
                int stepY = (dirY > 0) ? 1 : -1;
                unsigned char face = (dirY > 0) ? 0 : 1; // 上なら下(0)、下なら上(1)を指定（反転）

                // 指定された距離（g_placeDistance）の数だけ直線上に設置します。
                // 実装理由：視線角度や交点判定による制限を排除し、設定されたブロック数分確実に設置するため。
                for (int i = 1; i <= g_placeDistance; ++i) {
                    int y = startY + stepY * i;
                    BlockPos basePos = { g_savedX, y, startZ };

                    bool success = fnBuildBlock(g_savedGameMode, &basePos, face, 0);
                    if (success) {
                        anySuccess = true;
                    }
                }
            } else {
                int startY = static_cast<int>(std::floor(playerView->y));
                int startZ = static_cast<int>(std::floor(playerView->z));
                int stepZ = (horizZ > 0) ? 1 : -1;
                unsigned char face = (horizZ > 0) ? 2 : 3; // 南なら北(2)、北なら南(3)を指定（反転）

                // 指定された距離（g_placeDistance）の数だけ直線上に設置します。
                // 実装理由：視線角度や交点判定による制限を排除し、設定されたブロック数分確実に設置するため。
                for (int i = 1; i <= g_placeDistance; ++i) {
                    int z = startZ + stepZ * i;
                    BlockPos basePos = { g_savedX, startY, z };

                    bool success = fnBuildBlock(g_savedGameMode, &basePos, face, 0);
                    if (success) {
                        anySuccess = true;
                    }
                }
            }
        }
        else if (g_savedAxis == AXIS_Z) {
            // XY平面上の主軸の決定
            bool isXAxis = std::abs(dirX) > std::abs(dirY);

            if (isXAxis) {
                int startX = static_cast<int>(std::floor(playerView->x));
                int startY = static_cast<int>(std::floor(playerView->y));
                int stepX = (horizX > 0) ? 1 : -1;
                unsigned char face = (horizX > 0) ? 4 : 5; // 東なら西(4)、西なら東(5)（反転）

                // 指定された距離（g_placeDistance）の数だけ直線上に設置します。
                // 実装理由：視線角度や交点判定による制限を排除し、設定されたブロック数分確実に設置するため。
                for (int i = 1; i <= g_placeDistance; ++i) {
                    int x = startX + stepX * i;
                    BlockPos basePos = { x, startY, g_savedZ };

                    bool success = fnBuildBlock(g_savedGameMode, &basePos, face, 0);
                    if (success) {
                        anySuccess = true;
                    }
                }
            } else {
                int startX = static_cast<int>(std::floor(playerView->x));
                int startY = static_cast<int>(std::floor(playerView->y));
                int stepY = (dirY > 0) ? 1 : -1;
                unsigned char face = (dirY > 0) ? 0 : 1; // 上なら下(0)、下なら上(1)（反転）

                // 指定された距離（g_placeDistance）の数だけ直線上に設置します。
                // 実装理由：視線角度や交点判定による制限を排除し、設定されたブロック数分確実に設置するため。
                for (int i = 1; i <= g_placeDistance; ++i) {
                    int y = startY + stepY * i;
                    BlockPos basePos = { startX, y, g_savedZ };

                    bool success = fnBuildBlock(g_savedGameMode, &basePos, face, 0);
                    if (success) {
                        anySuccess = true;
                    }
                }
            }
        }

        g_isPlacingBlock = false; // 処理完了後にガードを解除します

        if (anySuccess) {
            g_lastPlaceTime = now;
        }
    }
}

bool IsFastPlacementActive() {
    // 実装理由：GetAsyncKeyStateは一瞬 false を返すブレがあるため直接のキー判定は行わず、
    // デバウンス処理によって適切に状態管理されている g_hasSavedY のみを用いてアクティブ判定を行います。
    return g_enabled && g_hasSavedY;
}

bool IsFastPlacementRunning() {
    return g_isPlacingBlock;
}


