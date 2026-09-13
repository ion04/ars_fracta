#include "render/Shader.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ars::render {

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

Shader::Shader(Shader&& o) noexcept {
    program_ = o.program_;
    locations_ = std::move(o.locations_);
    o.program_ = 0;
}

Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) {
        if (program_) glDeleteProgram(program_);
        program_ = o.program_;
        locations_ = std::move(o.locations_);
        o.program_ = 0;
    }
    return *this;
}

std::string Shader::readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    if (in) ss << in.rdbuf();
    return ss.str();
}

GLuint Shader::compileShader(GLenum type, const std::string& source, const std::string& name) {
    const GLuint id = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(std::max(len, 1)), '\0');
        glGetShaderInfoLog(id, len, nullptr, log.data());
        std::cerr << "[Shader] Failed to compile '" << name << "':\n" << log << '\n';
        glDeleteShader(id);
        return 0;
    }
    return id;
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    if (program_) {
        glDeleteProgram(program_);
        program_ = 0;
        locations_.clear();
    }

    const std::string vs = readFile(vertexPath);
    const std::string fs = readFile(fragmentPath);
    if (vs.empty() || fs.empty()) {
        std::cerr << "[Shader] Cannot read '" << vertexPath << "' or '" << fragmentPath << "'\n";
        return false;
    }

    const GLuint v = compileShader(GL_VERTEX_SHADER, vs, vertexPath);
    if (!v) return false;
    const GLuint f = compileShader(GL_FRAGMENT_SHADER, fs, fragmentPath);
    if (!f) {
        glDeleteShader(v);
        return false;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(std::max(len, 1)), '\0');
        glGetProgramInfoLog(program, len, nullptr, log.data());
        std::cerr << "[Shader] Link error:\n" << log << '\n';
        glDeleteProgram(program);
        return false;
    }

    program_ = program;
    return true;
}

void Shader::use() const { glUseProgram(program_); }

GLint Shader::location(const std::string& name) const {
    const auto it = locations_.find(name);
    if (it != locations_.end()) return it->second;
    const GLint loc = glGetUniformLocation(program_, name.c_str());
    locations_[name] = loc;
    return loc;
}

void Shader::setBool(const std::string& name, bool value) const {
    glUniform1i(location(name), value ? 1 : 0);
}

void Shader::setInt(const std::string& name, int value) const {
    glUniform1i(location(name), value);
}

void Shader::setFloat(const std::string& name, float value) const {
    glUniform1f(location(name), value);
}

void Shader::setVec2(const std::string& name, const glm::vec2& value) const {
    glUniform2fv(location(name), 1, &value[0]);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    glUniform3fv(location(name), 1, &value[0]);
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const {
    glUniform4fv(location(name), 1, &value[0]);
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const {
    glUniformMatrix4fv(location(name), 1, GL_FALSE, &value[0][0]);
}

} // namespace ars::render