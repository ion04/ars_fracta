#pragma once

#include "core/fractal/Fractal.h"
#include "core/math/Complex.h"

namespace ars::core::fractal {

/**
 * @brief Плоский фрактал Мандельброта (2D).
 */
class Mandelbrot2D final : public Fractal {
public:
    Mandelbrot2D() {
        params_.type = FractalType::Mandelbrot2D;
        params_.iterations = 255;
    }

    /// Классический escape-time алгоритм: номер итерации, на которой точка уходит за радиус.
    static int escapeTime(const math::Complex& c, int maxIter = 255, double escapeRadius = 2.0);

    /// Сглаженный (дробный) escape-time для красивого градиента.
    static double smoothEscapeTime(const math::Complex& c, int maxIter = 255,
                                   double escapeRadius = 2.0);

    const char* name() const noexcept override { return "Mandelbrot 2D"; }
    bool is3D() const noexcept override { return false; }
    FractalType type() const noexcept override { return FractalType::Mandelbrot2D; }

protected:
    FractalParams sanitize(const FractalParams& p) const override;
};

} // namespace ars::core::fractal