#include "gui/ControlPanel.h"
#include "gui/Layout.h"
#include "gui/UiKit.h"

#include "render/Camera.h"
#include "utils/Config.h"
#include "utils/Logger.h"

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
#include <utility>

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
static const char* kTargetFps[] = {"20 FPS", "30 FPS", "45 FPS", "60 FPS", "90 FPS"};
static const int   kTargetFpsVal[] = {20, 30, 45, 60, 90};

constexpr float kMinGroundHeight = 0.3f;
constexpr float kNoGroundLimit = -std::numeric_limits<float>::infinity();
constexpr float kMinScale = 0.10f;

// ---------------------------------------------------------------------------
//  Камера
// ---------------------------------------------------------------------------

// Стартовые виды пейзажа/побережья вынесены в render/Camera.h, чтобы не
// дублировать магические коэффициенты в ControlPanel и main.cpp.
using render::placeCoastCamera;
using render::placeTerrainCamera;

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
    colorModeIdx_ = std::clamp(params.colorMode, 0,
                               static_cast<int>(IM_ARRAYSIZE(kPalettes)) - 1);
    params.colorMode = colorModeIdx_;
}

// ---------------------------------------------------------------------------
//  Пресеты
// ---------------------------------------------------------------------------

void ControlPanel::loadPresetsIntoList() {
    if (presetsLoaded_) return;
    presetsLoaded_ = true;
    presets_.clear();

    const std::string dir = std::string(ARS_FRACTA_ASSETS_DIR) + "/presets";
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.path().extension() != ".json") continue;
        FractalParams tmp;
        if (!Config::loadFractal(entry.path().string(), tmp)) continue;
        PresetEntry e;
        e.label = entry.path().stem().string();
        e.file = e.label + ".json";
        e.type = sanitizeParams(tmp).type;
        presets_.push_back(std::move(e));
    }
    std::sort(presets_.begin(), presets_.end(),
              [](const PresetEntry& a, const PresetEntry& b) { return a.label < b.label; });
    presetIndex_ = -1;   // ничего не выбрано, пока пользователь не выберет
}

void ControlPanel::applyPreset(int index) {
    FractalParams& p = *params_;
    const std::string path =
        std::string(ARS_FRACTA_ASSETS_DIR) + "/presets/" + presets_[index].file;
    FractalParams loaded;
    if (!Config::loadFractal(path, loaded)) {
        utils::Logger::instance().error("Пресет: не удалось загрузить " +
                                        presets_[index].file);
        return;
    }
    p = sanitizeParams(loaded);
    typeIndex_ = static_cast<int>(p.type);
    colorModeIdx_ = std::clamp(p.colorMode, 0,
                               static_cast<int>(IM_ARRAYSIZE(kPalettes)) - 1);
    p.colorMode = colorModeIdx_;
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
    if (!Config::saveFractal(path, p)) {
        utils::Logger::instance().error("Пресет: не удалось сохранить " + name + ".json");
        return false;
    }

    presetsLoaded_ = false;
    loadPresetsIntoList();
    for (int i = 0; i < static_cast<int>(presets_.size()); ++i)
        if (presets_[i].label == name) presetIndex_ = i;
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
                          ImGui::GetTextLineHeightWithSpacing() * 2.0f +
                          st.WindowPadding.y;

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
    // Палитра — целочисленный индекс, sanitizeParams его не трогает.
    params_->colorMode = std::clamp(params_->colorMode, 0,
                                    static_cast<int>(IM_ARRAYSIZE(kPalettes)) - 1);

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
    // Название фрактала — крупным шрифтом (headingFont), как заголовок.
    if (ui::headingFont) ImGui::PushFont(ui::headingFont);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##type", &typeIndex_, kTypeNames, IM_ARRAYSIZE(kTypeNames)))
        changeType(static_cast<FractalType>(typeIndex_));
    if (ui::headingFont) ImGui::PopFont();
    ImGui::Spacing();

    if (p.type == FractalType::Terrain3D) {
        ui::heading("Ландшафт");
        const float prevAmp = p.terrainAmplitude;
        ui::sliderFloat("Высота рельефа", &p.terrainAmplitude, 1.f, 20.f, 7.f, "");
        if (p.terrainAmplitude > prevAmp + 1e-4f &&
            p.terrainAmplitude * 1.1f > camera_->position().y)
            placeTerrainCamera(*camera_, p);
        camera_->setZoomLimits(p.terrainAmplitude * 1.2f, 100.0f);
        ui::sliderFloat("Частота холмов", &p.terrainFrequency, 0.03f, 0.5f, 0.12f, "");

        ui::heading("Атмосфера и лес");
        ui::sliderFloat("Густота тумана", &p.fogDensity, 0.001f, 0.02f, 0.006f,
                        "%.4f", true);
        ui::sliderPercent("Облачность", &p.cloudDensity, 0.f, 1.5f, 0.6f);
        ui::sliderPercent("Густота леса", &p.treeDensity, 0.f, 1.f, 0.4f);
        ui::timeOfDay("Время суток", &p.timeOfDay, 0.4f);
    } else if (p.type == FractalType::Coast3D) {
        ui::heading("Берег");
        ui::sliderInt("Неровность берега", &p.iterations, 16, 512, 64, true, "");
        ui::sliderFloat("Долгота", &p.coastCenterRe, -2.f, 1.f, -0.75f, "%.5f");
        ui::sliderFloat("Широта", &p.coastCenterIm, -1.2f, 1.2f, 0.f, "%.5f");
        ui::sliderFloat("Размер области", &p.m2dZoom, 0.05f, 40.f, 1.f, "×%.2f", true);
        const float prevAmp = p.terrainAmplitude;
        ui::sliderFloat("Высота суши", &p.terrainAmplitude, 1.f, 20.f, 6.f, "%.1f");
        // Центр/размер области двигают фрактал, а не камеру: сбрасывать вид
        // на каждый кадр драга нельзя. Оставляем только «страховку», как у
        // Terrain — не оказаться под выросшим рельефом.
        if (p.terrainAmplitude > prevAmp + 1e-4f &&
            p.terrainAmplitude * 1.1f > camera_->position().y)
            placeCoastCamera(*camera_, p);
        camera_->setZoomLimits(0.5f, 100.0f);

        ui::heading("Атмосфера");
        ui::sliderPercent("Облачность", &p.cloudDensity, 0.f, 1.5f, 0.6f);
        ui::timeOfDay("Время суток", &p.timeOfDay, 0.4f);
    } else {
        ui::heading("Геометрия");
        if (p.type == FractalType::MengerSponge) {
            // Губка сходится быстро: sanitize и рендерер держат потолок 12.
            // Общий диапазон 16..512 для неё бессмысленен (ползунок залипал на 16).
            ui::sliderInt("Итерации", &p.iterations, 1, 12, 4, true, "%d");
        } else {
            ui::sliderInt("Итерации", &p.iterations, 16, 512, 64, true, "%d");
        }
        ui::sliderFloat("Граница", &p.bailout, 1.2f, 8.f, 4.f, "", false);
        if (p.type == FractalType::Mandelbulb3D || p.type == FractalType::Julia3D)
            ui::sliderFloat("Степень", &p.power, 2.f, 32.f, 8.f, "%.1f", false);
        if (p.type == FractalType::Julia3D) {
            ui::sliderFloat("Форма X", &p.juliaReal, -2.f, 2.f, 0.f, "%.4f", false);
            ui::sliderFloat("Форма Y", &p.juliaImag, -2.f, 2.f, 0.f, "%.4f", false);
            ui::sliderFloat("Форма Z", &p.juliaImag3D, -2.f, 2.f, 0.f, "%.4f", false);
        }
        if (p.type == FractalType::Mandelbrot2D)
            ui::sliderFloat("Масштаб", &p.m2dZoom, 0.01f, 30.f, 1.f, "×%.2f", true);
        ui::sliderFloat("Детализация", &p.detail, 0.0005f, 0.02f, 0.002f,
                        "%.4f ед.", true);
    }
}

void ControlPanel::drawColorTab() {
    FractalParams& p = *params_;
    ui::sliderFloat("Масштаб цвета", &p.colorScale, 0.5f, 40.f, 5.f, "", true);
    ui::sliderFloat("Сдвиг цвета", &p.hueShift, -3.f, 3.f, 0.f, "", false);
    if (ui::combo("Палитра", &colorModeIdx_, kPalettes, IM_ARRAYSIZE(kPalettes)))
        p.colorMode = colorModeIdx_;
}

void ControlPanel::drawCameraTab() {
    FractalParams& p = *params_;
    const glm::vec3 pos = camera_->position();

    ImGui::TextDisabled("Позиция");
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.2f  %.2f  %.2f  ед.", pos.x, pos.y, pos.z);
    ImGui::TextUnformatted(buf);
    ImGui::SameLine();
    if (ImGui::SmallButton("Копировать")) ImGui::SetClipboardText(buf);

    ImGui::Spacing();
    if (ImGui::Button("Сброс камеры", ImVec2(-FLT_MIN, 0))) applyCameraFor(p.type, p.type, true);
    if (ImGui::Button("Вид сверху", ImVec2(-FLT_MIN, 0)))
        camera_->topView(std::max(glm::length(pos), 1.0f));

    if (p.type != FractalType::Terrain3D && p.type != FractalType::Coast3D) {
        ImGui::SeparatorText("Движение");
        ImGui::Checkbox("Автовращение", &p.autoRotate);
    }

    //ImGui::SeparatorText("Управление");
    //ImGui::TextDisabled("ЛКМ + движение — вращение\nКолесо — приближение");
}

void ControlPanel::drawRenderTab() {
    if (!adaptive_) { ImGui::TextDisabled("Нет настроек"); return; }
    ImGui::Checkbox("Адаптивное разрешение", &adaptive_->enabled);
    ui::tip("При движении камеры качество снижается до целевого FPS,\nкогда всё замерло — рендер в полном качестве");
    ImGui::BeginDisabled(!adaptive_->enabled);
    int idx = 1;
    const int fpsCount = IM_ARRAYSIZE(kTargetFpsVal);
    static_assert(IM_ARRAYSIZE(kTargetFps) == IM_ARRAYSIZE(kTargetFpsVal),
                  "таблицы FPS должны совпадать по длине");
    for (int i = 0; i < fpsCount; ++i)
        if (kTargetFpsVal[i] == adaptive_->targetFps) idx = i;
    if (ui::combo("Целевой FPS при движении", &idx, kTargetFps, fpsCount))
        adaptive_->targetFps = kTargetFpsVal[idx];
    ImGui::EndDisabled();
    ImGui::Spacing();
}

// ---------------------------------------------------------------------------
//  Закреплённый низ: разрешение + пресеты
// ---------------------------------------------------------------------------

void ControlPanel::drawFooter() {
    FractalParams& p = *params_;

    if (renderScale_) {
        ui::sliderPercent("Максимальное разрешение", renderScale_, kMinScale, 1.0f, 1.0f);
    }

    loadPresetsIntoList();
    const bool hasPreset = presetActive_ && presetIndex_ >= 0 &&
                           presetIndex_ < static_cast<int>(presets_.size());
    const bool dirty = hasPreset && !(p == presetSnapshot_);
    std::string cur = "-- выбрать пресет --";
    if (hasPreset) cur = presets_[presetIndex_].label + (dirty ? " *" : "");

    ImGui::TextUnformatted("Пресет");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##presets", cur.c_str())) {
        bool any = false;
        for (int i = 0; i < static_cast<int>(presets_.size()); ++i) {
            if (presets_[i].type != p.type) continue;          // только пресеты этого типа
            any = true;
            if (ImGui::Selectable(presets_[i].label.c_str(), hasPreset && i == presetIndex_))
                applyPreset(i);
        }
        if (!any) ImGui::TextDisabled("Нет пресетов для этого типа");
        ImGui::EndCombo();
    }
    if (dirty) ui::tip("Параметры изменены относительно пресета");

    const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    ImGui::BeginDisabled(!dirty);
    if (ImGui::Button("Заменить", ImVec2(w, 0)) && hasPreset)
        savePreset(presets_[presetIndex_].label);           // перезапись выбранного
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Сохранить как", ImVec2(w, 0))) {
        saveError_ = false;
        ImGui::OpenPopup("save_as");
    }

    if (ImGui::BeginPopup("save_as")) {
        ImGui::TextUnformatted("Название пресета");
        ImGui::SetNextItemWidth(220.0f);
        const bool enter = ImGui::InputText("##name", nameBuf_, sizeof nameBuf_,
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        if ((ImGui::Button("Заменить") || enter) && nameBuf_[0]) {
            saveError_ = !savePreset(nameBuf_);
            if (!saveError_) {
                nameBuf_[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
        }
        if (saveError_)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "Не удалось сохранить пресет");
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
        if (ImGui::Button("Вид сверху"))
            camera_->topView(std::max(glm::length(camera_->position()), 1.0f));
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
        if (ImGui::Button("Скриншот") && onScreenshot_) onScreenshot_();
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

} // namespace ars::gui