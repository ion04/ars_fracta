#include "gui/ControlPanel.h"

#include "compression/Decoder.h"
#include "compression/Encoder.h"
#include "compression/Metrics.h"
#include "core/fractal/Mandelbrot2D.h"
#include "render/Camera.h"
#include "utils/Config.h"
#include "utils/ImageLoader.h"

#include <imgui.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace ars::gui {

using core::fractal::FractalParams;
using core::fractal::FractalType;
using core::math::Complex;
using core::fractal::Mandelbrot2D;
using utils::Config;

static const char* kTypeNames[] = {
    "Mandelbrot 2D", "Mandelbulb 3D", "Menger Sponge", "Julia 3D", "Пейзаж"};

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
    drawCompressionSection();
    ImGui::End();
}

void ControlPanel::drawFractalSection() {
    FractalParams& p = *params_;

    ImGui::Text("Фрактал");
    if (ImGui::Combo("Тип", &typeIndex_, kTypeNames, 5)) {
        const FractalType newType = static_cast<FractalType>(typeIndex_);
        if (newType != p.type) {
            // центр 2D-вида берётся из камеры; сбрасываем, чтобы после
            // орбиты в 3D Мандельброт не «уехал» за пределы экрана.
            if (newType == FractalType::Terrain3D) {
                camera_->setView(glm::vec3(0.0f, 2.0f, -14.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
                camera_->setFov(55.0f);
            } else {
                camera_->reset();
            }
        }
        p.type = newType;
    }

    if (p.type == FractalType::Terrain3D) {
        ImGui::SliderFloat("Высота рельефа", &p.terrainAmplitude, 1.0f, 20.0f, "%.1f");
        ImGui::SliderFloat("Масштаб шума", &p.terrainFrequency, 0.03f, 0.5f, "%.2f");
        ImGui::SliderFloat("Облачность", &p.cloudDensity, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Плотность леса", &p.treeDensity, 0.0f, 1.0f, "%.2f");
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
                        typeIndex_ = static_cast<int>(p.type);
                        colorModeIdx_ = p.colorMode;
                        if (p.type == FractalType::Terrain3D) {
                            camera_->setView(glm::vec3(10.0f, 3.5f, -14.0f),
                                             glm::vec3(0.0f, 2.0f, 0.0f));
                            camera_->setFov(55.0f);
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
        Config::saveFractal(path, p);
        presetsLoaded_ = false;
        loadPresetsIntoList();
    }
}

// ---------------------------------------------------------------------------
//  Сжатие
// ---------------------------------------------------------------------------

void ControlPanel::drawCompressionSection() {
    ImGui::Text("Фрактальное сжатие");
    if (ImGui::Button("Сжать вид (Mandelbrot 2D, 256x256)")) {
        compressCurrentView();
    }
    if (compressionDone_) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", compressionInfo_.c_str());
    }
}

void ControlPanel::compressCurrentView() {
    const int size = 256;
    const int maxIter = std::max(16, params_->iterations);

    // центр и масштаб берутся из положения камеры (2D: cam.xy = центр, m2dZoom)
    const double cx = static_cast<double>(camera_->position().x);
    const double cy = static_cast<double>(camera_->position().y);
    const double zoom = std::max(0.05, static_cast<double>(params_->m2dZoom));
    const double scale = 3.5 / zoom;

    utils::Image img;
    img.width = size;
    img.height = size;
    img.channels = 1;
    img.data.resize(static_cast<std::size_t>(size) * size);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const double u = (static_cast<double>(x) + 0.5) / size - 0.5;
            const double v = (static_cast<double>(y) + 0.5) / size - 0.5;
            const Complex c(cx + u * scale, cy + v * scale);
            const int it = Mandelbrot2D::escapeTime(c, maxIter);
            img.data[static_cast<std::size_t>(y) * size + x] =
                static_cast<std::uint8_t>((it * 255) / maxIter);
        }
    }

    compression::Encoder enc;
    const auto encoded = enc.encode(img, 8, 2);
    if (encoded.codes.empty()) {
        compressionInfo_ = "Ошибка: кодер не вернул результат (изображение слишком маленькое?)";
        compressionDone_ = false;
        return;
    }

    const auto decoded = compression::Decoder::decode(encoded, 8);
    const double mse = compression::computeMSE(img, decoded);
    const double psnr = compression::computePSNR(mse);
    const double ratio = compression::compressionRatio(img.data.size(), encoded.byteSize());

    utils::ImageLoader::savePng("fractal_original.png", img);
    utils::ImageLoader::savePng("fractal_decoded.png", decoded);
    encoded.writeFile("fractal_compressed.frax");

    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "MSE: %.2f\nPSNR: %.2f dB\nКоэффициент сжатия: %.2f:1\n"
                  "Сохранено: fractal_original.png, fractal_decoded.png, fractal_compressed.frax",
                  mse, psnr, ratio);
    compressionInfo_ = buf;
    compressionDone_ = true;
}

} // namespace ars::gui