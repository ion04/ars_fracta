#include "render/Camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace ars::render {

Camera::Camera() { updateVectors(); }

void Camera::updateVectors() {
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    front_ = glm::normalize(target_ - position_);
    right_ = glm::cross(front_, worldUp);
    const float len = glm::length(right_);
    right_ = (len > 1e-5f) ? right_ / len : glm::vec3(1.0f, 0.0f, 0.0f);
    up_ = glm::normalize(glm::cross(right_, front_));
}

void Camera::orbit(float dxDeg, float dyDeg) {
    glm::vec3 dir = position_ - target_;
    const float dist = glm::length(dir);
    if (dist < 1e-6f) return;
    dir /= dist;

    const float yaw = std::atan2(dir.x, dir.z) + glm::radians(dxDeg);
    const float pitch = glm::clamp(
        std::asin(glm::clamp(dir.y, -1.0f, 1.0f)) + glm::radians(dyDeg),
        glm::radians(-89.0f), glm::radians(89.0f));

    dir = glm::normalize(glm::vec3(std::cos(pitch) * std::sin(yaw),
                                   std::sin(pitch),
                                   std::cos(pitch) * std::cos(yaw)));
    position_ = target_ + dir * dist;
    ++revision_;
    updateVectors();
}

void Camera::zoom(float factor) {
    const glm::vec3 to = position_ - target_;
    const float dist = glm::length(to);
    if (dist < 1e-6f) return;
    const float newDist = glm::clamp(dist * (1.0f - factor), 0.05f, 100.0f);
    position_ = target_ + to * (newDist / dist);
    ++revision_;
    updateVectors();
}

void Camera::pan(const glm::vec2& ndcOffset, float /*planeDepth*/) {
    const float dist = glm::length(position_ - target_);
    const float k = dist * 0.02f;
    const glm::vec3 delta = right_ * (-ndcOffset.x * k) + up_ * (ndcOffset.y * k);
    target_ += delta;
    position_ += delta;
    ++revision_;
    updateVectors();
}

void Camera::moveForward(float amount) {
    const glm::vec3 d = front_ * amount;
    position_ += d;
    target_ += d;
    ++revision_;
}

void Camera::moveRight(float amount) {
    const glm::vec3 d = right_ * amount;
    position_ += d;
    target_ += d;
    ++revision_;
}

void Camera::moveUp(float amount) {
    const glm::vec3 d = up_ * amount;
    position_ += d;
    target_ += d;
    ++revision_;
}

void Camera::reset() {
    position_ = glm::vec3(0.0f, 0.0f, -3.0f);
    target_ = glm::vec3(0.0f);
    fovDeg_ = 60.0f;
    ++revision_;
    updateVectors();
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position_, target_, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::projectionMatrix(float aspect) const {
    return glm::perspective(glm::radians(fovDeg_), aspect, 0.05f, 1000.0f);
}

ViewBasis Camera::basis() const {
    ViewBasis b;
    b.position = position_;
    b.right = right_;
    b.up = up_;
    b.forward = front_;
    b.fovDeg = fovDeg_;
    b.aspect = aspect_;
    return b;
}

} // namespace ars::render