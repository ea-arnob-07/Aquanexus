#pragma once

// GLM's GTX helpers are intentionally used for length2 / component helpers.
// Newer GLM releases require this opt-in before any GLM include.
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>
#else
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace aqua {

constexpr float PI = 3.14159265358979323846f;

inline float clampf(float v, float a, float b) { return std::max(a, std::min(b, v)); }
inline float saturate(float v) { return clampf(v, 0.0f, 1.0f); }
inline float smoothstep(float a, float b, float x) {
    float t = saturate((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline glm::vec3 lerp(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * t; }
inline float fractf(float x) { return x - std::floor(x); }

inline uint32_t hashU32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x;
}
inline float hash01(uint32_t x) { return float(hashU32(x) & 0x00ffffffu) / float(0x01000000u); }

inline glm::mat4 alignZTo(const glm::vec3& direction) {
    glm::vec3 z = glm::normalize(direction);
    glm::vec3 helper = std::fabs(z.y) > 0.95f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    glm::vec3 x = glm::normalize(glm::cross(helper, z));
    glm::vec3 y = glm::cross(z, x);
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(x, 0.0f);
    m[1] = glm::vec4(y, 0.0f);
    m[2] = glm::vec4(z, 0.0f);
    return m;
}

inline glm::mat4 alignYTo(const glm::vec3& direction) {
    glm::vec3 y = glm::normalize(direction);
    glm::vec3 helper = std::fabs(y.z) > 0.95f ? glm::vec3(1, 0, 0) : glm::vec3(0, 0, 1);
    glm::vec3 x = glm::normalize(glm::cross(y, helper));
    glm::vec3 z = glm::normalize(glm::cross(x, y));
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(x, 0.0f);
    m[1] = glm::vec4(y, 0.0f);
    m[2] = glm::vec4(z, 0.0f);
    return m;
}

inline glm::vec3 bezierPoint(const std::array<glm::vec3, 4>& p, float t) {
    float u = 1.0f - t;
    return u*u*u*p[0] + 3.0f*u*u*t*p[1] + 3.0f*u*t*t*p[2] + t*t*t*p[3];
}

inline glm::vec3 bezierTangent(const std::array<glm::vec3, 4>& p, float t) {
    float u = 1.0f - t;
    glm::vec3 d = 3.0f*u*u*(p[1]-p[0]) + 6.0f*u*t*(p[2]-p[1]) + 3.0f*t*t*(p[3]-p[2]);
    if (glm::length2(d) < 1e-6f) return glm::vec3(0,0,1);
    return glm::normalize(d);
}

inline glm::vec3 projectToScreen(const glm::vec3& world, const glm::mat4& viewProj, int width, int height, bool& visible) {
    glm::vec4 clip = viewProj * glm::vec4(world, 1.0f);
    visible = clip.w > 0.001f;
    if (!visible) return glm::vec3(0);
    glm::vec3 ndc = glm::vec3(clip.x, clip.y, clip.z) / clip.w;
    visible = ndc.z >= -1.0f && ndc.z <= 1.0f && ndc.x > -1.2f && ndc.x < 1.2f && ndc.y > -1.2f && ndc.y < 1.2f;
    return glm::vec3((ndc.x * 0.5f + 0.5f) * width,
                     (1.0f - (ndc.y * 0.5f + 0.5f)) * height,
                     ndc.z);
}

} // namespace aqua
