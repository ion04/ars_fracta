#include "core/fractal/Fractal.h"

#include "core/fractal/Julia3D.h"
#include "core/fractal/Mandelbrot2D.h"
#include "core/fractal/Mandelbulb3D.h"
#include "core/fractal/MengerSponge.h"

namespace ars::core::fractal {

FractalParams sanitizeParams(const FractalParams& raw) {
    switch (raw.type) {
        case FractalType::Mandelbrot2D: {
            Mandelbrot2D fractal;
            fractal.apply(raw);
            return fractal.params();
        }
        case FractalType::Mandelbulb3D: {
            Mandelbulb3D fractal;
            fractal.apply(raw);
            return fractal.params();
        }
        case FractalType::MengerSponge: {
            MengerSponge fractal;
            fractal.apply(raw);
            return fractal.params();
        }
        case FractalType::Julia3D: {
            Julia3D fractal;
            fractal.apply(raw);
            return fractal.params();
        }
        default:
            // Terrain3D / Coast3D генерируются только шейдером, CPU-класса нет.
            return raw;
    }
}

} // namespace ars::core::fractal