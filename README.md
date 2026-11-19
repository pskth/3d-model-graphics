# Lumina — 3D Shading & Illumination Laboratory

An interactive browser-based 3D graphics viewer with real-time, switchable GLSL shading models. Load any of the built-in geometries or upload your own mesh, then manipulate the light source, material coefficients, and shading algorithm to observe how different illumination models behave on a 3D surface.

---

## Table of Contents

- [Features](#features)
- [Shading Models](#shading-models)
- [Getting Started](#getting-started)
- [Building](#building)
- [Usage](#usage)
- [Controls](#controls)
- [Project Structure](#project-structure)
- [Dependencies](#dependencies)
- [Contributing](#contributing)
- [License](#license)

---

## Features

- Seven real-time GLSL shading models switchable without page reload
- Full 360-degree spherical light positioning via sliders or Alt+Drag directly on the canvas
- Auto-orbit mode that continuously rotates the light around the scene
- Seven built-in geometry presets: Sphere, Teapot, Torus Knot, Cube, Icosahedron, Cylinder, Cone
- Custom mesh upload supporting `.obj`, `.gltf`, `.glb`, and `.stl` formats
- Per-material coefficient controls: Ka, Kd, Ks, shininess, roughness, toon bands, bump strength
- Live shader formulation panel displaying the mathematical specification of the active shading model
- Neubrutalism UI — high-contrast black-and-white interface that keeps visual focus on the 3D viewport
- Keyboard shortcuts for all primary actions

---

## Shading Models

| Key | Model | Pipeline Stage | Description |
|-----|-------|---------------|-------------|
| `1` | Flat | Per-primitive | Reconstructs face normals from screen-space derivatives. Produces hard faceted shading. |
| `2` | Gouraud | Per-vertex | Evaluates full Phong lighting at each vertex and interpolates color across the face. |
| `3` | Phong | Per-fragment | Interpolates surface normals and evaluates specular reflection `R = reflect(-L, N)` per pixel. |
| `4` | Blinn-Phong | Per-fragment | Replaces the reflection vector with a halfway vector `H = normalize(L + V)` for more physically plausible highlights. |
| `5` | Toon / Cel | Per-fragment | Quantizes diffuse into discrete luminance bands with a hard-threshold specular step for a stylized look. |
| `6` | Bump Map | Per-fragment | Perturbs interpolated normals using a procedural height field `h(p) = sin(8x) * cos(8y) * sin(6z)` and finite-difference gradients. |
| `7` | Oren-Nayar | Per-fragment | Microfacet diffuse model accounting for inter-facet masking and limb brightening. Suited for matte surfaces. |

---

## Getting Started

### Prerequisites

- [Node.js](https://nodejs.org/) 18 or later
- npm (bundled with Node.js)

### Installation

Clone the repository and install dependencies:

```shell
git clone https://github.com/pskth/3d-model-graphics.git
cd 3d-model-graphics
npm install
```

---

## Building

The project uses Clang to compile `lumina_logic.c` into `lumina.wasm`, and [esbuild](https://esbuild.github.io/) to bundle `main.js` and its dependencies into `bundle.js`. The HTML page loads `lumina-wasm.js` before the Three.js application bundle.

```shell
npm run build
```

This runs:

```shell
esbuild main.js --bundle --outfile=bundle.js --format=iife --target=es2020
```

After the build completes, open `index.html` in any modern browser. No dev server is required — the file can be opened directly from disk.

> Re-run `npm run build` after any change to `main.js` or `shaders.js`.

---

## Usage

1. Run `npm run build` to produce `bundle.js`.
2. Open `index.html` in a browser (Chrome, Firefox, Safari, or Edge — WebGL 2 required).
3. Select a geometry preset from the sidebar or upload a custom mesh.
4. Switch shading models using the tab bar at the top or the number keys `1`–`7`.
5. Adjust the light position, color, and intensity, as well as the material coefficients, using the sidebar controls.

---

## Controls

### Keyboard

| Key | Action |
|-----|--------|
| `1` – `7` | Switch shading model |
| `R` | Reset camera to default position |
| `L` | Toggle light orb and guide ray visibility |
| `A` | Toggle auto-orbit (continuous light rotation) |

### Mouse — Viewport

| Input | Action |
|-------|--------|
| Left drag | Orbit camera around the model |
| Scroll wheel | Zoom in / out |
| Alt + drag | Move light source in 360-degree arc |

### Sidebar

| Control | Description |
|---------|-------------|
| Geometry presets | Switch between the seven built-in shapes |
| Upload Mesh | Load a local `.obj`, `.gltf`, `.glb`, or `.stl` file |
| Azimuth / Elevation / Distance | Position the light source in spherical coordinates |
| Quick angle presets | One-click lighting setups: Key 45°, Fill 225°, Rim 180°, Top Down |
| Light Color / Intensity | Set the light's hue and brightness |
| Ambient Illumination | Global ambient light level |
| Base Tint | Object's base diffuse color |
| Ka / Kd / Ks / Shininess | Phong material coefficients |
| Roughness | Microfacet roughness for Oren-Nayar (shown when active) |
| Toon Bands | Number of discrete shading steps for Toon/Cel (shown when active) |
| Bump Depth | Procedural bump perturbation strength (shown when active) |

---

## Project Structure

```
3d-model-graphics/
├── index.html      # Page layout, UI structure, ARIA roles
├── style.css       # Neubrutalism UI design system
├── main.js         # Three.js scene, camera, controls, UI bindings, render loop
├── shaders.js      # All 7 GLSL vertex + fragment shader pairs and uniform defaults
├── bundle.js       # esbuild output — do not edit directly
└── package.json    # Project metadata and build scripts
```

---

## Dependencies

| Package | Version | Purpose |
|---------|---------|---------|
| [three](https://threejs.org/) | ^0.187.0 | WebGL renderer, geometries, loaders, OrbitControls |
| [esbuild](https://esbuild.github.io/) | ^0.28.2 | Zero-config JavaScript bundler (dev dependency) |

Three.js addons used directly (via `three/addons`):

- `OrbitControls` — camera orbit/zoom interaction
- `TeapotGeometry` — Utah Teapot mesh
- `OBJLoader`, `GLTFLoader`, `STLLoader` — custom mesh upload

---

## Contributing

Fork the repository and create a feature branch from `main`. Pull requests are welcome.

```shell
git checkout -b feature/your-feature-name
```

Please keep the following in mind:

- Shader changes go in `shaders.js`. Each new shading model needs a vertex shader, a fragment shader, and an entry in `SHADER_INFO` inside `main.js`.
- UI changes belong in `style.css` and `index.html`. The design system uses CSS custom properties defined at the top of `style.css` — prefer editing those tokens over adding new hardcoded values.
- `bundle.js` is generated and should not be committed unless distributing a standalone build.

Report bugs and request features via the [issue tracker](https://github.com/pskth/3d-model-graphics/issues).

---