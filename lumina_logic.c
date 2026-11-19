#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float azimuth;
    float elevation;
    float distance;
    float intensity;
    float ambient;
    float ka;
    float kd;
    float ks;
    float shininess;
    float roughness;
    float toon_bands;
    float bump_strength;
    float light_position[3];
    float light_color[3];
    float object_color[3];
} LuminaState;

static LuminaState state;

void lumina_update_light(void);
void lumina_set_intensity(float value);
void lumina_set_ambient(float value);

static float lumina_clamp(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static float lumina_degrees_to_radians(float degrees) {
    return degrees * (float)(M_PI / 180.0);
}

static float lumina_mod(float value, float divisor) {
    if (divisor == 0.0f) return 0.0f;
    return value - ((int)(value / divisor) * divisor);
}

static float lumina_sin(float x) {
    x = lumina_mod(x + (float)M_PI, (float)(2.0 * M_PI)) - (float)M_PI;
    float x2 = x * x;
    return x * (1.0f - x2 / 6.0f + x2 * x2 / 120.0f - x2 * x2 * x2 / 5040.0f);
}

static float lumina_cos(float x) {
    x = lumina_mod(x + (float)M_PI, (float)(2.0 * M_PI)) - (float)M_PI;
    float x2 = x * x;
    return 1.0f - x2 / 2.0f + x2 * x2 / 24.0f - x2 * x2 * x2 / 720.0f;
}

__attribute__((export_name("lumina_init")))
void lumina_init(void) {
    state.azimuth = 45.0f;
    state.elevation = 45.0f;
    state.distance = 3.0f;
    state.intensity = 1.0f;
    state.ambient = 0.3f;
    state.ka = 1.0f;
    state.kd = 0.8f;
    state.ks = 0.5f;
    state.shininess = 32.0f;
    state.roughness = 0.5f;
    state.toon_bands = 4.0f;
    state.bump_strength = 0.3f;
    state.light_color[0] = 1.0f;
    state.light_color[1] = 1.0f;
    state.light_color[2] = 1.0f;
    state.object_color[0] = 0.2666667f;
    state.object_color[1] = 0.5333333f;
    state.object_color[2] = 1.0f;
    lumina_update_light();
}

__attribute__((export_name("lumina_update_light")))
void lumina_update_light(void) {
    float az = lumina_degrees_to_radians(state.azimuth);
    float el = lumina_degrees_to_radians(state.elevation);
    float horizontal = lumina_cos(el) * state.distance;
    state.light_position[0] = horizontal * lumina_sin(az);
    state.light_position[1] = state.distance * lumina_sin(el);
    state.light_position[2] = horizontal * lumina_cos(az);
}

__attribute__((export_name("lumina_set_azimuth")))
void lumina_set_azimuth(float value) {
    float normalized = lumina_mod(value, 360.0f);
    if (normalized < 0.0f) normalized += 360.0f;
    state.azimuth = normalized;
    lumina_update_light();
}

__attribute__((export_name("lumina_set_elevation")))
void lumina_set_elevation(float value) {
    state.elevation = lumina_clamp(value, -89.0f, 89.0f);
    lumina_update_light();
}

__attribute__((export_name("lumina_set_distance")))
void lumina_set_distance(float value) {
    state.distance = lumina_clamp(value, 1.0f, 5.0f);
    lumina_update_light();
}

__attribute__((export_name("lumina_set_float")))
void lumina_set_float(int id, float value) {
    switch (id) {
        case 3: lumina_set_intensity(value); break;
        case 4: lumina_set_ambient(value); break;
        case 5: state.ka = value; break;
        case 6: state.kd = value; break;
        case 7: state.ks = value; break;
        case 8: state.shininess = value; break;
        case 9: state.roughness = value; break;
        case 10: state.toon_bands = value; break;
        case 11: state.bump_strength = value; break;
    }
}

__attribute__((export_name("lumina_set_intensity")))
void lumina_set_intensity(float value) { state.intensity = lumina_clamp(value, 0.0f, 3.0f); }

__attribute__((export_name("lumina_set_ambient")))
void lumina_set_ambient(float value) { state.ambient = lumina_clamp(value, 0.0f, 1.0f); }

__attribute__((export_name("lumina_set_hex_color")))
void lumina_set_hex_color(uint32_t rgb, int object) {
    float red = (float)((rgb >> 16) & 0xff) / 255.0f;
    float green = (float)((rgb >> 8) & 0xff) / 255.0f;
    float blue = (float)(rgb & 0xff) / 255.0f;
    float *target = object ? state.object_color : state.light_color;
    target[0] = red;
    target[1] = green;
    target[2] = blue;
}

__attribute__((export_name("lumina_set_material")))
void lumina_set_material(float ka, float kd, float ks, float shininess,
                         float roughness, float toon_bands, float bump_strength) {
    state.ka = ka;
    state.kd = kd;
    state.ks = ks;
    state.shininess = shininess;
    state.roughness = roughness;
    state.toon_bands = toon_bands;
    state.bump_strength = bump_strength;
}

__attribute__((export_name("lumina_get_float")))
float lumina_get_float(int id) {
    switch (id) {
        case 0: return state.azimuth;
        case 1: return state.elevation;
        case 2: return state.distance;
        case 3: return state.intensity;
        case 4: return state.ambient;
        case 5: return state.ka;
        case 6: return state.kd;
        case 7: return state.ks;
        case 8: return state.shininess;
        case 9: return state.roughness;
        case 10: return state.toon_bands;
        case 11: return state.bump_strength;
        default: return 0.0f;
    }
}

__attribute__((export_name("lumina_get_light_x"))) float lumina_get_light_x(void) { return state.light_position[0]; }
__attribute__((export_name("lumina_get_light_y"))) float lumina_get_light_y(void) { return state.light_position[1]; }
__attribute__((export_name("lumina_get_light_z"))) float lumina_get_light_z(void) { return state.light_position[2]; }
__attribute__((export_name("lumina_get_light_red"))) float lumina_get_light_red(void) { return state.light_color[0]; }
__attribute__((export_name("lumina_get_light_green"))) float lumina_get_light_green(void) { return state.light_color[1]; }
__attribute__((export_name("lumina_get_light_blue"))) float lumina_get_light_blue(void) { return state.light_color[2]; }
__attribute__((export_name("lumina_get_object_red"))) float lumina_get_object_red(void) { return state.object_color[0]; }
__attribute__((export_name("lumina_get_object_green"))) float lumina_get_object_green(void) { return state.object_color[1]; }
__attribute__((export_name("lumina_get_object_blue"))) float lumina_get_object_blue(void) { return state.object_color[2]; }
