#include "DxLib.h"
#include "Vector.hpp"
#include "Camera.hpp"
#include "Player.hpp"
#include "Water.hpp"
#include "Terrain.hpp"
#include <cmath>
#include <vector>

int playerScore = 0;
const float PLAYER_COLLISION_RADIUS = 16.0f; // 与你解算障碍物用的半径一致

// ========================================================
// 游戏全局重置函数
// ========================================================
void ResetGame(Player& player, TerrainManager& terrain, Camera& camera, int& score) {
    // 1. 重置得分
    score = 0;

    // 2. 重置玩家物理状态与核心变量
    player.position = Vector2{ 100.0f, 300.0f }; // 回到初始出生点
    player.velocity = Vector2{ 0.0f, 0.0f };     // 速度清零
    player.isDead = false;                     // 复活

    // 记得把上一把飞到一半的钩爪状态彻底掐断，否则出生瞬间绳子会乱飞！
    player.grappleState = Player::GrappleState::None;
    player.isGrappling = false;

    // 3.【核心卡点】：彻底清空并重置地形管理器
    // 假设你的 TerrainManager 内部有 clear 相关的函数，或者可以直接调用其初始化
    terrain.obstacles.clear(); // 强行清空所有砖块数组！
    terrain.cores.clear();     // 强行清空所有可收集核心！

    // 重新调用你最开始在 main 里面调用过的地形初始生成逻辑
    // 比如：生成出生点附近的头几块打底砖块
    terrain.Init();

    // 4. 重置摄像机位置，防止画面卡在上一把死亡的老地方
    camera.position.x = 0.0f;
    camera.position.y = 0.0f;
}

// ==========================================
// 3. 游戏主程序入口（相机与无限世界测试版）
// ==========================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 基础引擎配置
    ChangeWindowMode(TRUE);          // 窗口模式
    SetGraphMode(800, 600, 32);      // 800x600 分辨率
    SetWindowText("test");

    if (DxLib_Init() == -1) return -1;
    SetDrawScreen(DX_SCREEN_BACK);   // 开启双缓冲

    Player player;
    Water water;
    Camera camera;
    TerrainManager terrain;

    // プレイヤー位置初期化
    player.position = { 400.0f, 400.0f };

    terrain.Init();

    // 用高性能计数器精确计算 DeltaTime
    LONGLONG prevTime = GetNowHiPerformanceCount();

    // 主游戏循环
    while (ProcessMessage() == 0 && ClearDrawScreen() == 0) {
        // ---- A. 计算 DeltaTime ----
        LONGLONG currTime = GetNowHiPerformanceCount();
        float dt = static_cast<float>(currTime - prevTime) / 1000000.0f;
        prevTime = currTime;
        if (dt > 0.05f) dt = 0.05f; // 限制帧率突变对物理的干扰

        // ---- B. 获取玩家输入 ----
        int horizontalInput = 0;
        if (CheckHitKey(KEY_INPUT_A) || CheckHitKey(KEY_INPUT_LEFT))  horizontalInput = -1;
        if (CheckHitKey(KEY_INPUT_D) || CheckHitKey(KEY_INPUT_RIGHT)) horizontalInput = 1;

        int verticalInput = 0;
        if (CheckHitKey(KEY_INPUT_S) || CheckHitKey(KEY_INPUT_DOWN)) verticalInput = 1;

        // 跳跃判定
        bool jumpInput = false;
        if (CheckHitKey(KEY_INPUT_SPACE)) {
            jumpInput = true;
            player.Jump();
        }

        bool reelInput = (CheckHitKey(KEY_INPUT_W) || CheckHitKey(KEY_INPUT_UP));

        // 获取当前鼠标在屏幕上的像素位置
        int mouseX, mouseY;
        GetMousePoint(&mouseX, &mouseY);

        // 把鼠标的屏幕像素坐标转换为大世界的【绝对世界坐标】
        Vector2 mouseWorldPos = {
            static_cast<float>(mouseX) + camera.position.x - 400.0f,
            static_cast<float>(mouseY) + camera.position.y - 300.0f
        };

        // 鼠标左键按下：发射钩爪
        if ((GetMouseInput() & MOUSE_INPUT_LEFT) != 0) {
            jumpInput = true;
            //player.LaunchGrapple(mouseWorldPos, terrain); // 传入转换后的世界坐标
            // 现在没有左/右/上方边界了，只要没挂钩，你可以点大世界的任何地方挂载！
            if (player.grappleState == Player::GrappleState::None) {
                player.LaunchGrapple(mouseWorldPos, terrain);
            }
        }

        // 鼠标右键：松开钩爪
        if ((GetMouseInput() & MOUSE_INPUT_RIGHT) != 0) {
            player.ReleaseGrapple();
        }

        if (player.isDead && CheckHitKey(KEY_INPUT_R)) {
            // 重置所有状态
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

        if (player.isDead && CheckHitKey(KEY_INPUT_ESCAPE)) {
            break;
        }

        //【核心新增】：实时计算相机视口的右边缘世界坐标
        // 驱动无限生成与过时方块销毁
        terrain.UpdateAllDirections(camera.position);
        // ========================================================
        // 【全新新增】：金币/核心收集的碰撞实时解算
        // ========================================================
        if (!player.isDead) {
            for (auto& core : terrain.cores) {
                if (core.isCollected) continue;

                // 计算玩家中心点和核心中心点的绝对距离
                float dx = player.position.x - core.x;
                float dy = player.position.y - core.y;
                float distance = std::sqrt(dx * dx + dy * dy);

                // 圆形碰撞公式：如果两点距离小于“玩家半径 + 核心半径”
                if (distance < (PLAYER_COLLISION_RADIUS + core.radius)) {

                    // 判定成功吃掉！
                    core.isCollected = true;
                    playerScore += 1; // 喜提 100 分！

                    //【核心新增】：吃掉核心的瞬间，在核心的位置原地引爆霓虹粒子特效！
                    terrain.SpawnExplosion({ core.x, core.y }, core.color);

                    // 顺便往玩家当前的尾巴里扔一个发光爆发点
                    player.trail.push_back({ player.position, 2.0f });
                }
            }
        }

        // ---- C. 物理与相机跟随更新 ----
        water.Update(dt);
        float cameraLeft = camera.position.x - 400.0f;

        // 调用解耦后的物理扩展函数，将水体和控制量传入
        player.Update(dt, horizontalInput,verticalInput, jumpInput, reelInput, water, cameraLeft, terrain);

        // 让相机利用 Lerp 算法平滑追踪玩家的世界坐标
        camera.Follow(player.position, dt);


        // ==========================================
        // 动态无限网格背景渲染
        // ==========================================
        const int gridSpacing = 40; // 保持原有的 40 像素赛博朋克空间感

        // 动态计算当前相机视野在世界中的四个边界坐标
        float worldLeft = camera.position.x - 400.0f;
        float worldRight = camera.position.x + 400.0f;
        float worldTop = camera.position.y - 300.0f;
        float worldBottom = camera.position.y + 300.0f;

        // 根据边界世界坐标进行向下取整，获取当前视野里第一条网格线的世界起始位置
        int startGridX = static_cast<int>(floorf(worldLeft / gridSpacing)) * gridSpacing;
        int startGridY = static_cast<int>(floorf(worldTop / gridSpacing)) * gridSpacing;

        // 1. 绘制纵向世界网格线 (在水面 550.0f 处自动截断)
        for (int x = startGridX; x <= worldRight; x += gridSpacing) {
            Vector2 sTop = camera.WorldToScreen({ static_cast<float>(x), worldTop });
            Vector2 sBottom = camera.WorldToScreen({ static_cast<float>(x), 550.0f });

            if (sBottom.y < sTop.y) sBottom.y = sTop.y; // 防御性裁切

            DrawLine(static_cast<int>(sTop.x), static_cast<int>(sTop.y),
                static_cast<int>(sBottom.x), static_cast<int>(sBottom.y),
                GetColor(30, 30, 45));
        }

        // 2. 绘制横向世界网格线 (只在水面 550.0f 以上的空间绘制)
        for (int y = startGridY; y <= 550; y += gridSpacing) {
            if (y > 550) continue;

            Vector2 sLeft = camera.WorldToScreen({ worldLeft,  static_cast<float>(y) });
            Vector2 sRight = camera.WorldToScreen({ worldRight, static_cast<float>(y) });

            DrawLine(static_cast<int>(sLeft.x), static_cast<int>(sLeft.y),
                static_cast<int>(sRight.x), static_cast<int>(sRight.y),
                GetColor(30, 30, 45));
        }

        // ---- D. 实体统一渲染（上交相机） ----
        terrain.Draw(camera);
        player.Draw(camera, mouseWorldPos, terrain);

        water.Draw(camera);

        // ========================================================
        // 【全新新增】：在屏幕左上角打印极具未来感的得分 UI
        // ========================================================
        // 绘制一个半透明的黑色底框，显得很高级
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 120);
        DrawBox(15, 15, 250, 50, GetColor(0, 0, 0), TRUE);
        DrawBox(15, 15, 250, 50, GetColor(0, 240, 210), FALSE); // 霓虹蓝绿边框
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // 打印分数文本（DxLib 默认字体）
        DrawFormatString(30, 24, GetColor(255, 255, 255), "CORE COLLECTED: %06d", playerScore);

        // ---- E. UI 调试信息界面 (不随相机滚动，直接固定在物理屏幕上) ----
        //DrawString(20, 20, "[CAMERA TEST] Mouse Left: Shoot Hook anywhere | Mouse Right: Release Hook", GetColor(255, 255, 255));
        //DrawString(20, 40, "              A / D: Add Swing Force | W: Reel In | SPACE: Jump", GetColor(255, 255, 255));
        if (player.isDead) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
            DrawBox(0, 0, 800, 600, GetColor(255, 255, 255), TRUE); // 整个屏幕蒙上一层白纱
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

            //DrawString(320, 260, "YOU DROWNED IN CYBER MATRIX", GetColor(255, 50, 50));
            DrawString(340, 300, "Press [R] to Respawn", GetColor(255, 50, 50));
            DrawString(340, 330, "Press [Esc] to Quit", GetColor(255, 50, 50));
        }

        // 实时打印大世界的绝对物理坐标，方便观察无限位移
        DrawFormatString(20, 60, GetColor(0, 255, 255), "Player World Pos: X:%.1f, Y:%.1f", player.position.x, player.position.y);
        DrawFormatString(20, 80, GetColor(0, 255, 255), "Camera World Pos: X:%.1f, Y:%.1f", camera.position.x, camera.position.y);
        DrawFormatString(20, 100, GetColor(255, 255, 0), "Player Speed    : %.2f px/s", player.velocity.Length());
        DrawFormatString(20, 120, GetColor(255, 255, 0), "Rope Length     : %.2f px", player.isGrappling ? player.ropeLength : 0.0f);

        DrawFormatString(20, 150, GetColor(255, 255, 255),
            "Obstacles: %d | Loaded Chunks: %d | Cam: (%.1f, %.1f)",
            terrain.obstacles.size(), terrain.loadedChunks.size(), camera.position.x, camera.position.y);

        ScreenFlip();
    }

    DxLib_End();
    return 0;
}