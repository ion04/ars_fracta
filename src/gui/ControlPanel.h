#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/async/JobSystem.h"
#include "core/fractal/Fractal.h"

namespace ars::render {
class Camera;
}

namespace ars::gui {

// Ширина панели управления и высота статус-бара (px).
constexpr float kPanelWidth = 340.0f;
constexpr float kStatusBarHeight = 26.0f;

/**
 * @brief Панель управления параметрами фрактала и экспортом меша.
 */
class ControlPanel {
public:
    ControlPanel(core::fractal::FractalParams& params, render::Camera& camera,
                     float* renderScale);

    void draw();

private:
    void drawFractalSection();
    void drawExportSection();
    void exportMengerMesh();
    void pollExportJob();
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

    // результат последнего экспорта меша
    bool exportDone_ = false;
    std::string exportInfo_;
    struct MeshExportResult;
    std::shared_ptr<MeshExportResult> exportResult_;
    core::async::Job exportJob_;
    int meshIterations_ = 2;
};

} // namespace ars::gui