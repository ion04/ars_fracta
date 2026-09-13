#pragma once

#include <string>

struct GLFWwindow;

namespace ars::render {

/**
 * @brief Создание GLFW-окна с OpenGL 3.3 core-контекстом.
 */
class Window {
public:
    struct Settings {
        int width = 1280;
        int height = 720;
        std::string title = "ars_fracta — Фракталы и фрактальное сжатие (курсовая работа)";
        bool vsync = true;
        int glMajor = 3;
        int glMinor = 3;
    };

    Window();
    explicit Window(const Settings& settings);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /// Инициализировать GLFW, создать окно и загрузить функции OpenGL (GLAD).
    bool init();

    void pollEvents();
    void swapBuffers();
    bool shouldClose() const;

    GLFWwindow* handle() const { return window_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    static void onFramebufferResize(GLFWwindow* w, int fbWidth, int fbHeight);

    Settings settings_;
    GLFWwindow* window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

} // namespace ars::render