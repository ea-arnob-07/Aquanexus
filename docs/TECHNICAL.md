# Renderer / Simulation Architecture

## Frame pipeline

1. 4096x4096 directional-light shadow pass.
2. Opaque HDR scene pass containing sky, rural environment, ponds, fish, sensors, pumps and machinery.
3. Opaque color/depth copy to a second scene framebuffer.
4. Refractive blue-water pass sampling the opaque color/depth buffers.
5. Transparent pipe pass for inner water, moving flow particles, discharge bubbles and outer glass.
6. Bright-pass extraction at half resolution.
7. Eight-pass separable Gaussian bloom blur.
8. Final post-process: bloom composite, edge-aware smoothing, exposure, ACES-like mapping, gamma, contrast, vignette and grain.
9. 2D telemetry HUD.

## PBR approximation

The desktop path uses a compact Cook-Torrance/GGX-style BRDF for direct sunlight, Schlick Fresnel and roughness/metallic material parameters. Procedural material modes layer deterministic value-noise/FBM to add scale-appropriate variation to ground, earthen banks, leaves, wood, thatch, metal, concrete and sludge.

## Water

The water vertex shader combines four moving wave fields. The fragment shader adds smaller stochastic capillary normal variation, Fresnel reflection, screen-space refraction, approximate depth absorption, water-quality/turbidity tint, sun glints, foam and atmospheric color.

## Pipes

Each connection is a cubic Bezier path converted to a high-resolution tube mesh. Two concentric tubes are drawn: a blue inner water volume and a low-alpha reflective outer glass shell. Fan speed controls animated packets and outlet bubbles.

## Sensors

Pond 2 contains six visible probes. Sensor values are updated every simulation tick and coupled to pond waste, fish load, circulation and water level.

## Build validation

The project was syntax-checked as C++17 after generation using a local compile-stub validation environment. The earlier GLM GTX issue is prevented in two places: `GLM_ENABLE_EXPERIMENTAL` is defined before GLM includes in `Common.hpp` and also supplied as a target compile definition in CMake.
