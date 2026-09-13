#include "core/fractal/MengerSponge.h"

#include <algorithm>
#include <cmath>

namespace ars::core::fractal {

namespace {

inline double min3(double a, double b, double c) { return std::min(a, std::min(b, c)); }

/// Положительное по модулю: x mod m в диапазоне [0, m).
inline double posMod(double x, double m) { return x - std::floor(x / m) * m; }

} // namespace

double MengerSponge::distanceEstimate(const math::Vector3& p, int iterations) {
    using math::Vector3;

    // sdBox куба [-1, 1]^3
    const Vector3 q = p.abs();
    double d = std::max(q.x, std::max(q.y, q.z)) - 1.0;

    double s = 1.0;
    for (int i = 0; i < iterations; ++i) {
        const Vector3 a(posMod(p.x * s, 2.0) - 1.0,
                        posMod(p.y * s, 2.0) - 1.0,
                        posMod(p.z * s, 2.0) - 1.0);
        s *= 3.0;
        const Vector3 r = (Vector3(1.0, 1.0, 1.0) - a.abs() * 3.0).abs();
        const double da = std::max(r.x, r.y);
        const double db = std::max(r.y, r.z);
        const double dc = std::max(r.z, r.x);
        const double c = (min3(da, db, dc) - 1.0) / s;
        d = std::max(d, c);
    }
    return d;
}

FractalParams MengerSponge::sanitize(const FractalParams& p) const {
    FractalParams out = p;
    out.type = FractalType::MengerSponge;
    out.iterations = std::clamp(p.iterations, 1, 16);
    out.power = 0.0f;
    return out;
}

} // namespace ars::core::fractal