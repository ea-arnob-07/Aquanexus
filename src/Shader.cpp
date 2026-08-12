#include "Shader.hpp"
#include <fstream>

namespace aqua {

std::string glslHeader() {
#ifdef __EMSCRIPTEN__
    return "#version 300 es\nprecision highp float;\nprecision highp int;\n";
#else
    return "#version 330 core\n";
#endif
}



std::string Shader::readTextFile(const std::string& path) {
    const std::array<std::string,4> candidates = {path, std::string("../")+path, std::string("../../")+path, std::string("../../../")+path};
    for(const auto& c : candidates){
        std::ifstream in(c, std::ios::binary);
        if(in){ std::ostringstream ss; ss << in.rdbuf(); return ss.str(); }
    }
    throw std::runtime_error("Cannot open shader file: " + path + " (tried project root and build directories)");
}

Shader Shader::fromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    return Shader(glslHeader() + readTextFile(vertexPath), glslHeader() + readTextFile(fragmentPath));
}

Shader::Shader(const std::string& vs, const std::string& fs) { compile(vs, fs); }
Shader::~Shader() { if (program_) glDeleteProgram(program_); }

Shader::Shader(Shader&& o) noexcept : program_(o.program_) { o.program_ = 0; }
Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) {
        if (program_) glDeleteProgram(program_);
        program_ = o.program_;
        o.program_ = 0;
    }
    return *this;
}

GLuint Shader::compileStage(GLenum type, const std::string& src) {
    GLuint shader = glCreateShader(type);
    const char* s = src.c_str();
    glShaderSource(shader, 1, &s, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(std::max(1, len)), '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed:\n" + log + "\n--- source ---\n" + src);
    }
    return shader;
}

void Shader::compile(const std::string& vs, const std::string& fs) {
    GLuint v = compileStage(GL_VERTEX_SHADER, vs);
    GLuint f = compileStage(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = GL_FALSE; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(std::max(1, len)), '\0');
        glGetProgramInfoLog(p, len, nullptr, log.data());
        glDeleteProgram(p);
        throw std::runtime_error("Shader link failed:\n" + log);
    }
    if (program_) glDeleteProgram(program_);
    program_ = p;
}

void Shader::use() const { glUseProgram(program_); }
GLint Shader::loc(const char* n) const { return glGetUniformLocation(program_, n); }
void Shader::set(const char* n, int v) const { glUniform1i(loc(n), v); }
void Shader::set(const char* n, float v) const { glUniform1f(loc(n), v); }
void Shader::set(const char* n, const glm::vec2& v) const { glUniform2fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, const glm::vec3& v) const { glUniform3fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, const glm::vec4& v) const { glUniform4fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, const glm::mat4& v) const { glUniformMatrix4fv(loc(n), 1, GL_FALSE, glm::value_ptr(v)); }

} // namespace aqua
