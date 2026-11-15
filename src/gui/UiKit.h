#pragma once
// gui/UiKit.h — тема, шрифты и обёртки виджетов для панели управления.

#include <imgui.h>
#include <cfloat>
#include <cstdio>
#include <initializer_list>

namespace ars::gui::ui {

// Шрифт заголовков секций (заполняется в loadFont; nullptr — обычный шрифт).
inline ImFont* headingFont = nullptr;

// ---------------------------------------------------------------------------
//  Шрифты: вызвать ОДИН раз после ImGui::CreateContext(), до первого кадра.
//  Пути перебираются по порядку; берётся первый реально открывшийся файл.
//  Он грузится дважды: основным размером и крупнее — для заголовков.
//  Если ни один файл не открылся — встроенный шрифт ImGui (без кириллицы).
// ---------------------------------------------------------------------------
inline void loadFont(std::initializer_list<const char*> ttfPaths,
                     float sizePx = 16.0f, float headingPx = 22.0f) {
    ImGuiIO& io = ImGui::GetIO();
    const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();
    for (const char* path : ttfPaths) {
        if (!path) continue;
        // ImGui падает на отсутствующем файле, поэтому проверяем заранее.
        std::FILE* file = std::fopen(path, "rb");
        if (!file) continue;
        std::fclose(file);

        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.PixelSnapH = true;
        // Первый добавленный шрифт становится шрифтом по умолчанию.
        if (io.Fonts->AddFontFromFileTTF(path, sizePx, &cfg, ranges)) {
            ImFontConfig cfgHead;
            cfgHead.OversampleH = 2;
            cfgHead.PixelSnapH = true;
            headingFont = io.Fonts->AddFontFromFileTTF(path, headingPx, &cfgHead, ranges);
            return;
        }
    }
    io.Fonts->AddFontDefault();  // файлы не найдены — не падаем
}

// ---------------------------------------------------------------------------
//  Тема: один акцентный цвет, скругления, нормальные отступы.
//  dpiScale — коэффициент масштаба окна (1.0 для 96 DPI).
// ---------------------------------------------------------------------------
inline void applyTheme(float dpiScale = 1.0f) {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();  // сброс к умолчаниям

    s.WindowPadding     = ImVec2(12, 12);
    s.FramePadding      = ImVec2(8, 5);
    s.ItemSpacing       = ImVec2(8, 7);
    s.ItemInnerSpacing  = ImVec2(6, 4);
    s.IndentSpacing     = 14.0f;
    s.ScrollbarSize     = 12.0f;
    s.GrabMinSize       = 14.0f;

    s.WindowRounding    = 0.0f;
    s.ChildRounding     = 6.0f;
    s.FrameRounding     = 5.0f;
    s.PopupRounding     = 6.0f;
    s.GrabRounding      = 5.0f;
    s.ScrollbarRounding = 6.0f;

    s.WindowBorderSize  = 0.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;

    const ImVec4 accent      (0.36f, 0.62f, 1.00f, 1.00f);
    const ImVec4 accentActive(0.28f, 0.52f, 0.92f, 1.00f);
    const ImVec4 bg          (0.075f, 0.080f, 0.095f, 1.00f);
    const ImVec4 panel       (0.115f, 0.125f, 0.150f, 1.00f);
    const ImVec4 panelHover  (0.150f, 0.165f, 0.195f, 1.00f);
    const ImVec4 text        (0.92f, 0.93f, 0.95f, 1.00f);
    const ImVec4 textDim     (0.55f, 0.58f, 0.64f, 1.00f);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                 = text;
    c[ImGuiCol_TextDisabled]         = textDim;
    c[ImGuiCol_WindowBg]             = bg;
    c[ImGuiCol_ChildBg]              = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.11f, 0.13f, 0.98f);
    c[ImGuiCol_Border]               = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_FrameBg]              = panel;
    c[ImGuiCol_FrameBgHovered]       = panelHover;
    c[ImGuiCol_FrameBgActive]        = panelHover;
    c[ImGuiCol_SliderGrab]           = accent;
    c[ImGuiCol_SliderGrabActive]     = accentActive;
    c[ImGuiCol_CheckMark]            = accent;
    c[ImGuiCol_Button]               = panel;
    c[ImGuiCol_ButtonHovered]        = accent;
    c[ImGuiCol_ButtonActive]         = accentActive;
    c[ImGuiCol_Header]               = panel;
    c[ImGuiCol_HeaderHovered]        = panelHover;
    c[ImGuiCol_HeaderActive]         = panelHover;
    c[ImGuiCol_Separator]            = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(1, 1, 1, 0.15f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1, 1, 1, 0.25f);
    c[ImGuiCol_ScrollbarGrabActive]  = accent;
    c[ImGuiCol_TextSelectedBg]       = ImVec4(accent.x, accent.y, accent.z, 0.35f);

    s.ScaleAllSizes(dpiScale);
}

// ---------------------------------------------------------------------------
//  Виджеты
// ---------------------------------------------------------------------------

// Тултип с задержкой — вызывать сразу после виджета.
inline void tip(const char* text) {
    if (text && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", text);
}

// Крупный заголовок секции (шрифт headingFont, если загружен).
inline void heading(const char* text) {
    if (headingFont) ImGui::PushFont(headingFont);
    ImGui::SeparatorText(text);
    if (headingFont) ImGui::PopFont();
}

// Подпись сверху, слайдер на всю ширину панели (никаких обрезанных подписей).
// Ctrl+клик — ввод числа; двойной клик или ПКМ — сброс к значению def.
inline bool sliderFloat(const char* label, float* v, float lo, float hi,
                        float def, const char* fmt = "%.2f",
                        bool logarithmic = false, const char* hint = nullptr) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp;
    if (logarithmic) flags |= ImGuiSliderFlags_Logarithmic;
    bool changed = ImGui::SliderFloat("##v", v, lo, hi, fmt, flags);
    if (ImGui::IsItemHovered() &&
        (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
        *v = def;
        changed = true;
    }
    tip(hint);
    ImGui::PopID();
    return changed;
}

inline bool sliderInt(const char* label, int* v, int lo, int hi, int def,
                      bool logarithmic = false, const char* hint = nullptr) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp;
    if (logarithmic) flags |= ImGuiSliderFlags_Logarithmic;
    bool changed = ImGui::SliderInt("##v", v, lo, hi, "%d", flags);
    if (ImGui::IsItemHovered() &&
        (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
        *v = def;
        changed = true;
    }
    tip(hint);
    ImGui::PopID();
    return changed;
}

// Combo с подписью сверху.
inline bool combo(const char* label, int* idx, const char* const* items, int n,
                  const char* hint = nullptr) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = ImGui::Combo("##v", idx, items, n);
    tip(hint);
    ImGui::PopID();
    return changed;
}

}  // namespace ars::gui::ui