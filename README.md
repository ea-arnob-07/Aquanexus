<div align="center">
  <h1 align="center">🌊 AquaNexus 🌊</h1>
  <p align="center">
    <strong>A real-time C++17 + raw OpenGL 3.3 aquaculture digital-twin demo for MSYS2 UCRT64.</strong>
  </p>

  <p align="center">
    <img src="https://img.shields.io/badge/C++-17-blue.svg?style=for-the-badge&logo=c%2B%2B" alt="C++17" />
    <img src="https://img.shields.io/badge/OpenGL-3.3_Core-5586A4.svg?style=for-the-badge&logo=opengl" alt="OpenGL" />
    <img src="https://img.shields.io/badge/CMake-Ready-064F8C.svg?style=for-the-badge&logo=cmake" alt="CMake" />
    <img src="https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge" alt="License" />
    <br>
    <img src="https://img.shields.io/badge/Platform-MSYS2_UCRT64-8A2BE2.svg?style=for-the-badge&logo=windows" alt="Platform" />
    <img src="https://img.shields.io/badge/Target-Desktop_&_WebGL-orange.svg?style=for-the-badge" alt="Target" />
  </p>
</div>

<hr>

## 🐟 Overview

This rebuild is aimed at the supplied rural reference: realistic earthen ponds, blue pond water, dense vegetation, village background, transparent circulation pipes, rotating fans, fish, debris accumulation, and live Pond-2 sensors.

### 🏞️ Default Composition:
- **Pond 1**: Near / lower-right
- **Pond 2**: Left-middle
- **Pond 3**: Far / upper-right
- **Pipe Network**: Triangular transparent circulation `P1 -> P2 -> P3 -> P1`

> 💡 *Note: `docs/reference_target.jpg` is the supplied visual reference. `docs/visual_goal.png` is the photoreal dashboard concept used as a secondary visual target.*

---

## 🎨 Why v4 looks less cartoon-like

The previous version had advanced features, but the scene was built from clean boxes and saturated flat colors. **v4 changes what matters visually:**

- 🌊 **Continuous rounded/eroded pond-basin mesh** instead of four box walls.
- 🪨 **Irregular embedded crest rocks** and a dark wet shoreline band.
- 📉 **Lower, physically believable pond water surface** relative to the bank.
- 🌾 **Thousands of procedural grass/rice/reed blades** rendered in large batched meshes.
- 💨 **Wind animation** in vegetation from the PBR vertex shader.
- 🌳 **Irregular organic bush geometry** instead of perfect spheres.
- 🍂 **Procedural micro-variation** for ground, bank, wood, thatch, metal, and vegetation.
- 🎨 **Restrained natural color palette** instead of game-like saturated greens.
- 🔎 **Depth-aware water optics** and post-process contact grounding.

---

## 🛠️ Rendering Stack

<details>
<summary><b>Click to expand Technology Stack</b></summary>

- 🖥️ **C++17**
- 🎮 **OpenGL 3.3 Core** on desktop
- 🖼️ **GLFW** window / input
- 🔌 **GLEW** OpenGL extension loading
- 📐 **GLM** mathematics
- 🔨 **CMake + Ninja**
- 🌐 **Emscripten / WebGL 2** optional web target
- 📜 **Runtime GLSL files** in `shaders/`
</details>

### 📸 Photoreal Rendering Features
* 🌟 **Lighting:** Cook-Torrance / GGX-style PBR direct lighting, Schlick Fresnel
* 🌤️ **Shadows & Sky:** 4096x4096 shadow map (weighted PCF), procedural atmospheric sky & clouds
* 🌿 **Materials:** Procedural 2D value noise + FBM materials, wind-deformed vegetation
* 🎬 **Post-Processing:** HDR render target, ACES filmic tone mapping, FXAA-style edge smoothing, ambient occlusion, half-resolution bloom, atmospheric distance haze, and vignette

### 💧 Water Optics
Each pond uses a subdivided animated surface rather than a flat transparent rectangle:
- Four directional wave bands & high-frequency capillary normal detail
- Fan-outlet radial ripples & physically inspired Fresnel reflection
- Screen-space refraction of the opaque scene
- **Linearized depth difference** between water and pond bottom (Beer-Lambert absorption)
- Depth-driven shoreline foam & GGX-like sun glints

### 🧪 Transparent Pipes and Circulation
- Cubic Bezier pipe routes with a high-resolution outer clear tube + inner water tube
- Refractive/Fresnel glass shader
- Individually controlled fan speeds
- **Realistic Flow:** Water packets only move while the corresponding fan is spinning (with outlet bubbles/turbulence)

---

## 🔬 Simulation Mechanics

> ⚠️ The simulation is a visual digital twin for demonstration, not a certified CFD or aquaculture-control model.

* 🌀 Fan speed transfers water between the connected ponds, creating small water-level differences.
* 💨 Circulation improves simulated dissolved oxygen (DO).
* 🐟 Waste & fish loading increase simulated ammonia and reduce DO.
* 📊 **Pond-2 live metrics:** pH, temperature, pressure, level, NH3, and DO update continuously.
* 🏊 Poor water quality changes fish speed and swimming depth.
* 🍂 Visible organic debris slowly accumulates in one corner of every pond.

*(Use **X** to switch 1x -> 5x -> 20x during a presentation)*

---

## 🚀 MSYS2 UCRT64 Install

Open **MSYS2 UCRT64** and update:
```bash
pacman -Syu
```
*If MSYS2 asks you to close/reopen the terminal, open **UCRT64** again, then install dependencies:*
```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-glfw \
  mingw-w64-ucrt-x86_64-glew \
  mingw-w64-ucrt-x86_64-glm
```

---

## 🏗️ Build Instructions

From the extracted project folder:
```bash
chmod +x build_msys2.sh
./build_msys2.sh
```
Run the application:
```bash
./build/bin/AquaVillage3D_Cinematic.exe
```

### ⚙️ Manual Build
```bash
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/bin/AquaVillage3D_Cinematic.exe
```

---

## 🎮 Controls

| ⌨️ Key/Mouse | 🎯 Action |
|:---:|---|
| **Right Mouse Drag** | Orbit camera |
| **Mouse Wheel** | Zoom |
| **Arrow Keys** | Move camera target |
| **Q / E** | Move camera target up / down |
| **V** | Restore reference composition |
| **C** | Close auto-tour: Pond 1 -> Pond 2 -> Pond 3 cinematic path |
| **1 / 2 / 3** | Fan 1 / 2 / 3 ON/OFF |
| **F** | Toggle all fans |
| **D** / **N** | Switch to **Day** / **Night** mode |
| **SPACE** | Pause / resume |
| **X** | `1x` -> `5x` -> `20x` simulation speed |
| **R** | Reset simulation + camera |
| **H** | Hide / show monitoring HUD |
| **F11** | Toggle fullscreen / restore window |
| **ESC** | Exit |

---

## 🌐 Optional AquaNexus Login Portal

The fish-themed portal is in `portal/`. Presentation credentials:
* **Username:** `aquanexus`
* **Password:** `aquanexus`

> From an activated Emscripten SDK shell, run `./web/build_web.sh`, then `./web/run_portal.sh`, and open `http://localhost:8080`. *(Intended for demonstrations; production requires a backend identity system).*

---

## 📈 Performance & Tweaks

If you are running on an older integrated GPU:
1. Resize the window smaller.
2. Change `shadowSize_` in `src/Scene.hpp` from `4096` to `2048`.
3. Reduce `2200` / `1800` grass blade counts in the `SceneRenderer` constructor.
4. Reduce the `Mesh::roundedWaterSurface(128,40,...)` subdivisions.

*(No Unity/Unreal or external game engine is used. Everything is generated by the custom C++ / OpenGL renderer.)*

---

<div align="center">
  <i>Built with ❤️ for digital aquaculture visualization.</i>
</div>
