#pragma once

// ============================================================
//  lumina_logic.h — C interface for lumina_logic.c
//
//  This header is shared by the C++ OpenGL application and by
//  the original WebAssembly build.  The function bodies live in
//  lumina_logic.c; they are linked directly into the native
//  binary by CMakeLists.txt.
// ============================================================

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void  lumina_init(void);
void  lumina_update_light(void);

void  lumina_set_azimuth(float value);
void  lumina_set_elevation(float value);
void  lumina_set_distance(float value);
void  lumina_set_intensity(float value);
void  lumina_set_ambient(float value);
void  lumina_set_float(int id, float value);
void  lumina_set_hex_color(uint32_t rgb, int object);
void  lumina_set_material(float ka, float kd, float ks, float shininess,
                          float roughness, float toon_bands, float bump_strength);

float lumina_get_float(int id);
float lumina_get_light_x(void);
float lumina_get_light_y(void);
float lumina_get_light_z(void);
float lumina_get_light_red(void);
float lumina_get_light_green(void);
float lumina_get_light_blue(void);
float lumina_get_object_red(void);
float lumina_get_object_green(void);
float lumina_get_object_blue(void);

#ifdef __cplusplus
}
#endif
