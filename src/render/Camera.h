#pragma once

#include <glm/glm.hpp>

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

    void orbit(float dxDeg, float dyDeg);              // вращение вокруг цели
    void zoom(float factor);                            // factor > 0 — приближение
    void pan(const glm::vec2& ndcOffset, float planeDepth = 5.0f);
    void moveForward(float amount);
    void moveRight(float amount);
    void moveUp(float amount);

    void reset();

    /// Точная установка камеры (eye/target) — используется для просмотра пейзажей.
    void setView(const glm::vec3& eye, const glm::vec3& aim);
    void setFov(float fovDeg);

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspect) const;

    void setAspect(float aspect) { aspect_ = aspect; }

    const glm::vec3& position() const { return position_; }
    const glm::vec3& target() const { return target_; }

    /// Увеличивается при каждом изменении кадра камеры (для selective redraw).
    uint64_t revision() const { return revision_; }

    ViewBasis basis() const;

private:
    void updateVectors();

    uint64_t revision_ = 1;

    glm::vec3 position_{0.0f, 0.0f, -3.0f};
    glm::vec3 target_{0.0f, 0.0f, 0.0f};
    glm::vec3 front_{0.0f, 0.0f, 1.0f};
    glm::vec3 right_{1.0f, 0.0f, 0.0f};
    glm::vec3 up_{0.0f, 1.0f, 0.0f};
    float fovDeg_ = 60.0f;
    float aspect_ = 1.0f;
};

} // namespace ars::render