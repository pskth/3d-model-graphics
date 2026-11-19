cmake_minimum_required(VERSION 3.20)
project(lumina VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Build type default ────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ── Output directories ────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── Platform detection ────────────────────────────────────────────────────────
if(APPLE)
  # Homebrew prefix (Intel or Apple Silicon)
  if(EXISTS "/opt/homebrew")
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
  elseif(EXISTS "/usr/local")
    list(APPEND CMAKE_PREFIX_PATH "/usr/local")
  endif()
endif()

# ── Dependencies (find system packages) ──────────────────────────────────────

# OpenGL
find_package(OpenGL REQUIRED)

# GLFW 3
find_package(glfw3 3.3 REQUIRED)

# GLM (header-only)
find_package(glm REQUIRED)

# ── GLAD (vendored, single compilation unit) ─────────────────────────────────
# Place glad.c + glad/glad.h + KHR/khrplatform.h under third_party/glad/
add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# ── Dear ImGui (vendored) ─────────────────────────────────────────────────────
# Place the ImGui sources under third_party/imgui/
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

# ── tinyobjloader (single-header, vendored) ───────────────────────────────────
# Place tinyobjloader.h under third_party/tinyobjloader/
# The implementation is compiled inside geometry.h via TINYOBJLOADER_IMPLEMENTATION

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(lumina
  src/main.cpp
  lumina_logic.c          # reuse existing C state/math module
)

target_include_directories(lumina PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyobjloader
)

target_link_libraries(lumina PRIVATE
  OpenGL::GL
  glfw
  glm::glm
  glad
  imgui
)

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(MSVC)
  target_compile_options(lumina PRIVATE /W3)
else()
  target_compile_options(lumina PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# macOS: link against system frameworks required by GLFW
if(APPLE)
  target_link_libraries(lumina PRIVATE
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
  )
endif()

# ── Install (optional) ────────────────────────────────────────────────────────
install(TARGETS lumina RUNTIME DESTINATION bin)
