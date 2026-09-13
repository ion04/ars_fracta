#include "gui/Viewport.h"
#include "gui/ControlPanel.h"
#include "render/Renderer.h"

#include <imgui.h>

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ars::gui {

Viewport::Viewport(render::Renderer& renderer, core::fractal::FractalParams& params)
    : renderer_(&renderer), params_(&params) {}

void Viewport::draw(float /*deltaTime*/) {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(kPanelWidth, 0.0f));
    ImGui::SetNextWindowSize(
        ImVec2(io.DisplaySize.x - kPanelWidth, io.DisplaySize.y - kStatusBarHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Видовое окно", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const bool hovered = ImGui::IsWindowHovered();

    // --- мышиный ввод ---
    if (hovered) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            renderer_->camera().orbit(io.MouseDelta.x * 0.15f,
                                      io.MouseDelta.y * 0.15f);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            const glm::vec2 d(io.MouseDelta.x, io.MouseDelta.y);
            const float denom = std::max(avail.x, 1.0f);
            renderer_->camera().pan(d / denom);
        }
        if (io.MouseWheel != 0.0f) {
            if (params_ && params_->type == core::fractal::FractalType::Mandelbrot2D) {
                // в 2D зум управляется m2dZoom, а не дистанцией камеры
                float z = params_->m2dZoom * std::pow(1.15f, io.MouseWheel);
                params_->m2dZoom = std::max(0.01f, std::min(30.0f, z));
            } else {
                renderer_->camera().zoom(io.MouseWheel * 0.07f);
            }
        }
    }

    // --- изображение (сохраняем пропорции кадра) ---
    const GLuint tex = renderer_->frameTexture();
    if (tex) {
        const int fw = renderer_->width();
        const int fh = renderer_->height();
        ImVec2 imgSize = avail;
        if (fw > 0 && fh > 0) {
            const float targetAspect = static_cast<float>(fw) / static_cast<float>(fh);
            const float availAspect = avail.x / std::max(avail.y, 1.0f);
            if (availAspect > targetAspect) {
                imgSize.x = avail.y * targetAspect;
            } else {
                imgSize.y = avail.x / targetAspect;
            }
        }
        const ImVec2 offset((avail.x - imgSize.x) * 0.5f,
                            (avail.y - imgSize.y) * 0.5f);
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offset.x,
                                   ImGui::GetCursorPosY() + offset.y));
        ImGui::Image((ImTextureID)(intptr_t)(tex),
                     imgSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() - offset.x,
                                   ImGui::GetCursorPosY() - offset.y));
        (void)p0;
    }

    // --- оверлей ---
    const ImVec2 winPos = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 col = IM_COL32(255, 255, 255, 210);

    const std::string fps = "FPS: " + std::to_string(static_cast<int>(io.Framerate));
    dl->AddText(ImVec2(winPos.x + 12, winPos.y + 12), col, fps.c_str());

    const glm::vec3 pos = renderer_->camera().position();
    char posBuf[64];
    std::snprintf(posBuf, sizeof(posBuf), "Pos: %.1f %.1f %.1f",
                  static_cast<double>(pos.x), static_cast<double>(pos.y),
                  static_cast<double>(pos.z));
    dl->AddText(ImVec2(winPos.x + 12, winPos.y + 30), col, posBuf);

    ImGui::End();
}

} // namespace ars::gui