#pragma once

// ============================================================
//  camera.h — Arcball / orbit camera with exponential damping
//
//  Mirrors the Three.js OrbitControls behaviour used in the
//  original web version:
//    - Left-drag  : orbit (azimuth + elevation)
//    - Scroll     : dolly zoom
//    - dampingFactor = 0.08  (matches orbitControls.dampingFactor)
//    - FOV 45 degrees, near 0.1, far 100
//    - Default position (0, 1.2, 4.0) looking at origin
//
//  Alt + left-drag is NOT handled here; that belongs to the
//  light system in main.cpp.  Camera input is only applied
//  when altHeld == false.
//
//  Call sequence each frame:
//    1. camera.processMouseButton(button, action, mods)
//    2. camera.processMouseMove(x, y)
//    3. camera.processScroll(yOffset)
//    4. camera.update(deltaTime)         <- applies damping
//    5. Use camera.viewMatrix() and camera.projMatrix(aspect)
// ============================================================

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

class Camera {
public:
    // ── Construction ─────────────────────────────────────────────────────────

    Camera() { reset(); }

    // ── Persistent state ─────────────────────────────────────────────────────

    // Spherical coordinates around the target
    float azimuth   = 0.6f;     // radians, horizontal angle
    float elevation = 0.28f;    // radians, vertical angle (matches y=1.2, z=4 default)
    float radius    = 4.12f;    // distance from target (||default pos||)

    glm::vec3 target = glm::vec3(0.0f);

    float fovDeg  = 45.0f;
    float nearZ   = 0.1f;
    float farZ    = 100.0f;

    // Damping factor — matches Three.js dampingFactor 0.08
    float dampingFactor = 0.08f;

    // ── Default snapshot for reset ────────────────────────────────────────────

    static constexpr float DEFAULT_AZIMUTH   = 0.6f;
    static constexpr float DEFAULT_ELEVATION = 0.28f;
    static constexpr float DEFAULT_RADIUS    = 4.12f;

    void reset() {
        azimuth   = DEFAULT_AZIMUTH;
        elevation = DEFAULT_ELEVATION;
        radius    = DEFAULT_RADIUS;
        target    = glm::vec3(0.0f);

        // Flush all velocities
        dAzimuth   = 0.0f;
        dElevation = 0.0f;
        dRadius    = 0.0f;
    }

    // ── Matrices ──────────────────────────────────────────────────────────────

    // Camera world position from spherical coordinates
    glm::vec3 position() const {
        return target + glm::vec3(
            radius * std::cos(elevation) * std::sin(azimuth),
            radius * std::sin(elevation),
            radius * std::cos(elevation) * std::cos(azimuth)
        );
    }

    glm::mat4 viewMatrix() const {
        return glm::lookAt(position(), target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 projMatrix(float aspectRatio) const {
        return glm::perspective(glm::radians(fovDeg), aspectRatio, nearZ, farZ);
    }

    // ── GLFW input callbacks ──────────────────────────────────────────────────

    // Call this from glfwSetMouseButtonCallback
    void processMouseButton(int button, int action, int mods, double x, double y) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                orbiting    = true;
                lastX       = x;
                lastY       = y;
                altHeld     = (mods & GLFW_MOD_ALT) != 0;
            } else {
                orbiting = false;
                altHeld  = false;
            }
        }
    }

    // Call this from glfwSetCursorPosCallback.
    // Returns true when the camera consumed the event (altHeld == false).
    // Returns false when Alt is held — main.cpp should route this to the light.
    bool processMouseMove(double x, double y) {
        double dx = x - lastX;
        double dy = y - lastY;
        lastX = x;
        lastY = y;

        if (!orbiting) return false;
        if (altHeld)   return false;   // let main.cpp handle light drag

        // Accumulate velocity — same sensitivity as the JS dx*0.5 / dy*0.3
        // but adapted to radians: divide by 300 for a natural arc-ball feel.
        dAzimuth   -= static_cast<float>(dx) * 0.005f;
        dElevation += static_cast<float>(dy) * 0.005f;

        return true;
    }

    // Returns true when Alt is held at the time of the press (for the caller
    // to start a light-drag instead).
    bool isAltDragging() const { return orbiting && altHeld; }

    // Raw delta since last call — used by main.cpp for light azimuth/elevation.
    // Consumes the deltas (resets to zero) so they aren't double-applied.
    glm::vec2 consumeAltDelta() {
        glm::vec2 d{lastX - altPrevX, lastY - altPrevY};
        altPrevX = lastX;
        altPrevY = lastY;
        return d;
    }

    void beginAltDrag(double x, double y) {
        altPrevX = x;
        altPrevY = y;
        altHeld  = true;
        orbiting = true;
        lastX    = x;
        lastY    = y;
    }

    void endAltDrag() {
        altHeld  = false;
        orbiting = false;
    }

    // Call this from glfwSetScrollCallback
    void processScroll(double yOffset) {
        // Dolly: accumulate radial velocity
        dRadius -= static_cast<float>(yOffset) * 0.3f;
    }

    // ── Per-frame update ──────────────────────────────────────────────────────
    // Apply velocities and exponential damping.  Call once per frame.
    void update(float /*deltaTime*/) {
        // Apply velocity
        azimuth   += dAzimuth;
        elevation += dElevation;
        radius    += dRadius;

        // Clamp elevation to avoid gimbal flip
        const float EL_MIN = -glm::radians(89.0f);
        const float EL_MAX =  glm::radians(89.0f);
        elevation = std::clamp(elevation, EL_MIN, EL_MAX);

        // Clamp radius
        radius = std::clamp(radius, 0.5f, 50.0f);

        // Exponential damping — keep the same feel as Three.js dampingFactor
        // Three.js applies: velocity *= (1 - dampingFactor) each frame.
        const float decay = 1.0f - dampingFactor;
        dAzimuth   *= decay;
        dElevation *= decay;
        dRadius    *= decay;

        // Kill micro-velocities to avoid perpetual drift
        if (std::abs(dAzimuth)   < 1e-5f) dAzimuth   = 0.0f;
        if (std::abs(dElevation) < 1e-5f) dElevation = 0.0f;
        if (std::abs(dRadius)    < 1e-5f) dRadius    = 0.0f;
    }

private:
    // Velocity components (radians/frame before damping)
    float dAzimuth   = 0.0f;
    float dElevation = 0.0f;
    float dRadius    = 0.0f;

    // Drag state
    bool   orbiting = false;
    bool   altHeld  = false;
    double lastX    = 0.0;
    double lastY    = 0.0;
    double altPrevX = 0.0;
    double altPrevY = 0.0;
};
