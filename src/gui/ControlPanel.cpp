#include "gui/ControlPanel.h"

#include "core/export/MeshExporter.h"
#include "core/fractal/MengerMesh.h"
#include "render/Camera.h"
#include "utils/Config.h"

#include <imgui.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace ars::gui {

struct ControlPanel::MeshExportResult {
    bool cancelled = false;
    std::string info;
};

using core::fractal::FractalParams;
using core::fractal::FractalType;
using core::fractal::sanitizeParams;
using utils::Config;

static const char* kTypeNames[] = {
    "Mandelbrot 2D", "Mandelbulb 3D", "Menger Sponge", "Julia 3D",
    "Пейзаж", "Морское побережье"};

// Камера над ландшафтом: высота зависит от амплитуды рельефа, чтобы камера
// никогда не оказывалась под горами (иначе экран «чернеет» — марш снаружи
// поверхности сразу возвращает t=0). Взгляд сохраняет неизменный наклон.
static void placeTerrainCamera(render::Camera& cam,
                               const core::fractal::FractalParams& p) {
    const float eyeY = std::max(7.0f, p.terrainAmplitude * 1.45f);
    const glm::vec3 eye(10.0f, eyeY, -20.0f);
    cam.setView(eye, glm::vec3(-1.5f, 2.2f, 10.0f));
    cam.setFov(52.0f);
}

// Камера над морем: побережье видно под наклоном, горизонт — вода.
static void placeCoastCamera(render::Camera& cam,
                             const core::fractal::FractalParams& p) {
    // coastC(0, 0) == uCoastCenter, so the coastline focus stays at the world origin.
    const float eyeY = std::max(9.0f, p.terrainAmplitude * 1.55f);
    const glm::vec3 target(0.0f, 1.0f, 7.0f);
    cam.setView(glm::vec3(0.0f, eyeY, -22.0f), target);
    cam.setFov(52.0f);
}

ControlPanel::ControlPanel(core::fractal::FractalParams& params,
                           render::Camera& camera, float* renderScale)
    : params_(&params), camera_(&camera), renderScale_(renderScale) {
    typeIndex_ = static_cast<int>(params.type);
    colorModeIdx_ = params.colorMode;
}

// ---------------------------------------------------------------------------
//  Пресеты
// ---------------------------------------------------------------------------

void ControlPanel::loadPresetsIntoList() {
    if (presetsLoaded_) return;
    presetsLoaded_ = true;
    presetFiles_.clear();
    presetLabels_.clear();

    const std::string dir = std::string(ARS_FRACTA_ASSETS_DIR) + "/presets";
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.path().extension() == ".json") {
            presetFiles_.push_back(entry.path().filename().string());
            presetLabels_.push_back(entry.path().stem().string());
        }
    }
    std::sort(presetFiles_.begin(), presetFiles_.end());
    std::sort(presetLabels_.begin(), presetLabels_.end());
    if (!presetLabels_.empty()) presetIndex_ = 0;
}

// ---------------------------------------------------------------------------
//  Визуализация
// ---------------------------------------------------------------------------

void ControlPanel::draw() {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(kPanelWidth, io.DisplaySize.y - kStatusBarHeight));
    ImGui::Begin("Управление", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    drawFractalSection();
    ImGui::Separator();
    drawExportSection();
    ImGui::End();
}

void ControlPanel::drawFractalSection() {
    FractalParams& p = *params_;

    ImGui::Text("Фрактал");
    if (ImGui::Combo("Тип", &typeIndex_, kTypeNames, 6)) {
        const FractalType newType = static_cast<FractalType>(typeIndex_);
        if (newType != p.type) {
            // центр 2D-вида берётся из камеры; сбрасываем, чтобы после
            // орбиты в 3D Мандельброт не «уехал» за пределы экрана.
            if (newType == FractalType::Terrain3D) {
                placeTerrainCamera(*camera_, p);
            } else if (newType == FractalType::Coast3D) {
                placeCoastCamera(*camera_, p);
            } else {
                camera_->reset();
            }
        }
        p.type = newType;
    }

    if (p.type == FractalType::Terrain3D) {
        float prevAmp = p.terrainAmplitude;
        ImGui::SliderFloat("Высота рельефа", &p.terrainAmplitude, 1.0f, 20.0f, "%.1f");
        // камера должна оставаться над горами при повышении рельефа
        if (p.terrainAmplitude > prevAmp + 1e-4f &&
            p.terrainAmplitude * 1.1f > camera_->position().y) {
            placeTerrainCamera(*camera_, p);
        }
        ImGui::SliderFloat("Масштаб шума", &p.terrainFrequency, 0.03f, 0.5f, "%.2f");
        ImGui::SliderFloat("Облачность", &p.cloudDensity, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Плотность леса", &p.treeDensity, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Время суток", &p.timeOfDay, 0.0f, 1.0f, "%.2f");
    } else if (p.type == FractalType::Coast3D) {
        ImGui::SliderInt("Итерации берега", &p.iterations, 16, 512);
        // берег = граница Мандельброта: центр выбирается в параметре ниже
        const float previousCenterRe = p.coastCenterRe;
        const float previousCenterIm = p.coastCenterIm;
        const float previousZoom = p.m2dZoom;
        const float previousAmplitude = p.terrainAmplitude;
        ImGui::SliderFloat("Re центра", &p.coastCenterRe, -2.0f, 1.0f, "%.5f");
        ImGui::SliderFloat("Im центра", &p.coastCenterIm, -1.2f, 1.2f, "%.5f");
        ImGui::SliderFloat("Масштаб берега", &p.m2dZoom, 0.05f, 40.0f, "%.1f");
        ImGui::SliderFloat("Высота суши", &p.terrainAmplitude, 1.0f, 20.0f, "%.1f");
        if (p.coastCenterRe != previousCenterRe || p.coastCenterIm != previousCenterIm ||
            p.m2dZoom != previousZoom || p.terrainAmplitude != previousAmplitude) {
            placeCoastCamera(*camera_, p);
        }
        ImGui::SliderFloat("Облачность", &p.cloudDensity, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Время суток", &p.timeOfDay, 0.0f, 1.0f, "%.2f");
    } else {
        ImGui::SliderInt("Итерации", &p.iterations, 16, 512);
        ImGui::SliderFloat("Радиус сходимости", &p.bailout, 1.2f, 8.0f, "%.2f");

        if (p.type == FractalType::Mandelbulb3D || p.type == FractalType::Julia3D) {
            ImGui::SliderFloat("Степень", &p.power, 2.0f, 32.0f, "%.1f");
        }
        if (p.type == FractalType::Julia3D) {
            ImGui::SliderFloat("Re(c)", &p.juliaReal, -2.0f, 2.0f, "%.4f");
            ImGui::SliderFloat("Im(c)", &p.juliaImag, -2.0f, 2.0f, "%.4f");
            ImGui::SliderFloat("Im3(c)", &p.juliaImag3D, -2.0f, 2.0f, "%.4f");
        }
        if (p.type == FractalType::Mandelbrot2D) {
            ImGui::SliderFloat("Масштаб", &p.m2dZoom, 0.01f, 30.0f, "%.2f");
        }

        ImGui::SliderFloat("Детализация", &p.detail, 0.0005f, 0.02f, "%.4f");
        ImGui::SliderFloat("Масштаб цвета", &p.colorScale, 0.5f, 40.0f, "%.1f");
        ImGui::SliderFloat("Сдвиг цвета", &p.hueShift, -3.0f, 3.0f, "%.2f");

        if (ImGui::Combo("Палитра", &colorModeIdx_, "Градиент\0Оттенки\0")) {
            p.colorMode = colorModeIdx_;
        }
        ImGui::Checkbox("Автовращение", &p.autoRotate);
    }

    // Любое изменение параметров проходит через валидацию по типу фрактала:
    // слайдеры ImGui пишут в params_ напрямую, поэтому прогоняем итоговое
    // значение через sanitizeParams() (идемпотентно — неподвижный кадр не
    // порождает лишних перерисовок).
    p = sanitizeParams(p);

    ImGui::Spacing();
    if (renderScale_) {
        ImGui::SliderFloat("Разрешение рендера", renderScale_, 0.25f, 1.0f, "%.2f");
    }

    // --- пресеты ---
    ImGui::Spacing();
    ImGui::Text("Пресеты");
    loadPresetsIntoList();
    if (!presetLabels_.empty()) {
        if (ImGui::BeginCombo("##presets", presetLabels_[presetIndex_].c_str())) {
            for (int i = 0; i < static_cast<int>(presetLabels_.size()); ++i) {
                const bool selected = (i == presetIndex_);
                if (ImGui::Selectable(presetLabels_[i].c_str(), selected)) {
                    presetIndex_ = i;
                    const std::string path = std::string(ARS_FRACTA_ASSETS_DIR) +
                                             "/presets/" + presetFiles_[i];
                    if (Config::loadFractal(path, p)) {
                        p = sanitizeParams(p);   // пресет из файла мог быть испорчен
                        typeIndex_ = static_cast<int>(p.type);
                        colorModeIdx_ = p.colorMode;
                        if (p.type == FractalType::Terrain3D) {
                            placeTerrainCamera(*camera_, p);
                        } else if (p.type == FractalType::Coast3D) {
                            placeCoastCamera(*camera_, p);
                        } else {
                            camera_->reset();   // фрактал в исходной позиции
                        }
                    }
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    if (ImGui::Button("Сохранить текущий пресет")) {
        const std::string path = std::string(ARS_FRACTA_ASSETS_DIR) +
                                 "/presets/custom.json";
        p = sanitizeParams(p);   // в файл попадают только валидированные значения
        Config::saveFractal(path, p);
        presetsLoaded_ = false;
        loadPresetsIntoList();
    }
}

// ---------------------------------------------------------------------------
//  Экспорт
// ---------------------------------------------------------------------------

void ControlPanel::drawExportSection() {
    pollExportJob();
    ImGui::Text("Экспорт");
    ImGui::SliderInt("Глубина mesh Менгера", &meshIterations_, 0, 4);
    const bool running = static_cast<bool>(exportResult_);
    ImGui::BeginDisabled(running);
    if (ImGui::Button("Экспортировать губку Менгера (OBJ + STL)")) {
        exportMengerMesh();
    }
    ImGui::EndDisabled();
    if (running) {
        ImGui::SameLine();
        if (ImGui::Button("Отмена")) exportJob_.cancel();
        ImGui::TextUnformatted("Выполняется в фоне...");
    }
    if (exportDone_) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", exportInfo_.c_str());
    }
}

void ControlPanel::exportMengerMesh() {
    exportDone_ = false;
    exportInfo_.clear();
    exportResult_ = std::make_shared<MeshExportResult>();
    const auto result = exportResult_;
    const int depth = meshIterations_;
    exportJob_ = core::async::JobSystem::submit([result, depth](const auto& token) {
        const auto mesh = core::fractal::MengerMesh::build(
            depth, [&token] { return token.isCancelled(); });
        if (token.isCancelled()) {
            result->cancelled = true;
            return;
        }
        // File writing is a non-cancellable final stage; UI keeps polling the worker.
        const bool objSaved = core::exporter::MeshExporter::writeObj("menger_sponge.obj", mesh);
        const bool stlSaved = core::exporter::MeshExporter::writeBinaryStl("menger_sponge.stl", mesh);
        result->info = objSaved && stlSaved
            ? "Сохранено: menger_sponge.obj, menger_sponge.stl (" +
                  std::to_string(mesh.triangleCount()) + " треугольников)"
            : "Ошибка экспорта губки Менгера";
    });
}

void ControlPanel::pollExportJob() {
    if (!exportResult_ || !exportJob_.ready()) return;
    try {
        exportJob_.wait();
        exportInfo_ = exportResult_->cancelled ? "Операция отменена"
                                               : exportResult_->info;
        exportDone_ = true;
    } catch (const std::exception& error) {
        exportInfo_ = std::string("Ошибка фоновой задачи: ") + error.what();
        exportDone_ = true;
    } catch (...) {
        exportInfo_ = "Неизвестная ошибка фоновой задачи";
        exportDone_ = true;
    }
    exportResult_.reset();
}

} // namespace ars::gui