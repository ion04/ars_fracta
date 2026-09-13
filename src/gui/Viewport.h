#pragma once

#include <memory>

#include "core/fractal/Fractal.h"

namespace ars::render {
class Renderer;
}

namespace ars::gui {

/**
 * @brief ImGui-виджет для отображения текстуры рендерера и обработки
 *        мышиного ввода (вращение, панорама, зум).
 */
class Viewport {
public:
    Viewport(render::Renderer& renderer, core::fractal::FractalParams& params);

    /// Нарисовать виджет (вызывать каждый кадр).
    void draw(float deltaTime);

private:
    render::Renderer* renderer_ = nullptr;
    core::fractal::FractalParams* params_ = nullptr;
};

} // namespace ars::gui