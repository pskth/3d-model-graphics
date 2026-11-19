#pragma once

// ============================================================
//  shaders.h — All 7 GLSL shader pairs for Lumina
//
//  Every shader receives these uniforms from main.cpp:
//    mat4  uModel, uView, uProjection
//    vec3  uCameraPos
//    vec3  uLightPosition
//    vec3  uLightColor
//    float uLightIntensity
//    float uAmbientIntensity
//    vec3  uObjectColor
//    float uKa, uKd, uKs
//    float uShininess
//    float uRoughness      (Oren-Nayar only)
//    int   uToonBands      (Toon only)
//    float uBumpStrength   (Bump Map only)
//
//  Vertex attribute layout (matches Mesh::upload()):
//    location 0 — position  (vec3)
//    location 1 — normal    (vec3)
// ============================================================

#include <array>
#include <string_view>

// ── GLSL version preamble ─────────────────────────────────────────────────────
// OpenGL 3.3 Core — matches the GLAD loader we vendor.
// dFdx / dFdy are core in GLSL 3.30, so no extension needed.

static constexpr const char* GLSL_VERSION = "#version 330 core\n";

// ── Shared vertex shader used by Phong, Blinn-Phong, Toon, Bump Map, Oren-Nayar
// Outputs world-space position and normal for per-fragment lighting.
static constexpr const char* VERT_STANDARD = R"GLSL(
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;
out vec3 vWorldPos;

void main() {
    vec4 worldPos4 = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos4.xyz;
    // Normal matrix: transpose of inverse of upper-left 3x3 of model matrix.
    // Using mat3(uModel) is correct when model has uniform scale (all our presets do).
    vNormal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    gl_Position = uProjection * uView * worldPos4;
}
)GLSL";

// ── 1. FLAT ───────────────────────────────────────────────────────────────────
// Passes only world-space position; face normal reconstructed per-fragment
// from screen-space derivatives — identical algorithm to the JS version.

static constexpr const char* VERT_FLAT = R"GLSL(
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;   // unused, but kept for consistent VAO layout

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPos;

void main() {
    vec4 worldPos4 = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos4.xyz;
    gl_Position = uProjection * uView * worldPos4;
}
)GLSL";

static constexpr const char* FRAG_FLAT = R"GLSL(
#version 330 core

uniform vec3  uLightPosition;
uniform vec3  uLightColor;
uniform float uLightIntensity;
uniform float uAmbientIntensity;
uniform vec3  uObjectColor;
uniform float uKa;
uniform float uKd;

in  vec3 vWorldPos;
out vec4 fragColor;

void main() {
    // Reconstruct flat (face) normal from screen-space derivatives
    vec3 dx = dFdx(vWorldPos);
    vec3 dy = dFdy(vWorldPos);
    vec3 N  = normalize(cross(dx, dy));

    vec3  L    = normalize(uLightPosition - vWorldPos);
    float diff = max(dot(N, L), 0.0);

    vec3 ambient = uKa * uAmbientIntensity * uLightColor * uObjectColor;
    vec3 diffuse = uKd * diff * uLightIntensity * uLightColor * uObjectColor;

    fragColor = vec4(clamp(ambient + diffuse, 0.0, 1.0), 1.0);
}
)GLSL";

// ── 2. GOURAUD ────────────────────────────────────────────────────────────────
// Full Phong lighting calculated per-vertex; interpolated colour passed to
// fragment stage unchanged.

static constexpr const char* VERT_GOURAUD = R"GLSL(
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4  uModel;
uniform mat4  uView;
uniform mat4  uProjection;
uniform vec3  uCameraPos;
uniform vec3  uLightPosition;
uniform vec3  uLightColor;
uniform float uLightIntensity;
uniform float uAmbientIntensity;
uniform vec3  uObjectColor;
uniform float uKa;
uniform float uKd;
uniform float uKs;
uniform float uShininess;

out vec3 vColor;

void main() {
    vec3 worldPos = (uModel * vec4(aPosition, 1.0)).xyz;
    vec3 N = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vec3 L = normalize(uLightPosition - worldPos);
    vec3 V = normalize(uCameraPos - worldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N, L), 0.0);
    float spec = (diff > 0.0) ? pow(max(dot(R, V), 0.0), uShininess) : 0.0;

    vec3 ambient  = uKa * uAmbientIntensity * uLightColor * uObjectColor;
    vec3 diffuse  = uKd * diff * uLightIntensity * uLightColor * uObjectColor;
    vec3 specular = uKs * spec * uLightIntensity * uLightColor;

    vColor = clamp(ambient + diffuse + specular, 0.0, 1.0);

    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
)GLSL";

static constexpr const char* FRAG_GOURAUD = R"GLSL(
#version 330 core

in  vec3 vColor;
out vec4 fragColor;

void main() {
    fragColor = vec4(vColor, 1.0);
}
)GLSL";

// ── 3. PHONG ──────────────────────────────────────────────────────────────────
// Smooth normals interpolated per-fragment; specular via reflection vector.

static constexpr const char* FRAG_PHONG = R"GLSL(
#version 330 core

uniform vec3  uLightPosition;
uniform vec3  uLightColor;
uniform float uLightIntensity;
uniform float uAmbientIntensity;
uniform vec3  uObjectColor;
uniform vec3  uCameraPos;
uniform float uKa;
uniform float uKd;
uniform float uKs;
uniform float uShininess;

in  vec3 vNormal;
in  vec3 vWorldPos;
out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPosition - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N, L), 0.0);
    float spec = (diff > 0.0) ? uKs * pow(max(dot(R, V), 0.0), uShininess) : 0.0;

    vec3 ambient  = uKa * uAmbientIntensity * uLightColor * uObjectColor;
    vec3 diffuse  = uKd * diff * uLightIntensity * uLightColor * uObjectColor;
    vec3 specular = spec * uLightIntensity * uLightColor;

    fragColor = vec4(clamp(ambient + diffuse + specular, 0.0, 1.0), 1.0);
}
)GLSL";

// ── 4. BLINN-PHONG ────────────────────────────────────────────────────────────
// Halfway vector H = normalize(L + V) replaces the reflection vector.

static constexpr const char* FRAG_BLINNPHONG = R"GLSL(
#version 330 core

uniform vec3  uLightPosition;
uniform vec3  uLightColor;
uniform float uLightIntensity;
uniform float uAmbientIntensity;
uniform vec3  uObjectColor;
uniform vec3  uCameraPos;
uniform float uKa;
uniform float uKd;
uniform float uKs;
uniform float uShininess;

in  vec3 vNormal;
in  vec3 vWorldPos;
out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPosition - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = (diff > 0.0) ? uKs * pow(max(dot(N, H), 0.0), uShininess) : 0.0;

    vec3 ambient  = uKa * uAmbientIntensity * uLightColor * uObjectColor;
    vec3 diffuse  = uKd * diff * uLightIntensity * uLightColor * uObjectColor;
    vec3 specular = spec * uLightIntensity * uLightColor;

    fragColor = vec4(clamp(ambient + diffuse + specular, 0.0, 1.0), 1.0);
}
)GLSL";

// ── 5. TOON / CEL ─────────────────────────────────────────────────────────────
// Diffuse quantized into discrete bands; specular hard-thresholded via step().

static constexpr const char* FRAG_TOON = R"GLSL(
#version 330 core

uniform vec3  uLightPosition;
uniform vec3  uLightColor;
uniform float uLightIntensity;
uniform float uAmbientIntensity;
uniform vec3  uObjectColor;
uniform vec3  uCameraPos;
uniform float uKa;
uniform float uKd;
uniform float uKs;
uniform float uShininess;
uniform int   uToonBands;

in  vec3 vNormal;
in  vec3 vWorldPos;
out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPosition - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    float diff      = max(dot(N, L), 0.0);
    float bands     = float(uToonBands);
    float quantDiff = floor(diff * bands) / bands;

    float specRaw = (diff > 0.0) ? pow(max(dot(N, H), 0.0), uShininess) : 0.0;
    float spec    = step(0.6, specRaw);

    vec3 ambient  = uKa * uAmbientIntensity * uLightColor * uObjectColor;
    vec3 diffuse  = uKd * quantDiff * uLightIntensity * uLightColor * uObjectColor;
    vec3 specular = uKs * spec * uLightIntensity * uLightColor;

    fragColor = vec4(clamp(ambient + diffuse + specular, 0.0, 1.0), 1.0);
}
)GLSL";

// ── 6. PROCEDURAL BUMP MAP ────────────────────────────────────────────────────
// 3D harmonic height field h(p) = sin(8x)*cos(8y)*sin(6z); finite-difference
// gradient perturbs the interpolated world-space normal.

static constexpr const char* FRAG_BUMPMAP = R"GLSL(
#version 330 core

uniform vec3  uLightPosition;
uniform vec3  uLightColor;
uniform float uLightIntensity;
uniform float uAmbientIntensity;
uniform vec3  uObjectColor;
uniform vec3  uCameraPos;
uniform float uKa;
uniform float uKd;
uniform float uKs;
uniform float uShininess;
uniform float uBumpStrength;

in  vec3 vNormal;
in  vec3 vWorldPos;
out vec4 fragColor;

float heightAt(vec3 p) {
    return sin(p.x * 8.0) * cos(p.y * 8.0) * sin(p.z * 6.0);
}

void main() {
    // Finite-difference gradient of the height field
    const float eps = 0.005;
    float hC  = heightAt(vWorldPos);
    float hX  = heightAt(vWorldPos + vec3(eps, 0.0, 0.0));
    float hY  = heightAt(vWorldPos + vec3(0.0, eps, 0.0));
    float hZ  = heightAt(vWorldPos + vec3(0.0, 0.0, eps));
    vec3  grad = vec3(hX - hC, hY - hC, hZ - hC) / eps;

    vec3 N = normalize(vNormal - uBumpStrength * grad);
    vec3 L = normalize(uLightPosition - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = (diff > 0.0) ? uKs * pow(max(dot(N, H), 0.0), uShininess) : 0.0;

    vec3 ambient  = uKa * uAmbientIntensity * uLightColor * uObjectColor;
    vec3 diffuse  = uKd * diff * uLightIntensity * uLightColor * uObjectColor;
    vec3 specular = spec * uLightIntensity * uLightColor;

    fragColor = vec4(clamp(ambient + diffuse + specular, 0.0, 1.0), 1.0);
}
)GLSL";

// ── 7. OREN-NAYAR ─────────────────────────────────────────────────────────────
// Ramamoorthi & Hanrahan simplified microfacet diffuse for matte surfaces.

static constexpr const char* FRAG_ORENNAYAR = R"GLSL(
#version 330 core

uniform vec3  uLightPosition;
uniform vec3  uLightColor;
uniform float uLightIntensity;
uniform float uAmbientIntensity;
uniform vec3  uObjectColor;
uniform vec3  uCameraPos;
uniform float uKa;
uniform float uKd;
uniform float uRoughness;   // sigma in [0, 1]

in  vec3 vNormal;
in  vec3 vWorldPos;
out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPosition - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);

    float NdotL  = max(dot(N, L), 0.0);
    float NdotV  = max(dot(N, V), 0.0);
    float thetaI = acos(clamp(NdotL, 0.0, 1.0));
    float thetaR = acos(clamp(NdotV, 0.0, 1.0));

    float sigma  = uRoughness;
    float sigma2 = sigma * sigma;
    float A = 1.0 - 0.5 * sigma2 / (sigma2 + 0.33);
    float B = 0.45 * sigma2 / (sigma2 + 0.09);

    // cos(phi_i - phi_r) via projected vectors in the tangent plane
    vec3  Ldiff  = L - NdotL * N;
    vec3  Vdiff  = V - NdotV * N;
    float lenL   = length(Ldiff);
    float lenV   = length(Vdiff);
    float cosPhi = (lenL > 0.001 && lenV > 0.001)
                   ? max(dot(Ldiff / lenL, Vdiff / lenV), 0.0)
                   : 0.0;

    float alpha     = max(thetaI, thetaR);
    float beta      = min(thetaI, thetaR);
    float orenNayar = NdotL * (A + B * cosPhi * sin(alpha) * tan(beta));

    vec3 ambient = uKa * uAmbientIntensity * uLightColor * uObjectColor;
    vec3 diffuse = uKd * orenNayar * uLightIntensity * uLightColor * uObjectColor;

    fragColor = vec4(clamp(ambient + diffuse, 0.0, 1.0), 1.0);
}
)GLSL";

// ── Light orb (unlit, constant yellow-white colour) ───────────────────────────

static constexpr const char* VERT_UNLIT = R"GLSL(
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;   // unused

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
)GLSL";

static constexpr const char* FRAG_UNLIT = R"GLSL(
#version 330 core

uniform vec3 uUnlitColor;

out vec4 fragColor;

void main() {
    fragColor = vec4(uUnlitColor, 1.0);
}
)GLSL";

// ── Descriptor table ─────────────────────────────────────────────────────────
// Provides human-readable metadata and formula strings shown in the ImGui panel.

struct ShaderInfo {
    const char* key;
    const char* name;
    const char* stage;
    const char* description;
    const char* formula;
    const char* vertSrc;
    const char* fragSrc;
};

static const ShaderInfo SHADER_TABLE[] = {
    {
        "flat",
        "Flat Shading",
        "PRIMITIVE",
        "Computes a constant colour per triangle using screen-space derivatives of\n"
        "world position (dFdx x dFdy). Each polygon displays faceted shading without\n"
        "normal interpolation across vertices.",
        "N = normalize(dFdx(vWorldPos) x dFdy(vWorldPos))\n"
        "Diffuse = Kd * max(dot(N, L), 0) * Id * Color\n"
        "Output = Ambient + Diffuse  (faceted)",
        VERT_FLAT,
        FRAG_FLAT,
    },
    {
        "gouraud",
        "Gouraud Shading",
        "VERTEX",
        "Evaluates the full Phong lighting model per vertex and interpolates the\n"
        "resulting RGB colour across the primitive. Fast, but specular highlights\n"
        "blur or vanish between vertices.",
        "vColor = Ka*Ia*Cd + Kd*max(N.L,0)*Id*Cd + Ks*pow(max(R.V,0),s)*Is\n"
        "Fragment: fragColor = vec4(vColor, 1.0)",
        VERT_GOURAUD,
        FRAG_GOURAUD,
    },
    {
        "phong",
        "Phong Shading",
        "FRAGMENT",
        "Interpolates smooth surface normals per fragment and evaluates the reflection\n"
        "vector R = reflect(-L, N) at every pixel. Produces precise, natural specular\n"
        "highlights.",
        "R = reflect(-L, N)\n"
        "Specular = Ks * pow(max(dot(R, V), 0), shininess) * Is\n"
        "Output = Ka*Ia*Cd + Kd*max(N.L,0)*Id*Cd + Specular",
        VERT_STANDARD,
        FRAG_PHONG,
    },
    {
        "blinnphong",
        "Blinn-Phong Shading",
        "FRAGMENT",
        "Replaces the reflection vector with the halfway vector H = normalize(L + V).\n"
        "More physically plausible at grazing angles and computationally efficient.",
        "H = normalize(L + V)\n"
        "Specular = Ks * pow(max(dot(N, H), 0), shininess) * Is\n"
        "Output = Ka*Ia*Cd + Kd*max(N.L,0)*Id*Cd + Specular",
        VERT_STANDARD,
        FRAG_BLINNPHONG,
    },
    {
        "toon",
        "Toon / Cel Shading",
        "FRAGMENT",
        "Quantizes continuous diffuse illumination into discrete stepped luminance\n"
        "bands with hard specular thresholding for a graphic-novel appearance.",
        "quantDiff = floor(max(N.L, 0) * bands) / bands\n"
        "Specular  = step(0.6, pow(max(N.H, 0), s))\n"
        "Output    = Ambient + Kd*quantDiff*Id + Ks*Specular",
        VERT_STANDARD,
        FRAG_TOON,
    },
    {
        "normalmap",
        "Procedural Bump Mapping",
        "FRAGMENT",
        "Evaluates a 3D harmonic height field h(p) and perturbs the interpolated\n"
        "normal via finite-difference gradient: N' = normalize(N - strength * grad h).",
        "h(p) = sin(8x) * cos(8y) * sin(6z)\n"
        "grad h = vec3(h(p+ex)-h(p), h(p+ey)-h(p), h(p+ez)-h(p)) / e\n"
        "N' = normalize(N - bumpStrength * grad h)",
        VERT_STANDARD,
        FRAG_BUMPMAP,
    },
    {
        "orennayar",
        "Oren-Nayar Diffuse",
        "FRAGMENT",
        "Microfacet reflectance model for rough, matte surfaces (clay, chalk, skin,\n"
        "fabric). Accounts for microscopic inter-facet masking and limb brightening.",
        "L = Kd * (A + B*sin(a)*tan(b)*max(0, cos(phi_i - phi_r))) * (N.L)\n"
        "A = 1 - 0.5*s2/(s2+0.33),  B = 0.45*s2/(s2+0.09),  s = roughness",
        VERT_STANDARD,
        FRAG_ORENNAYAR,
    },
};

static constexpr int SHADER_COUNT = static_cast<int>(
    sizeof(SHADER_TABLE) / sizeof(SHADER_TABLE[0])
);
