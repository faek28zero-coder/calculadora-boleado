// ============================================================
//  BLACK VR — vr_math.h
//
//  A matemática do estéreo, isolada de D3D/OpenXR para poder ser testada fora do Windows
//  (`tests/` compila este cabeçalho com g++ e compara o resultado com um caminho direto).
//
//  Convenção (a mesma do D3D do Xbox e do port):
//      - matrizes 4x4 guardadas em ordem de LINHA: m[linha * 4 + coluna]
//      - vetores são LINHA: v' = v · M
//      - produto: mul(a, b) = a · b   (logo v · (a·b) = (v·a)·b)
//      - espaço de visão: x direita, y cima, z PARA FRENTE (mão esquerda, como o D3D)
//      - clip space do D3D: z em [0, 1] depois da divisão por w
//
//  A correção por olho é
//      C = (V_mono · P_mono)⁻¹ · (V_olho · P_olho)
//  e o vertex shader traduzido do port aplica, no fim,
//      o.pos = (dot(o.pos, c201), dot(o.pos, c202), dot(o.pos, c203), dot(o.pos, c204))
//  onde c(201+i) é a linha i de C. Fora do VR, C = identidade.
// ============================================================
#pragma once

#include <cmath>
#include <cstring>

namespace vr_math {

constexpr float kPi = 3.14159265358979f;
constexpr float kDegToRad = kPi / 180.0f;

inline void identity(float m[16])
{
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

inline void mul(const float a[16], const float b[16], float out[16]) // out = a · b
{
    float r[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            r[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] + a[i * 4 + 2] * b[2 * 4 + j] +
                           a[i * 4 + 3] * b[3 * 4 + j];
    std::memcpy(out, r, sizeof r);
}

/** Inversa pela adjunta (a mesma rotina clássica). Devolve false se singular. */
inline bool invert(const float m[16], float out[16])
{
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] -
             m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] -
             m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];

    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::fabs(det) < 1e-12f) return false;
    const float d = 1.0f / det;
    for (int i = 0; i < 16; i++) out[i] = inv[i] * d;
    return true;
}

/**
 * Projeção perspectiva assimétrica em convenção de linha (a do D3D).
 * `tan_*` são as tangentes dos ângulos até cada borda (esquerda/direita/baixo/cima) — ou seja,
 * as coordenadas da borda a 1 m de distância, que é o que `XrFovf` e um FOV em graus dão.
 * `zn` e `zf` são as distâncias do near e do far.
 *
 * Atenção à armadilha: a fórmula clássica do D3DX (`2·zn/(r-l)`) vale quando l/r/t/b são as
 * coordenadas da borda NO PLANO DO NEAR. Aqui as tangentes são a 1 m, então o fator é `2/(r-l)`.
 * Multiplicar por zn encolhe x e y por 20× (zn = 0,05) e a conta do IPD sai errada — foi o que
 * o teste `tests/vr_math_test.cpp` pegou.
 */
inline void projection(float tan_l, float tan_r, float tan_b, float tan_t, float zn, float zf, float out[16])
{
    identity(out);
    const float w = tan_r - tan_l, h = tan_t - tan_b;
    if (std::fabs(w) < 1e-8f || std::fabs(h) < 1e-8f) return;
    out[0] = 2.0f / w;
    out[5] = 2.0f / h;
    out[8] = -(tan_r + tan_l) / w;
    out[9] = -(tan_t + tan_b) / h;
    out[10] = zf / (zf - zn);
    out[11] = 1.0f;
    out[14] = -zn * zf / (zf - zn);
    out[15] = 0.0f;
}

/** FOV horizontal (graus) -> tangente da metade (projeção simétrica). */
inline float tan_half_h(float fov_deg) { return std::tan(fov_deg * 0.5f * kDegToRad); }

/**
 * V_olho · P_olho: a translação que traz o mundo para o referencial do olho, seguida da projeção.
 * `fov` traz as quatro tangentes (assimétrica, como um headset real).
 *
 * O sinal: o olho esquerdo está em x = -ipd/2 no mundo, então um ponto do mundo em x = 0 aparece
 * em x = +ipd/2 no referencial DELE — a translação é +ipd/2. Trocar isso olha o estéreo de
 * dentro para fora (a cena "salta" e a dor de cabeça aparece em segundos).
 */
inline void eye_view_projection(float ipd_half, int eye, const float fov[4], float zn, float zf, float out[16])
{
    float v[16], p[16];
    identity(v);
    v[12] = eye == 0 ? ipd_half : -ipd_half;
    projection(fov[0], fov[1], fov[2], fov[3], zn, zf, p);
    mul(v, p, out);
}

/**
 * As tangentes do FOV do jogo para um olho: o que o jogo usa hoje é uma projeção simétrica de
 * `fov_deg` horizontal com o aspecto do render target.
 */
inline void game_fov_tangents(float fov_deg, float aspect, float out[4])
{
    const float tx = tan_half_h(fov_deg);
    const float ty = tx / (aspect > 1e-3f ? aspect : 1.0f);
    out[0] = -tx;
    out[1] = tx;
    out[2] = -ty;
    out[3] = ty;
}

/**
 * C = (V_mono · P_mono)⁻¹ · (V_olho · P_olho), que leva o clip mono do jogo ao clip do olho.
 * `mono_fov` = tangentes da projeção que o jogo usa (simétrica), `eye_fov` = as do olho.
 */
inline bool eye_clip_matrix(float ipd_half, int eye, const float mono_fov[4], const float eye_fov[4], float zn, float zf,
                            float out[16])
{
    float mono[16], mono_inv[16], eye_vp[16];
    eye_view_projection(0.0f, 0, mono_fov, zn, zf, mono);
    if (!invert(mono, mono_inv)) {
        identity(out);
        return false;
    }
    eye_view_projection(ipd_half, eye, eye_fov, zn, zf, eye_vp);
    mul(mono_inv, eye_vp, out);
    return true;
}

/** v · M (vetor linha, w = 1 por padrão). */
inline void transform(const float v[4], const float m[16], float out[4])
{
    float r[4];
    for (int j = 0; j < 4; j++)
        r[j] = v[0] * m[0 * 4 + j] + v[1] * m[1 * 4 + j] + v[2] * m[2 * 4 + j] + v[3] * m[3 * 4 + j];
    std::memcpy(out, r, sizeof r);
}

/** Divisão por w (o que o rasterizador faz). Devolve false se w for degenerado. */
inline bool to_ndc(const float clip[4], float ndc[3])
{
    if (std::fabs(clip[3]) < 1e-8f) return false;
    const float iw = 1.0f / clip[3];
    ndc[0] = clip[0] * iw;
    ndc[1] = clip[1] * iw;
    ndc[2] = clip[2] * iw;
    return true;
}

} // namespace vr_math
