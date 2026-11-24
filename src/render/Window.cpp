#include "render/Window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <array>
#include <iostream>
#include <vector>

#include "utils/ImageLoader.h"

namespace ars::render {

namespace {

void applyWindowIcon(GLFWwindow* window) {
    constexpr std::size_t kIconCount = 4;
    const std::array<const char*, kIconCount> files = {
        ARS_FRACTA_ASSETS_DIR "/icon/icon_16.png",
        ARS_FRACTA_ASSETS_DIR "/icon/icon_32.png",
        ARS_FRACTA_ASSETS_DIR "/icon/icon_48.png",
        ARS_FRACTA_ASSETS_DIR "/icon/icon_256.png",
    };
    std::array<utils::Image, kIconCount> images;
    std::vector<GLFWimage> glfwImages;
    glfwImages.reserve(kIconCount);
    for (std::size_t i = 0; i < kIconCount; ++i) {
        if (!utils::ImageLoader::loadPng(files[i], images[i])) continue;
        glfwImages.push_back(GLFWimage{
            images[i].width, images[i].height, images[i].data.data()});
    }
    if (!glfwImages.empty())
        glfwSetWindowIcon(window, static_cast<int>(glfwImages.size()),
                          glfwImages.data());
}

} // namespace

Window::Window() : Window(Settings{}) {}

Window::Window(const Settings& settings) : settings_(settings),
                                            width_(settings_.width),
                                            height_(settings_.height) {}

Window::~Window() {
    if (window_) glfwDestroyWindow(window_);
    if (glfwInitialized_) glfwTerminate();
}

bool Window::init() {
    if (!glfwInit()) {
        std::cerr << "[Window] glfwInit() failed\n";
        return false;
    }
    glfwInitialized_ = true;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, settings_.glMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, settings_.glMinor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = settings_.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    if (settings_.fullscreen && monitor) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        width_ = mode->width;
        height_ = mode->height;
    }
    window_ = glfwCreateWindow(width_, height_, settings_.title.c_str(), monitor, nullptr);
    if (!window_) {
        std::cerr << "[Window] Cannot create GLFW window\n";
        return false;
    }

    applyWindowIcon(window_);

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(settings_.vsync ? 1 : 0);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, Window::onFramebufferResize);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "[Window] gladLoadGLLoader() failed\n";
        return false;
    }

    glfwGetFramebufferSize(window_, &width_, &height_);
    return true;
}

void Window::onFramebufferResize(GLFWwindow* w, int fbWidth, int fbHeight) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    self->width_ = fbWidth > 0 ? fbWidth : self->width_;
    self->height_ = fbHeight > 0 ? fbHeight : self->height_;
    glViewport(0, 0, self->width_, self->height_);
}

void Window::pollEvents() { glfwPollEvents(); }

void Window::swapBuffers() { glfwSwapBuffers(window_); }

bool Window::shouldClose() const { return glfwWindowShouldClose(window_) != 0; }

} // namespace ars::render
