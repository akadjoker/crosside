#pragma once
#include <raylib.h>
#include <cmath>

#define radian 57295.77951
 
const float RECIPROCAL_PI = 1.0f / PI;
const float HALF_PI = PI / 2.0f;
const float DEGTORAD = PI / 180.0f;
const float RADTODEG = 180.0f / PI;
const float MinValue = -3.40282347E+38;
const float MaxValue = 3.40282347E+38;
const float Epsilon = 1.401298E-45;


#define PI_TIMES_TWO 6.28318530718f
#define PI2 PI * 2
#define DEG -180 / PI
#define RAD PI / -180

struct Vec2
{
    float x;
    float y;

    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float xy) : x(xy), y(xy) {}
    Vec2(float x, float y) : x(x), y(y) {}

    void set(float x, float y);
    void set(const Vec2 &other);

    Vec2 &add(const Vec2 &other);
    Vec2 &subtract(const Vec2 &other);
    Vec2 &multiply(const Vec2 &other);
    Vec2 &divide(const Vec2 &other);

    Vec2 &add(float value);
    Vec2 &subtract(float value);
    Vec2 &multiply(float value);
    Vec2 &divide(float value);

    Vec2 normal();
    Vec2 perp() const;

    static Vec2 RotatePoint(float X, float Y, float PivotX, float PivotY, float Angle);

    friend Vec2 operator+(Vec2 left, const Vec2 &right);
    friend Vec2 operator-(Vec2 left, const Vec2 &right);
    friend Vec2 operator*(Vec2 left, const Vec2 &right);
    friend Vec2 operator/(Vec2 left, const Vec2 &right);

    friend Vec2 operator+(Vec2 left, float value);
    friend Vec2 operator-(Vec2 left, float value);
    friend Vec2 operator*(Vec2 left, float value);
    friend Vec2 operator/(Vec2 left, float value);

    friend Vec2 operator*(float value, Vec2 left);
    friend Vec2 operator-(Vec2 left);

    bool operator==(const Vec2 &other) const;
    bool operator!=(const Vec2 &other) const;

    Vec2 &operator+=(const Vec2 &other);
    Vec2 &operator-=(const Vec2 &other);
    Vec2 &operator*=(const Vec2 &other);
    Vec2 &operator/=(const Vec2 &other);

    Vec2 &operator+=(float value);
    Vec2 &operator-=(float value);
    Vec2 &operator*=(float value);
    Vec2 &operator/=(float value);

    bool operator<(const Vec2 &other) const;
    bool operator<=(const Vec2 &other) const;
    bool operator>(const Vec2 &other) const;
    bool operator>=(const Vec2 &other) const;

    float magnitude() const;
    Vec2 normalised() const;
    float distance(const Vec2 &other) const;
    float dot(const Vec2 &other) const;
    Vec2 rotate(float angle) const;
};
  
struct Matrix2D
{

    Matrix2D();
    virtual ~Matrix2D();
    void Identity();
    void Set(float a, float b, float c, float d, float tx, float ty);
    void Concat(const Matrix2D &m);

    
    Vector2 TransformCoords(Vector2 point);
    Vector2 TransformCoords(float x, float y);

    Vector2 TransformCoords(const Vector2 &point) const;
    Vector2 TransformCoords(float x, float y) const;

    static Matrix2D GetTransformation(double x, double y, double angle, const Vector2 &pivot, const Vector2 &scale);

    Matrix2D Mult(const Matrix2D &m);
    void Rotate(float angle);
    void Scale(float x, float y);
    void Translate(float x, float y);
    void Skew(float skewX, float skewY);

    float a;
    float b;
    float c;
    float d;
    float tx;
    float ty;
};


static inline Matrix2D Matrix2DMult(const Matrix2D &m1, const Matrix2D &m2)
{
    Matrix2D result;
    
    result.a = m1.a * m2.a + m1.b * m2.c;
    result.b = m1.a * m2.b + m1.b * m2.d;
    result.c = m1.c * m2.a + m1.d * m2.c;
    result.d = m1.c * m2.b + m1.d * m2.d;
    
    result.tx = m1.tx * m2.a + m1.ty * m2.c + m2.tx;
    result.ty = m1.tx * m2.b + m1.ty * m2.d + m2.ty;
    
    return result;
}


static inline Matrix2D GetRelativeTransformation(float finalX, float finalY, float scale_x, float scale_y, float skew_x, float skew_y, float pivot_x, float pivot_y, float angle)
{
    Matrix2D mat;
    if (skew_x == 0.0f && skew_y == 0.0f)
    {

        if (angle == 0.0)
        {

            mat.Set(scale_x, 0.0, 0.0, scale_y,
                    finalX - pivot_x * scale_x,
                    finalY - pivot_y * scale_y);
        }
        else
        {
            float acos = cos(angle * RAD);
            float asin = sin(angle * RAD);
            float a = scale_x * acos;
            float b = scale_x * asin;
            float c = scale_y * -asin;
            float d = scale_y * acos;
            float tx = finalX - pivot_x * a - pivot_y * c;
            float ty = finalY - pivot_x * b - pivot_y * d;

            mat.Set(a, b, c, d, tx, ty);
        }
    }
    else
    {

        mat.Identity();
        mat.Scale(scale_x, scale_y);
        mat.Skew(skew_x, skew_y);
        mat.Rotate(angle);
        mat.Translate(finalX, finalY);

        if (pivot_x != 0.0f || pivot_y != 0.0f)
        {

            mat.tx = finalX - mat.a * pivot_x - mat.c * pivot_y;
            mat.ty = finalY - mat.b * pivot_x - mat.d * pivot_y;
        }
    }

    return mat;
}
 