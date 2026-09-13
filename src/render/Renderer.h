#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <algorithm>

#include "core/fractal/Fractal.h"
#include "render/Camera.h"
#include "render/Shader.h"

namespace ars::render {

class Window;

/**
 * @brief Основной цикл рендеринга: рисует фрактал в offscreen-FBO (текстуру),
 * которую затем отображает Viewport (ImGui).
 */
class Renderer {
public:
    explicit Renderer(Window& window);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init();

    /// Рендер кадра. Если параметры, камера и размеры не изменились с прошлого
    /// вызова, кадр не пересчитывается (экономия GPU для статичной сцены).
    /// Возвращает true, если фрактал реально был отрисован в этом вызове.
    bool renderFrame(const core::fractal::FractalParams& params, float timeSec);

    /// Коэффициент масштаба внутреннего буфера (0.125..1.0). Меньше — быстрее.
    void setRenderScale(float scale) {
        renderScale_ = std::max(0.125f, std::min(1.0f, scale));
    }
    float renderScale() const { return renderScale_; }

    GLuint frameTexture() const { return colorTex_; }
    Camera& camera() { return camera_; }
    const Camera& camera() const { return camera_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    void ensureFramebuffer(int w, int h);

    Window* window_ = nullptr;
    Shader shader_;
    Camera camera_;

    GLuint quadVAO_ = 0;
    GLuint quadVBO_ = 0;
    GLuint fbo_ = 0;
    GLuint colorTex_ = 0;
    GLuint depthRbo_ = 0;
    int width_ = 0;
    int height_ = 0;
    float renderScale_ = 1.0f;

    // кэш последнего отрендеренного состояния
    core::fractal::FractalParams lastParams_{};
    uint64_t lastCameraRev_ = 0;
    int lastW_ = 0;
    int lastH_ = 0;
    bool lastRendered_ = false;
};

} // namespace ars::render