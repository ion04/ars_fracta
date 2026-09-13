#pragma once

#include <cmath>

#include "core/math/Vector3.h"

namespace ars::core::math {

/**
 * @brief Матрица 4x4 (column-major) для трансформаций.
 * Заголовочный класс; данные доступны как m[col][row].
 */
class Matrix4 {
public:
    double m[4][4]{};

    Matrix4() { *this = identity(); }

    static Matrix4 identity() {
        Matrix4 r;
        for (int c = 0; c < 4; ++c)
            for (int r2 = 0; r2 < 4; ++r2) r.m[c][r2] = (c == r2) ? 1.0 : 0.0;
        return r;
    }

    static Matrix4 translation(double x, double y, double z) {
        Matrix4 r = identity();
        r.m[3][0] = x;
        r.m[3][1] = y;
        r.m[3][2] = z;
        return r;
    }

    static Matrix4 rotationX(double angleRad) {
        Matrix4 r = identity();
        const double c = std::cos(angleRad), s = std::sin(angleRad);
        r.m[1][1] = c; r.m[2][1] = -s;
        r.m[1][2] = s; r.m[2][2] = c;
        return r;
    }

    static Matrix4 rotationY(double angleRad) {
        Matrix4 r = identity();
        const double c = std::cos(angleRad), s = std::sin(angleRad);
        r.m[0][0] = c;  r.m[2][0] = s;
        r.m[0][2] = -s; r.m[2][2] = c;
        return r;
    }

    static Matrix4 rotationZ(double angleRad) {
        Matrix4 r = identity();
        const double c = std::cos(angleRad), s = std::sin(angleRad);
        r.m[0][0] = c;  r.m[1][0] = -s;
        r.m[0][1] = s;  r.m[1][1] = c;
        return r;
    }

    static Matrix4 scale(double sx, double sy, double sz) {
        Matrix4 r = identity();
        r.m[0][0] = sx;
        r.m[1][1] = sy;
        r.m[2][2] = sz;
        return r;
    }

    /// Умножение двух column-major матриц: результат = A * B.
    Matrix4 operator*(const Matrix4& o) const {
        Matrix4 out;
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r) {
                double sum = 0.0;
                for (int k = 0; k < 4; ++k) sum += m[k][r] * o.m[c][k];
                out.m[c][r] = sum;
            }
        return out;
    }

    /// Трансформация точки (с учётом переноса).
    Vector3 transformPoint(const Vector3& p) const {
        const double x = m[0][0] * p.x + m[1][0] * p.y + m[2][0] * p.z + m[3][0];
        const double y = m[0][1] * p.x + m[1][1] * p.y + m[2][1] * p.z + m[3][1];
        const double z = m[0][2] * p.x + m[1][2] * p.y + m[2][2] * p.z + m[3][2];
        return {x, y, z};
    }

    /// Трансформация направления (без переноса).
    Vector3 transformDirection(const Vector3& p) const {
        const double x = m[0][0] * p.x + m[1][0] * p.y + m[2][0] * p.z;
        const double y = m[0][1] * p.x + m[1][1] * p.y + m[2][1] * p.z;
        const double z = m[0][2] * p.x + m[1][2] * p.y + m[2][2] * p.z;
        return {x, y, z};
    }

    double& operator()(int col, int row) { return m[col][row]; }
    double operator()(int col, int row) const { return m[col][row]; }
    const double* data() const { return &m[0][0]; }
};

} // namespace ars::core::math