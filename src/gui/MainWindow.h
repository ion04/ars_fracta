#pragma once

#include <memory>
#include <vector>
#include <cstdint>

#include "core/fractal/Fractal.h"
#include "gui/ControlPanel.h"   // ControlPanel + AdaptiveSettings

struct GLFWwindow;

namespace ars::render {
class Renderer;
class Window;
}

namespace ars::gui {

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

    // стартовая камера по типу текущих параметров (пейзаж/побережье)
    void applyDefaultCamera();

    /// Отрисовать один кадр в полном разрешении и прочитать пиксели
    /// (для --screenshot; без адаптивного понижения масштаба).
    bool renderScreenshot(std::vector<std::uint8_t>& pixels,
                          int& width, int& height);

    /// Снимок из GUI (кнопка «Скриншот»): рендер в полном разрешении + запись PNG.
    void saveScreenshot();

private:
    void handleInput(float deltaTime);

    render::Window* window_ = nullptr;
    std::unique_ptr<render::Renderer> renderer_;
    std::unique_ptr<Viewport> viewport_;
    std::unique_ptr<ControlPanel> controlPanel_;
    core::fractal::FractalParams params_{};
    float renderScale_ = 1.0f;

    // флаги инициализации бэкендов ImGui: Shutdown вызываем только для тех,
    // что реально поднялись (иначе assert/UB в деструкторе).
    bool glfwInited_ = false;
    bool gl3Inited_ = false;

    // настройки адаптивного разрешения (редактируются на вкладке «Рендер»)
    AdaptiveSettings adaptive_;

    // адаптивное разрешение: пока сцена движется, рендерим в пониженном
    // разрешении (FPS при вращении/драге), при остановке — полное качество.
    // Масштаб подстраивается под целевое время кадра (см. runFrame).
    uint64_t lastCameraRev_ = 0;
    core::fractal::FractalParams lastParams_{};
    float motionScale_ = 0.0f;  // 0 = не инициализирован (сброшен в статике)
    double startTime_ = 0.0;  // начало отсчёта времени сцены (для uTime)
    float frameMs_ = 0.0f;  // EMA чистого времени рендера FBO (без vsync)
};

} // namespace ars::gui
