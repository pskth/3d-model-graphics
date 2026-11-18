import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { TeapotGeometry } from 'three/addons/geometries/TeapotGeometry.js';
import { OBJLoader } from 'three/addons/loaders/OBJLoader.js';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { STLLoader } from 'three/addons/loaders/STLLoader.js';
import { SHADERS, makeUniforms } from './shaders.js';

// ─────────────────────────────────────────────
//  Shader info descriptions & mathematical specifications
// ─────────────────────────────────────────────
const SHADER_INFO = {
  flat: {
    name: 'Flat Shading',
    stage: 'PRIMITIVE',
    desc: 'Computes a constant color per triangle using screen-space derivatives of world position (dFdx × dFdy). Each polygon displays faceted shading without normal interpolation across vertices.',
    formula: 'N = normalize(dFdx(vWorldPos) × dFdy(vWorldPos))\nDiffuse = Kd · max(dot(N, L), 0.0) · Id · Color\nColor = Ambient + Diffuse (Faceted)',
  },
  gouraud: {
    name: 'Gouraud Shading',
    stage: 'VERTEX',
    desc: 'Evaluates the complete Phong lighting model per vertex in the vertex shader, interpolating the resulting RGB color across the primitive. Fast, but highlights blur or miss between vertices.',
    formula: 'vColor = Ka·Ia·Cd + Kd·max(N·L, 0)·Id·Cd + Ks·pow(max(R·V, 0), s)·Is\nFragment: gl_FragColor = vec4(vColor, 1.0)',
  },
  phong: {
    name: 'Phong Shading',
    stage: 'FRAGMENT',
    desc: 'Interpolates smooth surface normals per fragment and evaluates reflection R = reflect(-L, N) at every pixel. Produces precise, natural specular highlights.',
    formula: 'R = reflect(-L, N)\nSpecular = Ks · pow(max(dot(R, V), 0.0), shininess) · Is\nColor = Ka·Ia·Cd + Kd·max(N·L, 0)·Id·Cd + Specular',
  },
  blinnphong: {
    name: 'Blinn-Phong Shading',
    stage: 'FRAGMENT',
    desc: 'Replaces the reflection vector with the halfway vector H = normalize(L + V). More physically plausible at grazing angles and computationally efficient.',
    formula: 'H = normalize(L + V)\nSpecular = Ks · pow(max(dot(N, H), 0.0), shininess) · Is\nColor = Ka·Ia·Cd + Kd·max(N·L, 0)·Id·Cd + Specular',
  },
  toon: {
    name: 'Toon / Cel Shading',
    stage: 'FRAGMENT',
    desc: 'Quantizes continuous diffuse illumination into discrete stepped luminance bands with hard specular thresholding for an illustrative graphic novel appearance.',
    formula: 'quantDiff = floor(max(dot(N, L), 0) · bands) / bands\nSpecular = step(0.6, pow(max(dot(N, H), 0), s))\nColor = Ambient + Kd·quantDiff·Id + Ks·Specular',
  },
  normalmap: {
    name: 'Procedural Bump Mapping',
    stage: 'FRAGMENT',
    desc: 'Evaluates a 3D procedural harmonic height field h(p) and perturbs the interpolated normal via finite-difference gradient: N′ = normalize(N - strength · ∇h).',
    formula: 'h(p) = sin(8x)·cos(8y)·sin(6z)\n∇h = vec3(h(p+εx)-h(p), h(p+εy)-h(p), h(p+εz)-h(p)) / ε\nN′ = normalize(N - bumpStrength · ∇h)',
  },
  orennayar: {
    name: 'Oren-Nayar Diffuse',
    stage: 'FRAGMENT',
    desc: 'Microfacet reflectance model for rough, matte surfaces (clay, chalk, skin, fabric). Accounts for microscopic inter-facet masking and limb brightening.',
    formula: 'L = Kd · (A + B · sin(α) · tan(β) · max(0, cos(φᵢ - φᵣ))) · (N·L)\nA = 1 - 0.5·σ²/(σ² + 0.33), B = 0.45·σ²/(σ² + 0.09)',
  },
};

const SHADER_DISPLAY_NAMES = {
  flat:       'Flat',
  gouraud:    'Gouraud',
  phong:      'Phong',
  blinnphong: 'Blinn-Phong',
  toon:       'Toon / Cel',
  normalmap:  'Bump Map',
  orennayar:  'Oren-Nayar',
};

const KEY_SHADERS = ['flat', 'gouraud', 'phong', 'blinnphong', 'toon', 'normalmap', 'orennayar'];

// ─────────────────────────────────────────────
//  Renderer, Camera, Scene Setup
// ─────────────────────────────────────────────
const viewportContainer = document.getElementById('viewport-container');

const renderer = new THREE.WebGLRenderer({
  canvas: document.getElementById('viewport'),
  antialias: true,
  powerPreference: 'high-performance',
});
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));

const scene = new THREE.Scene();
// Deep radial background color
scene.background = new THREE.Color(0x070a0f);

const camera = new THREE.PerspectiveCamera(45, 1, 0.1, 100);
camera.position.set(0, 1.2, 4.0);
const DEFAULT_CAMERA_POS = new THREE.Vector3(0, 1.2, 4.0);
const DEFAULT_CAMERA_TARGET = new THREE.Vector3(0, 0, 0);

const orbitControls = new OrbitControls(camera, renderer.domElement);
orbitControls.enableDamping = true;
orbitControls.dampingFactor = 0.08;
orbitControls.target.copy(DEFAULT_CAMERA_TARGET);

// Responsive canvas sizing
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
//  Uniforms Pipeline
// ─────────────────────────────────────────────
let uniforms = makeUniforms();

// ─────────────────────────────────────────────
//  Illumination Studio System (360° Light)
// ─────────────────────────────────────────────
const lightParams = {
  azimuth: 45,
  elevation: 45,
  distance: 3.0,
  color: '#ffffff',
  intensity: 1.0,
  ambientIntensity: 0.3,
  autoOrbit: false,
  orbitSpeed: 0.5,
};

// Composite light marker group
const lightGroup = new THREE.Group();
scene.add(lightGroup);

// Illuminated inner core
const lightCore = new THREE.Mesh(
  new THREE.SphereGeometry(0.12, 16, 16),
  new THREE.MeshBasicMaterial({ color: 0xffea00 })
);
lightGroup.add(lightCore);

// Wireframe halo ring
const lightHalo = new THREE.Mesh(
  new THREE.SphereGeometry(0.18, 12, 12),
  new THREE.MeshBasicMaterial({ color: 0xffaa00, wireframe: true, transparent: true, opacity: 0.7 })
);
lightGroup.add(lightHalo);

// Directional ray line connecting light to origin
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

  // Update guide line endpoints
  const posAttr = lightLineGeo.attributes.position;
  posAttr.setXYZ(0, x, y, z);
  posAttr.setXYZ(1, 0, 0, 0);
  posAttr.needsUpdate = true;

  const c = new THREE.Color(lightParams.color);
  uniforms.lightColor.value.set(c.r, c.g, c.b);
  lightCore.material.color.set(c);
  uniforms.lightIntensity.value  = lightParams.intensity;
  uniforms.ambientIntensity.value = lightParams.ambientIntensity;

  syncLightUI();
}

function syncLightUI() {
  const inputAz = document.getElementById('input-azimuth');
  const valAz = document.getElementById('val-azimuth');
  if (inputAz) inputAz.value = Math.round(lightParams.azimuth);
  if (valAz) valAz.textContent = Math.round(lightParams.azimuth) + '°';

  const inputEl = document.getElementById('input-elevation');
  const valEl = document.getElementById('val-elevation');
  if (inputEl) inputEl.value = Math.round(lightParams.elevation);
  if (valEl) valEl.textContent = Math.round(lightParams.elevation) + '°';

  const inputDist = document.getElementById('input-distance');
  const valDist = document.getElementById('val-distance');
  if (inputDist) inputDist.value = lightParams.distance;
  if (valDist) valDist.textContent = lightParams.distance.toFixed(1);

  const inputInt = document.getElementById('input-light-intensity');
  const valInt = document.getElementById('val-intensity');
  if (inputInt) inputInt.value = lightParams.intensity;
  if (valInt) valInt.textContent = lightParams.intensity.toFixed(2);

  const inputAmb = document.getElementById('input-ambient');
  const valAmb = document.getElementById('val-ambient');
  if (inputAmb) inputAmb.value = lightParams.ambientIntensity;
  if (valAmb) valAmb.textContent = lightParams.ambientIntensity.toFixed(2);

  const inputColor = document.getElementById('input-light-color');
  const previewColor = document.getElementById('light-color-preview');
  if (inputColor) inputColor.value = lightParams.color;
  if (previewColor) previewColor.textContent = lightParams.color;
}
updateLightFromSpherical();

// ─────────────────────────────────────────────
//  Alt+Drag Light Orbit Navigation
// ─────────────────────────────────────────────
let altDragging = false;
let lastMouseX  = 0;
let lastMouseY  = 0;

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
  lightParams.azimuth   = (lightParams.azimuth + dx * 0.5 + 360) % 360;
  lightParams.elevation = THREE.MathUtils.clamp(lightParams.elevation - dy * 0.3, -89, 89);
  updateLightFromSpherical();
});

function endAltDrag() {
  altDragging = false;
  orbitControls.enabled = true;
}
renderer.domElement.addEventListener('mouseup', endAltDrag);
renderer.domElement.addEventListener('mouseleave', endAltDrag);

// ─────────────────────────────────────────────
//  Preset 3D Geometries
// ─────────────────────────────────────────────
const PRESETS = {
  'Sphere':      () => new THREE.SphereGeometry(1, 64, 64),
  'Teapot':      () => new TeapotGeometry(0.8, 10),
  'Torus Knot':  () => new THREE.TorusKnotGeometry(0.7, 0.25, 128, 32),
  'Cube':        () => new THREE.BoxGeometry(1.5, 1.5, 1.5),
  'Icosahedron': () => new THREE.IcosahedronGeometry(1, 4),
  'Cylinder':    () => new THREE.CylinderGeometry(0.6, 0.6, 1.5, 64),
  'Cone':        () => new THREE.ConeGeometry(0.8, 1.6, 64),
};

// ─────────────────────────────────────────────
//  Mesh Management
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
  const geoFn = PRESETS[name];
  if (!geoFn) return;
  const geo = geoFn();
  fitAndAdd(geo);

  // Update HUD & active indicators
  const hudEl = document.getElementById('hud-model-name');
  if (hudEl) hudEl.textContent = name;
  const activeBadge = document.getElementById('active-model-name');
  if (activeBadge) activeBadge.textContent = name;
  const statusEl = document.getElementById('status-model');
  if (statusEl) statusEl.textContent = 'Mesh: ' + name;

  // Sync preset buttons
  document.querySelectorAll('.preset-btn').forEach((btn) => {
    const isActive = btn.dataset.preset === name;
    btn.classList.toggle('active', isActive);
    btn.setAttribute('aria-checked', isActive ? 'true' : 'false');
  });
}

// ─────────────────────────────────────────────
//  File Upload Handler
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
  if (loaded && loaded.scene) {
    let geo = null;
    loaded.scene.traverse((child) => {
      if (!geo && child.isMesh) geo = child.geometry;
    });
    return geo;
  }
  if (loaded && loaded.isGroup) {
    let geo = null;
    loaded.traverse((child) => {
      if (!geo && child.isMesh) geo = child.geometry;
    });
    return geo;
  }
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

    const displayName = file.name.replace(/\.[^/.]+$/, '');
    const hudEl = document.getElementById('hud-model-name');
    if (hudEl) hudEl.textContent = displayName;
    const activeBadge = document.getElementById('active-model-name');
    if (activeBadge) activeBadge.textContent = 'Custom';
    const statusEl = document.getElementById('status-model');
    if (statusEl) statusEl.textContent = 'Mesh: ' + displayName;
  };

  const onError = (err) => {
    URL.revokeObjectURL(url);
    showLoading(false);
    showError(err?.message || 'Failed to parse model file.');
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
    showError('Unsupported format. Please select .obj, .glb, .gltf, or .stl');
  }
}

// ─────────────────────────────────────────────
//  Shader Switching & Mathematical Formulation
// ─────────────────────────────────────────────
function setShader(key) {
  currentShaderKey = key;
  const displayName = SHADER_DISPLAY_NAMES[key] || key;

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

  // Update HUD & status bar
  const hudShader = document.getElementById('hud-shader-name');
  if (hudShader) hudShader.textContent = displayName + ' Shading';
  const statusEl = document.getElementById('status-shader');
  if (statusEl) statusEl.textContent = 'Shader: ' + displayName;

  // Update stage badge
  const stageBadge = document.getElementById('shader-stage-badge');
  if (stageBadge && SHADER_INFO[key]) {
    stageBadge.textContent = SHADER_INFO[key].stage;
  }

  // Toggle conditional material slider rows
  const rowRoughness = document.getElementById('row-roughness');
  if (rowRoughness) rowRoughness.classList.toggle('hidden', key !== 'orennayar');

  const rowToon = document.getElementById('row-toon');
  if (rowToon) rowToon.classList.toggle('hidden', key !== 'toon');

  const rowBump = document.getElementById('row-bump');
  if (rowBump) rowBump.classList.toggle('hidden', key !== 'normalmap');
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
//  Camera Reset
// ─────────────────────────────────────────────
function resetCamera() {
  camera.position.copy(DEFAULT_CAMERA_POS);
  orbitControls.target.copy(DEFAULT_CAMERA_TARGET);
  orbitControls.update();
}

// ─────────────────────────────────────────────
//  Studio Controls Binding
// ─────────────────────────────────────────────
function bindStudioControls() {
  // Preset model buttons
  document.querySelectorAll('.preset-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      const p = btn.dataset.preset;
      if (p) loadPreset(p);
    });
  });

  // Upload button & hidden input
  const uploadBtn = document.getElementById('btn-upload');
  const fileInputEl = document.getElementById('file-upload');
  if (uploadBtn && fileInputEl) {
    uploadBtn.addEventListener('click', () => fileInputEl.click());
    fileInputEl.addEventListener('change', (e) => {
      if (e.target.files && e.target.files[0]) handleUpload(e.target.files[0]);
      fileInputEl.value = '';
    });
  }

  // Light Spherical Sliders
  const inputAz = document.getElementById('input-azimuth');
  if (inputAz) {
    inputAz.addEventListener('input', (e) => {
      lightParams.azimuth = parseFloat(e.target.value);
      updateLightFromSpherical();
    });
  }

  const inputEl = document.getElementById('input-elevation');
  if (inputEl) {
    inputEl.addEventListener('input', (e) => {
      lightParams.elevation = parseFloat(e.target.value);
      updateLightFromSpherical();
    });
  }

  const inputDist = document.getElementById('input-distance');
  if (inputDist) {
    inputDist.addEventListener('input', (e) => {
      lightParams.distance = parseFloat(e.target.value);
      updateLightFromSpherical();
    });
  }

  // Quick Angle Chips
  document.querySelectorAll('.angle-chip').forEach((chip) => {
    chip.addEventListener('click', () => {
      lightParams.azimuth = parseFloat(chip.dataset.az);
      lightParams.elevation = parseFloat(chip.dataset.el);
      updateLightFromSpherical();
    });
  });

  // Light Color & Intensity
  const inputColor = document.getElementById('input-light-color');
  if (inputColor) {
    inputColor.addEventListener('input', (e) => {
      lightParams.color = e.target.value;
      updateLightFromSpherical();
    });
  }

  const inputInt = document.getElementById('input-light-intensity');
  if (inputInt) {
    inputInt.addEventListener('input', (e) => {
      lightParams.intensity = parseFloat(e.target.value);
      updateLightFromSpherical();
    });
  }

  const inputAmb = document.getElementById('input-ambient');
  if (inputAmb) {
    inputAmb.addEventListener('input', (e) => {
      lightParams.ambientIntensity = parseFloat(e.target.value);
      updateLightFromSpherical();
    });
  }

  // Auto-Orbit Toggle Button
  const btnAutoOrbit = document.getElementById('btn-auto-orbit');
  if (btnAutoOrbit) {
    btnAutoOrbit.addEventListener('click', () => {
      lightParams.autoOrbit = !lightParams.autoOrbit;
      btnAutoOrbit.classList.toggle('active', lightParams.autoOrbit);
    });
  }

  // Material Object Color
  const inputObjColor = document.getElementById('input-object-color');
  const previewObjColor = document.getElementById('object-color-preview');
  if (inputObjColor) {
    inputObjColor.addEventListener('input', (e) => {
      const hex = e.target.value;
      if (previewObjColor) previewObjColor.textContent = hex;
      const c = new THREE.Color(hex);
      uniforms.objectColor.value.set(c.r, c.g, c.b);
    });
  }

  // Material Parameter Sliders
  const bindSlider = (id, valId, callback, formatFn) => {
    const el = document.getElementById(id);
    const valEl = document.getElementById(valId);
    if (!el) return;
    el.addEventListener('input', (e) => {
      const v = parseFloat(e.target.value);
      callback(v);
      if (valEl) valEl.textContent = formatFn ? formatFn(v) : v;
    });
  };

  bindSlider('input-ka', 'val-ka', (v) => { uniforms.Ka.value = v; }, (v) => v.toFixed(2));
  bindSlider('input-kd', 'val-kd', (v) => { uniforms.Kd.value = v; }, (v) => v.toFixed(2));
  bindSlider('input-ks', 'val-ks', (v) => { uniforms.Ks.value = v; }, (v) => v.toFixed(2));
  bindSlider('input-shininess', 'val-shininess', (v) => { uniforms.shininess.value = v; }, (v) => Math.round(v));
  bindSlider('input-roughness', 'val-roughness', (v) => { uniforms.roughness.value = v; }, (v) => v.toFixed(2));
  bindSlider('input-toon-bands', 'val-toon-bands', (v) => { uniforms.toonBands.value = Math.round(v); }, (v) => Math.round(v));
  bindSlider('input-bump', 'val-bump', (v) => { uniforms.bumpStrength.value = v; }, (v) => v.toFixed(2));

  // Header Actions
  const btnReset = document.getElementById('btn-reset-cam');
  if (btnReset) btnReset.addEventListener('click', resetCamera);

  const btnToggleLight = document.getElementById('btn-toggle-light');
  if (btnToggleLight) {
    btnToggleLight.addEventListener('click', () => {
      lightGroup.visible = !lightGroup.visible;
      btnToggleLight.classList.toggle('active', lightGroup.visible);
    });
  }

  // Header Shader Tabs
  document.querySelectorAll('.shader-tab').forEach((btn) => {
    btn.addEventListener('click', () => {
      const key = btn.dataset.shader;
      if (key) setShader(key);
    });
  });
}

// ─────────────────────────────────────────────
//  Keyboard Shortcuts
// ─────────────────────────────────────────────
window.addEventListener('keydown', (e) => {
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

  const digit = parseInt(e.key, 10);
  if (digit >= 1 && digit <= 7) {
    const key = KEY_SHADERS[digit - 1];
    setShader(key);
    return;
  }

  switch (e.key.toUpperCase()) {
    case 'R': resetCamera(); break;
    case 'L': {
      lightGroup.visible = !lightGroup.visible;
      const btn = document.getElementById('btn-toggle-light');
      if (btn) btn.classList.toggle('active', lightGroup.visible);
      break;
    }
    case 'A': {
      lightParams.autoOrbit = !lightParams.autoOrbit;
      const btn = document.getElementById('btn-auto-orbit');
      if (btn) btn.classList.toggle('active', lightParams.autoOrbit);
      break;
    }
  }
});

// ─────────────────────────────────────────────
//  Animation Render Loop
// ─────────────────────────────────────────────
let lastTime = performance.now();
let frameCount = 0;
let fpsAccum = 0;

function animate() {
  requestAnimationFrame(animate);

  const now   = performance.now();
  const delta = (now - lastTime) / 1000;
  lastTime    = now;

  // FPS Telemetry
  frameCount++;
  fpsAccum += delta;
  if (fpsAccum >= 0.5) {
    const fps = Math.round(frameCount / fpsAccum);
    const fpsEl = document.getElementById('fps-counter');
    if (fpsEl) fpsEl.textContent = fps + ' fps';
    frameCount = 0;
    fpsAccum = 0;
  }

  // Auto-Orbit Light
  if (lightParams.autoOrbit) {
    lightParams.azimuth = (lightParams.azimuth + delta * lightParams.orbitSpeed * 60) % 360;
    updateLightFromSpherical();
  }

  orbitControls.update();
  renderer.render(scene, camera);
}

// ─────────────────────────────────────────────
//  Studio Initialization
// ─────────────────────────────────────────────
bindStudioControls();
loadPreset('Sphere');
setShader('phong');
animate();
