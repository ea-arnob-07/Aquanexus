# Validation notes

- C++17 translation units were syntax checked after the photoreal rebuild.
- `GLM_ENABLE_EXPERIMENTAL` is defined before `glm/gtx/norm.hpp` is included.
- The old GLSL custom function name `noise2` is not used. Procedural shaders use uniquely named helpers such as `valueNoise2D`, `skyNoise`, and `waterNoise` to avoid collision with legacy GLSL noise built-ins.
- CMake links OpenGL, GLFW and GLEW through MSYS2/pkg-config and locates GLM headers.
- The MSYS2 build script refuses to build from the wrong MSYS2 environment and asks for UCRT64.

A Windows `.exe` is intentionally not bundled because it should be linked against the libraries installed in the user's own MSYS2 UCRT64 environment.
