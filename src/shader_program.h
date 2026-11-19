#pragma once

// ============================================================
//  shader_program.h — OpenGL shader compile / link helpers
//
//  Usage:
//    ShaderProgram prog;
//    prog.build(vertSrc, fragSrc);     // compile + link
//    prog.use();                       // glUseProgram
//    prog.setMat4("uModel", m);
//    prog.setVec3("uLightPosition", p);
//    prog.setFloat("uKd", 0.8f);
//    prog.setInt("uToonBands", 4);
//
//  Compilation errors are thrown as std::runtime_error so
//  main.cpp can catch and display them without crashing.
// ============================================================

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>
#include <string>
#include <unordered_map>

class ShaderProgram {
public:
    ShaderProgram() = default;

    // Non-copyable — owns a GL object
    ShaderProgram(const ShaderProgram&)            = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ShaderProgram(ShaderProgram&& o) noexcept : id_(o.id_) { o.id_ = 0; }
    ShaderProgram& operator=(ShaderProgram&& o) noexcept {
        destroy();
        id_ = o.id_;
        o.id_ = 0;
        return *this;
    }

    ~ShaderProgram() { destroy(); }

    // ── Build ─────────────────────────────────────────────────────────────────

    // Compile vertSrc + fragSrc, link, and store the program ID.
    // Throws std::runtime_error on any failure.
    void build(const char* vertSrc, const char* fragSrc) {
        destroy();

        GLuint vert = compile(GL_VERTEX_SHADER,   vertSrc);
        GLuint frag = compile(GL_FRAGMENT_SHADER, fragSrc);

        id_ = glCreateProgram();
        glAttachShader(id_, vert);
        glAttachShader(id_, frag);
        glLinkProgram(id_);

        // Shaders are linked into the program; they can be deleted now
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint ok = 0;
        glGetProgramiv(id_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(id_, sizeof(log), nullptr, log);
            glDeleteProgram(id_);
            id_ = 0;
            throw std::runtime_error(std::string("Shader link error:\n") + log);
        }

        // Flush the location cache — new program, new locations
        locationCache_.clear();
    }

    // ── Activation ────────────────────────────────────────────────────────────

    void use() const { glUseProgram(id_); }

    GLuint id() const { return id_; }

    bool valid() const { return id_ != 0; }

    // ── Uniform setters ───────────────────────────────────────────────────────

    void setFloat(const char* name, float v) const {
        glUniform1f(loc(name), v);
    }

    void setInt(const char* name, int v) const {
        glUniform1i(loc(name), v);
    }

    void setVec3(const char* name, const glm::vec3& v) const {
        glUniform3fv(loc(name), 1, glm::value_ptr(v));
    }

    void setVec3(const char* name, float x, float y, float z) const {
        glUniform3f(loc(name), x, y, z);
    }

    void setMat4(const char* name, const glm::mat4& m) const {
        glUniformMatrix4fv(loc(name), 1, GL_FALSE, glm::value_ptr(m));
    }

    // ── Convenience: upload the full shared uniform set ───────────────────────
    // Binds every uniform that every shader might have.  Missing uniforms
    // return -1 from glGetUniformLocation and the driver silently ignores them,
    // so it is safe to call this for all 7 shaders without branching.

    struct Uniforms {
        glm::mat4  model;
        glm::mat4  view;
        glm::mat4  projection;
        glm::vec3  cameraPos;
        glm::vec3  lightPosition;
        glm::vec3  lightColor;
        float      lightIntensity;
        float      ambientIntensity;
        glm::vec3  objectColor;
        float      Ka;
        float      Kd;
        float      Ks;
        float      shininess;
        float      roughness;
        int        toonBands;
        float      bumpStrength;
    };

    void uploadUniforms(const Uniforms& u) const {
        setMat4 ("uModel",            u.model);
        setMat4 ("uView",             u.view);
        setMat4 ("uProjection",       u.projection);
        setVec3 ("uCameraPos",        u.cameraPos);
        setVec3 ("uLightPosition",    u.lightPosition);
        setVec3 ("uLightColor",       u.lightColor);
        setFloat("uLightIntensity",   u.lightIntensity);
        setFloat("uAmbientIntensity", u.ambientIntensity);
        setVec3 ("uObjectColor",      u.objectColor);
        setFloat("uKa",               u.Ka);
        setFloat("uKd",               u.Kd);
        setFloat("uKs",               u.Ks);
        setFloat("uShininess",        u.shininess);
        setFloat("uRoughness",        u.roughness);
        setInt  ("uToonBands",        u.toonBands);
        setFloat("uBumpStrength",     u.bumpStrength);
    }

    // Upload unlit orb uniforms (model/view/proj + flat colour)
    void uploadUnlit(const glm::mat4& model,
                     const glm::mat4& view,
                     const glm::mat4& proj,
                     const glm::vec3& colour) const {
        setMat4("uModel",      model);
        setMat4("uView",       view);
        setMat4("uProjection", proj);
        setVec3("uUnlitColor", colour);
    }

private:
    GLuint id_ = 0;

    // Location cache — avoids repeated string-based lookups in hot path
    mutable std::unordered_map<std::string, GLint> locationCache_;

    GLint loc(const char* name) const {
        auto it = locationCache_.find(name);
        if (it != locationCache_.end()) return it->second;

        GLint l = glGetUniformLocation(id_, name);
        locationCache_.emplace(name, l);
        return l;
    }

    // Compile a single shader stage.  Throws on error.
    static GLuint compile(GLenum type, const char* src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            glDeleteShader(shader);
            const char* typeName = (type == GL_VERTEX_SHADER) ? "Vertex" : "Fragment";
            throw std::runtime_error(
                std::string(typeName) + " shader compile error:\n" + log);
        }
        return shader;
    }

    void destroy() {
        if (id_) {
            glDeleteProgram(id_);
            id_ = 0;
        }
        locationCache_.clear();
    }
};
