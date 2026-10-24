// ============================================================
//  BLACK VR — vr_pose.cpp
//
//  Onde a cabeça do jogador entra na câmera do BLACK.
//
//  O port já documenta (NOTES.md "Tactical lean", src/input/lean.cpp) que a matriz de
//  visão do jogador é montada uma vez por frame em FUN_001dfd90 (chamada só do update em
//  primeira pessoa, FUN_001e5fa0 em 0x1E5FE1) e que a altura do olho é gravada pela
//  instrução `movss [esi+0x104], xmm0` em 0x1DFFEC. Rows da matriz (vetores-linha):
//
//      +0xD0 = esquerda    +0xE0 = cima    +0xF0 = frente    +0x100 = olho
//
//  O lean substitui essa mesma instrução por um thunk. Aqui fazemos igual: gravamos a
//  altura do olho (o que a instrução faria) e aplicamos por cima a pose do headset.
//  Como essa matriz é também a fonte da mira e da posição da arma, tudo acompanha; o
//  corpo continua com o yaw do jogo (analógico direito), o que mantém IA e movimento
//  coerentes — o modelo híbrido usado por mods sérios de FPS em VR.
//
//  Estéreo sem tocar no jogo: a imagem que o jogo desenha é "mono" (IPD zero). Para cada
//  olho calculamos
//        C = (V_mono · P_mono)⁻¹ · (V_olho · P_olho)
//  e multiplicamos a posição final do vertex shader traduzido por C (constante c201, ver
//  src/d3d/nv2a_vsh.cpp). O olho esquerdo usa C desde o começo do frame; o direito é
//  desenhado de novo no Swap, reexecutando o frame com o C do olho direito.
//
//  Endereços = build NTSC-U, mapeados por xbe_va (o PAL tem o mesmo código deslocado).
// ============================================================
#include "vr/vr.h"

#include "common.h"
#include "xbe.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "vr/vr_internal.h"
#include "vr/vr_math.h"

extern "C" void __cdecl vr_eye_height_thunk();

namespace vr {
namespace internal {
namespace {

// 0x1DFFEC: `movss [esi+0x104], xmm0` — o mesmo site que o lean usa.
constexpr uint32_t kEyeHeightStore = 0x001DFFEC;
const uint8_t kExpect[8] = {0xF3, 0x0F, 0x11, 0x86, 0x04, 0x01, 0x00, 0x00};

bool g_installed;
bool g_pose_ok;
float g_turn;  // snap turn acumulado (radianos em torno de +Y)
int g_eye;     // olho que os próximos draws representam
float g_clip[kEyes][16];
float g_identity[16];

struct Basis {
    float forward[3], left[3], up[3];
};
Basis g_basis;

void normalize3(float v[3])
{
    const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l < 1e-6f) {
        v[0] = 0;
        v[1] = 0;
        v[2] = 1;
        return;
    }
    for (int i = 0; i < 3; i++) v[i] /= l;
}

void cross3(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void rotate_y(float v[3], float radians)
{
    const float c = std::cos(radians), s = std::sin(radians);
    const float x = v[0] * c + v[2] * s;
    const float z = -v[0] * s + v[2] * c;
    v[0] = x;
    v[2] = z;
}

void quat_rotate(const float q[4], const float v[3], float out[3])
{
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const float cx = y * v[2] - z * v[1];
    const float cy = z * v[0] - x * v[2];
    const float cz = x * v[1] - y * v[0];
    const float tx = cx + w * v[0];
    const float ty = cy + w * v[1];
    const float tz = cz + w * v[2];
    out[0] = v[0] + 2.0f * (y * tz - z * ty);
    out[1] = v[1] + 2.0f * (z * tx - x * tz);
    out[2] = v[2] + 2.0f * (x * ty - y * tx);
}

/**
 * V_olho · P_olho para um olho (a orientação já está na view do jogo; aqui só entra a separação
 * lateral dos olhos, ±ipd/2). A matemática mora em vr_math.h, que é testada fora do Windows
 * (tests/vr_math_test.cpp) — inclusive a armadilha do fator `zn` na projeção, que estava errada
 * aqui e no vr_openxr.cpp.
 */
void build_eye(float ipd_half, float game_fov_deg, float aspect, int eye, float vp[16])
{
    const float zn = 0.05f, zf = 5000.0f;
    float fov[4];
    if (g_rt.view_count >= (uint32_t)kEyes) {
        const XrFovf &f = g_rt.views[eye].fov;
        fov[0] = std::tan(f.angleLeft);
        fov[1] = std::tan(f.angleRight);
        fov[2] = std::tan(f.angleDown);
        fov[3] = std::tan(f.angleUp);
    } else {
        vr_math::game_fov_tangents(game_fov_deg, aspect, fov); // sem runtime: FOV do jogo, um olho só
    }
    vr_math::eye_view_projection(ipd_half, eye, fov, zn, zf, vp);
}

} // namespace

// ---------------------------------------------------------------- pose na view do jogo

void apply_head_to_view(uint8_t *cam_base)
{
    if (!g_pose_ok) return;

    float *l = reinterpret_cast<float *>(cam_base + 0xD0);
    float *u = reinterpret_cast<float *>(cam_base + 0xE0);
    float *f = reinterpret_cast<float *>(cam_base + 0xF0);
    float *p = reinterpret_cast<float *>(cam_base + 0x100);

    for (int i = 0; i < 3; i++) {
        g_basis.forward[i] = f[i];
        g_basis.up[i] = u[i];
    }
    normalize3(g_basis.forward);
    normalize3(g_basis.up);
    if (std::fabs(g_turn) > 1e-6f) {
        rotate_y(g_basis.forward, g_turn); // snap turn: gira o corpo, não só a cabeça
        rotate_y(g_basis.up, g_turn);
        normalize3(g_basis.forward);
        normalize3(g_basis.up);
    }
    cross3(g_basis.up, g_basis.forward, g_basis.left);
    normalize3(g_basis.left);

    const float right[3] = {-g_basis.left[0], -g_basis.left[1], -g_basis.left[2]};

    float hpos[3], hquat[4];
    if (!head_pose(hpos, hquat)) return;

    // OpenXR (+x direita, +y cima, -z frente) -> base do corpo (frente/esquerda/cima)
    auto to_body = [&](const float in[3], float out[3]) {
        const float ar = in[0], au = in[1], af = -in[2];
        for (int i = 0; i < 3; i++)
            out[i] = g_basis.forward[i] * af + right[i] * ar + g_basis.up[i] * au;
    };

    static const float kFwd[3] = {0, 0, -1}, kUp[3] = {0, 1, 0}, kRight[3] = {1, 0, 0};
    float rf[3], ru[3], rr[3], hf[3], hu[3], hr[3];
    quat_rotate(hquat, kFwd, rf);
    quat_rotate(hquat, kUp, ru);
    quat_rotate(hquat, kRight, rr);
    to_body(rf, hf);
    to_body(ru, hu);
    to_body(rr, hr);
    normalize3(hf);
    normalize3(hr);
    const float d = hu[0] * hf[0] + hu[1] * hf[1] + hu[2] * hf[2];
    for (int i = 0; i < 3; i++) hu[i] -= hf[i] * d;
    normalize3(hu);

    for (int i = 0; i < 3; i++) {
        f[i] = hf[i];
        u[i] = hu[i];
        l[i] = -hr[i]; // a row do jogo aponta para a esquerda
    }

    // Agachar, inclinar, dar um passo: deslocamento da cabeça (escala configurável).
    float dp[3];
    to_body(hpos, dp);
    const float s = g_cfg.vr_move_scale;
    for (int i = 0; i < 3; i++) p[i] += dp[i] * s;
}

// ---------------------------------------------------------------- matrizes de clip

void pose_frame_update(float game_fov_deg, float aspect)
{
    g_game_fov_deg = game_fov_deg;
    g_game_aspect = aspect;
    vr_math::identity(g_identity);
    const float ipd_half = std::max(0.0f, g_cfg.vr_ipd) * 0.001f * 0.5f; // mm -> m -> metade
    const float zn = 0.05f, zf = 5000.0f;

    // A projeção que o jogo usa (mono): FOV horizontal do jogo x aspecto do render target.
    float mono_fov[4];
    vr_math::game_fov_tangents(game_fov_deg, aspect, mono_fov);

    for (int e = 0; e < kEyes; e++) {
        float eye_fov[4];
        if (g_rt.view_count >= (uint32_t)kEyes) {
            const XrFovf &f = g_rt.views[e].fov;
            eye_fov[0] = std::tan(f.angleLeft);
            eye_fov[1] = std::tan(f.angleRight);
            eye_fov[2] = std::tan(f.angleDown);
            eye_fov[3] = std::tan(f.angleUp);
        } else {
            std::memcpy(eye_fov, mono_fov, sizeof mono_fov); // sem runtime: nada de estéreo
        }
        // C = (V_mono·P_mono)⁻¹ · (V_olho·P_olho)  ->  constante c201..c204 do shader
        vr_math::eye_clip_matrix(ipd_half, e, mono_fov, eye_fov, zn, zf, g_clip[e]);
    }
    g_pose_ok = true;
}

void set_eye(int eye_index) { g_eye = std::clamp(eye_index, 0, kEyes - 1); }
int eye() { return g_eye; }

const float *clip_matrix(int eye_index)
{
    if (eye_index < 0 || eye_index >= kEyes) return g_identity;
    return g_clip[eye_index];
}

void set_turn(float radians) { g_turn = radians; }
void add_turn(float radians) { g_turn += radians; }
float turn() { return g_turn; }

// ---------------------------------------------------------------- patch no código do jogo

void install_patch()
{
    if (g_installed) return;
    g_installed = true;
    const uint32_t site = xbe_va(kEyeHeightStore);
    auto *code = reinterpret_cast<uint8_t *>(site);
    if (memcmp(code, kExpect, sizeof kExpect) != 0) {
        logf("[vr] 0x%08x não tem a gravação da altura do olho neste build; sem rastreamento de cabeça", site);
        return;
    }
    if (code[0] == 0xE8) {
        logf("[vr] o lean tático já ocupa 0x%08x: use lean_angle=0 para o VR controlar a câmera", site);
        return;
    }
    code[0] = 0xE8; // call vr_eye_height_thunk (os 8 bytes viram call + nops)
    const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&vr_eye_height_thunk) - (site + 5));
    memcpy(code + 1, &rel, 4);
    code[5] = code[6] = code[7] = 0x90;
    logf("[vr] rastreamento de cabeça instalado em 0x%08x (view FUN_001dfd90)", site);
}

} // namespace internal

// ---------------------------------------------------------------- fachada para o port

void install()
{
    internal::install_patch();
    internal::g_requested = true; // a partir daqui o port tenta subir o runtime no primeiro Swap
}

void pose_frame_update(float game_fov_deg, float aspect) { internal::pose_frame_update(game_fov_deg, aspect); }

const float *clip_matrix(int eye) { return internal::clip_matrix(eye); }

void set_eye(int eye) { internal::set_eye(eye); }

int eye() { return internal::eye(); }

void snap_turn(float degrees) { internal::add_turn(degrees * 3.14159265f / 180.0f); }

float game_fov() { return internal::g_game_fov_deg; }

void game_render_size(int *w, int *h)
{
    if (w) *w = internal::g_render_w;
    if (h) *h = internal::g_render_h;
}

void set_game_render_size(int w, int h)
{
    internal::g_render_w = w;
    internal::g_render_h = h;
}

} // namespace vr

// ---- thunk instalado em 0x1DFFEC (substitui a gravação da altura do olho) ----
extern "C" void __cdecl vr_apply_view(uint8_t *cam)
{
    vr::internal::apply_head_to_view(cam);
}

#if defined(_MSC_VER)
// O port exige MSVC (asm inline x86), então este thunk é MSVC-only por natureza. O guarda existe
// para que o resto do módulo possa ser checado com outro compilador (tests/syntax_check.sh).
extern "C" __declspec(naked) void vr_eye_height_thunk()
{
    __asm {
        movss dword ptr [esi + 0x104], xmm0   // o que a instrução original fazia
        push eax
        push ecx
        push edx
        push esi
        call vr_apply_view
        add esp, 4
        pop edx
        pop ecx
        pop eax
        ret
    }
}
#else
extern "C" void vr_eye_height_thunk() {} // sem MSVC não há asm inline: o patch não é aplicado
#endif
