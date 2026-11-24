#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "core/fractal/Fractal.h"
#include "render/Camera.h"
#include "render/Shader.h"

namespace ars::render {

class Window;

// ---------------------------------------------------------------------------
//  UBO layout (std140) — must match shaders/inc/common.inc exactly
// ---------------------------------------------------------------------------
struct FractalParamsUBO {
    int   uFractalType;      // 0
    int   uIterations;       // 4
    float uBailout;          // 8
    float uPower;            // 12
    glm::vec3 uJuliaC;       // 16 (aligned to 16)
    float uDetail;           // 32
    float uColorScale;       // 36
    float uHueShift;         // 40
    int   uColorMode;        // 44
    float uM2dZoom;          // 48
    int   uMaxSteps;         // 52
    float uTime;             // 56

    // Terrain3D
    float uTerrainAmplitude; // 60
    float uTerrainFrequency; // 64
    float uCloudDensity;     // 68
    float uTreeDensity;      // 72
    float uTimeOfDay;        // 76
    float uFogDensity;       // 80

    // Coast3D
    glm::vec2 uCoastCenter;  // 84 (aligned to 8)

    // Total: 92 bytes, pad to 96 (multiple of 16)
    float _pad[2];           // 92-96 (was 1, need 2 for alignment)
};

static_assert(sizeof(FractalParamsUBO) == 96, "FractalParamsUBO must be 96 bytes (std140)");

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

    /// Read the rendered offscreen frame as top-to-bottom RGBA8 pixels.
    bool captureFrame(std::vector<std::uint8_t>& pixels) const;

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
    GLuint ubo_ = 0;          // uniform buffer for fractal params
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
