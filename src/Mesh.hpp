#pragma once
#include "Common.hpp"

namespace aqua {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

class Mesh {
public:
    Mesh() = default;
    Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void upload(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void draw() const;
    bool valid() const { return vao_ != 0; }

    static Mesh cube();
    static Mesh plane(int divisions = 1);
    static Mesh uvSphere(int slices = 28, int stacks = 16);
    static Mesh cylinder(int segments = 32, bool capped = true);
    static Mesh cone(int segments = 32);
    static Mesh trapezoidPrism();
    static Mesh leafBlade(int segments = 10);
    static Mesh disc(int segments = 32);
    static Mesh roundedPondBank(int perimeterSegments = 96);
    static Mesh roundedWaterSurface(int perimeterSegments = 128, int radialSegments = 40, float radius = 0.490f);
    static Mesh irregularRock(int slices = 18, int stacks = 10);
    static Mesh grassPatch(int bladeCount = 1600, uint32_t seed = 1u);

private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLsizei indexCount_ = 0;
    void destroy();
};

} // namespace aqua
