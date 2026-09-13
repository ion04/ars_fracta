#include "core/fractal/Mandelbulb3D.h"

#include <algorithm>
#include <cmath>

namespace ars::core::fractal {

double Mandelbulb3D::distanceEstimate(const math::Vector3& p, double power, int iterations) {
    using math::Vector3;
    Vector3 z = p;
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
                    zr * std::cos(th)) + p;
    }

    r = std::max(r, 1e-12);
    return 0.5 * std::log(r) * r / dr;
}

FractalParams Mandelbulb3D::sanitize(const FractalParams& p) const {
    FractalParams out = p;
    out.type = FractalType::Mandelbulb3D;
    out.iterations = std::clamp(p.iterations, 8, 256);
    out.power = std::clamp(p.power, 2.0f, 32.0f);
    out.bailout = std::max(p.bailout, 2.0f);
    out.detail = std::clamp(p.detail, 0.0005f, 0.02f);
    return out;
}

} // namespace ars::core::fractal