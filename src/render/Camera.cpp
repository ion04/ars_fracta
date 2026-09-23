#include "render/Camera.h"

#include <algorithm>
#include <cmath>

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
    // Применяем экспоненциальное сглаживание (EMA)
    smoothDelta_ = glm::mix(smoothDelta_, glm::vec2(dxDeg, dyDeg), smoothAlpha_);

    glm::vec3 dir = position_ - target_;
    const float dist = glm::length(dir);
    if (dist < 1e-6f) return;
    dir /= dist;

    const float yaw = std::atan2(dir.x, dir.z) + glm::radians(smoothDelta_.x);
    const float pitch = glm::clamp(
        std::asin(glm::clamp(dir.y, -1.0f, 1.0f)) + glm::radians(smoothDelta_.y),
        glm::radians(-89.0f), glm::radians(89.0f));

    dir = glm::normalize(glm::vec3(std::cos(pitch) * std::sin(yaw),
                                   std::sin(pitch),
                                   std::cos(pitch) * std::cos(yaw)));
    position_ = target_ + dir * dist;
    clampToMinHeight();
    ++revision_;
    updateVectors();
}

void Camera::zoom(float factor) {
    const glm::vec3 to = position_ - target_;
    const float dist = glm::length(to);
    if (dist < 1e-6f) return;
    const float newDist = glm::clamp(dist * (1.0f - factor), minDistance_, maxDistance_);
    position_ = target_ + to * (newDist / dist);
    clampToMinHeight();
    ++revision_;
    updateVectors();
}

void Camera::pan(const glm::vec2& ndcOffset) {
    const float dist = glm::length(position_ - target_);
    const float k = dist * 0.02f;
    const glm::vec3 delta = right_ * (-ndcOffset.x * k) + up_ * (ndcOffset.y * k);
    target_ += delta;
    position_ += delta;
    clampToMinHeight();
    ++revision_;
    updateVectors();
}

void Camera::moveForward(float amount) {
    const glm::vec3 d = front_ * amount;
    position_ += d;
    target_ += d;
    clampToMinHeight();
    ++revision_;
}

void Camera::moveRight(float amount) {
    const glm::vec3 d = right_ * amount;
    position_ += d;
    target_ += d;
    clampToMinHeight();
    ++revision_;
}

void Camera::moveUp(float amount) {
    const glm::vec3 d = up_ * amount;
    position_ += d;
    target_ += d;
    clampToMinHeight();
    ++revision_;
}

void Camera::reset() {
    position_ = glm::vec3(0.0f, 0.0f, -3.0f);
    target_ = glm::vec3(0.0f);
    fovDeg_ = 60.0f;
    ++revision_;
    updateVectors();
}

void Camera::setView(const glm::vec3& eye, const glm::vec3& aim) {
    position_ = eye;
    target_ = aim;
    ++revision_;
    updateVectors();
}

void Camera::setFov(float fovDeg) {
    fovDeg_ = fovDeg;
    ++revision_;
}

void Camera::setZoomLimits(float minDist, float maxDist) {
    // Валидация: нормализуем некорректный диапазон (min <= max, >= 0)
    if (minDist > maxDist) std::swap(minDist, maxDist);
    minDistance_ = std::max(minDist, 0.0f);
    maxDistance_ = std::max(maxDist, 0.0f);
    // Применяем ограничения к текущей дистанции
    const glm::vec3 to = position_ - target_;
    const float dist = glm::length(to);
    if (dist > 1e-6f) {
        const float clampedDist = glm::clamp(dist, minDistance_, maxDistance_);
        if (std::abs(clampedDist - dist) > 1e-6f) {
            position_ = target_ + to * (clampedDist / dist);
            ++revision_;
        }
    }
}

void Camera::setMinHeight(float minHeight) {
    minHeight_ = minHeight;
    clampToMinHeight();
}

void Camera::resetSmoothing() {
    smoothDelta_ = glm::vec2(0.0f, 0.0f);
}

void Camera::clampToMinHeight() {
    const float clampedY = glm::max(position_.y, minHeight_);
    if (std::abs(clampedY - position_.y) > 1e-6f) {
        position_.y = clampedY;
        ++revision_;
        updateVectors();
    }
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