#include "gui/MainWindow.h"
#include "gui/ControlPanel.h"
#include "gui/Layout.h"
#include "gui/Viewport.h"
#include "render/Renderer.h"
#include "render/Window.h"
#include "utils/Logger.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>

namespace ars::gui {

// Автовращение — режим только 3D-фракталов (0..3). Для «пейзажа»/«побережья»
// (перекрытие типов 4/5, где камера закреплена своим видом) вращение не
// применяется никогда, даже если флаг autoRotate попал в параметры из
// config.json/пресета (доп. страховка к sanitizeParams).
static bool isAutoRotateSupported(core::fractal::FractalType type) {
    return type != core::fractal::FractalType::Terrain3D &&
           type != core::fractal::FractalType::Coast3D;
}

MainWindow::MainWindow(render::Window& window) : window_(&window) {}

void MainWindow::setCameraView(const glm::vec3& eye, const glm::vec3& aim) {
    if (renderer_) renderer_->camera().setView(eye, aim);
}

void MainWindow::setCameraFov(float fovDeg) {
    if (renderer_) renderer_->camera().setFov(fovDeg);
}

bool MainWindow::renderScreenshot(std::vector<std::uint8_t>& pixels,
                                  int& width, int& height) {
    if (!renderer_) return false;
    // Снимок рендерим в полном разрешении: адаптивный масштаб в runFrame
    // на первом кадре режет буфер до ~200k пикселей, что не нужно для снимка.
    motionScale_ = 0.0f;
    renderer_->setRenderScale(1.0f);
    if (!renderer_->renderFrame(params_, 0.0f)) return false;
    if (!renderer_->captureFrame(pixels)) return false;
    width = renderer_->width();
    height = renderer_->height();
    return true;
}

MainWindow::~MainWindow() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

bool MainWindow::init() {
    renderer_ = std::make_unique<render::Renderer>(*window_);
    if (!renderer_->init()) {
        utils::Logger::instance().error("Рендерер: не удалось инициализировать");
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // не создавать imgui.ini

    // ImGui-шрифт по умолчанию не содержит кириллицы => загружаем системный
    // шрифт с кириллицей (Windows), при неудаче остаётся шрифт по умолчанию.
    {
        const char* kFallbackFonts[] = {
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/tahoma.ttf",
            "C:/Windows/Fonts/consola.ttf",
        };
        ImFontConfig cfg;
        cfg.SizePixels = 16.0f;
        const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();
        for (const char* path : kFallbackFonts) {
            ImFont* f = io.Fonts->AddFontFromFileTTF(path, cfg.SizePixels, &cfg, ranges);
            if (f) break;
        }
    }

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 2.0f;
    style.FramePadding = ImVec2(6, 4);

    if (!ImGui_ImplGlfw_InitForOpenGL(window_->handle(), true)) {
        std::cerr << "[MainWindow] ImGui_ImplGlfw_InitForOpenGL failed\n";
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        std::cerr << "[MainWindow] ImGui_ImplOpenGL3_Init failed\n";
        return false;
    }

    controlPanel_ = std::make_unique<ControlPanel>(params_, renderer_->camera(),
                                                    &renderScale_);
    viewport_ = std::make_unique<Viewport>(*renderer_, params_);

    utils::Logger::instance().info("Приложение инициализировано");
    return true;
}

void MainWindow::runFrame(float deltaTime) {
    handleInput();

    // --- автовращение ---
    if (params_.autoRotate && isAutoRotateSupported(params_.type)) {
        renderer_->camera().orbit(deltaTime * 10.0f, 0.0f);
    }

    // --- адаптивное разрешение ---
    const bool animatedScene =
        params_.type == core::fractal::FractalType::Coast3D ||
        (params_.type == core::fractal::FractalType::Terrain3D &&
         params_.cloudDensity > 0.0f);
    const bool userInteraction =
         params_.autoRotate ||
         renderer_->camera().revision() != lastCameraRev_ ||
         !(params_ == lastParams_);
    lastCameraRev_ = renderer_->camera().revision();
    lastParams_ = params_;

    // Адаптивное разрешение применяется как для интерактивного движения,
    // так и для фоновой анимации (разные целевые времена кадра).
    const bool needsAdaptiveScale = userInteraction || animatedScene;
    float effectiveScale = renderScale_;
    if (needsAdaptiveScale) {
        // Интерактивное движение: 20-23 мс (45+ FPS).
        // Фоновая анимация: 30-35 мс (30+ FPS).
        const float kLowerMs = userInteraction ? 20.0f : 30.0f;
        const float kUpperMs = userInteraction ? 23.0f : 35.0f;

        if (motionScale_ <= 0.0f) {
            const int winW = renderer_->width() > 0 ? renderer_->width()
                                                    : window_->width();
            const int winH = renderer_->height() > 0 ? renderer_->height()
                                                     : window_->height();
            const float pxTarget = userInteraction ? 200000.0f : 400000.0f;
            motionScale_ = std::min(renderScale_,
                                     std::sqrt(pxTarget /
                                               std::max(1.0f,
                                                       static_cast<float>(winW) *
                                                           static_cast<float>(winH))));
            motionScale_ = std::max(0.125f, motionScale_);
        } else if (frameMs_ > kUpperMs) {
            const float ratio = std::sqrt(kUpperMs / std::max(frameMs_, 1.0f));
            motionScale_ *= std::max(0.85f, std::min(0.98f, ratio));
        } else if (frameMs_ > 0.0f && frameMs_ < kLowerMs) {
            motionScale_ *= 1.05f;
        }
        effectiveScale = std::max(0.125f, std::min(renderScale_, motionScale_));
        renderer_->setRenderScale(effectiveScale);
    } else {
        motionScale_ = 0.0f;
        renderer_->setRenderScale(renderScale_);
    }

    // --- рендер фрактала в offscreen FBO ---
    // Измеряем ЧИСТОЕ время рендера (glFinish до swap), без влияния vsync,
    // чтобы автоподбор масштаба не осциллировал от ожидания обновления.
    const float t = static_cast<float>(glfwGetTime());
    std::chrono::steady_clock::time_point renderStart;
    if (needsAdaptiveScale) renderStart = std::chrono::steady_clock::now();

    // Рендер обязателен также для неподвижной статичной сцены.
    const bool frameRendered = renderer_->renderFrame(params_, t);
    if (needsAdaptiveScale && frameRendered) {
        glFinish();
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - renderStart)
                              .count();
        // EMA: подавляем одиночные выбросы (первый кадр нового масштаба и т.п.)
        frameMs_ = frameMs_ <= 0.0 ? static_cast<float>(ms)
                                   : frameMs_ * 0.8f + static_cast<float>(ms) * 0.2f;
    }

    // --- ImGui ---
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    controlPanel_->draw();
    viewport_->draw();

    // --- статус-бар ---
    {
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
        ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - kStatusBarHeight));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, kStatusBarHeight));
        ImGui::Begin("Статус", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoScrollWithMouse |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::PopStyleVar();
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();
        const auto& entries = utils::Logger::instance().entries();
        if (!entries.empty()) {
            ImGui::TextUnformatted(entries.back().c_str());
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // FPS в заголовке окна (обновляем ~2 раза в секунду)
    fpsAccumSec_ += deltaTime;
    const double now = glfwGetTime();
    if (now - lastTitleUpdate_ > 0.5) {
        char title[128];
        std::snprintf(title, sizeof(title),
                      "ars_fracta — FPS: %.0f, frame: %.1f ms, bufsz %dx%d, res x%.2f",
                      static_cast<double>(ImGui::GetIO().Framerate),
                      static_cast<double>(frameMs_),
                      renderer_->width(), renderer_->height(),
                      static_cast<double>(renderer_->renderScale()));
        glfwSetWindowTitle(window_->handle(), title);
        lastTitleUpdate_ = now;
    }
}

void MainWindow::handleInput() {
    GLFWwindow* w = window_->handle();
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    const float speed = 0.05f;
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) renderer_->camera().moveForward(speed);
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) renderer_->camera().moveForward(-speed);
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) renderer_->camera().moveRight(-speed);
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) renderer_->camera().moveRight(speed);
    if (glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS) renderer_->camera().moveUp(-speed);
    if (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS) renderer_->camera().moveUp(speed);
}

} // namespace ars::gui
