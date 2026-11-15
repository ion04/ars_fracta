#include "gui/ControlPanel.h"
#include "gui/Layout.h"
#include "gui/UiKit.h"

#include "render/Camera.h"
#include "utils/Config.h"

#include <imgui.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
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
    "Мандельброт 2D", "Мандельбульб 3D", "Губка Менгера", "Юлия 3D",
    "Пейзаж", "Морское побережье"};
static const char* kPalettes[] = {"Градиент", "Оттенки"};
static const char* kTargetFps[] = {"20 FPS", "30 FPS", "45 FPS", "60 FPS"};
static const int   kTargetFpsVal[] = {20, 30, 45, 60, 90};

constexpr float kMinGroundHeight = 0.3f;
constexpr float kNoGroundLimit = -std::numeric_limits<float>::infinity();
constexpr float kMinScale = 0.10f;

// ---------------------------------------------------------------------------
//  Камера
// ---------------------------------------------------------------------------

static void placeTerrainCamera(render::Camera& cam, const FractalParams& p) {
    const float eyeY = std::max(7.0f, p.terrainAmplitude * 1.45f);
    cam.setView(glm::vec3(10.0f, eyeY, -20.0f), glm::vec3(-1.5f, 2.2f, 10.0f));
    cam.setFov(52.0f);
}

static void placeCoastCamera(render::Camera& cam, const FractalParams& p) {
    const float eyeY = std::max(9.0f, p.terrainAmplitude * 1.55f);
    cam.setView(glm::vec3(0.0f, eyeY, -22.0f), glm::vec3(0.0f, 1.0f, 7.0f));
    cam.setFov(52.0f);
}

// Единая точка настройки камеры (раньше блок дублировался в 2 местах).
// force=true — безусловный сброс (загрузка пресета / кнопка «Сброс камеры»).
void ControlPanel::applyCameraFor(FractalType oldType, FractalType newType, bool force) {
    FractalParams& p = *params_;
    if (newType == FractalType::Terrain3D) {
        placeTerrainCamera(*camera_, p);
        camera_->setZoomLimits(p.terrainAmplitude * 1.2f, 100.0f);
        camera_->setMinHeight(kMinGroundHeight);
        p.autoRotate = false;
    } else if (newType == FractalType::Coast3D) {
        placeCoastCamera(*camera_, p);
        camera_->setZoomLimits(0.5f, 100.0f);
        camera_->setMinHeight(kMinGroundHeight);
        p.autoRotate = false;
    } else if (force || newType == FractalType::Mandelbrot2D ||
               oldType == FractalType::Mandelbrot2D ||
               oldType == FractalType::Terrain3D || oldType == FractalType::Coast3D) {
        camera_->reset();
        camera_->setZoomLimits(0.05f, 100.0f);
        camera_->setMinHeight(kNoGroundLimit);
    }
    // 3D → 3D без force: камера сохраняется.
}

void ControlPanel::changeType(FractalType newType) {
    FractalParams& p = *params_;
    if (newType != p.type) {
        applyCameraFor(p.type, newType, false);
        presetActive_ = false;      // пресет другого типа больше не «текущий»
    }
    p.type = newType;
}

ControlPanel::ControlPanel(FractalParams& params, render::Camera& camera, float* renderScale,
                           AdaptiveSettings* adaptive)
    : params_(&params), camera_(&camera), renderScale_(renderScale), adaptive_(adaptive) {
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
    presetTypes_.clear();

    const std::string dir = std::string(ARS_FRACTA_ASSETS_DIR) + "/presets";
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;

    std::vector<std::pair<std::string, FractalType>> found;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.path().extension() != ".json") continue;
        FractalParams tmp;
        if (!Config::loadFractal(entry.path().string(), tmp)) continue;
        found.emplace_back(entry.path().stem().string(), sanitizeParams(tmp).type);
    }
    std::sort(found.begin(), found.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& f : found) {
        presetLabels_.push_back(f.first);
        presetFiles_.push_back(f.first + ".json");
        presetTypes_.push_back(f.second);
    }
    presetIndex_ = -1;   // ничего не выбрано, пока пользователь не выберет
}

void ControlPanel::applyPreset(int index) {
    FractalParams& p = *params_;
    const std::string path = std::string(ARS_FRACTA_ASSETS_DIR) + "/presets/" + presetFiles_[index];
    if (!Config::loadFractal(path, p)) return;
    p = sanitizeParams(p);
    typeIndex_ = static_cast<int>(p.type);
    colorModeIdx_ = p.colorMode;
    applyCameraFor(p.type, p.type, /*force=*/true);
    presetIndex_ = index;
    presetSnapshot_ = p;
    presetActive_ = true;
}

bool ControlPanel::savePreset(const std::string& label) {
    std::string name;
    for (unsigned char ch : label)               // байты UTF-8 (>=128) оставляем как есть
        name += (std::isalnum(ch) || ch == '-' || ch == '_' || ch >= 128) ? char(ch) : '_';
    if (name.empty()) return false;

    FractalParams& p = *params_;
    p = sanitizeParams(p);
    const std::string path = std::string(ARS_FRACTA_ASSETS_DIR) + "/presets/" + name + ".json";
    if (!Config::saveFractal(path, p)) return false;

    presetsLoaded_ = false;
    loadPresetsIntoList();
    for (int i = 0; i < static_cast<int>(presetLabels_.size()); ++i)
        if (presetLabels_[i] == name) presetIndex_ = i;
    presetSnapshot_ = p;
    presetActive_ = true;
    return true;
}

// ---------------------------------------------------------------------------
//  Каркас окна
// ---------------------------------------------------------------------------

void ControlPanel::draw() {
    const ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(kPanelWidth, io.DisplaySize.y - kStatusBarHeight));
    ImGui::Begin("Управление", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    // Высота закреплённого низа: подберите под свой шрифт (после loadFont/applyTheme).
    const ImGuiStyle& st = ImGui::GetStyle();
    const float footerH = ImGui::GetFrameHeightWithSpacing() * 5.2f +
                          ImGui::GetTextLineHeightWithSpacing() + st.WindowPadding.y;

    if (ImGui::BeginTabBar("tabs")) {
        const bool is3dFractal = params_->type != FractalType::Terrain3D &&
                                 params_->type != FractalType::Coast3D;
        // прокручиваемая область для каждой вкладки
        auto tab = [&](const char* name, auto&& body) {
            if (ImGui::BeginTabItem(name)) {
                ImGui::BeginChild("tabbody", ImVec2(0, -footerH), false);
                ImGui::Spacing();
                body();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
        };
        tab("Фрактал", [&] { drawFractalTab(); });
        if (is3dFractal) tab("Цвет", [&] { drawColorTab(); });
        tab("Камера", [&] { drawCameraTab(); });
        tab("Рендер", [&] { drawRenderTab(); });
        ImGui::EndTabBar();
    }

    // Любое изменение параметров проходит через валидацию (идемпотентно).
    *params_ = sanitizeParams(*params_);

    ImGui::Separator();
    drawFooter();
    ImGui::End();

    drawViewportToolbar();
}

// ---------------------------------------------------------------------------
//  Вкладки
// ---------------------------------------------------------------------------

void ControlPanel::drawFractalTab() {
    FractalParams& p = *params_;

    ui::heading("Тип");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##type", &typeIndex_, kTypeNames, 6))
        changeType(static_cast<FractalType>(typeIndex_));
    ImGui::Spacing();

    if (p.type == FractalType::Terrain3D) {
        ui::heading("Ландшафт");
        const float prevAmp = p.terrainAmplitude;
        ui::sliderFloat("Высота рельефа", &p.terrainAmplitude, 1.f, 20.f, 7.f, "%.1f");
        if (p.terrainAmplitude > prevAmp + 1e-4f &&
            p.terrainAmplitude * 1.1f > camera_->position().y)
            placeTerrainCamera(*camera_, p);
        camera_->setZoomLimits(p.terrainAmplitude * 1.2f, 100.0f);
        ui::sliderFloat("Частота холмов", &p.terrainFrequency, 0.03f, 0.5f, 0.12f, "%.2f");

        ui::heading("Атмосфера и лес");
        ui::sliderFloat("Густота тумана", &p.fogDensity, 0.001f, 0.02f, 0.006f, "%.4f", true);
        ui::sliderFloat("Облачность", &p.cloudDensity, 0.f, 1.5f, 0.6f, "%.2f");
        ui::sliderFloat("Густота леса", &p.treeDensity, 0.f, 1.f, 0.4f, "%.2f");
        ui::sliderFloat("Время суток", &p.timeOfDay, 0.f, 1.f, 0.4f, "%.2f");
    } else if (p.type == FractalType::Coast3D) {
        ui::heading("Берег");
        const float c0 = p.coastCenterRe, c1 = p.coastCenterIm, z0 = p.m2dZoom, a0 = p.terrainAmplitude;
        ui::sliderInt("Неровность берега", &p.iterations, 16, 512, 64, true,
                      "Больше итераций — детальнее линия берега");
        ui::sliderFloat("Долгота", &p.coastCenterRe, -2.f, 1.f, -0.75f, "%.5f");
        ui::sliderFloat("Широта", &p.coastCenterIm, -1.2f, 1.2f, 0.f, "%.5f");
        ui::sliderFloat("Размер области", &p.m2dZoom, 0.05f, 40.f, 1.f, "%.2f", true);
        ui::sliderFloat("Высота суши", &p.terrainAmplitude, 1.f, 20.f, 6.f, "%.1f");
        if (p.coastCenterRe != c0 || p.coastCenterIm != c1 || p.m2dZoom != z0 ||
            p.terrainAmplitude != a0)
            placeCoastCamera(*camera_, p);

        ui::heading("Атмосфера");
        ui::sliderFloat("Облачность", &p.cloudDensity, 0.f, 1.5f, 0.6f, "%.2f");
        ui::sliderFloat("Время суток", &p.timeOfDay, 0.f, 1.f, 0.4f, "%.2f");
    } else {
        ui::heading("Геометрия");
        ui::sliderInt("Итерации", &p.iterations, 16, 512, 64, true,
                      "Больше итераций — точнее края, но медленнее");
        ui::sliderFloat("Граница", &p.bailout, 1.2f, 8.f, 4.f, "%.2f", false,
                        "Радиус, за которым точка считается «убежавшей»");
        if (p.type == FractalType::Mandelbulb3D || p.type == FractalType::Julia3D)
            ui::sliderFloat("Степень", &p.power, 2.f, 32.f, 8.f, "%.1f", false,
                            "Степень в формуле z^n + c");
        if (p.type == FractalType::Julia3D) {
            ui::sliderFloat("Форма X", &p.juliaReal, -2.f, 2.f, 0.f, "%.4f", false, "Компонента константы c");
            ui::sliderFloat("Форма Y", &p.juliaImag, -2.f, 2.f, 0.f, "%.4f", false, "Компонента константы c");
            ui::sliderFloat("Форма Z", &p.juliaImag3D, -2.f, 2.f, 0.f, "%.4f", false, "Компонента константы c");
        }
        if (p.type == FractalType::Mandelbrot2D)
            ui::sliderFloat("Масштаб", &p.m2dZoom, 0.01f, 30.f, 1.f, "%.2f", true);
        ui::sliderFloat("Детализация", &p.detail, 0.0005f, 0.02f, 0.002f, "%.4f", true,
                        "Точность трассировки: меньше — чётче и медленнее");
    }
}

void ControlPanel::drawColorTab() {
    FractalParams& p = *params_;
    ui::sliderFloat("Масштаб цвета", &p.colorScale, 0.5f, 40.f, 5.f, "%.1f", true);
    ui::sliderFloat("Сдвиг цвета", &p.hueShift, -3.f, 3.f, 0.f, "%.2f");
    if (ui::combo("Палитра", &colorModeIdx_, kPalettes, 2)) p.colorMode = colorModeIdx_;
}

void ControlPanel::drawCameraTab() {
    FractalParams& p = *params_;
    const glm::vec3 pos = camera_->position();

    ImGui::TextDisabled("Позиция");
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.2f  %.2f  %.2f", pos.x, pos.y, pos.z);
    ImGui::TextUnformatted(buf);
    ImGui::SameLine();
    if (ImGui::SmallButton("Копировать")) ImGui::SetClipboardText(buf);

    ImGui::Spacing();
    if (ImGui::Button("Сброс камеры", ImVec2(-FLT_MIN, 0))) applyCameraFor(p.type, p.type, true);
    if (ImGui::Button("Вид сверху", ImVec2(-FLT_MIN, 0))) {
        const float h = std::max(glm::length(pos), 1.0f);
        camera_->setView(glm::vec3(0.0f, h, 0.001f), glm::vec3(0.0f));
    }

    if (p.type != FractalType::Terrain3D && p.type != FractalType::Coast3D) {
        ImGui::SeparatorText("Движение");
        ImGui::Checkbox("Автовращение", &p.autoRotate);
    }

    ImGui::SeparatorText("Управление");
    ImGui::TextDisabled("ЛКМ + движение — вращение\nКолесо — приближение");
}

void ControlPanel::drawRenderTab() {
    if (!adaptive_) { ImGui::TextDisabled("Нет настроек"); return; }
    ImGui::Checkbox("Адаптивное разрешение", &adaptive_->enabled);
    ui::tip("При движении камеры качество снижается до целевого FPS,\nкогда всё замерло — рендер в полном качестве");
    ImGui::BeginDisabled(!adaptive_->enabled);
    int idx = 1;
    for (int i = 0; i < 4; ++i) if (kTargetFpsVal[i] == adaptive_->targetFps) idx = i;
    if (ui::combo("Целевой FPS при движении", &idx, kTargetFps, 4)) adaptive_->targetFps = kTargetFpsVal[idx];
    ImGui::EndDisabled();
    ImGui::Spacing();
    ImGui::TextDisabled("Слайдер «Макс. разрешение» внизу панели задаёт\nкачество в покое и верхний предел при движении.");
}

// ---------------------------------------------------------------------------
//  Закреплённый низ: разрешение + пресеты
// ---------------------------------------------------------------------------

void ControlPanel::drawFooter() {
    FractalParams& p = *params_;

    if (renderScale_) {
        ui::sliderFloat("Макс. разрешение", renderScale_, kMinScale, 1.0f, 1.0f, "×%.2f", false,
                        "Качество в покое. При движении может быть ниже (адаптивный режим)");
    }

    loadPresetsIntoList();
    const bool dirty = presetActive_ && presetIndex_ >= 0 && !(p == presetSnapshot_);
    std::string cur = "— выбрать пресет —";
    if (presetActive_ && presetIndex_ >= 0) cur = presetLabels_[presetIndex_] + (dirty ? " *" : "");

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##presets", cur.c_str())) {
        bool any = false;
        for (int i = 0; i < static_cast<int>(presetLabels_.size()); ++i) {
            if (presetTypes_[i] != p.type) continue;          // только пресеты этого типа
            any = true;
            if (ImGui::Selectable(presetLabels_[i].c_str(), presetActive_ && i == presetIndex_))
                applyPreset(i);
        }
        if (!any) ImGui::TextDisabled("Нет пресетов для этого типа");
        ImGui::EndCombo();
    }
    if (dirty) ui::tip("Параметры изменены относительно пресета");

    const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    ImGui::BeginDisabled(!dirty);
    if (ImGui::Button("Сохранить", ImVec2(w, 0)) && presetIndex_ >= 0)
        savePreset(presetLabels_[presetIndex_]);           // перезапись выбранного
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Как новый…", ImVec2(w, 0))) ImGui::OpenPopup("save_as");

    if (ImGui::BeginPopup("save_as")) {
        ImGui::TextUnformatted("Имя пресета");
        ImGui::SetNextItemWidth(220.0f);
        const bool enter = ImGui::InputText("##name", nameBuf_, sizeof nameBuf_,
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        if ((ImGui::Button("Сохранить") || enter) && nameBuf_[0]) {
            if (savePreset(nameBuf_)) nameBuf_[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
//  Панель над вьюпортом: сброс, вид сверху, скриншот + индикатор качества
// ---------------------------------------------------------------------------

void ControlPanel::drawViewportToolbar() {
    const ImGuiIO& io = ImGui::GetIO();
    const float vx = kPanelWidth;
    const float vw = io.DisplaySize.x - vx;
    const float vb = io.DisplaySize.y - kStatusBarHeight;

    const ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    // индикатор пониженного качества (слева сверху вьюпорта)
    if (effectiveScale_ < 0.999f) {
        ImGui::SetNextWindowPos(ImVec2(vx + 14.0f, 14.0f));
        ImGui::SetNextWindowBgAlpha(0.55f);
        if (ImGui::Begin("##quality", nullptr, fl | ImGuiWindowFlags_NoInputs)) {
            ImGui::Text("%.0f FPS  ·  ×%.2f%s", io.Framerate, effectiveScale_,
                        (adaptive_ && adaptive_->enabled) ? "  ·  адаптивное разрешение" : "");
        }
        ImGui::End();
    }

    // кнопки внизу по центру
    ImGui::SetNextWindowPos(ImVec2(vx + vw * 0.5f, vb - 16.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.85f);
    if (ImGui::Begin("##viewport_tools", nullptr, fl)) {
        FractalParams& p = *params_;
        if (ImGui::Button("Сброс камеры")) applyCameraFor(p.type, p.type, true);
        ImGui::SameLine();
        if (ImGui::Button("Вид сверху")) {
            const float h = std::max(glm::length(camera_->position()), 1.0f);
            camera_->setView(glm::vec3(0.0f, h, 0.001f), glm::vec3(0.0f));
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
        if (ImGui::Button("Скриншот") && onScreenshot_) onScreenshot_();
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

} // namespace ars::gui