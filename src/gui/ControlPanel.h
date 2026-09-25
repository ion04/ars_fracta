#pragma once

#include <string>
#include <vector>

#include "core/fractal/Fractal.h"

namespace ars::render {
class Camera;
}

namespace ars::gui {

/**
 * @brief Панель управления параметрами фрактала.
 */
class ControlPanel {
public:
    ControlPanel(core::fractal::FractalParams& params, render::Camera& camera,
                     float* renderScale);

    void draw();

private:
    void drawFractalSection();
    void loadPresetsIntoList();

    core::fractal::FractalParams* params_ = nullptr;
    render::Camera* camera_ = nullptr;

    int typeIndex_ = 1; // Mandelbulb3D
    int colorModeIdx_ = 0;
    float* renderScale_ = nullptr;

    // пресеты
    std::vector<std::string> presetFiles_;
    std::vector<std::string> presetLabels_;  // basename без расширения
    int presetIndex_ = -1;
    bool presetsLoaded_ = false;

};

} // namespace ars::gui
