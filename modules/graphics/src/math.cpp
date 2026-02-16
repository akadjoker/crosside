#include "math.hpp"



int sign(float value) { return value < 0 ? -1 : (value > 0 ? 1 : 0); }

float clip(float value, float min, float max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

float clamp(float value, float min, float max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

float lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

float normalize_angle(float angle)
{
    while (angle > 360)
        angle -= 360;
    while (angle < 0)
        angle += 360;
    return angle;
}

float clamp_angle(float angle, float min, float max)
{

    angle = normalize_angle(angle);
    if (angle > 180)
    {
        angle -= 360;
    }
    else if (angle < -180)
    {
        angle += 360;
    }

    min = normalize_angle(min);
    if (min > 180)
    {
        min -= 360;
    }
    else if (min < -180)
    {
        min += 360;
    }

    max = normalize_angle(max);
    if (max > 180)
    {
        max -= 360;
    }
    else if (max < -180)
    {
        max += 360;
    }

    return clamp(angle, min, max);
}

 

double get_distx(double a, double d)
{
    double angulo = (double)a * RAD;
    return ((double)(cos(angulo) * d));
}

double get_disty(double a, double d)
{
    double angulo = (double)a * RAD;
    return (-(double)(sin(angulo) * d));
}

//*********************************************************************************************************************
//**                         Vec2                                                                             **
//*********************************************************************************************************************

void Vec2::set(float x, float y)
{

    this->x = x;
    this->y = y;
}

void Vec2::set(const Vec2 &other)
{

    x = other.x;
    y = other.y;
}

Vec2 &Vec2::add(const Vec2 &other)
{
    x += other.x;
    y += other.y;

    return *this;
}

Vec2 &Vec2::subtract(const Vec2 &other)
{
    x -= other.x;
    y -= other.y;

    return *this;
}

Vec2 &Vec2::multiply(const Vec2 &other)
{
    x *= other.x;
    y *= other.y;

    return *this;
}

Vec2 &Vec2::divide(const Vec2 &other)
{
    x /= other.x;
    y /= other.y;

    return *this;
}

Vec2 &Vec2::add(float value)
{
    x += value;
    y += value;

    return *this;
}

Vec2 &Vec2::subtract(float value)
{
    x -= value;
    y -= value;

    return *this;
}

Vec2 &Vec2::multiply(float value)
{
    x *= value;
    y *= value;

    return *this;
}

Vec2 &Vec2::divide(float value)
{
    x /= value;
    y /= value;

    return *this;
}

Vec2 Vec2::normal()
{
    return Vec2(y, -x).normalised();
}

Vec2 operator+(Vec2 left, const Vec2 &right)
{
    return left.add(right);
}

Vec2 operator-(Vec2 left, const Vec2 &right)
{
    return left.subtract(right);
}

Vec2 operator*(Vec2 left, const Vec2 &right)
{
    return left.multiply(right);
}

Vec2 operator/(Vec2 left, const Vec2 &right)
{
    return left.divide(right);
}

Vec2 operator+(Vec2 left, float value)
{
    return Vec2(left.x + value, left.y + value);
}

Vec2 operator-(Vec2 left, float value)
{
    return Vec2(left.x - value, left.y - value);
}

Vec2 operator*(Vec2 left, float value)
{
    return Vec2(left.x * value, left.y * value);
}

Vec2 operator/(Vec2 left, float value)
{
    return Vec2(left.x / value, left.y / value);
}

Vec2 operator*(float value, Vec2 left)
{
    return Vec2(left.x * value, left.y * value);
}

Vec2 operator-(Vec2 left)
{
    return Vec2(-left.x, -left.y);
}

Vec2 &Vec2::operator+=(const Vec2 &other)
{
    return add(other);
}

Vec2 &Vec2::operator-=(const Vec2 &other)
{
    return subtract(other);
}

Vec2 &Vec2::operator*=(const Vec2 &other)
{
    return multiply(other);
}

Vec2 &Vec2::operator/=(const Vec2 &other)
{
    return divide(other);
}

Vec2 &Vec2::operator+=(float value)
{
    return add(value);
}

Vec2 &Vec2::operator-=(float value)
{
    return subtract(value);
}

Vec2 &Vec2::operator*=(float value)
{
    return multiply(value);
}

Vec2 &Vec2::operator/=(float value)
{
    return divide(value);
}

bool Vec2::operator==(const Vec2 &other) const
{
    return x == other.x && y == other.y;
}

bool Vec2::operator!=(const Vec2 &other) const
{
    return !(*this == other);
}

bool Vec2::operator<(const Vec2 &other) const
{
    return x < other.x && y < other.y;
}

bool Vec2::operator<=(const Vec2 &other) const
{
    return x <= other.x && y <= other.y;
}

bool Vec2::operator>(const Vec2 &other) const
{
    return x > other.x && y > other.y;
}

bool Vec2::operator>=(const Vec2 &other) const
{
    return x >= other.x && y >= other.y;
}

float Vec2::distance(const Vec2 &other) const
{
    float a = x - other.x;
    float b = y - other.y;
    return sqrt(a * a + b * b);
}

float Vec2::dot(const Vec2 &other) const
{
    return x * other.x + y * other.y;
}

float Vec2::magnitude() const
{
    return sqrt(x * x + y * y);
}

Vec2 Vec2::normalised() const
{
    float length = magnitude();
    return Vec2(x / length, y / length);
}

Vec2 Vec2::perp() const { return {-y, x}; }

Vec2 Vec2::RotatePoint(float X, float Y, float PivotX, float PivotY, float Angle)
{
    float sin = 0;
    float cos = 0;
    float radians = Angle * -0.017453293f;

    while (radians < -3.14159265f)
        radians += 6.28318531f;
    while (radians > 3.14159265f)
        radians -= 6.28318531f;

    if (radians < 0)
    {
        sin = 1.27323954f * radians + 0.405284735f * radians * radians;
        if (sin < 0)
            sin = 0.225f * (sin * -sin - sin) + sin;
        else
            sin = 0.225f * (sin * sin - sin) + sin;
    }
    else
    {
        sin = 1.27323954f * radians - 0.405284735f * radians * radians;
        if (sin < 0)
            sin = 0.225f * (sin * -sin - sin) + sin;
        else
            sin = 0.225f * (sin * sin - sin) + sin;
    }

    radians += 1.57079632f; // +90 graus
    if (radians > 3.14159265f)
        radians -= 6.28318531f;
    if (radians < 0)
    {
        cos = 1.27323954f * radians + 0.405284735f * radians * radians;
        if (cos < 0)
            cos = 0.225f * (cos * -cos - cos) + cos;
        else
            cos = 0.225f * (cos * cos - cos) + cos;
    }
    else
    {
        cos = 1.27323954f * radians - 0.405284735f * radians * radians;
        if (cos < 0)
            cos = 0.225f * (cos * -cos - cos) + cos;
        else
            cos = 0.225f * (cos * cos - cos) + cos;
    }

    float dx = X - PivotX;
    float dy = PivotY - Y;

    Vec2 result;

    result.x = PivotX + cos * dx - sin * dy;
    result.y = PivotY - sin * dx - cos * dy;

    return result;
}

Vec2 Vec2::rotate(float angle) const
{
    float s = sinf(angle);
    float c = cosf(angle);
    float x = this->x;
    float y = this->y;
    return Vec2(x * c - y * s, x * s + y * c);
}

 

//*********************************************************************************************************************
//**                         Matrix2D                                                                              **
//*********************************************************************************************************************

Matrix2D::Matrix2D()
{
    a = 1;
    b = 0;
    c = 0;
    d = 1;
    tx = 0;
    ty = 0;
}

Matrix2D::~Matrix2D()
{
}

void Matrix2D::Identity()
{
    a = 1;
    b = 0;
    c = 0;
    d = 1;
    tx = 0;
    ty = 0;
}

void Matrix2D::Set(float a, float b, float c, float d, float tx, float ty)
{

    this->a = a;
    this->b = b;
    this->c = c;
    this->d = d;
    this->tx = tx;
    this->ty = ty;
}

void Matrix2D::Concat(const Matrix2D &m)
{
    float a1 = this->a * m.a + this->b * m.c;
    this->b = this->a * m.b + this->b * m.d;
    this->a = a1;

    float c1 = this->c * m.a + this->d * m.c;
    this->d = this->c * m.b + this->d * m.d;

    this->c = c1;

    float tx1 = this->tx * m.a + this->ty * m.c + m.tx;
    this->ty = this->tx * m.b + this->ty * m.d + m.ty;
    this->tx = tx1;
}

Vector2 Matrix2D::TransformCoords(Vector2 point)
{

    Vector2  v;

    v.x = this->a * point.x + this->c * point.y + this->tx;
    v.y = this->d * point.y + this->b * point.x + this->ty;

    return v;
}
Vector2 Matrix2D::TransformCoords(float x, float y)
{
    Vector2 v;

    v.x = this->a * x + this->c * y + this->tx;
    v.y = this->d * y + this->b * x + this->ty;

    return v;
}

Vector2 Matrix2D::TransformCoords(const Vector2 &point) const
{
    Vector2 v;

    v.x = this->a * point.x + this->c * point.y + this->tx;
    v.y = this->d * point.y + this->b * point.x + this->ty;

    return v;
}

Vector2 Matrix2D::TransformCoords(float x, float y) const
{
    Vector2 v;

    v.x = this->a * x + this->c * y + this->tx;
    v.y = this->d * y + this->b * x + this->ty;

    return v;
}

Matrix2D Matrix2D::GetTransformation(double x, double y, double angle, const Vector2 &pivot, const Vector2 &scale)
{
    Matrix2D mat;

    if (angle == 0.0)
    {

        mat.Set(scale.x, 0.0, 0.0, scale.y,
                x - pivot.x * scale.x,
                y - pivot.y * scale.y);
    }
    else
    {
        float acos = cos(angle * RAD);
        float asin = sin(angle * RAD);
        float a = scale.x * acos;
        float b = scale.x * asin;
        float c = scale.y * -asin;
        float d = scale.y * acos;
        float tx = x - pivot.x * a - pivot.y * c;
        float ty = y - pivot.x * b - pivot.y * d;

        mat.Set(a, b, c, d, tx, ty);
    }

    return mat;
}

Matrix2D Matrix2D::Mult(const Matrix2D &m)
{
    Matrix2D result;

    result.a = this->a * m.a + this->b * m.c;
    result.b = this->a * m.b + this->b * m.d;
    result.c = this->c * m.a + this->d * m.c;
    result.d = this->c * m.b + this->d * m.d;

    result.tx = this->tx * m.a + this->ty * m.c + this->tx;
    result.ty = this->tx * m.b + this->ty * m.d + this->ty;

    return result;
}

void Matrix2D::Rotate(float angle)
{
    float acos = cos(angle);
    float asin = sin(angle);

    float a1 = this->a * acos - this->b * asin;
    this->b = this->a * asin + this->b * acos;
    this->a = a1;

    float c1 = this->c * acos - this->d * asin;
    this->d = this->c * asin + this->d * acos;
    this->c = c1;

    float tx1 = this->tx * acos - this->ty * asin;
    this->ty = this->tx * asin + this->ty * acos;
    this->tx = tx1;
}

void Matrix2D::Scale(float x, float y)
{
    this->a *= x;
    this->b *= y;

    this->c *= x;
    this->d *= y;

    this->tx *= x;
    this->ty *= y;
}

void Matrix2D::Translate(float x, float y)
{
    this->tx += x;
    this->ty += y;
}

void Matrix2D::Skew(float skewX, float skewY)
{
    float sinX = sin(skewX);
    float cosX = cos(skewX);
    float sinY = sin(skewY);
    float cosY = cos(skewY);

    Set(
        this->a * cosY - this->b * sinX,
        this->a * sinY + this->b * cosX,
        this->c * cosY - this->d * sinX,
        this->c * sinY + this->d * cosX,
        this->tx * cosY - this->ty * sinX,
        this->tx * sinY + this->ty * cosX);
}

 