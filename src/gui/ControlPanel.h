#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/fractal/Fractal.h"

namespace ars::render {
class Camera;
}

namespace ars::gui {

// Настройки адаптивного разрешения; сама логика живёт в MainWindow::runFrame.
struct AdaptiveSettings {
    bool enabled = true;
    int  targetFps = 45;   // целевой FPS при движении камеры
};

/**
 * @brief Панель управления (вариант B: вкладки + закреплённый низ + панель камеры).
 *
 * Требования к FractalParams: bool operator==(const FractalParams&) const
 * (в C++20 достаточно `bool operator==(const FractalParams&) const = default;`).
 */
class ControlPanel {
public:
    ControlPanel(core::fractal::FractalParams& params, render::Camera& camera,
                 float* renderScale, AdaptiveSettings* adaptive = nullptr);

    // Реальный масштаб рендера этого кадра (для индикатора во вьюпорте).
    void setEffectiveScale(float s) { effectiveScale_ = s; }

    // Вызывается из кнопки «Скриншот»; сам снимок делает рендерер.
    void setScreenshotCallback(std::function<void()> cb) { onScreenshot_ = std::move(cb); }

    void draw();

private:
    // каркас
    void drawFractalTab();
    void drawColorTab();
    void drawCameraTab();
    void drawRenderTab();
    void drawFooter();
    void drawViewportToolbar();

    // логика
    void changeType(core::fractal::FractalType newType);
    void applyCameraFor(core::fractal::FractalType oldType,
                        core::fractal::FractalType newType, bool force);

    // пресеты
    void loadPresetsIntoList();
    void applyPreset(int index);
    bool savePreset(const std::string& label);

    core::fractal::FractalParams* params_ = nullptr;
    render::Camera* camera_ = nullptr;
    float* renderScale_ = nullptr;
    std::function<void()> onScreenshot_;

    int typeIndex_ = 1;
    int colorModeIdx_ = 0;

    AdaptiveSettings* adaptive_ = nullptr;
    float effectiveScale_ = 1.0f;

    // пресеты
    std::vector<std::string> presetFiles_;
    std::vector<std::string> presetLabels_;
    std::vector<core::fractal::FractalType> presetTypes_;
    int  presetIndex_ = -1;
    bool presetsLoaded_ = false;
    bool presetActive_ = false;                 // параметры были взяты из пресета
    core::fractal::FractalParams presetSnapshot_;
    char nameBuf_[64] = "";
};

} // namespace ars::gui
