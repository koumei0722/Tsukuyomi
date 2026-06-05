#pragma once

// Freecam の有効/無効状態を取得・設定します。
// 実装理由：設定値やトグル操作に合わせてフックモジュールやUIからカメラ状態を制御するため。
bool GetFreeCameraEnabled();
void SetFreeCameraEnabled(bool enabled);

// プレイヤーの視線（Yaw）へのポインタを登録します。
void SetFreeCameraYawPtr(float* yawPtr);

// カメラ座標の更新処理を行います。
// 引数: cameraBase - ゲーム内カメラオブジェクトのベースアドレス
void UpdateFreeCameraPosition(void* cameraBase);

// Freecam の移動速度を取得・設定します。
float GetFreeCameraSpeed();
void SetFreeCameraSpeed(float speed);
