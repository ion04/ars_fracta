#include "core/fractal/Julia3D.h"

#include <algorithm>
#include <cmath>

#include "core/fractal/SphericalDistance.h"

namespace ars::core::fractal {

double Julia3D::distanceEstimate(const math::Vector3& p, const math::Vector3& c,
                                 double power, int iterations) {
    // Жюлиа: z_{n+1} = z^n + c, старт из p; c — постоянная Жюлиа.
    return sphericalDistanceEstimate(p, c, power, iterations);
}

FractalParams Julia3D::sanitize(const FractalParams& p) const {
    FractalParams out = p;
    out.type = FractalType::Julia3D;
    out.iterations = std::clamp(p.iterations, 8, 256);
    out.power = std::clamp(p.power, 2.0f, 32.0f);
    out.bailout = std::max(p.bailout, 2.0f);
    return out;
}

} // namespace ars::core::fractal