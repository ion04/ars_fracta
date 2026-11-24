#include "render/Renderer.h"

#include "render/Window.h"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>

namespace ars::render {

Renderer::Renderer(Window& window) : window_(&window) {}

Renderer::~Renderer() {
    if (quadVAO_) glDeleteVertexArrays(1, &quadVAO_);
    if (quadVBO_) glDeleteBuffers(1, &quadVBO_);
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (colorTex_) glDeleteTextures(1, &colorTex_);
    if (depthRbo_) glDeleteRenderbuffers(1, &depthRbo_);
    if (ubo_) glDeleteBuffers(1, &ubo_);
}

bool Renderer::init() {
    const std::string assets = ARS_FRACTA_ASSETS_DIR;
    if (!shader_.loadFromFiles(assets + "/shaders/fractal.vert",
                               assets + "/shaders/fractal.frag")) {
        std::cerr << "[Renderer] Cannot load shaders (assets dir: " << assets << ")\n";
        return false;
    }

    // Create and bind UBO for fractal parameters
    if (ubo_ == 0) {
        glGenBuffers(1, &ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(FractalParamsUBO), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    // Bind uniform block to binding point 0 (matches shader layout)
    shader_.use();
    shader_.bindUniformBlock("FractalParamsUBO", 0);

    if (quadVAO_ == 0) {
        // fullscreen triangle (покрывает весь экран без индексного буфера)
        const float verts[] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
        glGenVertexArrays(1, &quadVAO_);
        glGenBuffers(1, &quadVBO_);
        glBindVertexArray(quadVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glBindVertexArray(0);
    }

    ensureFramebuffer(window_->width(), window_->height());
    if (!fbo_ || width_ <= 0 || height_ <= 0) {
        std::cerr << "[Renderer] Cannot create framebuffer (size "
                  << width_ << "x" << height_ << ")\n";
        return false;
    }
    return true;
}

void Renderer::ensureFramebuffer(int w, int h) {
    if (w <= 0 || h <= 0) return;
    // избегаем пересоздания FBO при мелких изменениях (осцилляции масштаба):
    // оставляем существующий буфер, рендер просто пройдёт в его размере.
    if (fbo_ && std::abs(w - width_) < 8 && std::abs(h - height_) < 8) return;

    width_ = w;
    height_ = h;

    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        glDeleteTextures(1, &colorTex_);
        glDeleteRenderbuffers(1, &depthRbo_);
    }

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &colorTex_);
    glBindTexture(GL_TEXTURE_2D, colorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);

    glGenRenderbuffers(1, &depthRbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo_);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[Renderer] Framebuffer incomplete: 0x" << std::hex << status << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo_);
        glDeleteTextures(1, &colorTex_);
        glDeleteRenderbuffers(1, &depthRbo_);
        fbo_ = 0;
        colorTex_ = 0;
        depthRbo_ = 0;
        width_ = 0;
        height_ = 0;
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Renderer::renderFrame(const core::fractal::FractalParams& params, float timeSec) {
    const float terrainQuality = params.type == core::fractal::FractalType::Terrain3D
        ? 1.15f : 1.0f;
    const int targetW = static_cast<int>(window_->width() * renderScale_ * terrainQuality);
    const int targetH = static_cast<int>(window_->height() * renderScale_ * terrainQuality);
    ensureFramebuffer(targetW, targetH);

    const uint64_t camRev = camera_.revision();
    const bool animated =
        params.type == core::fractal::FractalType::Coast3D ||
        (params.type == core::fractal::FractalType::Terrain3D &&
         params.cloudDensity > 0.0f);
    const bool stateChanged =
        !lastRendered_ || params != lastParams_ || camRev != lastCameraRev_ ||
        targetW != lastW_ || targetH != lastH_ || animated;
    lastParams_ = params;
    lastCameraRev_ = camRev;
    lastW_ = targetW;
    lastH_ = targetH;
    lastRendered_ = true;

    // сцена не изменилась — кадр уже в FBO, не пересчитываем
    if (!stateChanged) return false;

    camera_.setAspect(static_cast<float>(width_) / static_cast<float>(height_));
    const ViewBasis b = camera_.basis();

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.06f, 0.06f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

shader_.use();
    shader_.setVec2("uResolution", glm::vec2(static_cast<float>(width_),
                                             static_cast<float>(height_)));
    shader_.setVec3("uCamPos", b.position);
    shader_.setVec3("uCamRight", b.right);
    shader_.setVec3("uCamUp", b.up);
    shader_.setVec3("uCamForward", b.forward);
    shader_.setFloat("uFov", b.fovDeg);

    // --- Prepare UBO data ---
    int iters = params.iterations;
    const core::fractal::FractalType ft = params.type;
    if (ft == core::fractal::FractalType::Terrain3D) {
        iters = params.iterations;
    } else if (ft == core::fractal::FractalType::MengerSponge) {
        iters = std::min(iters, 12);
    }
    if (ft != core::fractal::FractalType::Mandelbrot2D &&
        ft != core::fractal::FractalType::Terrain3D &&
        ft != core::fractal::FractalType::Coast3D) {
        const float factor = (ft == core::fractal::FractalType::MengerSponge)
            ? 0.40f + 0.60f * renderScale_
            : 0.65f + 0.35f * renderScale_;
        iters = static_cast<int>(iters * factor);
        iters = std::max(3, iters);
    }

    int maxSteps;
    switch (params.type) {
        case core::fractal::FractalType::MengerSponge:
            maxSteps = static_cast<int>(60 + 100 * renderScale_);
            break;
        case core::fractal::FractalType::Mandelbulb3D:
        case core::fractal::FractalType::Julia3D:
            maxSteps = static_cast<int>(140 + 160 * renderScale_);
            break;
        case core::fractal::FractalType::Terrain3D:
            maxSteps = static_cast<int>(64 + 90 * renderScale_);
            break;
        case core::fractal::FractalType::Coast3D:
            maxSteps = static_cast<int>(30 + 40 * renderScale_);
            break;
        default:
            maxSteps = static_cast<int>(100 + 100 * renderScale_);
            break;
    }

    FractalParamsUBO uboData{};
    uboData.uFractalType      = static_cast<int>(params.type);
    uboData.uIterations       = iters;
    uboData.uBailout          = params.bailout;
    uboData.uPower            = params.power;
    uboData.uJuliaC           = glm::vec3(params.juliaReal, params.juliaImag, params.juliaImag3D);
    uboData.uDetail           = params.detail;
    uboData.uColorScale       = params.colorScale;
    uboData.uHueShift         = params.hueShift;
    uboData.uColorMode        = params.colorMode;
    uboData.uM2dZoom          = params.m2dZoom;
    uboData.uMaxSteps         = maxSteps;
    uboData.uTime             = timeSec;
    uboData.uTerrainAmplitude = params.terrainAmplitude;
    uboData.uTerrainFrequency = params.terrainFrequency;
    uboData.uCloudDensity     = params.cloudDensity;
    uboData.uTreeDensity      = params.treeDensity;
    uboData.uTimeOfDay        = params.timeOfDay;
    uboData.uFogDensity       = params.fogDensity;
    uboData.uCoastCenter      = glm::vec2(params.coastCenterRe, params.coastCenterIm);

    // Upload UBO
    shader_.setUniformBuffer(ubo_, &uboData, sizeof(FractalParamsUBO));
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

bool Renderer::captureFrame(std::vector<std::uint8_t>& pixels) const {
    if (!fbo_ || width_ <= 0 || height_ <= 0) return false;

    pixels.resize(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4u);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    if (glGetError() != GL_NO_ERROR) {
        pixels.clear();
        return false;
    }

    // OpenGL's origin is bottom-left; image files conventionally use top-left.
    const std::size_t rowBytes = static_cast<std::size_t>(width_) * 4u;
    std::vector<std::uint8_t> row(rowBytes);
    for (int y = 0; y < height_ / 2; ++y) {
        auto* top = pixels.data() + static_cast<std::size_t>(y) * rowBytes;
        auto* bottom = pixels.data() + static_cast<std::size_t>(height_ - 1 - y) * rowBytes;
        std::copy(top, top + rowBytes, row.begin());
        std::copy(bottom, bottom + rowBytes, top);
        std::copy(row.begin(), row.end(), bottom);
    }
    return true;
}

} // namespace ars::render
