#pragma once

#include "core/fractal/Fractal.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <limits>

namespace ars::render {

/// Базис камеры + вертикальный FOV — всё, что нужно шейдеру для ray marching.
struct ViewBasis {
    glm::vec3 position{0.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    float fovDeg = 60.0f;
    float aspect = 1.0f;
};

/**
 * @brief Орбитальная камера: вращение, панорама, зум, движение.
 */
class Camera {
public:
    Camera();

    /// Вращение вокруг цели (градусы). Дельта сглаживается EMA — без рывков
    /// при быстрых движениях мыши.
    void orbit(float dxDeg, float dyDeg);
    /// Изменяет дистанцию камеры. Для Terrain/Coast сохраняет глаз выше
    /// 1.2 амплитуды рельефа.
    void zoom(float factor, const core::fractal::FractalParams& fractalParams);
    void pan(const glm::vec2& ndcOffset);
    void moveForward(float amount);
    void moveRight(float amount);
    void moveUp(float amount);

    void reset();

    /// Точная установка камеры (eye/target) — используется для просмотра пейзажей.
    void setView(const glm::vec3& eye, const glm::vec3& aim);
    void setFov(float fovDeg);
    void setZoomLimits(float minDist, float maxDist);

    /// Вид сверху на начало координат с заданной высоты (без вырожденного lookAt).
    void topView(float height);

    /// Минимальная высота камеры (по Y) при интерактивной навигации.
    /// По умолчанию отключено (-inf) — свободная орбита для 3D-фракталов.
    void setMinHeight(float minHeight);

    /// Забывает остаток EMA-сглаживания при старте нового жеста мыши.
    void resetSmoothing();

    void setAspect(float aspect) { aspect_ = aspect; }

    const glm::vec3& position() const { return position_; }
    const glm::vec3& target() const { return target_; }

    /// Увеличивается при каждом изменении кадра камеры (для selective redraw).
    uint64_t revision() const { return revision_; }

    ViewBasis basis() const;

private:
    void updateVectors();
    void clampToMinHeight();

    uint64_t revision_ = 1;

    glm::vec3 position_{0.0f, 0.0f, -3.0f};
    glm::vec3 target_{0.0f, 0.0f, 0.0f};
    glm::vec3 front_{0.0f, 0.0f, 1.0f};
    glm::vec3 right_{1.0f, 0.0f, 0.0f};
    glm::vec3 up_{0.0f, 1.0f, 0.0f};
    float fovDeg_ = 60.0f;
    float aspect_ = 1.0f;

    float minDistance_ = 0.05f;
    float maxDistance_ = 100.0f;

    // Абсолютный пол по Y для камеры при пользовательской навигации
    // (Terrain/Coast — «не залезать под землю»). -inf = ограничение выключено.
    float minHeight_ = -std::numeric_limits<float>::infinity();

    // Независимые EMA-состояния для разных жестов мыши. Их нельзя смешивать:
    // единицы измерения орбиты, панорамирования и зума различаются.
    glm::vec2 smoothOrbitDelta_{0.0f, 0.0f};
    glm::vec2 smoothPanDelta_{0.0f, 0.0f};
    float smoothZoomDelta_ = 0.0f;
    static constexpr float kSmoothingAlpha = 0.2f;
};

// ---------------------------------------------------------------------------
//  Стартовые виды для «пейзажа»/«побережья».
//  Коэффициенты 1.45/1.55 — эмпирический запас камеры над рельефом, чтобы
//  стартовый вид не оказался под горами при разной амплитуде.
// ---------------------------------------------------------------------------
constexpr float kTerrainEyeMinY = 7.0f;
constexpr float kCoastEyeMinY   = 9.0f;
constexpr float kTerrainEyeFactor = 1.45f;
constexpr float kCoastEyeFactor   = 1.55f;
constexpr float kLandscapeFov = 52.0f;

inline void placeTerrainCamera(Camera& cam,
                               const core::fractal::FractalParams& p) {
    const float eyeY = std::max(kTerrainEyeMinY, p.terrainAmplitude * kTerrainEyeFactor);
    cam.setView(glm::vec3(10.0f, eyeY, -20.0f), glm::vec3(-1.5f, 2.2f, 10.0f));
    cam.setFov(kLandscapeFov);
}

inline void placeCoastCamera(Camera& cam,
                             const core::fractal::FractalParams& p) {
    const float eyeY = std::max(kCoastEyeMinY, p.terrainAmplitude * kCoastEyeFactor);
    cam.setView(glm::vec3(0.0f, eyeY, -22.0f), glm::vec3(0.0f, 1.0f, 7.0f));
    cam.setFov(kLandscapeFov);
}

} // namespace ars::render
