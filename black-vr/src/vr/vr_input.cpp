// ============================================================
//  BLACK VR — vr_input.cpp
//
//  Controles do headset -> pad Xbox que o jogo lê.
//
//  Por que assim: o port já traduz o pad do Xbox em XInputGetState
//  (src/input/xinput.cpp, HLE em 0x0027ddf8). Em vez de inventar um caminho novo de
//  entrada (e brigar com o mapeamento de teclas do jogo), este módulo injeta o estado
//  dos controles VR no mesmo XGamepad que o jogo consome. Resultado: o jogo responde
//  exatamente como responderia a um controle, com zero patch no código do jogo.
//
//  Mapeamento (ajustável em black.ini [vr]):
//      gatilho direito  -> RT (atirar)
//      squeeze direito  -> X (recarregar)      vr_reload_button=16384
//      analógico esq.   -> stick esquerdo (mover)
//      analógico dir.   -> stick direito (girar o corpo) + snap turn de conforto
//      A / B / menu     -> A / B / Start
// ============================================================
#include "vr/vr.h"

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "vr/vr_internal.h"

namespace vr {
namespace {

// botões Xbox (mesma máscara do pad da Microsoft)
constexpr uint16_t kDpadUp = 0x0001, kDpadDown = 0x0002, kDpadLeft = 0x0004, kDpadRight = 0x0008;
constexpr uint16_t kStart = 0x0010, kBack = 0x0020, kThumbL = 0x0040, kThumbR = 0x0080;
constexpr uint16_t kA = 0x1000, kB = 0x2000, kX = 0x4000, kY = 0x8000;

// última leitura, para o snap turn e para a histerese dos gatilhos
struct Input {
    float trigger;
    float grip;
    float move[2];
    float turn[2];
    bool a, b, menu;
    bool grip_was_down;
    bool menu_was_down;
    float snap_timer;
} g_in;

int16_t to_axis(float v)
{
    v = std::clamp(v, -1.0f, 1.0f);
    // zona morta radial pequena; o jogo já tem a dele
    if (std::fabs(v) < 0.08f) return 0;
    return (int16_t)std::lround(v * 32767.0f);
}

} // namespace

/**
 * Estado dos controles VR -> pad que o jogo lê.
 * Devolve o estado aplicado; `pad` já vem preenchido com o pad físico (se houver), então
 * somamos em vez de sobrescrever: quem estiver com o controle na mão continua funcionando.
 */
void pad_merge(Pad *pad)
{
    if (!pad || !internal::g_rt.session || !internal::g_rt.active || !internal::g_cfg.vr_input) return;

    g_in.trigger = internal::action_trigger();
    g_in.grip = internal::action_grip();
    internal::action_move(g_in.move);
    internal::action_turn(g_in.turn);
    g_in.a = internal::action_a();
    g_in.b = internal::action_b();
    g_in.menu = internal::action_menu();

    // gatilho (atirar) e squeeze (recarregar)
    const uint8_t rt = (uint8_t)std::lround(std::clamp(g_in.trigger, 0.0f, 1.0f) * 255.0f);
    if (rt > pad->rt) pad->rt = rt;
    const bool grip_down = g_in.grip > 0.6f;
    if (grip_down && !g_in.grip_was_down) pad->buttons |= (uint16_t)internal::g_cfg.vr_reload_button;
    g_in.grip_was_down = grip_down;

    // analógico esquerdo = mover
    if (std::fabs(g_in.move[0]) > 0.08f) pad->lx = to_axis(g_in.move[0]);
    if (std::fabs(g_in.move[1]) > 0.08f) pad->ly = to_axis(-g_in.move[1]); // OpenXR: +y para cima

    // analógico direito = girar o corpo (como no jogo) ...
    if (std::fabs(g_in.turn[0]) > 0.08f) pad->rx = to_axis(g_in.turn[0]);
    if (std::fabs(g_in.turn[1]) > 0.08f) pad->ry = to_axis(-g_in.turn[1]);

    // ... e snap turn de conforto: um toque lateral no analógico direito gira N graus de uma vez
    const float snap = internal::g_cfg.vr_snap_turn;
    if (snap > 0.0f) {
        g_in.snap_timer = std::max(0.0f, g_in.snap_timer - 1.0f / (float)std::max(30, internal::g_cfg.fps));
        if (std::fabs(g_in.turn[0]) > 0.75f && g_in.snap_timer <= 0.0f) {
            snap_turn(g_in.turn[0] > 0.0f ? -snap : snap);
            g_in.snap_timer = 0.35f;
            pad->rx = 0; // não somar o giro contínuo ao snap
        }
    }

    // botões
    if (g_in.a) pad->buttons |= kA;
    if (g_in.b) pad->buttons |= kB;
    if (g_in.menu && !g_in.menu_was_down) pad->buttons |= kStart;
    g_in.menu_was_down = g_in.menu;

    (void)kDpadUp;
    (void)kDpadDown;
    (void)kDpadLeft;
    (void)kDpadRight;
    (void)kBack;
    (void)kThumbL;
    (void)kThumbR;
    (void)kX;
    (void)kY;
}

} // namespace vr
