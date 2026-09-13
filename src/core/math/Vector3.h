#pragma once

#include <cmath>

namespace ars::core::math {

/**
 * @brief Трёхмерный вектор (для distance estimation 3D-фракталов).
 * Заголовочный класс.
 */
class Vector3 {
public:
    double x{0.0};
    double y{0.0};
    double z{0.0};

    constexpr Vector3() = default;
    constexpr Vector3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    constexpr Vector3 operator+(const Vector3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vector3 operator-(const Vector3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vector3 operator*(double s) const { return {x * s, y * s, z * s}; }
    constexpr Vector3 operator/(double s) const { return {x / s, y / s, z / s}; }
    constexpr Vector3 operator-() const { return {-x, -y, -z}; }

    constexpr Vector3& operator+=(const Vector3& o) { return *this = *this + o; }
    constexpr Vector3& operator-=(const Vector3& o) { return *this = *this - o; }

    constexpr double dot(const Vector3& o) const { return x * o.x + y * o.y + z * o.z; }
    constexpr double lengthSquared() const { return dot(*this); }
    double length() const { return std::sqrt(lengthSquared()); }
    double distance(const Vector3& o) const { return (*this - o).length(); }

    constexpr Vector3 cross(const Vector3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }

    Vector3 normalized() const {
        const double l = length();
        return l > 1e-15 ? *this / l : Vector3{};
    }

    Vector3 abs() const { return {std::fabs(x), std::fabs(y), std::fabs(z)}; }

    /// Положительное по компонентам: (|x|, |y|, |z|).
    Vector3 componentMax(const Vector3& o) const {
        return {std::max(x, o.x), std::max(y, o.y), std::max(z, o.z)};
    }
    Vector3 componentMin(const Vector3& o) const {
        return {std::min(x, o.x), std::min(y, o.y), std::min(z, o.z)};
    }

private:
    static double max(double a, double b) { return a > b ? a : b; }
    static double min(double a, double b) { return a < b ? a : b; }
};

} // namespace ars::core::math