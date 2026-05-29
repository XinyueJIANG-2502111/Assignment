#pragma once
#include <cmath>

// 二次元ベクトルのクラス
// 基本の計算を実装する（+-*/、内積、長さ、規格化）
struct Vector2 {
    float x, y;

    // オペレーターオーバロード
    Vector2 operator+(const Vector2& o) const { return { x + o.x, y + o.y }; }
    Vector2 operator-(const Vector2& o) const { return { x - o.x, y - o.y }; }
    Vector2 operator*(float scalar) const { return { x * scalar, y * scalar }; }
    Vector2& operator+=(const Vector2& o) { x += o.x; y += o.y; return *this; }
    Vector2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
    Vector2& operator-=(const Vector2& v) { x -= v.x; y -= v.y; return *this; }

    // 内積
    float Dot(const Vector2& o) const { return x * o.x + y * o.y; }
    
    // 長さ
    float Length() const { return std::sqrt(x * x + y * y); }
    
    // 規格化
    Vector2 Normalized() const {
        float l = Length();
        return l > 0.0f ? Vector2{ x / l, y / l } : Vector2{ 0.0f, 0.0f };
    }
};