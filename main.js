import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { TeapotGeometry } from 'three/addons/geometries/TeapotGeometry.js';
import { OBJLoader } from 'three/addons/loaders/OBJLoader.js';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { STLLoader } from 'three/addons/loaders/STLLoader.js';
import GUI from 'lil-gui';
import { SHADERS, makeUniforms } from './shaders.js';

// ─────────────────────────────────────────────
//  Shader info descriptions
// ─────────────────────────────────────────────
const SHADER_INFO = {
  flat: {
    name: 'Flat Shading',
    desc: 'Computes a single constant color per triangle using face normals derived from screen-space derivatives. Each polygon appears as a solid, faceted patch with no interpolation across vertices.',
    formula: 'N = normalize(dFdx(vWorldPos) × dFdy(vWorldPos)) → Diffuse = Kd · max(N·L, 0)',
  },
  gouraud: {
    name: 'Gouraud Shading',
    desc: 'Evaluates the full Phong lighting equation at each vertex in the vertex shader and linearly interpolates the resulting color across the triangle. Specular highlights can appear washed out at low tessellation.',
    formula: 'vColor = Ka·Ia + Kd·max(N·L, 0)·Id + Ks·pow(max(R·V, 0), s)·Is (evaluated per vertex)',
  },
  phong: {
    name: 'Phong Shading',
    desc: 'Interpolates surface normals across the triangle and evaluates the Phong reflection model per fragment, producing smooth, accurate highlights even at moderate tessellation.',
    formula: 'R = reflect(-L, N), Specular = Ks · pow(max(dot(R, V), 0.0), shininess)',
  },
  blinnphong: {
    name: 'Blinn-Phong Shading',
    desc: 'A variation of Phong shading that replaces the reflection vector with a halfway vector H between the light and view directions, which is cheaper to compute and behaves better near grazing angles.',
    formula: 'H = normalize(L + V), Specular = Ks · pow(max(dot(N, H), 0.0), shininess)',
  },
  toon: {
    name: 'Toon / Cel Shading',
    desc: 'Quantizes diffuse lighting into discrete bands to produce a hand-drawn, cartoon appearance. Specular is a hard binary step, creating a crisp highlight blob.',
    formula: 'quantDiff = floor(N·L · bands) / bands, Specular = step(0.6, specRaw)',
  },
  normalmap: {
    name: 'Procedural Bump / Normal Mapping',
    desc: 'Perturbs the interpolated surface normal using screen-space derivatives of a procedural sin/cos height field, then shades with Blinn-Phong using the bumped normal. No external texture required.',
    formula: 'h = sin(8x)·cos(8y)·sin(6z), N′ = normalize(N - bumpStrength · ∇h)',
  },
  orennayar: {
    name: 'Oren-Nayar Diffuse',
    desc: 'Models rough matte surfaces by accounting for inter-microfacet masking and shadowing. Unlike Lambertian diffuse it brightens limbs at high roughness, mimicking materials like clay or fabric.',
    formula: 'L = Kd · (A + B · sin(α) · tan(β) · max(0, cos(φᵢ - φᵣ))) · (N·L)',
  },
};

// ─────────────────────────────────────────────
//  Renderer, Camera, Scene
// ─────────────────────────────────────────────
const viewportContainer = document.getElementById('viewport-container');

const renderer = new THREE.WebGLRenderer({
  canvas: document.getElementById('viewport'),
  antialias: true,
  powerPreference: 'high-performance',
});
renderer.setPixelRatio(window.devicePixelRatio);

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x111111);

const camera = new THREE.PerspectiveCamera(45, 1, 0.1, 100);
camera.position.set(0, 1, 4);
const DEFAULT_CAMERA_POS = new THREE.Vector3(0, 1, 4);
const DEFAULT_CAMERA_TARGET = new THREE.Vector3(0, 0, 0);

const orbitControls = new OrbitControls(camera, renderer.domElement);
orbitControls.enableDamping = true;
orbitControls.dampingFactor = 0.08;

// Responsive resize via ResizeObserver
function updateSize() {
  const w = viewportContainer.clientWidth;
  const h = viewportContainer.clientHeight;
  if (!w || !h) return;
  renderer.setSize(w, h, true);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
}
const resizeObserver = new ResizeObserver(updateSize);
resizeObserver.observe(viewportContainer);
window.addEventListener('resize', updateSize);
updateSize();

// ─────────────────────────────────────────────
//  Uniforms (shared across all materials)
// ─────────────────────────────────────────────
let uniforms = makeUniforms();

// ─────────────────────────────────────────────
//  Light system
// ─────────────────────────────────────────────
const lightParams = {
  azimuth: 45,
  elevation: 45,
  distance: 3,
  color: '#ffffff',
  intensity: 1.0,
  ambientIntensity: 0.3,
  autoOrbit: false,
  orbitSpeed: 0.5,
};

// Light visualization group
const lightGroup = new THREE.Group();
scene.add(lightGroup);

// Bright inner core
const lightCore = new THREE.Mesh(
  new THREE.SphereGeometry(0.12, 16, 16),
  new THREE.MeshBasicMaterial({ color: 0xffea00 })
);
lightGroup.add(lightCore);

// Wireframe outer halo for high visibility
const lightHalo = new THREE.Mesh(
  new THREE.SphereGeometry(0.18, 12, 12),
  new THREE.MeshBasicMaterial({ color: 0xffaa00, wireframe: true, transparent: true, opacity: 0.7 })
);
lightGroup.add(lightHalo);

// Line pointing from light to center (origin)
const lightLineGeo = new THREE.BufferGeometry().setFromPoints([
  new THREE.Vector3(0, 0, 0),
  new THREE.Vector3(0, 0, 0),
]);
const lightLine = new THREE.Line(
  lightLineGeo,
  new THREE.LineBasicMaterial({ color: 0xffdd44, transparent: true, opacity: 0.4 })
);
lightGroup.add(lightLine);

function updateLightFromSpherical() {
  const az = THREE.MathUtils.degToRad(lightParams.azimuth);
  const el = THREE.MathUtils.degToRad(lightParams.elevation);
  const d  = lightParams.distance;
  const x  = d * Math.cos(el) * Math.sin(az);
  const y  = d * Math.sin(el);
  const z  = d * Math.cos(el) * Math.cos(az);

  uniforms.lightPosition.value.set(x, y, z);
  lightCore.position.set(x, y, z);
  lightHalo.position.set(x, y, z);

  // Update line from light to origin
  const posAttr = lightLineGeo.attributes.position;
  posAttr.setXYZ(0, x, y, z);
  posAttr.setXYZ(1, 0, 0, 0);
  posAttr.needsUpdate = true;

  const c = new THREE.Color(lightParams.color);
  uniforms.lightColor.value.set(c.r, c.g, c.b);
  lightCore.material.color.set(c);
  uniforms.lightIntensity.value  = lightParams.intensity;
  uniforms.ambientIntensity.value = lightParams.ambientIntensity;
}
updateLightFromSpherical();

// ─────────────────────────────────────────────
//  Alt+drag light orbit
// ─────────────────────────────────────────────
let altDragging  = false;
let lastMouseX   = 0;
let lastMouseY   = 0;

renderer.domElement.addEventListener('mousedown', (e) => {
  if (e.altKey) {
    altDragging = true;
    lastMouseX  = e.clientX;
    lastMouseY  = e.clientY;
    orbitControls.enabled = false;
    e.preventDefault();
  }
});

renderer.domElement.addEventListener('mousemove', (e) => {
  if (!altDragging) return;
  const dx = e.clientX - lastMouseX;
  const dy = e.clientY - lastMouseY;
  lastMouseX = e.clientX;
  lastMouseY = e.clientY;
  lightParams.azimuth   = (lightParams.azimuth + dx * 0.5) % 360;
  lightParams.elevation = THREE.MathUtils.clamp(lightParams.elevation - dy * 0.3, -89, 89);
  updateLightFromSpherical();
  // keep GUI in sync if it exists
  if (guiControllers.azimuth)   guiControllers.azimuth.updateDisplay();
  if (guiControllers.elevation) guiControllers.elevation.updateDisplay();
});

function endAltDrag() {
  altDragging = false;
  orbitControls.enabled = true;
}
renderer.domElement.addEventListener('mouseup',    endAltDrag);
renderer.domElement.addEventListener('mouseleave', endAltDrag);

// ─────────────────────────────────────────────
//  Preset models
// ─────────────────────────────────────────────
const PRESETS = {
  'Sphere':      () => new THREE.SphereGeometry(1, 64, 64),
  'Cube':        () => new THREE.BoxGeometry(1.5, 1.5, 1.5),
  'Torus Knot':  () => new THREE.TorusKnotGeometry(0.7, 0.25, 128, 32),
  'Teapot':      () => new TeapotGeometry(0.8, 10),
  'Icosahedron': () => new THREE.IcosahedronGeometry(1, 4),
  'Cylinder':    () => new THREE.CylinderGeometry(0.6, 0.6, 1.5, 64),
  'Cone':        () => new THREE.ConeGeometry(0.8, 1.6, 64),
};

// ─────────────────────────────────────────────
//  Mesh management
// ─────────────────────────────────────────────
let currentMesh = null;
let currentShaderKey = 'phong';

function makeMaterial() {
  return new THREE.ShaderMaterial({
    vertexShader:   SHADERS[currentShaderKey].vertexShader,
    fragmentShader: SHADERS[currentShaderKey].fragmentShader,
    uniforms,
    extensions: { derivatives: true },
  });
}

function fitAndAdd(geometry) {
  // Dispose old mesh
  if (currentMesh) {
    scene.remove(currentMesh);
    currentMesh.geometry.dispose();
    currentMesh.material.dispose();
    currentMesh = null;
  }

  geometry.center();
  geometry.computeBoundingSphere();
  const radius = geometry.boundingSphere.radius || 1;
  const scale  = 1 / radius;

  geometry.computeVertexNormals();

  const mesh = new THREE.Mesh(geometry, makeMaterial());
  mesh.scale.setScalar(scale);
  scene.add(mesh);
  currentMesh = mesh;
}

function loadPreset(name) {
  hideOverlays();
  state.preset = name;
  if (guiControllers.preset) guiControllers.preset.updateDisplay();
  const geo = PRESETS[name]();
  fitAndAdd(geo);
  const el = document.getElementById('status-model');
  if (el) el.textContent = 'Model: ' + name;
}

// ─────────────────────────────────────────────
//  File upload helpers
// ─────────────────────────────────────────────
function showLoading(show) {
  const el = document.getElementById('loading-overlay');
  if (el) {
    if (show) {
      el.classList.remove('hidden');
      el.style.display = 'flex';
    } else {
      el.classList.add('hidden');
      el.style.display = 'none';
    }
  }
}

function showError(msg) {
  const dialog = document.getElementById('error-dialog');
  const msgEl = document.getElementById('error-msg');
  if (msgEl) msgEl.textContent = msg || 'Failed to load model.';
  if (dialog && typeof dialog.showModal === 'function') {
    if (!dialog.open) dialog.showModal();
  }
}

function hideOverlays() {
  showLoading(false);
  const dialog = document.getElementById('error-dialog');
  if (dialog && dialog.open) {
    dialog.close();
  }
}

function extractGeometry(loaded) {
  // GLTFLoader result: { scene: Group }
  if (loaded && loaded.scene) {
    let geo = null;
    loaded.scene.traverse((child) => {
      if (!geo && child.isMesh) geo = child.geometry;
    });
    return geo;
  }
  // OBJLoader result: Group
  if (loaded && loaded.isGroup) {
    let geo = null;
    loaded.traverse((child) => {
      if (!geo && child.isMesh) geo = child.geometry;
    });
    return geo;
  }
  // STLLoader result: BufferGeometry directly
  if (loaded && loaded.isBufferGeometry) {
    return loaded;
  }
  return null;
}

function handleUpload(file) {
  if (!file) return;
  const name = file.name.toLowerCase();
  const url  = URL.createObjectURL(file);

  showLoading(true);

  const onLoad = (loaded) => {
    URL.revokeObjectURL(url);
    const geo = extractGeometry(loaded);
    if (!geo) {
      showLoading(false);
      showError('Could not extract geometry from this file.');
      return;
    }
    fitAndAdd(geo);
    showLoading(false);
  };

  const onError = (err) => {
    URL.revokeObjectURL(url);
    showLoading(false);
    showError(err?.message || 'Failed to load model.');
  };

  if (name.endsWith('.obj')) {
    new OBJLoader().load(url, onLoad, undefined, onError);
  } else if (name.endsWith('.glb') || name.endsWith('.gltf')) {
    new GLTFLoader().load(url, onLoad, undefined, onError);
  } else if (name.endsWith('.stl')) {
    new STLLoader().load(url, onLoad, undefined, onError);
  } else {
    URL.revokeObjectURL(url);
    showLoading(false);
    showError('Unsupported file format. Use .obj, .glb, .gltf or .stl');
  }
}

// ─────────────────────────────────────────────
//  Shader switching
// ─────────────────────────────────────────────
function setShader(key) {
  currentShaderKey = key;
  state.shaderName = SHADER_DISPLAY_NAMES[key] || key;
  if (guiControllers.shaderName) guiControllers.shaderName.updateDisplay();
  if (currentMesh) {
    if (currentMesh.material) currentMesh.material.dispose();
    currentMesh.material = new THREE.ShaderMaterial({
      vertexShader:   SHADERS[key].vertexShader,
      fragmentShader: SHADERS[key].fragmentShader,
      uniforms,
      extensions: { derivatives: true },
    });
    currentMesh.material.needsUpdate = true;
  }
  updateInfoPanel(key);

  // Sync header tabs with ARIA state
  document.querySelectorAll('.shader-tab').forEach((btn) => {
    const isActive = btn.dataset.shader === key;
    btn.classList.toggle('active', isActive);
    btn.setAttribute('aria-selected', isActive ? 'true' : 'false');
  });

  // Update status bar
  const el = document.getElementById('status-shader');
  if (el) el.textContent = 'Shader: ' + (SHADER_DISPLAY_NAMES[key] || key);
}

function updateInfoPanel(key) {
  const titleEl = document.getElementById('info-title');
  const descEl = document.getElementById('info-desc');
  const formulaEl = document.getElementById('info-formula');
  const info = SHADER_INFO[key];

  if (!info) {
    if (titleEl) titleEl.replaceChildren();
    if (descEl) descEl.replaceChildren();
    if (formulaEl) formulaEl.replaceChildren();
    return;
  }

  if (titleEl) titleEl.textContent = info.name;
  if (descEl) descEl.textContent = info.desc;
  if (formulaEl) formulaEl.textContent = info.formula;
}

// ─────────────────────────────────────────────
//  Reset camera
// ─────────────────────────────────────────────
function resetCamera() {
  camera.position.copy(DEFAULT_CAMERA_POS);
  orbitControls.target.copy(DEFAULT_CAMERA_TARGET);
  orbitControls.update();
}

// ─────────────────────────────────────────────
//  GUI
// ─────────────────────────────────────────────
const SHADER_DISPLAY_NAMES = {
  flat:       'Flat',
  gouraud:    'Gouraud',
  phong:      'Phong',
  blinnphong: 'Blinn-Phong',
  toon:       'Toon / Cel',
  normalmap:  'Procedural Bump',
  orennayar:  'Oren-Nayar',
};

const SHADER_KEYS = ['flat', 'gouraud', 'phong', 'blinnphong', 'toon', 'normalmap', 'orennayar'];

// Reverse map: display name → key
const SHADER_NAME_TO_KEY = {};
SHADER_KEYS.forEach((k) => { SHADER_NAME_TO_KEY[SHADER_DISPLAY_NAMES[k]] = k; });

// GUI state object
const state = {
  preset:      'Sphere',
  shaderName:  SHADER_DISPLAY_NAMES['phong'],
  uploadModel: () => { fileInput.click(); },
  resetCamera: resetCamera,
  // light
  azimuth:          lightParams.azimuth,
  elevation:        lightParams.elevation,
  distance:         lightParams.distance,
  lightColor:       lightParams.color,
  intensity:        lightParams.intensity,
  ambientIntensity: lightParams.ambientIntensity,
  autoOrbit:        lightParams.autoOrbit,
  orbitSpeed:       lightParams.orbitSpeed,
  // material
  Ka:          uniforms.Ka.value,
  Kd:          uniforms.Kd.value,
  Ks:          uniforms.Ks.value,
  shininess:   uniforms.shininess.value,
  roughness:   uniforms.roughness.value,
  toonBands:   uniforms.toonBands.value,
  bumpStrength: uniforms.bumpStrength.value,
  objectColor: '#' + new THREE.Color(
    uniforms.objectColor.value.r,
    uniforms.objectColor.value.g,
    uniforms.objectColor.value.b
  ).getHexString(),
};

// Hidden file input
const fileInput = document.getElementById('file-upload') || document.createElement('input');
fileInput.type   = 'file';
fileInput.accept = '.obj,.glb,.gltf,.stl';
fileInput.style.display = 'none';
if (!fileInput.parentElement) document.body.appendChild(fileInput);
fileInput.addEventListener('change', (e) => {
  if (e.target.files && e.target.files[0]) handleUpload(e.target.files[0]);
  fileInput.value = '';
});

// Store GUI controller refs for programmatic update
const guiControllers = {};

function createGUI() {
  const gui = new GUI({ container: document.getElementById('gui-container'), width: 280 });

  // ── Model folder ──────────────────────────
  const modelFolder = gui.addFolder('Model');
  guiControllers.preset = modelFolder.add(state, 'preset', Object.keys(PRESETS))
    .name('Preset')
    .onChange(loadPreset);
  modelFolder.add(state, 'uploadModel').name('Upload (.obj / .glb / .stl)');
  modelFolder.open();

  // ── Shading folder ────────────────────────
  const shadingFolder = gui.addFolder('Shading');
  guiControllers.shaderName = shadingFolder.add(state, 'shaderName', Object.values(SHADER_DISPLAY_NAMES))
    .name('Shader')
    .onChange((displayName) => {
      const key = SHADER_NAME_TO_KEY[displayName];
      if (key) setShader(key);
    });
  shadingFolder.open();

  // ── Light folder ──────────────────────────
  const lightFolder = gui.addFolder('Light');

  guiControllers.azimuth = lightFolder
    .add(lightParams, 'azimuth', 0, 360)
    .name('Azimuth')
    .onChange(() => { state.azimuth = lightParams.azimuth; updateLightFromSpherical(); });

  guiControllers.elevation = lightFolder
    .add(lightParams, 'elevation', -89, 89)
    .name('Elevation')
    .onChange(() => { state.elevation = lightParams.elevation; updateLightFromSpherical(); });

  lightFolder
    .add(lightParams, 'distance', 1, 5)
    .name('Distance')
    .onChange(updateLightFromSpherical);

  lightFolder
    .addColor(lightParams, 'color')
    .name('Color')
    .onChange(updateLightFromSpherical);

  lightFolder
    .add(lightParams, 'intensity', 0, 3)
    .name('Intensity')
    .onChange(updateLightFromSpherical);

  lightFolder
    .add(lightParams, 'ambientIntensity', 0, 1)
    .name('Ambient')
    .onChange(updateLightFromSpherical);

  lightFolder
    .add(lightParams, 'autoOrbit')
    .name('Auto-Orbit')
    .onChange((v) => { lightParams.autoOrbit = v; });

  lightFolder
    .add(lightParams, 'orbitSpeed', 0.1, 5)
    .name('Orbit Speed');

  lightFolder.open();

  // ── Material folder ───────────────────────
  const matFolder = gui.addFolder('Material');

  matFolder.add(state, 'Ka', 0, 1).name('Ka (ambient)').onChange((v) => { uniforms.Ka.value = v; });
  matFolder.add(state, 'Kd', 0, 1).name('Kd (diffuse)').onChange((v) => { uniforms.Kd.value = v; });
  matFolder.add(state, 'Ks', 0, 1).name('Ks (specular)').onChange((v) => { uniforms.Ks.value = v; });

  matFolder.add(state, 'shininess', 1, 256, 1)
    .name('Shininess')
    .onChange((v) => { uniforms.shininess.value = v; });

  matFolder.add(state, 'roughness', 0, 1)
    .name('Roughness (σ)')
    .onChange((v) => { uniforms.roughness.value = v; });

  matFolder.add(state, 'toonBands', 2, 8, 1)
    .name('Toon Bands')
    .onChange((v) => { uniforms.toonBands.value = v; });

  matFolder.add(state, 'bumpStrength', 0, 2)
    .name('Bump Strength')
    .onChange((v) => { uniforms.bumpStrength.value = v; });

  matFolder.addColor(state, 'objectColor')
    .name('Object Color')
    .onChange((hex) => {
      const c = new THREE.Color(hex);
      uniforms.objectColor.value.set(c.r, c.g, c.b);
    });

  matFolder.open();

  // ── Camera button ─────────────────────────
  gui.add(state, 'resetCamera').name('⟳ Reset Camera');
}

// ─────────────────────────────────────────────
//  Keyboard shortcuts
// ─────────────────────────────────────────────
const KEY_SHADERS = ['flat', 'gouraud', 'phong', 'blinnphong', 'toon', 'normalmap', 'orennayar'];

window.addEventListener('keydown', (e) => {
  // Skip if typing in an input/textarea
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

  const digit = parseInt(e.key, 10);
  if (digit >= 1 && digit <= 7) {
    const key = KEY_SHADERS[digit - 1];
    setShader(key);
    return;
  }

  switch (e.key.toUpperCase()) {
    case 'R': resetCamera(); break;
    case 'L': lightGroup.visible = !lightGroup.visible; break;
    case 'A':
      lightParams.autoOrbit = !lightParams.autoOrbit;
      break;
  }
});

// ─────────────────────────────────────────────
//  Animate loop
// ─────────────────────────────────────────────
let lastTime = performance.now();
let frameCount = 0;
let fpsAccum = 0;

function animate() {
  requestAnimationFrame(animate);

  const now   = performance.now();
  const delta = (now - lastTime) / 1000;
  lastTime    = now;

  // FPS counter (update every ~0.5s)
  frameCount++;
  fpsAccum += delta;
  if (fpsAccum >= 0.5) {
    const fps = Math.round(frameCount / fpsAccum);
    const fpsEl = document.getElementById('fps-counter');
    if (fpsEl) fpsEl.textContent = fps + ' fps';
    frameCount = 0;
    fpsAccum = 0;
  }

  // Auto-orbit light
  if (lightParams.autoOrbit) {
    lightParams.azimuth = (lightParams.azimuth + delta * lightParams.orbitSpeed * 60) % 360;
    updateLightFromSpherical();
    if (guiControllers.azimuth) guiControllers.azimuth.updateDisplay();
  }

  orbitControls.update();
  renderer.render(scene, camera);
}

// ─────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────

// Wire header tab clicks → setShader
document.querySelectorAll('.shader-tab').forEach((btn) => {
  btn.addEventListener('click', () => {
    const key = btn.dataset.shader;
    if (key) setShader(key);
  });
});

// Wire error-dismiss button
const errorDialog = document.getElementById('error-dialog');
const errorDismiss = document.getElementById('error-dismiss');
if (errorDismiss && errorDialog) {
  errorDismiss.addEventListener('click', () => {
    errorDialog.close();
  });
}

loadPreset('Sphere');
createGUI();
setShader('phong'); // sets initial tab highlight + status bar + info panel
animate();
