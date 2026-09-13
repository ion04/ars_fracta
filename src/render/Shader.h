#pragma once

#include <map>
#include <string>

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace ars::render {

/**
 * @brief Загрузка, компиляция и использование OpenGL-шейдеров.
 */
class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& o) noexcept;
    Shader& operator=(Shader&& o) noexcept;

    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    void use() const;

    GLuint id() const { return program_; }

    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, const glm::vec2& value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;

private:
    GLuint program_ = 0;
    mutable std::map<std::string, GLint> locations_;

    static std::string readFile(const std::string& path);
    static GLuint compileShader(GLenum type, const std::string& source,
                                const std::string& name);
    GLint location(const std::string& name) const;
};

} // namespace ars::render