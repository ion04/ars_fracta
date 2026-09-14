#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

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
    // вид по умолчанию для пейзажа: камера над рельефом, смотрит на горизонт
    if (config.fractal.type == core::fractal::FractalType::Terrain3D) {
        // высота камеры зависит от амплитуды рельефа, чтобы не оказаться под горами
        const float eyeY = std::max(3.5f, config.fractal.terrainAmplitude * 1.1f);
        const glm::vec3 eye(10.0f, eyeY, -14.0f);
        app.setCameraView(eye, eye + glm::vec3(-10.0f, -2.0f, 14.0f));
        app.setCameraFov(55.0f);
    } else if (config.fractal.type == core::fractal::FractalType::Coast3D) {
        // вид «Морское побережье»: камера над водой, остров — в центре кадра
        const float scale = 0.15f / std::max(config.fractal.m2dZoom, 0.01f);
        const glm::vec3 target(config.fractal.coastCenterRe / scale * 0.6f, 0.8f,
                               config.fractal.coastCenterIm / scale * 0.6f);
        app.setCameraView(target + glm::vec3(0.0f, 9.0f, -18.0f), target);
        app.setCameraFov(55.0f);
    }

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