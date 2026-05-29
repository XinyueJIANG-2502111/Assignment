#include "Water.hpp"
#include "Vector.hpp"
#include "Camera.hpp"

#include "DxLib.h"
#include <random>

// この部分のコメントは GEMINI で作成

// コンストラクタ：水面グリッドの初期化
Water::Water() {
    points.resize(numPoints);
    for (int i = 0; i < numPoints; i++) {
        points[i].y = surfaceY;       // 現在のY座標
        points[i].targetY = surfaceY; // 目標となる静止水面のY座標
        points[i].speed = 0.0f;       // 水面波の垂直移動速度
    }
}

// プレイヤー入水時の水しぶき（Splash）および波の発生
void Water::Splash(float playerWorldX, float velocityY, float cameraLeft) {
    // 1. プレイヤーの衝突位置から対応する水面頂点のインデックスを算出
    float relativeX = playerWorldX - cameraLeft;
    int index = static_cast<int>(relativeX / spacing);

    // インデックスが有効範囲内なら、入水速度に応じた下向きの力を加える（クランプあり）
    if (index >= 0 && index < numPoints) {
        float force = velocityY * 0.4f;
        points[index].speed = force > 300.0f ? 300.0f : force;
    }

    // 2. 入水速度に応じた水しぶきパーティクルの生成
    int particleCount = static_cast<int>(velocityY * 0.05f);
    for (int i = 0; i < particleCount; i++) {
        SplashParticle p;
        p.pos = { playerWorldX, surfaceY };

        // 上向き（扇状）のランダムな放射角度と初速を計算
        float angle = ((rand() % 100) / 100.0f) * 3.14159f * 0.6f + 3.14159f * 0.2f;
        float speed = ((rand() % 100) / 100.0f) * (velocityY * 0.6f) + 100.0f;
        p.vel = { cosf(angle) * speed, -sinf(angle) * speed };

        // パーティクルの初期パラメータ設定（ライフタイム、サイズ、アルファ値）
        p.alpha = 1.0f; p.size = 3.0f + (rand() % 4); p.lifetime = 0.5f + ((rand() % 50) / 100.0f);
        particles.push_back(p);
    }
}

// プレイヤー死亡時（溺死など）の血中拡散エフェクトの生成
void Water::EmitBlood(Vector2 worldPos, int count) {
    int actualCount = count * 6; // 生成密度を調整

    for (int i = 0; i < actualCount; i++) {
        BloodParticle p;

        // 発生源を中心に360度ランダムなオフセットを適用し、初期位置を分散させる
        float offset = ((rand() % 100) / 100.0f) * 8.0f;
        float angle = ((rand() % 100) / 100.0f) * 2.0f * 3.14159f;
        p.pos.x = worldPos.x + cosf(angle) * offset;
        p.pos.y = worldPos.y + sinf(angle) * offset;

        // 微小な左右への散らばりと、水中をゆっくり上昇（湧き上がる）するための初速を設定
        p.vel.x = ((rand() % 20) - 10.0f) * 0.5f;
        p.vel.y = -(((rand() % 40) + 40.0f));

        // 基本外観とライフタイムのランダム設定
        p.size = 1.0f + ((rand() % 120) / 100.0f);
        p.alpha = 0.15f + ((rand() % 15) / 100.0f);
        p.maxLifetime = 2.0f + ((rand() % 100) / 100.0f) * 1.5f;
        p.currentLifetime = p.maxLifetime;

        // 水中でのゆらぎ（サイン波）用の初期フェーズ
        p.wavePhase = ((rand() % 100) / 100.0f) * 6.28f;

        bloodParticles.push_back(p);
    }
}

// レンダリング処理：水体・水面線・各種パーティクルの描画
void Water::Draw(const Camera& camera) {
    // 水体のブレンド設定とカラー定義
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 130);
    int waterColor = GetColor(134, 214, 217);  // 水体（半透明のシアン）
    int surfaceColor = GetColor(0, 230, 194); // 水面線（発光感のあるエメラルド）

    // カメラの表示領域基準で左端の世界座標を特定
    float worldLeft = camera.position.x - 400.0f;

    // 1. 水面グリッドをループし、四角形（メッシュ）を構築して水中を塗りつぶす
    for (int i = 0; i < numPoints - 1; i++) {
        // 現在のステップと次のステップの世界座標X
        float wx1 = worldLeft + static_cast<float>(i * spacing);
        float wx2 = worldLeft + static_cast<float>((i + 1) * spacing);

        // 各頂点の現在の水面Y座標
        float wy1 = points[i].y;
        float wy2 = points[i + 1].y;

        // スクリーン座標への変換（水面側）
        Vector2 sW1 = camera.WorldToScreen({ wx1, wy1 });
        Vector2 sW2 = camera.WorldToScreen({ wx2, wy2 });

        // スクリーン座標への変換（海底側：十分な深さ 2500f まで引き伸ばす）
        Vector2 sB1 = camera.WorldToScreen({ wx1, 2500.0f });
        Vector2 sB2 = camera.WorldToScreen({ wx2, 2500.0f });

        // 水体の四角形描画
        DrawQuadrangle(
            static_cast<int>(sW1.x), static_cast<int>(sW1.y),
            static_cast<int>(sW2.x), static_cast<int>(sW2.y),
            static_cast<int>(sB2.x), static_cast<int>(sB2.y),
            static_cast<int>(sB1.x), static_cast<int>(sB1.y),
            waterColor, TRUE
        );

        // エッジ（水面線）の描画
        DrawLine(static_cast<int>(sW1.x), static_cast<int>(sW1.y), static_cast<int>(sW2.x), static_cast<int>(sW2.y), surfaceColor, 2);
    }

    // 2. 死亡エフェクト（血中拡散パーティクル）の描画
    for (const auto& bp : bloodParticles) {
        Vector2 sPos = camera.WorldToScreen(bp.pos);
        // 画面外の簡易プレシザー（カリング）
        if (sPos.x < -10 || sPos.x > 810 || sPos.y < -10 || sPos.y > 610) continue;

        int alphaVal = static_cast<int>(bp.alpha * 255);
        if (alphaVal <= 0) continue;

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal);
        DrawCircle(static_cast<int>(sPos.x), static_cast<int>(sPos.y),
            static_cast<int>(bp.size), GetColor(200, 0, 20), TRUE); // 赤い血液ドット
    }

    // 3. 入水時水しぶきパーティクルの描画
    for (const auto& p : particles) {
        Vector2 sParticlePos = camera.WorldToScreen(p.pos);
        int a = static_cast<int>(p.alpha * 200);
        if (a < 0) a = 0;
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, a);
        DrawCircle(static_cast<int>(sParticlePos.x), static_cast<int>(sParticlePos.y), static_cast<int>(p.size), GetColor(100, 220, 255), TRUE);
    }

    // 描画後、後続の描画バッチへの影響を防ぐためブレンドモードをリセット
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

// 物理およびアニメーション状態の更新（毎フレーム実行）
void Water::Update(float dt) {
    // 1. 各水面頂点の独立したばね運動（フックの法則：F = -kx - cv）の計算
    for (int i = 0; i < numPoints; i++) {
        float diff = points[i].y - points[i].targetY; // 平衡状態からの変位
        float acceleration = -k * diff - damping * points[i].speed; // 加速度 = -k*x - damping*v
        points[i].speed += acceleration * dt * 60.0f; // 速度更新（60Hzベースのスケール補正）
        points[i].y += points[i].speed * dt;          // 位置更新
    }

    // 隣接する頂点間での波の伝播（ディフュージョン・パス）
    std::vector<float> leftDeltas(numPoints, 0.0f);
    std::vector<float> rightDeltas(numPoints, 0.0f);

    // 伝播速度と安定性のバランスを取るため、1フレーム内に8回反復処理を実行
    for (int j = 0; j < 8; j++) {
        for (int i = 0; i < numPoints; i++) {
            if (i > 0) {
                leftDeltas[i] = spread * (points[i].y - points[i - 1].y);
                points[i - 1].speed += leftDeltas[i] * dt * 60.0f;
            }
            if (i < numPoints - 1) {
                rightDeltas[i] = spread * (points[i].y - points[i + 1].y);
                points[i + 1].speed += rightDeltas[i] * dt * 60.0f;
            }
        }

        // 変位の結合
        for (int i = 0; i < numPoints; i++) {
            if (i > 0) points[i - 1].y += leftDeltas[i] * dt;
            if (i < numPoints - 1) points[i + 1].y += rightDeltas[i] * dt;
        }
    }


    // 2. 水しぶきパーティクルの物理更新（重力適用、フェードアウト、メモリ解放）
    for (size_t i = 0; i < particles.size(); ) {
        particles[i].vel.y += 800.0f * dt; // 下向きの重力加速度
        particles[i].pos += particles[i].vel * dt;
        particles[i].lifetime -= dt;
        particles[i].alpha = particles[i].lifetime / 1.0f; // 時間経過による線形フェードアウト

        // 寿命切れまたは完全に透明になったパーティクルの削除
        if (particles[i].lifetime <= 0.0f || particles[i].alpha <= 0.0f) {
            particles.erase(particles.begin() + i);
        }
        else {
            i++;
        }
    }


    const float WATER_LINE_Y = 550.0f; // 水面の静止基準高度

    // 3. 血中拡散パーティクルの物理更新
    for (auto it = bloodParticles.begin(); it != bloodParticles.end(); ) {
        it->currentLifetime -= dt;

        // 寿命切れ、または浮上して水面より上に出てしまった場合は削除
        if (it->currentLifetime <= 0.0f || it->pos.y < WATER_LINE_Y) {
            it = bloodParticles.erase(it);
        }
        else {
            // 水中上昇のシミュレーション（徐々に上昇速度が上がっていくが、終端速度 -90.0f でクランプ）
            it->vel.y -= 30.0f * dt;
            if (it->vel.y < -90.0f) it->vel.y = -90.0f;

            // 水中での横揺れアニメーション（サイン波）の更新
            it->wavePhase += dt * 5.0f;
            float swing = sinf(it->wavePhase) * 1.2f;

            // 最終位置の累積
            it->pos.y += it->vel.y * dt;
            it->pos.x += (it->vel.x + swing) * dt;

            // 水面接近時のフェードアウト処理：水面から60px以内に入ると、近づくほど透明になる
            float distanceToSurface = it->pos.y - WATER_LINE_Y;
            float surfaceAlphaScale = 1.0f;
            if (distanceToSurface < 60.0f) {
                surfaceAlphaScale = distanceToSurface / 60.0f;
            }

            // 残り寿命の割合と水面ブレンド率を乗算してアルファ値を確定
            float lifeRatio = it->currentLifetime / it->maxLifetime;
            it->alpha = (lifeRatio * 0.25f) * surfaceAlphaScale;

            // 時間経過とともに血痕が水中をじんわりと拡散（巨大化）していく表現
            it->size += dt * 0.5f;

            it++;
        }
    }
}