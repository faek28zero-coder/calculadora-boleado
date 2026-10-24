// ============================================================
//  BLACK VR — estado interno compartilhado entre os arquivos do módulo.
//  (Não é API para o port: para o port existe só vr.h.)
// ============================================================
#pragma once

#include <d3d11.h>
#include <atomic>
#include <cstdint>
#include <vector>

#include "openxr.h"
#include "openxr_platform.h"

namespace vr {
namespace internal {

constexpr int kEyes = 2;

struct EyeImage {
    XrSwapchain chain = XR_NULL_HANDLE;
    uint32_t width = 0, height = 0;
    uint32_t array_index = 0;
    std::vector<XrSwapchainImageD3D11KHR> images; // todas as imagens da swapchain
    ID3D11Texture2D *texture = nullptr;           // a adquirida neste frame
    bool acquired = false;
};

struct Actions {
    XrActionSet set = XR_NULL_HANDLE;
    XrAction trigger = XR_NULL_HANDLE;
    XrAction grip = XR_NULL_HANDLE;
    XrAction move = XR_NULL_HANDLE;
    XrAction turn = XR_NULL_HANDLE;
    XrAction a = XR_NULL_HANDLE;
    XrAction b = XR_NULL_HANDLE;
    XrAction menu = XR_NULL_HANDLE;
    XrAction haptic = XR_NULL_HANDLE;
    XrAction aim = XR_NULL_HANDLE;      // pose do controle direito
    XrSpace hand[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
    XrPath hand_path[2] = {XR_NULL_PATH, XR_NULL_PATH};
};

struct Runtime {
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace stage = XR_NULL_HANDLE;
    ID3D11Device *dev11 = nullptr;
    ID3D11DeviceContext *ctx11 = nullptr;
    XrView views[kEyes];
    XrViewState view_state;
    uint32_t view_count = 0;
    XrTime display_time = 0;
    XrDuration display_period = 0;
    bool should_render = false;
    EyeImage eye[kEyes];
    bool session_running = false;
    bool active = false;
    XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    int64_t format = 0;
    Actions actions;
};

extern Runtime g_rt;

/** O usuário pediu VR (vr::install()); pode ser verdade antes de o runtime existir. */
extern std::atomic<bool> g_requested;
extern std::atomic<bool> g_active;

/**
 * Configuração lida do black.ini pelo port (seção [vr]). Os padrões são os que funcionam
 * sem nenhuma entrada no .ini, então o black.ini pode ficar sem seção [vr] que nada quebra.
 */
struct Config {
    float vr_ipd = 63.0f;            // distância interpupilar em mm
    float vr_move_scale = 1.0f;      // quanto do deslocamento real da cabeça vira deslocamento do corpo
    float vr_snap_turn = 0.0f;       // graus por toque lateral no analógico direito (0 = desligado)
    float vr_game_fov = 0.0f;        // FOV horizontal do jogo em VR; 0 = usar o FOV do headset
    uint16_t vr_reload_button = 0x4000; // máscara Xbox do botão de recarregar (0x4000 = X)
    bool vr_input = true;            // traduzir os controles do headset para o pad
    bool vr_hud_in_headset = true;   // repetir o HUD nos dois olhos (desenho 2D)
    int fps = 60;                    // usado para converter o cooldown do snap turn em frames
};
extern Config g_cfg;

// FOV horizontal (graus) e tamanho do render target do jogo neste frame (escrito pelo port).
extern float g_game_fov_deg;
extern float g_game_aspect;
extern int g_render_w;
extern int g_render_h;

// matrizes 4x4 em convenção de linha (v' = v · M), como o D3D do Xbox
void mat_identity(float m[16]);
void mat_mul(const float a[16], const float b[16], float out[16]); // out = a · b
bool mat_invert(const float m[16], float out[16]);

/** Aplica a pose do headset na câmera do jogo (chamado pelo thunk de 0x1DFFEC). */
void apply_head_to_view(uint8_t *cam_base);

/** Upload de um bloco BGRA (pitch arbitrário, com ou sem escala) para a imagem adquirida. */
bool upload_eye(int eye, const uint8_t *bgra, uint32_t width, uint32_t height, uint32_t pitch);

/** Estado do input deste frame (chamado uma vez por frame por vr_input.cpp). */
void sync_actions();

/** Pose do centro dos olhos (OpenXR: +x direita, +y cima, -z frente), em metros. */
bool head_pose(float pos[3], float quat[4]);

// Estado das ações do frame (lido uma vez por pad_merge, já sincronizado).
float action_trigger();          // 0..1
float action_grip();             // 0..1
void action_move(float out[2]);  // analógico esquerdo (-1..1)
void action_turn(float out[2]);  // analógico direito (-1..1)
bool action_a();
bool action_b();
bool action_menu();
bool controllers_seen();

} // namespace internal
} // namespace vr
