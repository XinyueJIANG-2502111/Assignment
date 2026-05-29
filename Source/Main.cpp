#include "DxLib.h"
#include "Vector.hpp"
#include "Camera.hpp"
#include "Player.hpp"
#include "Water.hpp"
#include "Terrain.hpp"
#include <cmath>
#include <vector>

int playerScore = 0;                         // スコア
const float PLAYER_COLLISION_RADIUS = 16.0f; // プレイヤーの半径

// ==========================================
//  main function
// ==========================================
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    // DxLib の基本設定
    ChangeWindowMode(TRUE);          // ウィンドウモード
    SetGraphMode(800, 600, 32);      // 解像度（画面サイズ）
    SetWindowText("test");

    if (DxLib_Init() == -1) return -1;
    SetDrawScreen(DX_SCREEN_BACK);

    // ゲームに必要なオブジェクト
    Player player;
    Water water;
    Camera camera;
    TerrainManager terrain;

    // 初期化
    player.position = { 400.0f, 400.0f };
    terrain.Init();

    // DeltaTime
    LONGLONG prevTime = GetNowHiPerformanceCount();

    // main loop
    while (ProcessMessage() == 0 && ClearDrawScreen() == 0) {
        // DeltaTime
        LONGLONG currTime = GetNowHiPerformanceCount();
        float dt = static_cast<float>(currTime - prevTime) / 1000000.0f;
        prevTime = currTime;
        if (dt > 0.05f) dt = 0.05f;

        // ==========================================================================================================
        // プレイヤー入力
        // ==========================================================================================================
        int horizontalInput = 0;
        if (CheckHitKey(KEY_INPUT_A) || CheckHitKey(KEY_INPUT_LEFT))  horizontalInput = -1;
        if (CheckHitKey(KEY_INPUT_D) || CheckHitKey(KEY_INPUT_RIGHT)) horizontalInput = 1;

        int verticalInput = 0;
        if (CheckHitKey(KEY_INPUT_S) || CheckHitKey(KEY_INPUT_DOWN)) verticalInput = 1;

        // ジャンプ
        bool jumpInput = false;
        if (CheckHitKey(KEY_INPUT_SPACE)) {
            jumpInput = true;
            player.Jump();
        }

        bool reelInput = (CheckHitKey(KEY_INPUT_W) || CheckHitKey(KEY_INPUT_UP));

        // マウス位置を取得
        int mouseX, mouseY;
        GetMousePoint(&mouseX, &mouseY);

        // 座標変換
        Vector2 mouseWorldPos = {
            static_cast<float>(mouseX) + camera.position.x - 400.0f,
            static_cast<float>(mouseY) + camera.position.y - 300.0f
        };

        // 左クリック：ワイヤーを発射する
        if ((GetMouseInput() & MOUSE_INPUT_LEFT) != 0) {
            jumpInput = true;

            // なにもくっついていない場合
            if (player.grappleState == Player::GrappleState::None) {
                player.LaunchGrapple(mouseWorldPos, terrain);
            }
        }

        // 右クリック：
        if ((GetMouseInput() & MOUSE_INPUT_RIGHT) != 0) {
            player.ReleaseGrapple();
        }

        // ==========================================================================================================
        // ゲームオーバー
        // ==========================================================================================================
        //  Rを押してリセット
        if (player.isDead && CheckHitKey(KEY_INPUT_R)) {
            player.isDead = false;
            player.noInputWaterTimer = 0.f;
            player.buoyancyScale = 1.f;
            playerScore = 0;
            player.position = { 400.0f, 300.0f };
            player.velocity = { 0.0f, 0.0f };
            player.ReleaseGrapple();
            camera.Init();
            terrain.Init();
            terrain.UpdateAllDirections(camera.position);
        }

        // Escを押して終了
        if (player.isDead && CheckHitKey(KEY_INPUT_ESCAPE)) {
            break;
        }

        // マップを更新する
        terrain.UpdateAllDirections(camera.position);

        // ==========================================================================================================
        // アイテム獲得判定
        // ==========================================================================================================
        if (!player.isDead) {
            for (auto& core : terrain.cores) {
                if (core.isCollected) continue;

                // プレイヤーとアイテムの距離（中心点）
                float dx = player.position.x - core.x;
                float dy = player.position.y - core.y;
                float distance = std::sqrt(dx * dx + dy * dy);

                // 当たり判定
                if (distance < (PLAYER_COLLISION_RADIUS + core.radius)) {

                    // 当たった
                    core.isCollected = true;
                    playerScore += 1;

                    // エフェクトを再生する
                    terrain.SpawnExplosion({ core.x, core.y }, core.color);
                    player.trail.push_back({ player.position, 2.0f });
                }
            }
        }

        // ==========================================================================================================
        // 更新
        // ==========================================================================================================
        water.Update(dt);
        float cameraLeft = camera.position.x - 400.0f;
        player.Update(dt, horizontalInput,verticalInput, jumpInput, reelInput, water, cameraLeft, terrain);
        camera.Follow(player.position, dt);

        // ==========================================================================================================
        // 背景
        // ==========================================================================================================
        const int gridSpacing = 40; // 間隔

        // 境界座標
        float worldLeft = camera.position.x - 400.0f;
        float worldRight = camera.position.x + 400.0f;
        float worldTop = camera.position.y - 300.0f;
        float worldBottom = camera.position.y + 300.0f;

        // 境界座標から描画範囲を計算
        int startGridX = static_cast<int>(floorf(worldLeft / gridSpacing)) * gridSpacing;
        int startGridY = static_cast<int>(floorf(worldTop / gridSpacing)) * gridSpacing;

        // 描画する
        for (int x = startGridX; x <= worldRight; x += gridSpacing) {
            Vector2 sTop = camera.WorldToScreen({ static_cast<float>(x), worldTop });
            Vector2 sBottom = camera.WorldToScreen({ static_cast<float>(x), 550.0f });

            if (sBottom.y < sTop.y) sBottom.y = sTop.y;

            DrawLine(static_cast<int>(sTop.x), static_cast<int>(sTop.y),
                static_cast<int>(sBottom.x), static_cast<int>(sBottom.y),
                GetColor(30, 30, 45));
        }

        for (int y = startGridY; y <= 550; y += gridSpacing) {
            if (y > 550) continue;

            Vector2 sLeft = camera.WorldToScreen({ worldLeft,  static_cast<float>(y) });
            Vector2 sRight = camera.WorldToScreen({ worldRight, static_cast<float>(y) });

            DrawLine(static_cast<int>(sLeft.x), static_cast<int>(sLeft.y),
                static_cast<int>(sRight.x), static_cast<int>(sRight.y),
                GetColor(30, 30, 45));
        }

        // ==========================================================================================================
        // プレイヤー・マップを描画する
        // ==========================================================================================================
        terrain.Draw(camera);
        player.Draw(camera, mouseWorldPos, terrain);
        water.Draw(camera);

        // ==========================================================================================================
        // UI（スコア表示）
        // ==========================================================================================================
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 120);
        DrawBox(15, 15, 250, 50, GetColor(0, 0, 0), TRUE);
        DrawBox(15, 15, 250, 50, GetColor(0, 240, 210), FALSE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawFormatString(30, 24, GetColor(255, 255, 255), "CORE COLLECTED: %06d", playerScore);

        // ==========================================================================================================
        // ゲームオーバー画面
        // ==========================================================================================================
        if (player.isDead) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
            DrawBox(0, 0, 800, 600, GetColor(255, 255, 255), TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

            DrawString(340, 300, "Press [R] to Respawn", GetColor(255, 50, 50));
            DrawString(340, 330, "Press [Esc] to Quit", GetColor(255, 50, 50));
        }


        // ==========================================================================================================
        // デバッグ：いろいろな情報を表示する
        // ==========================================================================================================
        //DrawString(20, 20, "[CAMERA TEST] Mouse Left: Shoot Hook anywhere | Mouse Right: Release Hook", GetColor(255, 255, 255));
        //DrawString(20, 40, "              A / D: Add Swing Force | W: Reel In | SPACE: Jump", GetColor(255, 255, 255));
        
        //DrawFormatString(20, 60, GetColor(0, 255, 255), "Player World Pos: X:%.1f, Y:%.1f", player.position.x, player.position.y);
        //DrawFormatString(20, 80, GetColor(0, 255, 255), "Camera World Pos: X:%.1f, Y:%.1f", camera.position.x, camera.position.y);
        //DrawFormatString(20, 100, GetColor(255, 255, 0), "Player Speed    : %.2f px/s", player.velocity.Length());
        //DrawFormatString(20, 120, GetColor(255, 255, 0), "Rope Length     : %.2f px", player.isGrappling ? player.ropeLength : 0.0f);

        /*DrawFormatString(20, 150, GetColor(255, 255, 255),
            "Obstacles: %d | Loaded Chunks: %d | Cam: (%.1f, %.1f)",
            terrain.obstacles.size(), terrain.loadedChunks.size(), camera.position.x, camera.position.y);*/

        ScreenFlip();
    }

    DxLib_End();
    return 0;
}