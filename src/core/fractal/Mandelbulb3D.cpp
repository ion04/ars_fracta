#include "core/fractal/Mandelbulb3D.h"

#include <algorithm>
#include <cmath>

#include "core/fractal/SphericalDistance.h"

namespace ars::core::fractal {

double Mandelbulb3D::distanceEstimate(const math::Vector3& p, double power, int iterations) {
    // Мандельбульб: z_{n+1} = z^n + p, старт из p.
    return sphericalDistanceEstimate(p, p, power, iterations);
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