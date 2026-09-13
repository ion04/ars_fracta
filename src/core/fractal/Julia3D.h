#pragma once

#include "core/fractal/Fractal.h"
#include "core/math/Vector3.h"

namespace ars::core::fractal {

/**
 * @brief 3D-фрактал Жюлиа (обобщение комплексного множества Жюлиа в 3D).
 *
 * Параметр c задаётся интерактивно в панели управления.
 */
class Julia3D final : public Fractal {
public:
    Julia3D() {
        params_.type = FractalType::Julia3D;
        params_.iterations = 48;
        params_.power = 8.0f;
        params_.juliaReal = -0.70176f;
        params_.juliaImag = 0.2742f;
    }

    /// Distance estimation: итерации z_{n+1} = z_n^n + c, старт из точки p.
    static double distanceEstimate(const math::Vector3& p, const math::Vector3& c,
                                   double power, int iterations);

    /// Текущая константа c из параметров.
    math::Vector3 constant() const {
        return {params_.juliaReal, params_.juliaImag, params_.juliaImag3D};
    }

    const char* name() const noexcept override { return "Julia 3D"; }
    bool is3D() const noexcept override { return true; }
    FractalType type() const noexcept override { return FractalType::Julia3D; }

protected:
    FractalParams sanitize(const FractalParams& p) const override;
};

} // namespace ars::core::fractal