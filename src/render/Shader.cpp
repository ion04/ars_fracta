#include "render/Shader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace ars::render {

namespace {

std::string preprocessShader(const std::string& path, const std::string& baseDir,
                             std::unordered_set<std::string>& included, bool isMainFile = true) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::ostringstream ss;
    std::string line;
    bool versionWritten = false;
    while (std::getline(in, line)) {
        std::string trimmed = line;
        size_t first = trimmed.find_first_not_of(" \t");
        if (first != std::string::npos) trimmed = trimmed.substr(first);
        // Preserve #version only in main file, skip in includes
        if (trimmed.rfind("#version", 0) == 0) {
            if (isMainFile && !versionWritten) {
                ss << line << '\n';
                versionWritten = true;
            }
            continue;
        }
        // Check for #include
        if (trimmed.rfind("#include", 0) == 0) {
            size_t quote1 = trimmed.find('"');
            size_t quote2 = trimmed.find('"', quote1 + 1);
            if (quote1 != std::string::npos && quote2 != std::string::npos && quote2 > quote1) {
                std::string includePath = trimmed.substr(quote1 + 1, quote2 - quote1 - 1);
                std::string fullPath = baseDir + "/" + includePath;
                fullPath = std::filesystem::weakly_canonical(fullPath).string();
                if (included.insert(fullPath).second) {
                    ss << preprocessShader(fullPath, std::filesystem::path(fullPath).parent_path().string(), included, false);
                }
            }
        } else {
            ss << line << '\n';
        }
    }
    return ss.str();
}

} // anonymous namespace

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
    // Preprocess fragment shader to resolve #include directives
    std::unordered_set<std::string> included;
    std::string baseDir = std::filesystem::path(fragmentPath).parent_path().string();
    const std::string fs = preprocessShader(fragmentPath, baseDir, included);
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
    if (loc == -1) {
        std::cerr << "[Shader] Warning: uniform '" << name << "' not found\n";
    }
    locations_[name] = loc;
    return loc;
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

void Shader::bindUniformBlock(const std::string& blockName, GLuint bindingPoint) const {
    GLuint index = glGetUniformBlockIndex(program_, blockName.c_str());
    if (index == GL_INVALID_INDEX) {
        std::cerr << "[Shader] Warning: uniform block '" << blockName << "' not found\n";
        return;
    }
    glUniformBlockBinding(program_, index, bindingPoint);
}

void Shader::setUniformBuffer(GLuint buffer, const void* data, GLsizeiptr size) const {
    glBindBuffer(GL_UNIFORM_BUFFER, buffer);
    glBufferData(GL_UNIFORM_BUFFER, size, data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

} // namespace ars::render
