#pragma once
#include "Common.hpp"

namespace aqua {

class Shader {
public:
    Shader() = default;
    Shader(const std::string& vertexSource, const std::string& fragmentSource);
    static Shader fromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void compile(const std::string& vertexSource, const std::string& fragmentSource);
    void use() const;
    GLuint id() const { return program_; }

    GLint loc(const char* name) const;
    void set(const char* name, int value) const;
    void set(const char* name, float value) const;
    void set(const char* name, const glm::vec2& value) const;
    void set(const char* name, const glm::vec3& value) const;
    void set(const char* name, const glm::vec4& value) const;
    void set(const char* name, const glm::mat4& value) const;

private:
    GLuint program_ = 0;
    static GLuint compileStage(GLenum type, const std::string& source);
    static std::string readTextFile(const std::string& path);
};

std::string glslHeader();

} // namespace aqua
