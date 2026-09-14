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
}

bool Renderer::init() {
    const std::string assets = ARS_FRACTA_ASSETS_DIR;
    if (!shader_.loadFromFiles(assets + "/shaders/fractal.vert",
                               assets + "/shaders/fractal.frag")) {
        std::cerr << "[Renderer] Cannot load shaders (assets dir: " << assets << ")\n";
        return false;
    }

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
    return true;
}

void Renderer::ensureFramebuffer(int w, int h) {
    if (w <= 0 || h <= 0) return;
    // избегаем пересоздания FBO при мелких изменениях (осцилляции масштаба):
    // оставляем существующий буфер, рендер просто пройдёт в его размере.
    if (fbo_ && std::abs(w - width_) < 8 && std::abs(h - height_) < 8) return;
    if (fbo_ && w == width_ && h == height_) return;

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
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Renderer::renderFrame(const core::fractal::FractalParams& params, float timeSec) {
    const int targetW = static_cast<int>(window_->width() * renderScale_);
    const int targetH = static_cast<int>(window_->height() * renderScale_);
    ensureFramebuffer(targetW, targetH);

    const uint64_t camRev = camera_.revision();
    const bool stateChanged =
        !lastRendered_ || params != lastParams_ || camRev != lastCameraRev_ ||
        targetW != lastW_ || targetH != lastH_;
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
    shader_.setFloat("uTime", timeSec);
    shader_.setVec3("uCamPos", b.position);
    shader_.setVec3("uCamRight", b.right);
    shader_.setVec3("uCamUp", b.up);
    shader_.setVec3("uCamForward", b.forward);
    shader_.setFloat("uFov", b.fovDeg);

    shader_.setInt("uFractalType", static_cast<int>(params.type));
    // Детальность DE привязана к масштабу: на малом разрешении тонкие
    // структуры всё равно не видны, поэтому гоняем меньше итераций
    // (для 2D-фрактала это не применяем — там строгая формула).
    int iters = params.iterations;
    // Террейн не использует марш DE — шаги не нужны; ставим минимум.
    const core::fractal::FractalType ft = params.type;
    if (ft == core::fractal::FractalType::Terrain3D) {
        iters = params.iterations;
    } else if (ft == core::fractal::FractalType::MengerSponge) {
        // Menger сходится быстро: >12 итераций уже не меняют картинку,
        // но каждый лишний цикл тянет марш и нормали (до 5× DE на пиксель).
        iters = std::min(iters, 12);
    }
    if (ft != core::fractal::FractalType::Mandelbrot2D &&
        ft != core::fractal::FractalType::Terrain3D) {
        // Для ветвистых Mandelbulb/Julia режем итерации мягче, чем для
        // Menger: слишком сильное сокращение «оголяет» тонкие структуры.
        const float factor = (ft == core::fractal::FractalType::MengerSponge)
            ? 0.40f + 0.60f * renderScale_   // Menger: агрессивно
            : 0.65f + 0.35f * renderScale_;  // Bulb/Julia: мягко
        iters = static_cast<int>(iters * factor);
        iters = std::max(3, iters);
    }
    shader_.setInt("uIterations", iters);
    shader_.setFloat("uBailout", params.bailout);
    shader_.setFloat("uPower", params.power);
    shader_.setVec3("uJuliaC", glm::vec3(params.juliaReal, params.juliaImag,
                                        params.juliaImag3D));
    shader_.setFloat("uDetail", params.detail);
    shader_.setFloat("uColorScale", params.colorScale);
    shader_.setFloat("uHueShift", params.hueShift);
    shader_.setInt("uColorMode", params.colorMode);
    shader_.setFloat("uM2dZoom", params.m2dZoom);
    shader_.setFloat("uTerrainAmplitude", params.terrainAmplitude);
    shader_.setFloat("uTerrainFrequency", params.terrainFrequency);
    shader_.setFloat("uCloudDensity", params.cloudDensity);
    shader_.setFloat("uTreeDensity", params.treeDensity);
    shader_.setFloat("uTimeOfDay", params.timeOfDay);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

} // namespace ars::render