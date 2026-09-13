#pragma once

#include <memory>
#include <string>

#include "core/fractal/Fractal.h"

struct GLFWwindow;

namespace ars::render {
class Renderer;
class Window;
}

namespace ars::gui {

class ControlPanel;
class Viewport;

/**
 * @brief Главное окно приложения: ImGui context + docking + основной цикл кадра.
 */
class MainWindow {
public:
    explicit MainWindow(render::Window& window);
    ~MainWindow();

    bool init();
    void runFrame(float deltaTime);

    const core::fractal::FractalParams& params() const { return params_; }
    void setParams(const core::fractal::FractalParams& params) { params_ = params; }

private:
    void handleInput();

    render::Window* window_ = nullptr;
    std::unique_ptr<render::Renderer> renderer_;
    std::unique_ptr<Viewport> viewport_;
    std::unique_ptr<ControlPanel> controlPanel_;
    core::fractal::FractalParams params_{};
    float renderScale_ = 1.0f;

    // адаптивное разрешение: пока сцена движется, рендерим в пониженном
    // разрешении (FPS при вращении/драге), при остановке — полное качество.
    // Масштаб подстраивается под целевое время кадра (см. runFrame).
    uint64_t lastCameraRev_ = 0;
    core::fractal::FractalParams lastParams_{};
    float motionScale_ = 0.0f;  // 0 = не инициализирован (сброшен в статике)
    double fpsAccumSec_ = 0.0;
    double lastTitleUpdate_ = 0.0;
    float frameMs_ = 0.0f;  // EMA чистого времени рендера FBO (без vsync)
};

} // namespace ars::gui