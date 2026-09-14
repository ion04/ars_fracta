#pragma once

#include <string>

namespace ars::core::fractal {

/**
 * @brief Типы поддерживаемых фракталов (индексы совпадают с uFractalType в шейдере).
 */
enum class FractalType : int {
    Mandelbrot2D = 0,
    Mandelbulb3D = 1,
    MengerSponge = 2,
    Julia3D      = 3,
    Terrain3D    = 4,  // процедурный пейзаж: fBm-рельеф + фрактальные скалы
};

inline const char* toString(FractalType t) {
    switch (t) {
        case FractalType::Mandelbrot2D: return "Mandelbrot2D";
        case FractalType::Mandelbulb3D: return "Mandelbulb3D";
        case FractalType::MengerSponge: return "MengerSponge";
        case FractalType::Julia3D:      return "Julia3D";
        case FractalType::Terrain3D:    return "Terrain3D";
    }
    return "Unknown";
}

/**
 * @brief Набор параметров фрактала, передаваемых в шейдер как uniform.
 *
 * Единая структура для всех четырёх фракталов: поля, не применимые к текущему
 * типу, просто игнорируются шейдером.
 */
struct FractalParams {
    FractalType type = FractalType::Mandelbulb3D;
    int   iterations  = 48;
    float bailout     = 2.0f;
    float power       = 8.0f;
    float juliaReal   = -0.70176f;
    float juliaImag   = 0.2742f;
    float juliaImag3D = 0.0f;
    float detail      = 0.0025f;
    float colorScale  = 12.0f;
    float hueShift    = 0.0f;
    int   colorMode   = 0;
    float m2dZoom     = 1.0f;
    float terrainAmplitude = 6.0f;  // высота рельефа (Terrain3D)
    float terrainFrequency = 0.12f; // масштаб шума (Terrain3D)
    float cloudDensity      = 0.6f; // плотность облаков (Terrain3D)
    float treeDensity       = 0.5f; // плотность деревьев (Terrain3D)
    float timeOfDay         = 0.35f; // положение солнца (0..1, Terrain3D)
    bool  autoRotate  = false;

    bool operator==(const FractalParams& o) const {
        return type == o.type && iterations == o.iterations && bailout == o.bailout &&
               power == o.power && juliaReal == o.juliaReal && juliaImag == o.juliaImag &&
               juliaImag3D == o.juliaImag3D && detail == o.detail &&
               colorScale == o.colorScale && hueShift == o.hueShift &&
               colorMode == o.colorMode && m2dZoom == o.m2dZoom &&
               terrainAmplitude == o.terrainAmplitude &&
               terrainFrequency == o.terrainFrequency &&
               cloudDensity == o.cloudDensity && treeDensity == o.treeDensity &&
               timeOfDay == o.timeOfDay;
    }
    bool operator!=(const FractalParams& o) const { return !(*this == o); }
};

/**
 * @brief Абстрактный базовый класс фрактала.
 *
 * Класс, помимо общих параметров, предоставляет CPU-реализации итераций /
 * distance estimation для каждого фрактала (используются в метриках и тестах).
 */
class Fractal {
public:
    virtual ~Fractal() = default;

    virtual const char* name() const noexcept = 0;
    virtual bool is3D() const noexcept = 0;
    virtual FractalType type() const noexcept = 0;

    const FractalParams& params() const noexcept { return params_; }

    /// Применить параметры (с валидацией, специфичной для конкретного фрактала).
    void apply(const FractalParams& p) { params_ = sanitize(p); }

protected:
    virtual FractalParams sanitize(const FractalParams& p) const { return p; }

    FractalParams params_{};
};

} // namespace ars::core::fractal