#include "core/fractal/Mandelbrot2D.h"

#include <algorithm>
#include <cmath>

namespace ars::core::fractal {

int Mandelbrot2D::escapeTime(const math::Complex& c, int maxIter, double escapeRadius) {
    math::Complex z;
    const double r2 = escapeRadius * escapeRadius;
    for (int i = 0; i < maxIter; ++i) {
        z = z.squared() + c;
        if (z.magnitudeSquared() > r2) return i;
    }
    return maxIter;
}

double Mandelbrot2D::smoothEscapeTime(const math::Complex& c, int maxIter,
                                      double escapeRadius) {
    math::Complex z;
    const double r2 = escapeRadius * escapeRadius;
    for (int i = 0; i < maxIter; ++i) {
        z = z.squared() + c;
        if (z.magnitudeSquared() > r2) {
            // nu = log2(log2(|z|))
            const double nu = std::log2(std::log(z.magnitudeSquared()) * 0.5);
            return static_cast<double>(i) + 1.0 - nu;
        }
    }
    return static_cast<double>(maxIter);
}

FractalParams Mandelbrot2D::sanitize(const FractalParams& p) const {
    FractalParams out = p;
    out.type = FractalType::Mandelbrot2D;
    out.iterations = std::clamp(p.iterations, 16, 2000);
    out.bailout = std::max(p.bailout, 2.0f);
    out.m2dZoom = std::max(p.m2dZoom, 0.0001f);
    return out;
}

} // namespace ars::core::fractal