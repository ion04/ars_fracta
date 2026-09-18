#pragma once

#include <algorithm>
#include <cmath>

#include "core/math/Vector3.h"

namespace ars::core::fractal {

/**
 * @brief Distance estimation для 3D-итераций z_{n+1} = z^power + add.
 *
 * Общий цикл Мандельбульба и Жюлиа (3D-степень в сферических координатах).
 * Для Мандельбульба add == start (p), для Жюлиа add == константа c.
 * Возвращает приближённое расстояние от start до поверхности.
 */
inline double sphericalDistanceEstimate(const math::Vector3& start,
                                        const math::Vector3& add,
                                        double power, int iterations) {
    using math::Vector3;
    Vector3 z = start;
    double dr = 1.0;
    double r = 0.0;

    for (int i = 0; i < iterations; ++i) {
        r = z.length();
        if (r > 2.0) break;
        if (r < 1e-12) break;

        const double theta = std::acos(z.z / r);
        const double phi = std::atan2(z.y, z.x);

        dr = std::pow(r, power - 1.0) * power * dr + 1.0;
        const double zr = std::pow(r, power);
        const double th = theta * power;
        const double ph = phi * power;

        z = Vector3(zr * std::sin(th) * std::cos(ph),
                    zr * std::sin(th) * std::sin(ph),
                    zr * std::cos(th)) + add;
    }

    r = std::max(r, 1e-12);
    return 0.5 * std::log(r) * r / dr;
}

} // namespace ars::core::fractal