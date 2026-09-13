#pragma once

#include "core/fractal/Fractal.h"
#include "core/math/Vector3.h"

namespace ars::core::fractal {

/**
 * @brief 3D-фрактал Мандельбульб (обобщение множества Мандельброта в 3D).
 *
 * Поверхность задаётся неявно: distance estimation f(p) >= 0 внутри, влияние
 * знака — стандартное для ray marching.
 */
class Mandelbulb3D final : public Fractal {
public:
    Mandelbulb3D() {
        params_.type = FractalType::Mandelbulb3D;
        params_.iterations = 48;
        params_.power = 8.0f;
    }

    /// Distance estimation: приближённое расстояние от точки p до поверхности.
    static double distanceEstimate(const math::Vector3& p, double power, int iterations);

    const char* name() const noexcept override { return "Mandelbulb 3D"; }
    bool is3D() const noexcept override { return true; }
    FractalType type() const noexcept override { return FractalType::Mandelbulb3D; }

protected:
    FractalParams sanitize(const FractalParams& p) const override;
};

} // namespace ars::core::fractal