// ============================================================
//  BLACK VR — vr.cpp
//
//  A cola do módulo: configuração, estado do frame e o caminho do olho direito.
//
//  Por que o olho direito é desenhado de novo em vez de se usar uma segunda câmera:
//  o BLACK monta a câmera e a mira uma vez por frame, no código do jogo (ver vr_pose.cpp).
//  Chamar isso duas vezes estragaria a lógica (animação, retículo, IA do frame). O caminho
//  que não toca no jogo é: desenhar o frame normalmente (olho esquerdo), gravar os draws e,
//  no present, reemitir os mesmos draws com a matriz de clip do olho direito. Custa um draw
//  pass extra por frame — o que um jogo de 2006 em D3D9 aguenta com folga.
// ============================================================
#include "vr/vr.h"

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "vr/vr_frame.h"
#include "vr/vr_internal.h"

namespace vr {

namespace internal {

Config g_cfg;
float g_game_fov_deg = 70.0f;
float g_game_aspect = 4.0f / 3.0f;
int g_render_w = 640;
int g_render_h = 480;

} // namespace internal

namespace {

using internal::g_rt;

bool g_second_eye_failed; // se o port não tiver o gravador, avisa uma vez e não insiste
int g_frames;

} // namespace

void configure(const Settings &s)
{
    internal::Config &c = internal::g_cfg;
    c.vr_ipd = s.ipd_mm > 20.0f && s.ipd_mm < 90.0f ? s.ipd_mm : 63.0f;
    c.vr_move_scale = std::clamp(s.move_scale, 0.0f, 4.0f);
    c.vr_snap_turn = std::clamp(s.snap_turn_deg, 0.0f, 180.0f);
    c.vr_game_fov = std::clamp(s.game_fov, 0.0f, 170.0f);
    c.vr_reload_button = s.reload_button ? s.reload_button : (uint16_t)0x4000;
    c.vr_input = s.input;
    c.vr_hud_in_headset = s.hud_in_headset;
    c.fps = s.fps > 0 ? s.fps : 60;
}

/**
 * Fecha a gravação do frame que acabou de ir para o olho esquerdo, redesenha com a matriz do
 * olho direito e envia ao runtime. Chamado pelo `D3DDevice_Swap` do port no lugar do present
 * quando o VR está ativo, depois de `submit_eye(dev, 0)`.
 */
void render_second_eye(IDirect3DDevice9 *dev, IDirect3DTexture9 *frame)
{
    if (!active()) return;

    vr_draw::capture_end();
    const vr_draw::Stats before = vr_draw::stats();
    if (before.draws == 0 && ++g_frames > 240) {
        if (!g_second_eye_failed) {
            g_second_eye_failed = true;
            logf("[vr] nenhum draw foi gravado: o olho direito vai repetir o esquerdo. "
                 "Confira se draw.cpp do port está com o patch 0005 (vr_draw) aplicado.");
        }
    }

    set_eye(1);
    vr_draw::replay();
    const bool ok = frame ? submit_frame(frame, 1) : submit_eye(dev, 1);
    set_eye(0);
    vr_draw::capture_begin();

    if (!ok) {
        static int warned;
        if (warned++ == 0) logf("[vr] falha ao enviar o olho direito (textura da swapchain indisponível)");
    }

    if ((g_frames++ % 600) == 0) {
        const vr_draw::Stats s = vr_draw::stats();
        logf("[vr] frame %d: %u draws, %.1f KB gravados, replay %.2f ms", g_frames, s.draws, s.bytes / 1024.0f,
             s.last_replay_ms);
    }
}

} // namespace vr
