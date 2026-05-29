#include "Terrain.hpp"
#include "Camera.hpp"
#include "DxLib.h"

void TerrainManager::SpawnExplosion(Vector2 spawnPos, unsigned int coreColor) {
    const int particleCount = 16;
    for (int i = 0; i < particleCount; i++) {
        // 発散角度
        float angle = static_cast<float>(rand() % 360) * (3.1415926f / 180.0f);
        // 速度
        float speed = 150.0f + static_cast<float>(rand() % 250);

        Vector2 pVel = { std::cos(angle) * speed, std::sin(angle) * speed };

        particles.push_back({ spawnPos, pVel, 1.0f, coreColor });
    }
}

void TerrainManager::Init() {
    lastGeneratedX = 300.0f;

    obstacles.clear();
    cores.clear();
    particles.clear();
    loadedChunks.clear();

    // プレイヤーの足元にブロックを生成
    obstacles.push_back({ 200.0f, 500.0f, 400.0f, 40.0f, GetColor(110, 110, 120) });
}

void TerrainManager::UpdateAllDirections(Vector2 cameraPos) {
    // カメラ位置
    int currentCX = static_cast<int>(std::floor(cameraPos.x / CHUNK_SIZE));
    int currentCY = static_cast<int>(std::floor(cameraPos.y / CHUNK_SIZE));

    // カメラを中心に周りのゾーンを探す
    const int radius = 2;
    for (int cx = currentCX - radius; cx <= currentCX + radius; ++cx) {
        for (int cy = currentCY - radius; cy <= currentCY + radius; ++cy) {

            // 生成しない場合
            if (loadedChunks.find({ cx, cy }) == loadedChunks.end()) {
                GenerateChunk(cx, cy);
                loadedChunks.insert({ cx, cy });
            }
        }
    }

    // カメラから離れたブロックを削除する
    float maxActiveDistance = CHUNK_SIZE * 4.0f;
    for (auto it = obstacles.begin(); it != obstacles.end(); ) {
        float centerX = it->x + it->width * 0.5f;
        float centerY = it->y + it->height * 0.5f;
        float distToCamera = std::sqrt(std::pow(centerX - cameraPos.x, 2) + std::pow(centerY - cameraPos.y, 2));

        if (distToCamera > maxActiveDistance) {
            it = obstacles.erase(it);
        }
        else {
            it++;
        }
    }

    // アイテムを更新
    for (auto it = cores.begin(); it != cores.end(); ) {
        float distToCamera = std::sqrt(std::pow(it->x - cameraPos.x, 2) + std::pow(it->y - cameraPos.y, 2));
        if (distToCamera > maxActiveDistance || it->isCollected) {
            it = cores.erase(it);
        }
        else {
            it++;
        }
    }

    // エフェクト
    const float dt = 0.008f;
    for (auto it = particles.begin(); it != particles.end(); ) {
        it->pos += it->velocity * dt;

        it->velocity *= (1.0f - 2.5f * dt);

        it->alpha -= 2.5f * dt;

        if (it->alpha <= 0.0f) {
            it = particles.erase(it);
        }
        else {
            it++;
        }
    }
}

void TerrainManager::GenerateChunk(int cx, int cy) {
    float chunkStartY = cy * CHUNK_SIZE;
    const float WATER_SURFACE_Y = 550.0f;

    // 水面の下にブロックを生成しない
    if (chunkStartY >= WATER_SURFACE_Y) {
        return;
    }

    // 乱数発生
    unsigned int seed = std::abs(cx * 73856093 ^ cy * 19349663);
    srand(seed);

    float chunkStartX = cx * CHUNK_SIZE;

    if (rand() % 100 < 30) return;

    int count = 1 + (rand() % 2);

    for (int i = 0; i < count; i++) {
        // ブロックサイズ
        float w = 80.0f + (rand() % 140);
        float h = 30.0f + (rand() % 50);

        // 位置
        float rx = chunkStartX + (rand() % static_cast<int>(CHUNK_SIZE - w));
        float ry = chunkStartY + (rand() % static_cast<int>(CHUNK_SIZE - h));

        // 水面下にいかないように
        if (ry + h > WATER_SURFACE_Y) {
            h = WATER_SURFACE_Y - ry - 2.0f;
            if (h < 15.0f) continue;
        }

        unsigned int blockColor = (cy < 0) ? GetColor(140, 150, 160) : GetColor(110, 115, 100);

        obstacles.push_back({ rx, ry, w, h, blockColor });

        // ========================================================
        // アイテム生成
        // ========================================================
        if (rand() % 100 < 60) { // 60% 確率でアイテムを生成
            float coreX = rx + w * 0.5f;
            float coreY = 0.0f;

            if (rand() % 2 == 0) {
                coreY = ry + h + 60.0f;
            }
            else {
                coreY = ry - 50.0f;
            }

            // 水面下に行かないように
            if (coreY < WATER_SURFACE_Y - 20.0f) {
                unsigned int coreColor = GetColor(250, 0, 0);
                cores.push_back({ coreX, coreY, 12.0f, coreColor, false, static_cast<float>(rand() % 100) * 0.01f });
            }
        }
    }
}

void TerrainManager::Draw(const Camera& camera) {
    for (const auto& obs : obstacles) {
        // 座標変換
        Vector2 topLeft = camera.WorldToScreen({ obs.x, obs.y });
        Vector2 bottomRight = camera.WorldToScreen({ obs.x + obs.width, obs.y + obs.height });

        int x1 = static_cast<int>(topLeft.x);
        int y1 = static_cast<int>(topLeft.y);
        int x2 = static_cast<int>(bottomRight.x);
        int y2 = static_cast<int>(bottomRight.y);

        // ========================================================
        // ブロック描画
        // ========================================================
        unsigned int glowColor = GetColor(0, 240, 210);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 35);
        DrawBox(x1 - 6, y1 - 6, x2 + 6, y2 + 6, glowColor, FALSE);
        DrawBox(x1 - 5, y1 - 5, x2 + 5, y2 + 5, glowColor, FALSE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 75);
        DrawBox(x1 - 3, y1 - 3, x2 + 3, y2 + 3, glowColor, FALSE);
        DrawBox(x1 - 2, y1 - 2, x2 + 2, y2 + 2, glowColor, FALSE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160);
        DrawBox(x1 - 1, y1 - 1, x2 + 1, y2 + 1, glowColor, FALSE);

        unsigned int coreColor = GetColor(220, 255, 250);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawBox(x1, y1, x2, y2, coreColor, FALSE);
    }

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // ========================================================
    // アイテム描画（軽いアニメーション含む）
    // ========================================================
    for (auto& core : cores) {
        if (core.isCollected) continue;

        // アニメーション用
        core.pulseTimer += 0.05f;
        float pulseScale = 1.0f + std::sin(core.pulseTimer) * 0.25f;

        Vector2 sCorePos = camera.WorldToScreen({ core.x, core.y });
        int cx = static_cast<int>(sCorePos.x);
        int cy = static_cast<int>(sCorePos.y);
        int baseR = static_cast<int>(core.radius);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80);
        DrawCircle(cx, cy, static_cast<int>(baseR * pulseScale * 1.8f), core.color, TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
        DrawCircle(cx, cy, static_cast<int>(baseR * pulseScale), core.color, FALSE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawCircle(cx, cy, 4, GetColor(255, 205, 205), TRUE);
        DrawCircle(cx, cy, 1, GetColor(255, 255, 255), TRUE);
    }

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // ========================================================
    // エフェクト描画
    // ========================================================
    for (const auto& p : particles) {
        Vector2 sPartPos = camera.WorldToScreen(p.pos);
        int px = static_cast<int>(sPartPos.x);
        int py = static_cast<int>(sPartPos.y);

        int alphaVal = static_cast<int>(p.alpha * 255);
        if (alphaVal < 0) alphaVal = 0;

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal / 3);
        DrawCircle(px, py, 8, p.color, TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal);
        DrawCircle(px, py, 3, GetColor(255, 255, 255), TRUE);
        DrawCircle(px, py, 4, p.color, FALSE);
    }
}