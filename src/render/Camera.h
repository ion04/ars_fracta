#pragma once

#include <glm/glm.hpp>

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
    void zoom(float factor);                            // factor > 0 — приближение
    void pan(const glm::vec2& ndcOffset);
    void moveForward(float amount);
    void moveRight(float amount);
    void moveUp(float amount);

    void reset();

    /// Точная установка камеры (eye/target) — используется для просмотра пейзажей.
    void setView(const glm::vec3& eye, const glm::vec3& aim);
    void setFov(float fovDeg);
    void setZoomLimits(float minDist, float maxDist);

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

    // Сглаживание мышиного ввода (EMA для устранения рывков)
    glm::vec2 smoothDelta_{0.0f, 0.0f};
    float smoothAlpha_ = 0.2f;  // 0.2 = плавно, 0.3 = резче
};

} // namespace ars::render