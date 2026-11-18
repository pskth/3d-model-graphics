/**
 * shaders.js — All 7 GLSL shader pairs for a Three.js ShaderMaterial viewer.
 *
 * Exports:
 *   SHADERS         — { flat, gouraud, phong, blinnphong, toon, normalmap, orennayar }
 *   UNIFORM_DEFAULTS — template uniform object (clone via makeUniforms())
 *   makeUniforms()  — deep-clones UNIFORM_DEFAULTS for each ShaderMaterial
 */

import * as THREE from 'three';

// ---------------------------------------------------------------------------
// UNIFORM DEFAULTS
// ---------------------------------------------------------------------------

export const UNIFORM_DEFAULTS = {
  lightPosition:    { value: new THREE.Vector3(3, 3, 3) }, // placeholder – main.js replaces
  lightColor:       { value: new THREE.Color(1, 1, 1) },
  lightIntensity:   { value: 1.0 },
  ambientIntensity: { value: 0.3 },
  objectColor:      { value: new THREE.Color(0.27, 0.53, 1.0) },
  Ka:               { value: 1.0 },
  Kd:               { value: 0.8 },
  Ks:               { value: 0.5 },
  shininess:        { value: 32.0 },
  roughness:        { value: 0.5 },
  toonBands:        { value: 4 },
  bumpStrength:     { value: 0.3 },
};

/**
 * Deep-clone UNIFORM_DEFAULTS so each ShaderMaterial gets its own uniform
 * instances (Three.js uniforms are mutable objects — sharing them is a bug).
 */
export function makeUniforms() {
  return {
    lightPosition:    { value: UNIFORM_DEFAULTS.lightPosition.value.clone() },
    lightColor:       { value: UNIFORM_DEFAULTS.lightColor.value.clone() },
    lightIntensity:   { value: UNIFORM_DEFAULTS.lightIntensity.value },
    ambientIntensity: { value: UNIFORM_DEFAULTS.ambientIntensity.value },
    objectColor:      { value: UNIFORM_DEFAULTS.objectColor.value.clone() },
    Ka:               { value: UNIFORM_DEFAULTS.Ka.value },
    Kd:               { value: UNIFORM_DEFAULTS.Kd.value },
    Ks:               { value: UNIFORM_DEFAULTS.Ks.value },
    shininess:        { value: UNIFORM_DEFAULTS.shininess.value },
    roughness:        { value: UNIFORM_DEFAULTS.roughness.value },
    toonBands:        { value: UNIFORM_DEFAULTS.toonBands.value },
    bumpStrength:     { value: UNIFORM_DEFAULTS.bumpStrength.value },
  };
}

// ---------------------------------------------------------------------------
// SHADER SOURCES
// ---------------------------------------------------------------------------

// ── 1. FLAT ─────────────────────────────────────────────────────────────────

const flatVert = /* glsl */`
  varying vec3 vWorldPos;

  void main() {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vWorldPos = worldPos.xyz;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`;

const flatFrag = /* glsl */`
  uniform vec3  lightPosition;
  uniform vec3  lightColor;
  uniform float lightIntensity;
  uniform float ambientIntensity;
  uniform vec3  objectColor;
  uniform float Ka;
  uniform float Kd;

  varying vec3 vWorldPos;

  void main() {
    // Reconstruct flat (face) normal from screen-space derivatives of world pos
    vec3 dx = dFdx(vWorldPos);
    vec3 dy = dFdy(vWorldPos);
    vec3 N  = normalize(cross(dx, dy));

    vec3 L   = normalize(lightPosition - vWorldPos);
    float diff = max(dot(N, L), 0.0);

    vec3 ambient  = Ka * ambientIntensity * lightColor * objectColor;
    vec3 diffuse  = Kd * diff * lightIntensity * lightColor * objectColor;

    vec3 color = clamp(ambient + diffuse, 0.0, 1.0);
    gl_FragColor = vec4(color, 1.0);
  }
`;

// ── 2. GOURAUD ──────────────────────────────────────────────────────────────

const gouraudVert = /* glsl */`
  uniform vec3  lightPosition;
  uniform vec3  lightColor;
  uniform float lightIntensity;
  uniform float ambientIntensity;
  uniform vec3  objectColor;
  uniform float Ka;
  uniform float Kd;
  uniform float Ks;
  uniform float shininess;

  varying vec3 vColor;

  void main() {
    vec4 worldPos4 = modelMatrix * vec4(position, 1.0);
    vec3 worldPos  = worldPos4.xyz;

    // World-space normal
    vec3 N = normalize(mat3(modelMatrix) * normal);
    vec3 L = normalize(lightPosition - worldPos);
    vec3 V = normalize(cameraPosition - worldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N, L), 0.0);
    float spec = (diff > 0.0) ? pow(max(dot(R, V), 0.0), shininess) : 0.0;

    vec3 ambient  = Ka * ambientIntensity * lightColor * objectColor;
    vec3 diffuse  = Kd * diff * lightIntensity * lightColor * objectColor;
    vec3 specular = Ks * spec * lightIntensity * lightColor;

    vColor = clamp(ambient + diffuse + specular, 0.0, 1.0);

    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`;

const gouraudFrag = /* glsl */`
  varying vec3 vColor;

  void main() {
    gl_FragColor = vec4(vColor, 1.0);
  }
`;

// ── 3. PHONG ────────────────────────────────────────────────────────────────

const phongVert = /* glsl */`
  varying vec3 vNormal;
  varying vec3 vWorldPos;

  void main() {
    vec4 worldPos4 = modelMatrix * vec4(position, 1.0);
    vWorldPos = worldPos4.xyz;
    vNormal   = normalize(mat3(modelMatrix) * normal);
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`;

const phongFrag = /* glsl */`
  uniform vec3  lightPosition;
  uniform vec3  lightColor;
  uniform float lightIntensity;
  uniform float ambientIntensity;
  uniform vec3  objectColor;
  uniform float Ka;
  uniform float Kd;
  uniform float Ks;
  uniform float shininess;

  varying vec3 vNormal;
  varying vec3 vWorldPos;

  void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(lightPosition - vWorldPos);
    vec3 V = normalize(cameraPosition - vWorldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N, L), 0.0);
    float spec = (diff > 0.0) ? Ks * pow(max(dot(R, V), 0.0), shininess) : 0.0;

    vec3 ambient  = Ka * ambientIntensity * lightColor * objectColor;
    vec3 diffuse  = Kd * diff * lightIntensity * lightColor * objectColor;
    vec3 specular = spec * lightIntensity * lightColor;

    vec3 color = clamp(ambient + diffuse + specular, 0.0, 1.0);
    gl_FragColor = vec4(color, 1.0);
  }
`;

// ── 4. BLINN-PHONG ──────────────────────────────────────────────────────────

const blinnPhongVert = phongVert; // identical to phong vertex shader

const blinnPhongFrag = /* glsl */`
  uniform vec3  lightPosition;
  uniform vec3  lightColor;
  uniform float lightIntensity;
  uniform float ambientIntensity;
  uniform vec3  objectColor;
  uniform float Ka;
  uniform float Kd;
  uniform float Ks;
  uniform float shininess;

  varying vec3 vNormal;
  varying vec3 vWorldPos;

  void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(lightPosition - vWorldPos);
    vec3 V = normalize(cameraPosition - vWorldPos);
    vec3 H = normalize(L + V); // Blinn halfway vector

    float diff = max(dot(N, L), 0.0);
    float spec = (diff > 0.0) ? Ks * pow(max(dot(N, H), 0.0), shininess) : 0.0;

    vec3 ambient  = Ka * ambientIntensity * lightColor * objectColor;
    vec3 diffuse  = Kd * diff * lightIntensity * lightColor * objectColor;
    vec3 specular = spec * lightIntensity * lightColor;

    vec3 color = clamp(ambient + diffuse + specular, 0.0, 1.0);
    gl_FragColor = vec4(color, 1.0);
  }
`;

// ── 5. TOON / CEL ───────────────────────────────────────────────────────────

const toonVert = phongVert; // identical to phong vertex shader

const toonFrag = /* glsl */`
  uniform vec3  lightPosition;
  uniform vec3  lightColor;
  uniform float lightIntensity;
  uniform float ambientIntensity;
  uniform vec3  objectColor;
  uniform float Ka;
  uniform float Kd;
  uniform float Ks;
  uniform float shininess;
  uniform int   toonBands;

  varying vec3 vNormal;
  varying vec3 vWorldPos;

  void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(lightPosition - vWorldPos);
    vec3 V = normalize(cameraPosition - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);

    // Quantise diffuse into discrete toon bands
    float bands     = float(toonBands);
    float quantDiff = floor(diff * bands) / bands;

    // Hard-step specular highlight
    float specRaw = (diff > 0.0) ? pow(max(dot(N, H), 0.0), shininess) : 0.0;
    float spec    = step(0.6, specRaw);

    vec3 ambient  = Ka * ambientIntensity * lightColor * objectColor;
    vec3 diffuse  = Kd * quantDiff * lightIntensity * lightColor * objectColor;
    vec3 specular = Ks * spec * lightIntensity * lightColor;

    vec3 color = clamp(ambient + diffuse + specular, 0.0, 1.0);
    gl_FragColor = vec4(color, 1.0);
  }
`;

// ── 6. NORMAL-MAP (procedural bump) ─────────────────────────────────────────

const normalMapVert = phongVert; // identical to phong vertex shader

const normalMapFrag = /* glsl */`
  uniform vec3  lightPosition;
  uniform vec3  lightColor;
  uniform float lightIntensity;
  uniform float ambientIntensity;
  uniform vec3  objectColor;
  uniform float Ka;
  uniform float Kd;
  uniform float Ks;
  uniform float shininess;
  uniform float bumpStrength;

  varying vec3 vNormal;
  varying vec3 vWorldPos;

  // Procedural height function used for bump mapping
  float heightAt(vec3 p) {
    return sin(p.x * 8.0) * cos(p.y * 8.0) * sin(p.z * 6.0);
  }

  void main() {
    // Finite-difference gradient of the height field
    float eps = 0.005;
    float hC  = heightAt(vWorldPos);
    float hX  = heightAt(vWorldPos + vec3(eps, 0.0, 0.0));
    float hY  = heightAt(vWorldPos + vec3(0.0, eps, 0.0));
    float hZ  = heightAt(vWorldPos + vec3(0.0, 0.0, eps));
    vec3 grad = vec3(hX - hC, hY - hC, hZ - hC) / eps;

    // Perturb the interpolated world-space normal
    vec3 N = normalize(vNormal - bumpStrength * grad);

    vec3 L = normalize(lightPosition - vWorldPos);
    vec3 V = normalize(cameraPosition - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = (diff > 0.0) ? Ks * pow(max(dot(N, H), 0.0), shininess) : 0.0;

    vec3 ambient  = Ka * ambientIntensity * lightColor * objectColor;
    vec3 diffuse  = Kd * diff * lightIntensity * lightColor * objectColor;
    vec3 specular = spec * lightIntensity * lightColor;

    vec3 color = clamp(ambient + diffuse + specular, 0.0, 1.0);
    gl_FragColor = vec4(color, 1.0);
  }
`;

// ── 7. OREN-NAYAR (matte / rough diffuse) ───────────────────────────────────

const orenNayarVert = phongVert; // identical to phong vertex shader

const orenNayarFrag = /* glsl */`
  uniform vec3  lightPosition;
  uniform vec3  lightColor;
  uniform float lightIntensity;
  uniform float ambientIntensity;
  uniform vec3  objectColor;
  uniform float Ka;
  uniform float Kd;
  uniform float roughness; // sigma (surface roughness), 0–1

  varying vec3 vNormal;
  varying vec3 vWorldPos;

  void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(lightPosition - vWorldPos);
    vec3 V = normalize(cameraPosition - vWorldPos);

    // Angles
    float NdotL  = max(dot(N, L), 0.0);
    float NdotV  = max(dot(N, V), 0.0);
    float thetaI = acos(clamp(NdotL, 0.0, 1.0));
    float thetaR = acos(clamp(NdotV, 0.0, 1.0));

    // Ramamoorthi & Hanrahan simplified Oren-Nayar
    float sigma  = roughness;
    float sigma2 = sigma * sigma;

    float A = 1.0 - 0.5 * sigma2 / (sigma2 + 0.33);
    float B = 0.45 * sigma2 / (sigma2 + 0.09);

    // cos(phi_i - phi_r) via projected vectors in tangent plane
    vec3  Ldiff  = L - NdotL * N;
    vec3  Vdiff  = V - NdotV * N;
    float lenL   = length(Ldiff);
    float lenV   = length(Vdiff);
    float cosPhi = (lenL > 0.001 && lenV > 0.001) ? max(dot(Ldiff / lenL, Vdiff / lenV), 0.0) : 0.0;

    float alpha = max(thetaI, thetaR);
    float beta  = min(min(thetaI, thetaR), 1.56);

    float orenNayar = NdotL * (A + B * cosPhi * sin(alpha) * tan(beta));

    vec3 ambient = Ka * ambientIntensity * lightColor * objectColor;
    vec3 diffuse = Kd * orenNayar * lightIntensity * lightColor * objectColor;

    vec3 color = clamp(ambient + diffuse, 0.0, 1.0);
    gl_FragColor = vec4(color, 1.0);
  }
`;

// ---------------------------------------------------------------------------
// EXPORT — SHADERS MAP
// ---------------------------------------------------------------------------

export const SHADERS = {
  flat: {
    vertexShader:   flatVert,
    fragmentShader: flatFrag,
  },
  gouraud: {
    vertexShader:   gouraudVert,
    fragmentShader: gouraudFrag,
  },
  phong: {
    vertexShader:   phongVert,
    fragmentShader: phongFrag,
  },
  blinnphong: {
    vertexShader:   blinnPhongVert,
    fragmentShader: blinnPhongFrag,
  },
  toon: {
    vertexShader:   toonVert,
    fragmentShader: toonFrag,
  },
  normalmap: {
    vertexShader:   normalMapVert,
    fragmentShader: normalMapFrag,
  },
  orennayar: {
    vertexShader:   orenNayarVert,
    fragmentShader: orenNayarFrag,
  },
};
