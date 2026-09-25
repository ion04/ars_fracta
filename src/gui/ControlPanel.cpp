#include "gui/ControlPanel.h"
#include "gui/Layout.h"

#include "render/Camera.h"
#include "utils/Config.h"

#include <imgui.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <string>

namespace fs = std::filesystem;

namespace ars::gui {

using core::fractal::FractalParams;
using core::fractal::FractalType;
using core::fractal::sanitizeParams;
using utils::Config;

static const char* kTypeNames[] = {
    "Mandelbrot 2D", "Mandelbulb 3D", "Menger Sponge", "Julia 3D",
    "Пейзаж", "Морское побережье"};

// Ограничение высоты камеры над ландшафтом/морем (пол — y ≈ 0) и «нет
// ограничения» — свободная орбита для 3D-фракталов.
constexpr float kMinGroundHeight = 0.3f;
constexpr float kNoGroundLimit = -std::numeric_limits<float>::infinity();

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
// Центр побережья лежит в начале координат (coastCenter == (0, 0)).
static void placeCoastCamera(render::Camera& cam,
                             const core::fractal::FractalParams& p) {
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
    ImGui::End();
}

void ControlPanel::drawFractalSection() {
    FractalParams& p = *params_;

    // --- тип фрактала ---
    if (ImGui::CollapsingHeader("Тип фрактала", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Combo("Тип", &typeIndex_, kTypeNames, 6)) {
            const FractalType oldType = p.type;
            const FractalType newType = static_cast<FractalType>(typeIndex_);
            if (newType != oldType) {
                // При смене обычного 3D-фракта (Mandelbulb/Menger/Julia) камеру
                // не трогаем: пользователь продолжает смотреть с той же позиции.
                if (newType == FractalType::Terrain3D) {
                    placeTerrainCamera(*camera_, p);
                    camera_->setZoomLimits(p.terrainAmplitude * 1.2f, 100.0f);
                    camera_->setMinHeight(kMinGroundHeight);  // не залезать под ландшафт
                    p.autoRotate = false;  // отключаем автовращение для пейзажа
                } else if (newType == FractalType::Coast3D) {
                    placeCoastCamera(*camera_, p);
                    camera_->setZoomLimits(0.5f, 100.0f);
                    camera_->setMinHeight(kMinGroundHeight);  // не залезать под море/пляж
                    p.autoRotate = false;  // отключаем автовращение для побережья
                } else if (newType == FractalType::Mandelbrot2D ||
                           oldType == FractalType::Mandelbrot2D ||
                           oldType == FractalType::Terrain3D ||
                           oldType == FractalType::Coast3D) {
                    // При переходе между 2D и 3D либо из пейзажной категории
                    // возвращаем камеру к исходному положению.
                    camera_->reset();
                    camera_->setZoomLimits(0.05f, 100.0f);
                    camera_->setMinHeight(kNoGroundLimit);  // свободная 3D-орбита
                }
                // Иначе (обычный 3D → обычный 3D) — камера сохраняется.
            }
            p.type = newType;
        }
    }

    // --- параметры текущего типа: логические группы ---
    if (p.type == FractalType::Terrain3D) {
        ImGui::PushItemWidth(200.0f);
        if (ImGui::CollapsingHeader("Ландшафт", ImGuiTreeNodeFlags_DefaultOpen)) {
            float prevAmp = p.terrainAmplitude;
            ImGui::SliderFloat("Высота рельефа", &p.terrainAmplitude, 1.0f, 20.0f, "%.1f");
            // камера должна оставаться над горами при повышении рельефа
            if (p.terrainAmplitude > prevAmp + 1e-4f &&
                p.terrainAmplitude * 1.1f > camera_->position().y) {
                placeTerrainCamera(*camera_, p);
            }
            // Обновляем минимальный zoom при изменении высоты рельефа
            camera_->setZoomLimits(p.terrainAmplitude * 1.2f, 100.0f);
            ImGui::SliderFloat("Масштаб шума", &p.terrainFrequency, 0.03f, 0.5f, "%.2f");
        }
        if (ImGui::CollapsingHeader("Атмосфера и лес", ImGuiTreeNodeFlags_DefaultOpen)) {
            //ImGui::SliderFloat("Облачность", &p.cloudDensity, 0.0f, 1.5f, "%.2f");
            ImGui::SliderFloat("Плотность тумана", &p.fogDensity, 0.001f, 0.02f, "%.4f");
            ImGui::SliderFloat("Плотность леса", &p.treeDensity, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Время суток", &p.timeOfDay, 0.0f, 1.0f, "%.2f");
        }
        ImGui::PopItemWidth();
    } else if (p.type == FractalType::Coast3D) {
        ImGui::PushItemWidth(200.0f);
        if (ImGui::CollapsingHeader("Берег", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderInt("Итерации берега", &p.iterations, 16, 512);
            // берег = граница Мандельброта: центр выбирается в параметре ниже
            const float previousCenterRe = p.coastCenterRe;
            const float previousCenterIm = p.coastCenterIm;
            const float previousZoom = p.m2dZoom;
            const float previousAmplitude = p.terrainAmplitude;
            ImGui::SliderFloat("Долгота центра", &p.coastCenterRe, -2.0f, 1.0f, "%.5f");
            ImGui::SliderFloat("Широта центра", &p.coastCenterIm, -1.2f, 1.2f, "%.5f");
            ImGui::SliderFloat("Масштаб берега", &p.m2dZoom, 0.05f, 40.0f, "%.1f");
            ImGui::SliderFloat("Высота суши", &p.terrainAmplitude, 1.0f, 20.0f, "%.1f");
            if (p.coastCenterRe != previousCenterRe || p.coastCenterIm != previousCenterIm ||
                p.m2dZoom != previousZoom || p.terrainAmplitude != previousAmplitude) {
                placeCoastCamera(*camera_, p);
            }
        }
        if (ImGui::CollapsingHeader("Атмосфера", ImGuiTreeNodeFlags_DefaultOpen)) {
            //ImGui::SliderFloat("Облачность", &p.cloudDensity, 0.0f, 1.5f, "%.2f");
            ImGui::SliderFloat("Время суток", &p.timeOfDay, 0.0f, 1.0f, "%.2f");
        }
        ImGui::PopItemWidth();
    } else {
        if (ImGui::CollapsingHeader("Геометрия", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        }
        if (ImGui::CollapsingHeader("Цвет", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Масштаб цвета", &p.colorScale, 0.5f, 40.0f, "%.1f");
            ImGui::SliderFloat("Сдвиг цвета", &p.hueShift, -3.0f, 3.0f, "%.2f");
            if (ImGui::Combo("Палитра", &colorModeIdx_, "Градиент\0Оттенки\0")) {
                p.colorMode = colorModeIdx_;
            }
        }
        if (ImGui::CollapsingHeader("Движение", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Автовращение — только для 3D-фракталов; для Terrain/Coast секции нет,
            // а флаг дополнительно сбрасывает sanitizeParams (FractalValidation.cpp).
            ImGui::Checkbox("Автовращение", &p.autoRotate);
        }
    }

    // Любое изменение параметров проходит через валидацию по типу фрактала:
    // слайдеры ImGui пишут в params_ напрямую, поэтому прогоняем итоговое
    // значение через sanitizeParams() (идемпотентно — неподвижный кадр не
    // порождает лишних перерисовок).
    p = sanitizeParams(p);

    // --- разрешение рендера ---
    if (renderScale_) {
        if (ImGui::CollapsingHeader("Разрешение", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Разрешение рендера", renderScale_, 0.25f, 1.0f, "%.2f");
        }
    }

    // --- пресеты ---
    if (ImGui::CollapsingHeader("Пресеты", ImGuiTreeNodeFlags_DefaultOpen)) {
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
                                camera_->setZoomLimits(p.terrainAmplitude * 1.2f, 100.0f);
                                camera_->setMinHeight(kMinGroundHeight);
                            } else if (p.type == FractalType::Coast3D) {
                                placeCoastCamera(*camera_, p);
                                camera_->setZoomLimits(0.5f, 100.0f);
                                camera_->setMinHeight(kMinGroundHeight);
                            } else {
                                camera_->reset();   // фрактал в исходной позиции
                                camera_->setZoomLimits(0.05f, 100.0f);
                                camera_->setMinHeight(kNoGroundLimit);
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
}

} // namespace ars::gui
