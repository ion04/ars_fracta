#pragma once

#include "core/fractal/Fractal.h"
#include "core/math/Vector3.h"

namespace ars::core::fractal {

/**
 * @brief 3D-фрактал «Губка Менгера» (Menger Sponge).
 */
class MengerSponge final : public Fractal {
public:
    MengerSponge() {
        params_.type = FractalType::MengerSponge;
        params_.iterations = 4;
        params_.bailout = 2.0f;
    }

    /// Distance estimation для губки Менгера.
    static double distanceEstimate(const math::Vector3& p, int iterations);

    const char* name() const noexcept override { return "Menger Sponge"; }
    bool is3D() const noexcept override { return true; }
    FractalType type() const noexcept override { return FractalType::MengerSponge; }

protected:
    FractalParams sanitize(const FractalParams& p) const override;
};

} // namespace ars::core::fractal