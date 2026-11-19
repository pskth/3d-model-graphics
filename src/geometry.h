#pragma once

// ============================================================
//  geometry.h — Procedural mesh generators + OBJ / STL loader
//
//  Every generator returns a Mesh.  After construction call
//  mesh.upload() once to create the VAO/VBO/EBO on the GPU.
//  Thereafter call mesh.draw() each frame.
//
//  Vertex layout (matches shaders.h attribute locations):
//    location 0 — position  vec3
//    location 1 — normal    vec3
//  Stride: 6 floats = 24 bytes.
//
//  All generators apply the same normalisation pipeline that
//  the JS version used:
//    1. Centre the bounding box at the origin.
//    2. Scale so the bounding-sphere radius == 1.
//    3. Recompute smooth vertex normals (average of face normals).
// ============================================================

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// tinyobjloader — compiled here via the implementation guard
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader.h"

// ── Vertex ────────────────────────────────────────────────────────────────────

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

// ── Mesh ──────────────────────────────────────────────────────────────────────

struct Mesh {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    bool   uploaded = false;

    // ── GPU upload ────────────────────────────────────────────────────────────
    void upload() {
        if (uploaded) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
        }

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                     vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                     indices.data(), GL_STATIC_DRAW);

        // position — location 0
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, position)));

        // normal — location 1
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, normal)));

        glBindVertexArray(0);
        uploaded = true;
    }

    void draw() const {
        if (!uploaded) return;
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(indices.size()),
                       GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    void destroy() {
        if (!uploaded) return;
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
        vao = vbo = ebo = 0;
        uploaded = false;
    }
};

// ── Internal helpers ──────────────────────────────────────────────────────────

namespace detail {

// Centre positions at origin, scale so bounding-sphere radius == 1.
inline void normalise(Mesh& m) {
    if (m.vertices.empty()) return;

    // Centroid of bounding box
    glm::vec3 lo = m.vertices[0].position;
    glm::vec3 hi = m.vertices[0].position;
    for (auto& v : m.vertices) {
        lo = glm::min(lo, v.position);
        hi = glm::max(hi, v.position);
    }
    glm::vec3 centre = (lo + hi) * 0.5f;

    for (auto& v : m.vertices)
        v.position -= centre;

    // Bounding-sphere radius
    float maxR = 0.0f;
    for (auto& v : m.vertices)
        maxR = std::max(maxR, glm::length(v.position));

    if (maxR < 1e-6f) return;
    float inv = 1.0f / maxR;
    for (auto& v : m.vertices)
        v.position *= inv;
}

// Recompute smooth vertex normals by accumulating area-weighted face normals.
inline void computeNormals(Mesh& m) {
    for (auto& v : m.vertices)
        v.normal = glm::vec3(0.0f);

    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        uint32_t i0 = m.indices[i + 0];
        uint32_t i1 = m.indices[i + 1];
        uint32_t i2 = m.indices[i + 2];

        glm::vec3 a = m.vertices[i0].position;
        glm::vec3 b = m.vertices[i1].position;
        glm::vec3 c = m.vertices[i2].position;

        glm::vec3 n = glm::cross(b - a, c - a); // magnitude == 2 * area

        m.vertices[i0].normal += n;
        m.vertices[i1].normal += n;
        m.vertices[i2].normal += n;
    }

    for (auto& v : m.vertices) {
        float len = glm::length(v.normal);
        if (len > 1e-6f)
            v.normal /= len;
        else
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

} // namespace detail

// ── 1. Sphere ─────────────────────────────────────────────────────────────────
// UV sphere with 64 stacks and 64 slices (matches Three.js SphereGeometry).

inline Mesh makeSphere(int stacks = 64, int slices = 64) {
    Mesh m;
    const float PI = glm::pi<float>();

    for (int i = 0; i <= stacks; ++i) {
        float phi = PI * static_cast<float>(i) / static_cast<float>(stacks);
        for (int j = 0; j <= slices; ++j) {
            float theta = 2.0f * PI * static_cast<float>(j) / static_cast<float>(slices);

            Vertex v;
            v.position = {
                std::sin(phi) * std::cos(theta),
                std::cos(phi),
                std::sin(phi) * std::sin(theta)
            };
            // Unit sphere — position == normal
            v.normal = glm::normalize(v.position);
            m.vertices.push_back(v);
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            uint32_t a = static_cast<uint32_t>(i * (slices + 1) + j);
            uint32_t b = a + static_cast<uint32_t>(slices + 1);
            m.indices.insert(m.indices.end(), {a, b, a + 1,
                                               b, b + 1, a + 1});
        }
    }

    // Sphere is already centred and radius==1; skip normalise, keep analytic normals.
    return m;
}

// ── 2. Cube ───────────────────────────────────────────────────────────────────
// Axis-aligned box 1.5 × 1.5 × 1.5. Each face has its own 4 vertices so
// hard normals are preserved per-face (matches Three.js BoxGeometry).

inline Mesh makeCube(float size = 1.5f) {
    Mesh m;
    float h = size * 0.5f;

    // 6 faces × 4 vertices each
    // Each face: { normal, 4 corners in CCW order }
    struct Face { glm::vec3 n; glm::vec3 p[4]; };
    const Face faces[6] = {
        {{ 0, 0, 1}, {{ h, h, h}, {-h, h, h}, {-h,-h, h}, { h,-h, h}}},  // +Z
        {{ 0, 0,-1}, {{-h, h,-h}, { h, h,-h}, { h,-h,-h}, {-h,-h,-h}}},  // -Z
        {{ 1, 0, 0}, {{ h, h,-h}, { h, h, h}, { h,-h, h}, { h,-h,-h}}},  // +X
        {{-1, 0, 0}, {{-h, h, h}, {-h, h,-h}, {-h,-h,-h}, {-h,-h, h}}},  // -X
        {{ 0, 1, 0}, {{-h, h,-h}, {-h, h, h}, { h, h, h}, { h, h,-h}}},  // +Y
        {{ 0,-1, 0}, {{-h,-h, h}, {-h,-h,-h}, { h,-h,-h}, { h,-h, h}}},  // -Y
    };

    for (auto& f : faces) {
        uint32_t base = static_cast<uint32_t>(m.vertices.size());
        for (int i = 0; i < 4; ++i)
            m.vertices.push_back({f.p[i], f.n});
        // Two triangles per quad
        m.indices.insert(m.indices.end(),
                         {base, base+1, base+2,
                          base, base+2, base+3});
    }

    return m; // already centred, no normalise needed
}

// ── 3. Icosahedron ────────────────────────────────────────────────────────────
// Regular icosahedron subdivided 4 times (matches IcosahedronGeometry detail=4).

namespace detail {

inline uint32_t midpoint(std::unordered_map<uint64_t, uint32_t>& cache,
                         std::vector<glm::vec3>& pts,
                         uint32_t i0, uint32_t i1) {
    uint64_t key = (static_cast<uint64_t>(std::min(i0,i1)) << 32)
                 |  static_cast<uint64_t>(std::max(i0,i1));
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    glm::vec3 mid = glm::normalize(pts[i0] + pts[i1]);
    uint32_t idx = static_cast<uint32_t>(pts.size());
    pts.push_back(mid);
    cache[key] = idx;
    return idx;
}

} // namespace detail

inline Mesh makeIcosahedron(int subdivisions = 4) {
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;

    std::vector<glm::vec3> pts = {
        glm::normalize(glm::vec3(-1, t, 0)),
        glm::normalize(glm::vec3( 1, t, 0)),
        glm::normalize(glm::vec3(-1,-t, 0)),
        glm::normalize(glm::vec3( 1,-t, 0)),
        glm::normalize(glm::vec3( 0,-1, t)),
        glm::normalize(glm::vec3( 0, 1, t)),
        glm::normalize(glm::vec3( 0,-1,-t)),
        glm::normalize(glm::vec3( 0, 1,-t)),
        glm::normalize(glm::vec3( t, 0,-1)),
        glm::normalize(glm::vec3( t, 0, 1)),
        glm::normalize(glm::vec3(-t, 0,-1)),
        glm::normalize(glm::vec3(-t, 0, 1)),
    };

    std::vector<std::array<uint32_t,3>> tris = {
        {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
        {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
        {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
        {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1},
    };

    for (int s = 0; s < subdivisions; ++s) {
        std::unordered_map<uint64_t, uint32_t> cache;
        std::vector<std::array<uint32_t,3>> next;
        for (auto& tri : tris) {
            uint32_t a = detail::midpoint(cache, pts, tri[0], tri[1]);
            uint32_t b = detail::midpoint(cache, pts, tri[1], tri[2]);
            uint32_t c = detail::midpoint(cache, pts, tri[2], tri[0]);
            next.push_back({tri[0], a, c});
            next.push_back({tri[1], b, a});
            next.push_back({tri[2], c, b});
            next.push_back({a, b, c});
        }
        tris = std::move(next);
    }

    Mesh m;
    m.vertices.reserve(pts.size());
    for (auto& p : pts)
        m.vertices.push_back({p, p}); // unit sphere: position == normal

    for (auto& tri : tris)
        m.indices.insert(m.indices.end(), {tri[0], tri[1], tri[2]});

    return m;
}

// ── 4. Cylinder ───────────────────────────────────────────────────────────────
// Open-ended cylinder with 64 radial segments. Radius 0.6, height 1.5.

inline Mesh makeCylinder(float radius = 0.6f, float height = 1.5f, int segs = 64) {
    Mesh m;
    const float PI = glm::pi<float>();
    float hy = height * 0.5f;

    // Side vertices
    for (int i = 0; i <= segs; ++i) {
        float a = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segs);
        float cx = std::cos(a);
        float cz = std::sin(a);

        glm::vec3 nrm(cx, 0.0f, cz);
        m.vertices.push_back({{ radius*cx,  hy, radius*cz}, nrm});
        m.vertices.push_back({{ radius*cx, -hy, radius*cz}, nrm});
    }

    // Side indices
    for (int i = 0; i < segs; ++i) {
        uint32_t b = static_cast<uint32_t>(i * 2);
        m.indices.insert(m.indices.end(),
                         {b, b+1, b+2,
                          b+1, b+3, b+2});
    }

    // Cap helper
    auto addCap = [&](float y, float ny) {
        uint32_t centre = static_cast<uint32_t>(m.vertices.size());
        m.vertices.push_back({{0.0f, y, 0.0f}, {0.0f, ny, 0.0f}});

        uint32_t first = static_cast<uint32_t>(m.vertices.size());
        for (int i = 0; i <= segs; ++i) {
            float a = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segs);
            m.vertices.push_back({{radius * std::cos(a), y, radius * std::sin(a)},
                                  {0.0f, ny, 0.0f}});
        }
        for (int i = 0; i < segs; ++i) {
            uint32_t a = first + static_cast<uint32_t>(i);
            uint32_t b = a + 1;
            if (ny > 0.0f)
                m.indices.insert(m.indices.end(), {centre, a, b});
            else
                m.indices.insert(m.indices.end(), {centre, b, a});
        }
    };

    addCap( hy,  1.0f);
    addCap(-hy, -1.0f);

    detail::normalise(m);
    return m;
}

// ── 5. Cone ───────────────────────────────────────────────────────────────────
// Cone with 64 radial segments. Base radius 0.8, height 1.6.

inline Mesh makeCone(float radius = 0.8f, float height = 1.6f, int segs = 64) {
    Mesh m;
    const float PI = glm::pi<float>();
    float hy = height * 0.5f;

    // Side vertices — smooth normals for the lateral surface
    // Slope angle for outward normal
    float slopeLen = std::sqrt(height*height + radius*radius);
    float ny = radius  / slopeLen;
    float nxz = height / slopeLen;

    for (int i = 0; i <= segs; ++i) {
        float a  = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segs);
        float cx = std::cos(a);
        float cz = std::sin(a);

        // Apex
        m.vertices.push_back({{0.0f, hy, 0.0f}, glm::normalize(glm::vec3(nxz*cx, ny, nxz*cz))});
        // Base ring
        m.vertices.push_back({{radius*cx, -hy, radius*cz}, glm::normalize(glm::vec3(nxz*cx, ny, nxz*cz))});
    }

    for (int i = 0; i < segs; ++i) {
        uint32_t b = static_cast<uint32_t>(i * 2);
        m.indices.insert(m.indices.end(), {b, b+1, b+2,
                                           b+1, b+3, b+2});
    }

    // Base cap
    uint32_t centre = static_cast<uint32_t>(m.vertices.size());
    m.vertices.push_back({{0.0f, -hy, 0.0f}, {0.0f, -1.0f, 0.0f}});
    uint32_t first = static_cast<uint32_t>(m.vertices.size());
    for (int i = 0; i <= segs; ++i) {
        float a = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segs);
        m.vertices.push_back({{radius*std::cos(a), -hy, radius*std::sin(a)},
                              {0.0f, -1.0f, 0.0f}});
    }
    for (int i = 0; i < segs; ++i) {
        uint32_t a = first + static_cast<uint32_t>(i);
        m.indices.insert(m.indices.end(), {centre, a+1, a});
    }

    detail::normalise(m);
    return m;
}

// ── 6. Torus Knot ─────────────────────────────────────────────────────────────
// (3,2) torus knot. Tube radius 0.25, 128 tubular segments, 32 radial segments.
// Matches Three.js TorusKnotGeometry(0.7, 0.25, 128, 32).

inline Mesh makeTorusKnot(float R = 0.7f, float tubeR = 0.25f,
                           int tubularSegs = 128, int radialSegs = 32,
                           int p = 2, int q = 3) {
    Mesh m;
    const float PI  = glm::pi<float>();
    const float TAU = glm::two_pi<float>();

    // Helper: point on the knot spine at parameter t in [0, 2*pi)
    auto knotPoint = [&](float t) -> glm::vec3 {
        float r = R + R * 0.5f * std::cos(static_cast<float>(q) * t);
        return {
            r * std::cos(static_cast<float>(p) * t),
            r * std::sin(static_cast<float>(p) * t),
            -R * 0.5f * std::sin(static_cast<float>(q) * t)
        };
    };

    for (int i = 0; i <= tubularSegs; ++i) {
        float u  = TAU * static_cast<float>(i) / static_cast<float>(tubularSegs);
        glm::vec3 P  = knotPoint(u);

        // Frenet-Serret frame
        glm::vec3 T = glm::normalize(knotPoint(u + 0.01f) - knotPoint(u - 0.01f));
        glm::vec3 N = glm::normalize(knotPoint(u + 0.01f) + knotPoint(u - 0.01f) - 2.0f * P);
        if (glm::length(N) < 1e-5f)
            N = glm::vec3(0, 1, 0);
        else
            N = glm::normalize(N);
        glm::vec3 B = glm::cross(T, N);

        for (int j = 0; j <= radialSegs; ++j) {
            float v  = TAU * static_cast<float>(j) / static_cast<float>(radialSegs);
            float cx = std::cos(v);
            float cy = std::sin(v);

            glm::vec3 pos = P + tubeR * (cx * N + cy * B);
            glm::vec3 nrm = glm::normalize(pos - P);

            m.vertices.push_back({pos, nrm});
        }
    }

    for (int i = 0; i < tubularSegs; ++i) {
        for (int j = 0; j < radialSegs; ++j) {
            uint32_t a = static_cast<uint32_t>(i  * (radialSegs+1) + j);
            uint32_t b = static_cast<uint32_t>((i+1) * (radialSegs+1) + j);
            m.indices.insert(m.indices.end(),
                             {a, b, a+1,
                              b, b+1, a+1});
        }
    }

    detail::normalise(m);
    return m;
}

// ── 7. Small sphere (light orb) ───────────────────────────────────────────────
// Low-poly sphere used for the light source visualisation.

inline Mesh makeLightOrb(float radius = 0.12f) {
    return makeSphere(16, 16); // positions scaled below
    // Scaling to radius is applied by uModel in main.cpp, not here.
    // This just returns a unit sphere; pass a scale matrix to the shader.
    (void)radius;
}

// ── OBJ loader ────────────────────────────────────────────────────────────────
// Uses tinyobjloader. Loads the first shape, triangulates, builds index buffer.

inline Mesh loadOBJ(const std::string& path) {
    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate = true;
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path, cfg))
        throw std::runtime_error("OBJ load failed: " + reader.Error());

    if (!reader.Warning().empty())
        fprintf(stderr, "[OBJ warning] %s\n", reader.Warning().c_str());

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();

    if (shapes.empty())
        throw std::runtime_error("OBJ contains no shapes: " + path);

    Mesh m;

    // Build a de-duplicated vertex list using an index map keyed on
    // {position_index, normal_index}.
    using Key = std::pair<int,int>;
    struct PairHash {
        size_t operator()(const Key& k) const {
            return std::hash<int>()(k.first) ^ (std::hash<int>()(k.second) << 16);
        }
    };
    std::unordered_map<Key, uint32_t, PairHash> vertMap;

    for (auto& shape : shapes) {
        for (auto& idx : shape.mesh.indices) {
            Key k{idx.vertex_index, idx.normal_index};
            auto [it, inserted] = vertMap.emplace(k, static_cast<uint32_t>(m.vertices.size()));

            if (inserted) {
                Vertex v{};
                int vi = idx.vertex_index * 3;
                v.position = { attrib.vertices[vi],
                               attrib.vertices[vi+1],
                               attrib.vertices[vi+2] };

                if (idx.normal_index >= 0) {
                    int ni = idx.normal_index * 3;
                    v.normal = { attrib.normals[ni],
                                 attrib.normals[ni+1],
                                 attrib.normals[ni+2] };
                }
                m.vertices.push_back(v);
            }
            m.indices.push_back(it->second);
        }
    }

    // Recompute normals if the OBJ had none
    bool hasNormals = !attrib.normals.empty();
    if (!hasNormals)
        detail::computeNormals(m);

    detail::normalise(m);
    return m;
}

// ── STL loader ────────────────────────────────────────────────────────────────
// Handles both binary and ASCII STL. Each triangle gets its own 3 vertices
// (STL stores one normal per facet), then we recompute smooth normals.

inline Mesh loadSTL(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("Cannot open STL file: " + path);

    // Peek at the first 5 bytes to distinguish ASCII from binary
    char header[6] = {};
    f.read(header, 5);
    f.seekg(0, std::ios::beg);
    bool isAscii = (std::strncmp(header, "solid", 5) == 0);

    Mesh m;

    if (isAscii) {
        // ASCII STL
        std::string line, token;
        glm::vec3 facetNormal{};
        while (std::getline(f, line)) {
            std::istringstream ss(line);
            ss >> token;
            if (token == "facet") {
                std::string norm; float nx, ny, nz;
                ss >> norm >> nx >> ny >> nz;
                facetNormal = {nx, ny, nz};
            } else if (token == "vertex") {
                float x, y, z;
                ss >> x >> y >> z;
                uint32_t idx = static_cast<uint32_t>(m.vertices.size());
                m.vertices.push_back({{x,y,z}, facetNormal});
                m.indices.push_back(idx);
            }
        }
    } else {
        // Binary STL: 80-byte header, uint32 triangle count, then 50 bytes/tri
        char buf[80];
        f.read(buf, 80);  // skip header

        uint32_t triCount = 0;
        f.read(reinterpret_cast<char*>(&triCount), 4);

        m.vertices.reserve(triCount * 3);
        m.indices.reserve(triCount * 3);

        for (uint32_t t = 0; t < triCount; ++t) {
            float n[3], v[9];
            uint16_t attr;
            f.read(reinterpret_cast<char*>(n), 12);
            f.read(reinterpret_cast<char*>(v), 36);
            f.read(reinterpret_cast<char*>(&attr), 2);

            glm::vec3 facetN{n[0], n[1], n[2]};
            for (int i = 0; i < 3; ++i) {
                uint32_t idx = static_cast<uint32_t>(m.vertices.size());
                m.vertices.push_back({{v[i*3], v[i*3+1], v[i*3+2]}, facetN});
                m.indices.push_back(idx);
            }
        }
    }

    if (m.vertices.empty())
        throw std::runtime_error("STL file is empty or unreadable: " + path);

    detail::computeNormals(m);
    detail::normalise(m);
    return m;
}

// ── Preset dispatch ───────────────────────────────────────────────────────────

enum class GeomPreset {
    Sphere = 0,
    Icosahedron,
    Cube,
    Cylinder,
    Cone,
    TorusKnot,
    Custom,       // loaded from file
    COUNT
};

static const char* PRESET_NAMES[] = {
    "Sphere",
    "Icosahedron",
    "Cube",
    "Cylinder",
    "Cone",
    "Torus Knot",
    "Custom",
};

inline Mesh buildPreset(GeomPreset p) {
    switch (p) {
        case GeomPreset::Sphere:      return makeSphere();
        case GeomPreset::Icosahedron: return makeIcosahedron();
        case GeomPreset::Cube:        return makeCube();
        case GeomPreset::Cylinder:    return makeCylinder();
        case GeomPreset::Cone:        return makeCone();
        case GeomPreset::TorusKnot:   return makeTorusKnot();
        default:                      return makeSphere();
    }
}

inline Mesh loadMesh(const std::string& path) {
    // Extension dispatch
    auto ext = path.substr(path.rfind('.') + 1);
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ext == "obj")  return loadOBJ(path);
    if (ext == "stl")  return loadSTL(path);

    throw std::runtime_error("Unsupported format: ." + ext +
                             " (supported: .obj, .stl)");
}
