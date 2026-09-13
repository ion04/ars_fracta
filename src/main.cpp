#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "gui/MainWindow.h"
#include "render/Window.h"
#include "utils/Config.h"
#include "utils/Logger.h"

int main(int argc, char** argv) {
    using namespace ars;

#ifdef _WIN32
    // Кириллица в исходниках сохранена в UTF-8; переключаем консоль Windows
    // на UTF-8, чтобы логи (Logger) отображались корректно, а не кракозябрами.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::string configPath = "config.json";
    if (argc > 1) configPath = argv[1];

    utils::ConfigData config;
    utils::Config::load(configPath, config);

    render::Window::Settings ws;
    ws.width = std::max(320, config.windowWidth);
    ws.height = std::max(240, config.windowHeight);

    render::Window window(ws);
    if (!window.init()) {
        utils::Logger::instance().error("Не удалось создать окно и OpenGL-контекст");
        return 1;
    }
    utils::Logger::instance().info("ars_fracta запущен");

    gui::MainWindow app(window);
    app.setParams(config.fractal);
    if (!app.init()) return 1;

    double last = glfwGetTime();
    while (!window.shouldClose()) {
        window.pollEvents();

        if (glfwGetKey(window.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
        }

        const double now = glfwGetTime();
        const float dt = static_cast<float>(std::min(now - last, 0.1));
        last = now;

        app.runFrame(dt);
        window.swapBuffers();
    }

    // сохранить конфигурацию при выходе
    utils::ConfigData out;
    out.windowWidth = window.width();
    out.windowHeight = window.height();
    out.fractal = app.params();
    utils::Config::save(configPath, out);

    return 0;
}