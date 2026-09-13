#include "render/Window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <utility>

namespace ars::render {

Window::Window() : Window(Settings{}) {}

Window::Window(const Settings& settings) : settings_(settings),
                                            width_(settings_.width),
                                            height_(settings_.height) {}

Window::~Window() {
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();
}

bool Window::init() {
    if (!glfwInit()) {
        std::cerr << "[Window] glfwInit() failed\n";
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, settings_.glMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, settings_.glMinor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(width_, height_, settings_.title.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "[Window] Cannot create GLFW window\n";
        glfwTerminate();
        return false;
    }

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