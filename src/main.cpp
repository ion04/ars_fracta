#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "core/fractal/Fractal.h"
#include "gui/MainWindow.h"
#include "render/Window.h"
#include "utils/Config.h"
#include "utils/ImageLoader.h"
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
    bool screenshot = false;
    std::string screenshotPath;
    std::string presetPath;
    if (argc > 1 && std::string(argv[1]) == "--screenshot") {
        if (argc < 3) {
            std::cerr << "Usage: ars_fracta --screenshot <output.png> [preset.json]\n";
            return 2;
        }
        screenshot = true;
        screenshotPath = argv[2];
        if (argc > 3) presetPath = argv[3];
    } else if (argc > 1) {
        configPath = argv[1];
    }

    utils::ConfigData config;
    utils::Config::load(configPath, config);
    if (!presetPath.empty() && !utils::Config::loadFractal(presetPath, config.fractal)) {
        std::cerr << "Cannot load preset: " << presetPath << '\n';
        return 2;
    }
    // конфиг и пресет прочитаны из файла напрямую — валидируем сразу
    config.fractal = core::fractal::sanitizeParams(config.fractal);

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
    // стартовая камера по типу (пейзаж/побережье); коэффициенты — в render/Camera.h
    app.applyDefaultCamera();

    if (screenshot) {
        std::vector<std::uint8_t> pixels;
        int captureWidth = 0;
        int captureHeight = 0;
        utils::Image image;
        if (!app.renderScreenshot(pixels, captureWidth, captureHeight)) {
            std::cerr << "Cannot render screenshot\n";
            return 3;
        }
        image.width = captureWidth;
        image.height = captureHeight;
        image.channels = 4;
        image.data = std::move(pixels);
        if (!utils::ImageLoader::savePng(screenshotPath, image)) {
            std::cerr << "Cannot save screenshot: " << screenshotPath << '\n';
            return 3;
        }
        std::cout << "Saved screenshot: " << screenshotPath << " ("
                  << captureWidth << "x" << captureHeight << ")\n";
        return 0;
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
