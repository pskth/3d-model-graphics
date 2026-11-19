# Lumina — 3D Shading & Illumination Laboratory

An interactive 3D graphics viewer with real-time, switchable GLSL shading models,
written in C++17 with OpenGL 3.3 Core.

Load a built-in geometry or upload your own mesh, then manipulate the light
source, material coefficients, and shading algorithm to observe how different
illumination models behave on a 3D surface.

---

## Table of Contents

- [Features](#features)
- [Shading Models](#shading-models)
- [Prerequisites](#prerequisites)
- [Dependencies](#dependencies)
- [Building](#building)
- [Usage](#usage)
- [Controls](#controls)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [License](#license)

---

## Features

- Seven real-time GLSL shading models switchable without restarting
- Full 360-degree spherical light positioning via ImGui sliders or Alt + drag
  directly on the viewport
- Auto-orbit mode that continuously rotates the light around the scene
- Six built-in geometry presets: Sphere, Icosahedron, Cube, Cylinder, Cone,
  Torus Knot
- Custom mesh loading supporting `.obj` and `.stl` formats, via the ImGui panel
  or by dragging a file onto the window
- Per-material coefficient controls: Ka, Kd, Ks, shininess, roughness, toon
  bands, bump strength
- Dear ImGui panel displaying the mathematical formulation of the active
  shading model
- Arcball orbit camera with exponential damping matching the original
  Three.js OrbitControls feel
- C state module (`lumina_logic.c`) shared with the original WebAssembly build,
  handling spherical-to-Cartesian light conversion and parameter clamping

---

## Shading Models

| Key | Model | Pipeline Stage | Description |
|-----|-------|---------------|-------------|
| `1` | Flat | Per-primitive | Face normal reconstructed from screen-space derivatives `dFdx` / `dFdy`. Hard faceted shading. |
| `2` | Gouraud | Per-vertex | Full Phong model evaluated at each vertex; colour interpolated across the primitive. |
| `3` | Phong | Per-fragment | Smooth normals interpolated per pixel; specular via reflection vector `R = reflect(-L, N)`. |
| `4` | Blinn-Phong | Per-fragment | Halfway vector `H = normalize(L + V)` replaces the reflection vector for more physically plausible highlights. |
| `5` | Toon / Cel | Per-fragment | Diffuse quantized into discrete luminance bands; specular hard-thresholded via `step(0.6, ...)`. |
| `6` | Bump Map | Per-fragment | Procedural height field `h(p) = sin(8x)*cos(8y)*sin(6z)`; finite-difference gradient perturbs the surface normal. |
| `7` | Oren-Nayar | Per-fragment | Ramamoorthi & Hanrahan simplified microfacet diffuse for matte surfaces (clay, chalk, fabric). |

---

## Prerequisites

- CMake 3.20 or later
- A C++17-capable compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- C11 compiler (for `lumina_logic.c`)

---

## Dependencies

### System packages (install separately)

| Library | Version | Purpose |
|---------|---------|---------|
| [OpenGL](https://www.opengl.org/) | 3.3+ | Graphics API |
| [GLFW](https://www.glfw.org/) | 3.3+ | Window creation and input |
| [GLM](https://github.com/g-truc/glm) | 0.9.9+ | Mathematics (vectors, matrices) |

Install on Ubuntu / Debian:
```shell
sudo apt install libglfw3-dev libglm-dev
```

Install on macOS (Homebrew):
```shell
brew install glfw glm
```

Install on Fedora / RHEL:
```shell
sudo dnf install glfw-devel glm-devel
```

### Vendored libraries (place under `third_party/`)

The following must be downloaded and placed in the specified directories before
building. They are not bundled in this repository.

| Library | Location | Notes |
|---------|----------|-------|
| [GLAD](https://glad.dav1d.de/) | `third_party/glad/` | Generate for OpenGL 3.3 Core. Needs `src/glad.c` and `include/glad/glad.h`. |
| [Dear ImGui](https://github.com/ocornut/imgui) | `third_party/imgui/` | Copy `imgui*.cpp`, `imgui*.h`, and the `backends/` subfolder. |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | `third_party/tinyobjloader/` | Single header `tinyobjloader.h`. |

#### GLAD setup

Go to [glad.dav1d.de](https://glad.dav1d.de/), select:
- Language: C/C++
- Specification: OpenGL
- API gl: Version 3.3
- Profile: Core

Download the zip and extract so the layout is:

```
third_party/glad/
  include/
    glad/
      glad.h
    KHR/
      khrplatform.h
  src/
    glad.c
```

#### Dear ImGui setup

```shell
git clone https://github.com/ocornut/imgui.git third_party/imgui
```

Required files: `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`,
`imgui_widgets.cpp`, `imgui.h`, `imgui_internal.h`, `imstb_*.h`,
`backends/imgui_impl_glfw.cpp`, `backends/imgui_impl_glfw.h`,
`backends/imgui_impl_opengl3.cpp`, `backends/imgui_impl_opengl3.h`.

#### tinyobjloader setup

```shell
mkdir -p third_party/tinyobjloader
curl -L https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/release/tiny_obj_loader.h \
     -o third_party/tinyobjloader/tinyobjloader.h
```

---

## Building

```shell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The compiled binary is written to `build/bin/lumina`.

For a debug build:
```shell
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel
```

---

## Usage

```shell
./build/bin/lumina
```

The window opens to a Sphere rendered with Phong shading. Use the right-side
control panel to change shaders, geometry, lighting, and material parameters.

To load a custom mesh, either type the file path into the "Load" field in the
Geometry section, or drag a `.obj` or `.stl` file onto the window.

---

## Controls

### Keyboard

| Key | Action |
|-----|--------|
| `1` – `7` | Switch shading model |
| `R` | Reset camera to default position |
| `L` | Toggle light orb and guide ray visibility |
| `A` | Toggle auto-orbit (continuous light rotation) |
| `Q` / `Esc` | Quit |

### Mouse — Viewport

| Input | Action |
|-------|--------|
| Left drag | Orbit camera around the model |
| Scroll wheel | Zoom in / out |
| Alt + left drag | Move light source in 360-degree arc |

### ImGui Panel

| Control | Description |
|---------|-------------|
| Shading Model buttons | Switch between all seven shading models |
| Geometry presets | Switch between the six built-in shapes |
| Load field | Enter a file path and press Load to import an `.obj` or `.stl` |
| Azimuth / Elevation / Distance | Spherical position of the light source |
| Intensity / Ambient | Light and ambient brightness |
| Light Color | Light source colour |
| Auto-Orbit / Orb toggles | Enable continuous light rotation or hide the orb |
| Quick presets | One-click lighting setups: Key 45, Fill 225, Rim 180, Top Down |
| Base Tint | Object's diffuse colour |
| Ka / Kd / Ks / Shininess | Phong-family material coefficients |
| Roughness | Microfacet roughness for Oren-Nayar (shown when active) |
| Toon Bands | Discrete shading steps for Toon/Cel (shown when active) |
| Bump Depth | Procedural bump perturbation strength (shown when active) |

---

## Project Structure

```
3d-model-graphics/
├── src/
│   ├── main.cpp            GLFW window, render loop, Dear ImGui panel, callbacks
│   ├── shaders.h           All 7 GLSL vertex + fragment shader sources
│   ├── geometry.h          Procedural mesh generators and OBJ / STL loaders
│   ├── camera.h            Arcball orbit camera with exponential damping
│   └── shader_program.h    OpenGL shader compile / link / uniform helpers
├── lumina_logic.c          C state module: spherical light math, clamping
├── lumina_logic.h          C header for lumina_logic.c
├── CMakeLists.txt          CMake build system
├── third_party/
│   ├── glad/               GLAD OpenGL loader (vendored)
│   ├── imgui/              Dear ImGui (vendored)
│   └── tinyobjloader/      tinyobjloader single header (vendored)
├── design.md               Neubrutalism UI design system reference
└── README.md               This file
```

The original web application sources (`main.js`, `shaders.js`, `index.html`,
`style.css`, `bundle.js`) remain in the repository root and are independent of
the C++ build.

---

## Contributing

Fork the repository and create a feature branch from `main`. Pull requests are
welcome.

```shell
git checkout -b feature/your-feature-name
```

Guidelines:

- New shading models: add a vertex/fragment shader pair to `src/shaders.h` and
  a corresponding entry to `SHADER_TABLE[]`. No changes to `main.cpp` are needed
  unless the model requires a new uniform.
- New geometry presets: add a generator function to `src/geometry.h` and an
  entry to the `GeomPreset` enum and `buildPreset()` dispatch.
- `lumina_logic.c` is shared with the WebAssembly build; keep it free of C++
  and platform-specific headers.

Report bugs and request features via the
[issue tracker](https://github.com/pskth/3d-model-graphics/issues).

---

## License

This project is licensed under the ISC License.
