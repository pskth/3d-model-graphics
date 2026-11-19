// ============================================================
//  main.cpp — Lumina 3D Shading Laboratory
//
//  C++17 / OpenGL 3.3 Core rewrite of the original Three.js app.
//
//  Window layout
//  ┌─────────────────────────────┬──────────────┐
//  │                             │  Control     │
//  │    OpenGL viewport          │  Panel       │
//  │    (scene rendered here)    │  (ImGui)     │
//  │                             │              │
//  └─────────────────────────────┴──────────────┘
//
//  Dependencies (vendored under third_party/):
//    GLAD, Dear ImGui, tinyobjloader
//  System packages:
//    GLFW3, GLM, OpenGL
// ============================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include "camera.h"
#include "geometry.h"
#include "shader_program.h"
#include "shaders.h"

// lumina_logic.c — reused C state module
extern "C" {
#include "../lumina_logic.h"
}

// ── Window constants ──────────────────────────────────────────────────────────

static constexpr int   WIN_W        = 1400;
static constexpr int   WIN_H        = 820;
static constexpr int   PANEL_W      = 340;   // Dear ImGui right-side panel width
static constexpr float BACKGROUND_R = 0.027f;
static constexpr float BACKGROUND_G = 0.035f;
static constexpr float BACKGROUND_B = 0.051f; // #070a0d

// ── Application state ─────────────────────────────────────────────────────────

struct AppState {
    // Window / rendering
    GLFWwindow* window   = nullptr;
    int         winW     = WIN_W;
    int         winH     = WIN_H;

    // Scene objects
    std::unique_ptr<Mesh> mesh;
    Mesh                  orbMesh;    // light-source sphere
    Mesh                  lineMesh;   // light guide ray (GL_LINES)
    GLuint                lineVAO = 0;
    GLuint                lineVBO = 0;

    // Shader programs — one per shading model + one unlit for the orb
    std::array<ShaderProgram, SHADER_COUNT> programs;
    ShaderProgram                           unlitProgram;
    int                                     activeShader = 2; // Phong default

    // Geometry preset
    GeomPreset activePreset = GeomPreset::Sphere;
    char       customPath[512] = {};
    std::string loadError;

    // Camera
    Camera camera;

    // Light (spherical coordinates, mirroring lumina_logic.c LuminaState)
    float lightAzimuth   = 45.0f;  // degrees
    float lightElevation = 45.0f;  // degrees
    float lightDistance  = 3.0f;
    float lightColor[3]  = {1.0f, 1.0f, 1.0f};
    float lightIntensity = 1.0f;
    float ambientIntensity = 0.3f;
    bool  autoOrbit      = false;
    bool  showOrb        = true;

    // Alt-drag light control
    bool   altDragging = false;
    double altDragLastX = 0.0;
    double altDragLastY = 0.0;

    // Material coefficients
    float objectColor[3] = {0.267f, 0.533f, 1.0f}; // #4488ff
    float Ka          = 1.0f;
    float Kd          = 0.8f;
    float Ks          = 0.5f;
    float shininess   = 32.0f;
    float roughness   = 0.5f;
    int   toonBands   = 4;
    float bumpStrength = 0.3f;

    // FPS display
    double fpsTimer   = 0.0;
    int    fpsCounter = 0;
    float  fps        = 0.0f;

    // Keyboard state
    bool keyHeld[GLFW_KEY_LAST + 1] = {};
};

static AppState g;

// ── Forward declarations ──────────────────────────────────────────────────────

static void initGL();
static void initImGui();
static void initShaders();
static void initGeometry(GeomPreset preset);
static void initLightLine();
static void updateLightLine();
static void rebuildShader(int index);
static void renderScene(float aspect);
static void renderImGui();
static void processKeyboard();

// GLFW callbacks
static void cbFramebufferSize(GLFWwindow*, int w, int h);
static void cbMouseButton(GLFWwindow*, int button, int action, int mods);
static void cbCursorPos(GLFWwindow*, double x, double y);
static void cbScroll(GLFWwindow*, double, double yOff);
static void cbKey(GLFWwindow*, int key, int scancode, int action, int mods);
static void cbDrop(GLFWwindow*, int count, const char** paths);

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    // ── GLFW init ─────────────────────────────────────────────────────────────
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialise GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4); // MSAA 4x

    g.window = glfwCreateWindow(WIN_W, WIN_H,
                                "Lumina — 3D Shading Laboratory", nullptr, nullptr);
    if (!g.window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(g.window);
    glfwSwapInterval(1); // vsync

    // ── GLAD ──────────────────────────────────────────────────────────────────
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        fprintf(stderr, "Failed to load OpenGL via GLAD\n");
        glfwTerminate();
        return 1;
    }

    printf("OpenGL %s  |  GLSL %s\n",
           glGetString(GL_VERSION),
           glGetString(GL_SHADING_LANGUAGE_VERSION));

    // ── Callbacks ─────────────────────────────────────────────────────────────
    glfwSetFramebufferSizeCallback(g.window, cbFramebufferSize);
    glfwSetMouseButtonCallback(g.window, cbMouseButton);
    glfwSetCursorPosCallback(g.window, cbCursorPos);
    glfwSetScrollCallback(g.window, cbScroll);
    glfwSetKeyCallback(g.window, cbKey);
    glfwSetDropCallback(g.window, cbDrop);

    // Store initial framebuffer size (may differ from window size on HiDPI)
    glfwGetFramebufferSize(g.window, &g.winW, &g.winH);

    // ── Init subsystems ───────────────────────────────────────────────────────
    initGL();
    initImGui();
    initShaders();
    initGeometry(GeomPreset::Sphere);
    initLightLine();

    // lumina_logic.c state machine
    lumina_init();

    // ── Render loop ───────────────────────────────────────────────────────────
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(g.window)) {
        glfwPollEvents();

        double now   = glfwGetTime();
        float  delta = static_cast<float>(now - lastTime);
        lastTime     = now;

        // FPS counter
        g.fpsTimer   += delta;
        g.fpsCounter += 1;
        if (g.fpsTimer >= 0.5) {
            g.fps        = static_cast<float>(g.fpsCounter) / static_cast<float>(g.fpsTimer);
            g.fpsTimer   = 0.0;
            g.fpsCounter = 0;
        }

        // Auto-orbit light
        if (g.autoOrbit) {
            g.lightAzimuth = std::fmod(g.lightAzimuth + delta * 0.5f * 60.0f, 360.0f);
            lumina_set_azimuth(g.lightAzimuth);
        }

        // Camera damping
        g.camera.update(delta);

        // Keyboard shortcuts (non-toggle, held-key logic goes here if needed)
        processKeyboard();

        // ── Render scene into the left portion of the framebuffer ─────────────
        // ImGui panel will cover the right PANEL_W pixels.
        // We set the OpenGL viewport to the full window; the panel overlays it.
        glViewport(0, 0, g.winW, g.winH);
        glClearColor(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = static_cast<float>(g.winW) / static_cast<float>(g.winH);
        renderScene(aspect);

        // ── Dear ImGui frame ──────────────────────────────────────────────────
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        renderImGui();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(g.window);
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (g.mesh)      g.mesh->destroy();
    g.orbMesh.destroy();
    if (g.lineVAO)   glDeleteVertexArrays(1, &g.lineVAO);
    if (g.lineVBO)   glDeleteBuffers(1, &g.lineVBO);

    glfwDestroyWindow(g.window);
    glfwTerminate();
    return 0;
}

// ── Initialisation ────────────────────────────────────────────────────────────

static void initGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    // No face culling — lets shaders show back faces on thin geometry
}

static void initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // don't write imgui.ini

    // Neubrutalism-inspired black-and-white style
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 4.0f;
    s.FrameRounding     = 4.0f;
    s.GrabRounding      = 3.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 1.0f;
    s.ItemSpacing       = ImVec2(8, 6);
    s.FramePadding      = ImVec2(8, 4);

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = ImVec4(0.09f, 0.09f, 0.09f, 1.0f);
    c[ImGuiCol_ChildBg]          = ImVec4(0.11f, 0.11f, 0.11f, 1.0f);
    c[ImGuiCol_FrameBg]          = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
    c[ImGuiCol_FrameBgHovered]   = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    c[ImGuiCol_FrameBgActive]    = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
    c[ImGuiCol_TitleBg]          = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    c[ImGuiCol_Button]           = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    c[ImGuiCol_ButtonHovered]    = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    c[ImGuiCol_ButtonActive]     = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    c[ImGuiCol_Header]           = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
    c[ImGuiCol_HeaderActive]     = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    c[ImGuiCol_SliderGrab]       = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    c[ImGuiCol_CheckMark]        = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    c[ImGuiCol_Text]             = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
    c[ImGuiCol_TextDisabled]     = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
    c[ImGuiCol_Border]           = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    c[ImGuiCol_Separator]        = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
    c[ImGuiCol_SeparatorActive]  = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);

    ImGui_ImplGlfw_InitForOpenGL(g.window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

static void initShaders() {
    for (int i = 0; i < SHADER_COUNT; ++i)
        rebuildShader(i);

    g.unlitProgram.build(VERT_UNLIT, FRAG_UNLIT);
}

static void rebuildShader(int index) {
    try {
        g.programs[index].build(SHADER_TABLE[index].vertSrc,
                                SHADER_TABLE[index].fragSrc);
    } catch (const std::runtime_error& e) {
        fprintf(stderr, "Shader build error [%s]: %s\n",
                SHADER_TABLE[index].name, e.what());
    }
}

static void initGeometry(GeomPreset preset) {
    g.activePreset = preset;
    g.mesh = std::make_unique<Mesh>(buildPreset(preset));
    g.mesh->upload();

    // Light orb — small sphere, uploaded once
    g.orbMesh = makeSphere(16, 16);
    g.orbMesh.upload();
}

static void initLightLine() {
    // Two-point line from light position to origin.
    // Positions updated each frame via updateLightLine().
    glGenVertexArrays(1, &g.lineVAO);
    glGenBuffers(1, &g.lineVBO);

    glBindVertexArray(g.lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g.lineVBO);

    float pts[6] = {0,0,0, 0,0,0};
    glBufferData(GL_ARRAY_BUFFER, sizeof(pts), pts, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    // location 1 (normal) not used by VERT_UNLIT but the layout still needs it
    // bound to avoid validation warnings on some drivers — we bind a dummy vec3.
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    glBindVertexArray(0);
}

static void updateLightLine() {
    float lx = lumina_get_light_x();
    float ly = lumina_get_light_y();
    float lz = lumina_get_light_z();

    float pts[6] = {lx, ly, lz, 0.0f, 0.0f, 0.0f};
    glBindBuffer(GL_ARRAY_BUFFER, g.lineVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(pts), pts);
}

// ── Render ────────────────────────────────────────────────────────────────────

static void renderScene(float aspect) {
    if (!g.mesh) return;

    glm::mat4 model      = glm::mat4(1.0f);
    glm::mat4 view       = g.camera.viewMatrix();
    glm::mat4 projection = g.camera.projMatrix(aspect);
    glm::vec3 camPos     = g.camera.position();

    // Sync lumina_logic light position from current spherical params
    lumina_set_azimuth(g.lightAzimuth);
    lumina_set_elevation(g.lightElevation);
    lumina_set_distance(g.lightDistance);
    lumina_set_intensity(g.lightIntensity);
    lumina_set_ambient(g.ambientIntensity);

    glm::vec3 lightPos = {
        lumina_get_light_x(),
        lumina_get_light_y(),
        lumina_get_light_z()
    };
    glm::vec3 lightCol = {
        g.lightColor[0], g.lightColor[1], g.lightColor[2]
    };
    glm::vec3 objCol = {
        g.objectColor[0], g.objectColor[1], g.objectColor[2]
    };

    // ── Draw main mesh ────────────────────────────────────────────────────────
    ShaderProgram& prog = g.programs[g.activeShader];
    prog.use();

    ShaderProgram::Uniforms u{};
    u.model            = model;
    u.view             = view;
    u.projection       = projection;
    u.cameraPos        = camPos;
    u.lightPosition    = lightPos;
    u.lightColor       = lightCol;
    u.lightIntensity   = g.lightIntensity;
    u.ambientIntensity = g.ambientIntensity;
    u.objectColor      = objCol;
    u.Ka               = g.Ka;
    u.Kd               = g.Kd;
    u.Ks               = g.Ks;
    u.shininess        = g.shininess;
    u.roughness        = g.roughness;
    u.toonBands        = g.toonBands;
    u.bumpStrength     = g.bumpStrength;
    prog.uploadUniforms(u);

    g.mesh->draw();

    // ── Draw light orb and guide line ─────────────────────────────────────────
    if (g.showOrb) {
        updateLightLine();

        // Scale orb to radius 0.12
        glm::mat4 orbModel = glm::translate(glm::mat4(1.0f), lightPos);
        orbModel = glm::scale(orbModel, glm::vec3(0.12f));

        g.unlitProgram.use();
        g.unlitProgram.uploadUnlit(orbModel, view, projection,
                                   glm::vec3(1.0f, 0.95f, 0.0f)); // warm yellow
        g.orbMesh.draw();

        // Guide line
        glm::mat4 ident = glm::mat4(1.0f);
        g.unlitProgram.uploadUnlit(ident, view, projection,
                                   glm::vec3(1.0f, 0.85f, 0.3f));
        glBindVertexArray(g.lineVAO);
        glDrawArrays(GL_LINES, 0, 2);
        glBindVertexArray(0);
    }
}

// Keep g.ambientIntensity in sync when renderScene calls lumina_set_ambient.
// We use g.ambientIntensity directly (set by the ImGui slider).

// ── Dear ImGui panel ─────────────────────────────────────────────────────────

static void renderImGui() {
    ImGuiIO& io = ImGui::GetIO();

    // Full-height panel pinned to the right edge
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - PANEL_W, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(PANEL_W, io.DisplaySize.y), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove           |
        ImGuiWindowFlags_NoResize         |
        ImGuiWindowFlags_NoCollapse       |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("Studio Parameters", nullptr, flags);

    // ── FPS ──────────────────────────────────────────────────────────────────
    ImGui::TextDisabled("%.1f fps", g.fps);
    ImGui::Separator();

    // ── Shading Model ─────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Shading Model", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < SHADER_COUNT; ++i) {
            bool active = (i == g.activeShader);

            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.90f,0.90f,0.90f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f,1.00f,1.00f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.00f,1.00f,1.00f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.05f,0.05f,0.05f,1.0f));
            }

            // Two buttons per row
            if (i % 2 != 0) ImGui::SameLine();
            char label[64];
            std::snprintf(label, sizeof(label), "%s [%d]", SHADER_TABLE[i].name, i + 1);
            if (ImGui::Button(label, ImVec2((PANEL_W - 24) * 0.5f, 0)))
                g.activeShader = i;

            if (active) ImGui::PopStyleColor(4);
        }

        // Stage + formula block for active shader
        ImGui::Spacing();
        ImGui::TextDisabled("Stage: %s", SHADER_TABLE[g.activeShader].stage);
        ImGui::Spacing();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + PANEL_W - 24);
        ImGui::TextDisabled("%s", SHADER_TABLE[g.activeShader].description);
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f,0.06f,0.06f,1.0f));
        ImGui::BeginChild("formula_block", ImVec2(0, 70), true);
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1
                        ? ImGui::GetIO().Fonts->Fonts[1] : nullptr);
        ImGui::TextUnformatted(SHADER_TABLE[g.activeShader].formula);
        if (ImGui::GetIO().Fonts->Fonts.Size > 1) ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    // ── Geometry ──────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen)) {
        const int presetCount = static_cast<int>(GeomPreset::COUNT) - 1; // exclude Custom
        for (int i = 0; i < presetCount; ++i) {
            GeomPreset p = static_cast<GeomPreset>(i);
            bool active  = (g.activePreset == p && g.loadError.empty());

            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.90f,0.90f,0.90f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f,1.00f,1.00f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.00f,1.00f,1.00f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.05f,0.05f,0.05f,1.0f));
            }

            if (i % 2 != 0) ImGui::SameLine();
            if (ImGui::Button(PRESET_NAMES[i], ImVec2((PANEL_W - 24) * 0.5f, 0))) {
                g.loadError.clear();
                g.mesh = std::make_unique<Mesh>(buildPreset(p));
                g.mesh->upload();
                g.activePreset = p;
            }

            if (active) ImGui::PopStyleColor(4);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // File load
        ImGui::TextDisabled("Load .obj or .stl file");
        ImGui::InputText("##path", g.customPath, sizeof(g.customPath));
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            try {
                g.loadError.clear();
                g.mesh = std::make_unique<Mesh>(loadMesh(g.customPath));
                g.mesh->upload();
                g.activePreset = GeomPreset::Custom;
            } catch (const std::exception& e) {
                g.loadError = e.what();
            }
        }
        // Drag-and-drop hint
        ImGui::TextDisabled("(or drag a file onto the window)");

        if (!g.loadError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + PANEL_W - 24);
            ImGui::TextUnformatted(g.loadError.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }
    }

    ImGui::Spacing();

    // ── Light Source ──────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Light Source (360)", ImGuiTreeNodeFlags_DefaultOpen)) {

        // Auto-orbit toggle
        if (g.autoOrbit)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f,0.85f,0.85f,1.0f));
        if (ImGui::Button(g.autoOrbit ? "Auto-Orbit  ON" : "Auto-Orbit  OFF"))
            g.autoOrbit = !g.autoOrbit;
        if (g.autoOrbit) ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button(g.showOrb ? "Orb  ON" : "Orb  OFF"))
            g.showOrb = !g.showOrb;

        ImGui::Spacing();

        ImGui::SliderFloat("Azimuth",   &g.lightAzimuth,   0.0f, 360.0f, "%.0f deg");
        ImGui::SliderFloat("Elevation", &g.lightElevation, -89.0f, 89.0f, "%.0f deg");
        ImGui::SliderFloat("Distance",  &g.lightDistance,   1.0f, 5.0f,  "%.1f");
        ImGui::SliderFloat("Intensity", &g.lightIntensity,  0.0f, 3.0f,  "%.2f");
        ImGui::SliderFloat("Ambient",   &g.ambientIntensity,0.0f, 1.0f,  "%.2f");

        ImGui::Spacing();
        ImGui::Text("Light Color");
        ImGui::ColorEdit3("##lc", g.lightColor,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

        ImGui::Spacing();
        ImGui::TextDisabled("Quick presets");
        struct Preset { const char* label; float az; float el; };
        static const Preset qp[] = {
            {"Key 45",   45.0f,  45.0f},
            {"Fill 225", 225.0f, 25.0f},
            {"Rim 180",  180.0f, 10.0f},
            {"Top Down", 0.0f,   85.0f},
        };
        for (int i = 0; i < 4; ++i) {
            if (i % 2 != 0) ImGui::SameLine();
            if (ImGui::Button(qp[i].label, ImVec2((PANEL_W - 24) * 0.5f, 0))) {
                g.lightAzimuth   = qp[i].az;
                g.lightElevation = qp[i].el;
            }
        }
    }

    ImGui::Spacing();

    // ── Surface Material ──────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Surface Material", ImGuiTreeNodeFlags_DefaultOpen)) {

        ImGui::Text("Base Tint");
        ImGui::ColorEdit3("##oc", g.objectColor,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

        ImGui::Spacing();

        // 2-column slider grid
        ImGui::Columns(2, "matcols", false);

        ImGui::Text("Ka (Ambient)");
        ImGui::SliderFloat("##Ka", &g.Ka, 0.0f, 1.0f, "%.2f");
        ImGui::NextColumn();

        ImGui::Text("Kd (Diffuse)");
        ImGui::SliderFloat("##Kd", &g.Kd, 0.0f, 1.0f, "%.2f");
        ImGui::NextColumn();

        ImGui::Text("Ks (Specular)");
        ImGui::SliderFloat("##Ks", &g.Ks, 0.0f, 1.0f, "%.2f");
        ImGui::NextColumn();

        ImGui::Text("Shininess");
        ImGui::SliderFloat("##Sh", &g.shininess, 1.0f, 256.0f, "%.0f");

        ImGui::Columns(1);
        ImGui::Spacing();

        // Conditional sliders — shown only for the relevant shader
        const char* key = SHADER_TABLE[g.activeShader].key;
        bool isOrenNayar = (std::strcmp(key, "orennayar") == 0);
        bool isToon      = (std::strcmp(key, "toon")      == 0);
        bool isBump      = (std::strcmp(key, "normalmap") == 0);

        if (isOrenNayar) {
            ImGui::Text("Roughness s (Oren-Nayar)");
            ImGui::SliderFloat("##rough", &g.roughness, 0.0f, 1.0f, "%.2f");
        }
        if (isToon) {
            int tb = g.toonBands;
            ImGui::Text("Toon Bands");
            if (ImGui::SliderInt("##toonbands", &tb, 2, 8))
                g.toonBands = tb;
        }
        if (isBump) {
            ImGui::Text("Bump Depth");
            ImGui::SliderFloat("##bump", &g.bumpStrength, 0.0f, 2.0f, "%.2f");
        }
    }

    ImGui::Spacing();

    // ── Camera / Controls ─────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Reset Camera [R]", ImVec2(-1, 0)))
            g.camera.reset();

        ImGui::Spacing();
        ImGui::TextDisabled("Left drag      Orbit camera");
        ImGui::TextDisabled("Scroll         Zoom");
        ImGui::TextDisabled("Alt + drag     Move light");
        ImGui::TextDisabled("1 - 7          Switch shader");
        ImGui::TextDisabled("R              Reset camera");
        ImGui::TextDisabled("L              Toggle orb");
        ImGui::TextDisabled("A              Toggle auto-orbit");
    }

    ImGui::End();
}

// ── Keyboard handling ─────────────────────────────────────────────────────────

static void processKeyboard() {
    // All key logic is event-driven in cbKey; nothing held-key here for now.
    (void)g.keyHeld;
}

// ── GLFW Callbacks ────────────────────────────────────────────────────────────

static void cbFramebufferSize(GLFWwindow*, int w, int h) {
    g.winW = w;
    g.winH = h;
    glViewport(0, 0, w, h);
}

static void cbMouseButton(GLFWwindow* win, int button, int action, int mods) {
    // Let ImGui consume UI clicks first
    if (ImGui::GetIO().WantCaptureMouse) return;

    double x, y;
    glfwGetCursorPos(win, &x, &y);

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            bool alt = (mods & GLFW_MOD_ALT) != 0;
            if (alt) {
                g.altDragging   = true;
                g.altDragLastX  = x;
                g.altDragLastY  = y;
                g.camera.endAltDrag(); // keep camera still
            } else {
                g.altDragging = false;
                g.camera.processMouseButton(button, action, mods, x, y);
            }
        } else {
            g.altDragging = false;
            g.camera.processMouseButton(button, action, 0, x, y);
        }
    }
}

static void cbCursorPos(GLFWwindow*, double x, double y) {
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (g.altDragging) {
        double dx = x - g.altDragLastX;
        double dy = y - g.altDragLastY;
        g.altDragLastX = x;
        g.altDragLastY = y;

        // Same sensitivity as the JS version: dx*0.5 deg, dy*0.3 deg
        g.lightAzimuth   = std::fmod(g.lightAzimuth + static_cast<float>(dx) * 0.5f + 360.0f, 360.0f);
        g.lightElevation = std::clamp(g.lightElevation - static_cast<float>(dy) * 0.3f, -89.0f, 89.0f);
    } else {
        g.camera.processMouseMove(x, y);
    }
}

static void cbScroll(GLFWwindow*, double, double yOff) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    g.camera.processScroll(yOff);
}

static void cbKey(GLFWwindow* win, int key, int /*scancode*/, int action, int /*mods*/) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (action != GLFW_PRESS) return;

    // 1–7: shader selection
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_7) {
        int idx = key - GLFW_KEY_1;
        if (idx < SHADER_COUNT) g.activeShader = idx;
        return;
    }

    switch (key) {
        case GLFW_KEY_R:
            g.camera.reset();
            break;
        case GLFW_KEY_L:
            g.showOrb = !g.showOrb;
            break;
        case GLFW_KEY_A:
            g.autoOrbit = !g.autoOrbit;
            break;
        case GLFW_KEY_ESCAPE:
        case GLFW_KEY_Q:
            glfwSetWindowShouldClose(win, GLFW_TRUE);
            break;
        default:
            break;
    }
}

static void cbDrop(GLFWwindow*, int count, const char** paths) {
    if (count < 1) return;
    std::string path(paths[0]);
    std::snprintf(g.customPath, sizeof(g.customPath), "%s", path.c_str());
    try {
        g.loadError.clear();
        g.mesh = std::make_unique<Mesh>(loadMesh(path));
        g.mesh->upload();
        g.activePreset = GeomPreset::Custom;
    } catch (const std::exception& e) {
        g.loadError = e.what();
    }
}
