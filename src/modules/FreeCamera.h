#pragma once

// プレイヤーの視点情報（座標、Pitch、Yaw）を保持する構造体
// 実装理由：hooks.cppでフックされた視線更新処理から得られる最新データをモジュール間で共有するため。
struct PlayerView {
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
};

// Freecam の有効/無効状態を取得・設定します。
// 実装理由：設定値やトグル操作に合わせてフックモジュールやUIからカメラ状態を制御するため。
bool GetFreeCameraEnabled();
void SetFreeCameraEnabled(bool enabled);

// プレイヤーの視点情報（PlayerView）へのポインタを登録します。
void SetPlayerViewPtr(PlayerView* viewPtr);

// カメラ座標の更新処理を行います。
// 引数: cameraBase - ゲーム内カメラオブジェクトのベースアドレス
void UpdateFreeCameraPosition(void* cameraBase);

// Freecam の移動速度を取得・設定します。
float GetFreeCameraSpeed();
void SetFreeCameraSpeed(float speed);

// プレイヤーの視点情報（PlayerView）へのポインタを取得します。
// 実装理由：FastBlockPlacementなどの別モジュールからプレイヤーの現在位置や視線方向を特定するために使用するため。
PlayerView* GetPlayerViewPtr();
