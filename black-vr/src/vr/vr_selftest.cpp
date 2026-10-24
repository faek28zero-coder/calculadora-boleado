// ============================================================
//  BLACK VR — vr_selftest.cpp
//
//  `black.exe --vr-selftest`: mostra no headset um cenário métrico SEM carregar o jogo.
//
//  Serve para separar problemas. Se o cenário aparece certo, então runtime, sessão,
//  swapchain, poses, projeção por olho e envio de frame estão bons — e qualquer defeito
//  que apareça depois é do acoplamento com o BLACK, não da camada de VR.
//
//  O que se vê (tudo em metros reais, rasterizado na CPU em cada olho com a projeção
//  assimétrica e a pose que o runtime devolveu):
//    - piso quadriculado de 1 m (xadrez alternado), de -5 m a +5 m;
//    - régua vertical em z = -2 m, marcas a cada 10 cm;
//    - alvo em cruz a 2 m de distância, no centro do olhar;
//    - dois postes a 1 m à esquerda e à direita, à altura dos olhos (referência de IPD).
// ============================================================
#include "vr/vr.h"

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "vr/vr_internal.h"

namespace vr {
namespace {

using internal::g_rt;

struct Vec3 {
    float x, y, z;
};

Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vec3 norm(Vec3 a)
{
    const float l = std::sqrt(dot(a, a));
    return l > 1e-8f ? Vec3{a.x / l, a.y / l, a.z / l} : Vec3{0, 0, -1};
}

/** Projeta um ponto do mundo (espaço do palco) no NDC do olho; devolve false se atrás. */
bool project(const XrPosef &pose, const XrFovf &fov, Vec3 p, float *nx, float *ny, float *depth)
{
    // base da câmera a partir da pose (OpenXR: -z para frente)
    const float qx = pose.orientation.x, qy = pose.orientation.y, qz = pose.orientation.z, qw = pose.orientation.w;
    auto rot = [&](Vec3 v) {
        const Vec3 t = {2.0f * (qy * v.z - qz * v.y), 2.0f * (qz * v.x - qx * v.z), 2.0f * (qx * v.y - qy * v.x)};
        return Vec3{v.x + qw * t.x + (qy * t.z - qz * t.y), v.y + qw * t.y + (qz * t.x - qx * t.z),
                    v.z + qw * t.z + (qx * t.y - qy * t.x)};
    };
    const Vec3 eye = {pose.position.x, pose.position.y, pose.position.z};
    const Vec3 rel = sub(p, eye);
    const Vec3 right = rot({1, 0, 0}), up = rot({0, 1, 0}), back = rot({0, 0, 1});
    const float cx = dot(rel, right), cy = dot(rel, up), cz = -dot(rel, back); // cz > 0 = à frente
    if (cz < 0.05f) return false;
    const float tl = std::tan(fov.angleLeft), tr = std::tan(fov.angleRight);
    const float tb = std::tan(fov.angleDown), tt = std::tan(fov.angleUp);
    // interpolação linear das tangentes: aproximação suficiente para um alvo de calibração
    const float tx = tl + (tr - tl) * 0.5f, ty = tb + (tt - tb) * 0.5f;
    *nx = (cx / cz) / (tx != 0.0f ? tx : 1.0f);
    *ny = (cy / cz) / (ty != 0.0f ? ty : 1.0f);
    *depth = cz;
    return true;
}

void put_px(std::vector<uint8_t> &img, uint32_t w, uint32_t h, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || y < 0 || (uint32_t)x >= w || (uint32_t)y >= h) return;
    uint8_t *p = &img[((size_t)y * w + x) * 4];
    p[0] = b; // BGRA
    p[1] = g;
    p[2] = r;
    p[3] = 255;
}

void line(std::vector<uint8_t> &img, uint32_t w, uint32_t h, float x0, float y0, float x1, float y1, uint8_t r,
          uint8_t g, uint8_t b)
{
    const float dx = x1 - x0, dy = y1 - y0;
    const int steps = (int)std::max(std::fabs(dx), std::fabs(dy));
    for (int i = 0; i <= steps; i++) {
        const float t = steps ? (float)i / (float)steps : 0.0f;
        const int x = (int)std::lround(x0 + dx * t), y = (int)std::lround(y0 + dy * t);
        put_px(img, w, h, x, y, r, g, b);
        put_px(img, w, h, x + 1, y, r, g, b); // 2 px de espessura
    }
}

void render_eye(int eye, int frame)
{
    internal::EyeImage &ei = g_rt.eye[eye];
    if (!ei.acquired || ei.width == 0) return;

    const uint32_t w = ei.width, h = ei.height;
    std::vector<uint8_t> img((size_t)w * h * 4);
    // fundo: gradiente escuro do chão ao horizonte
    for (uint32_t y = 0; y < h; y++) {
        const uint8_t v = (uint8_t)(10 + (y * 40) / h);
        for (uint32_t x = 0; x < w; x++) {
            uint8_t *p = &img[((size_t)y * w + x) * 4];
            p[0] = v;
            p[1] = v;
            p[2] = (uint8_t)(v + 6);
            p[3] = 255;
        }
    }

    const XrPosef &pose = g_rt.views[eye].pose;
    const XrFovf &fov = g_rt.views[eye].fov;
    auto to_px = [&](float nx, float ny, int *px, int *py) {
        *px = (int)((nx * 0.5f + 0.5f) * (float)w);
        *py = (int)((0.5f - ny * 0.5f) * (float)h);
    };

    // piso: grade de 1 m, xadrez
    for (int gx = -5; gx <= 5; gx++) {
        for (int gz = -8; gz <= 0; gz++) {
            if (((gx + gz) & 1) == 0) continue;
            Vec3 corners[4] = {{(float)gx, -1.6f, (float)gz},
                               {(float)gx + 1, -1.6f, (float)gz},
                               {(float)gx + 1, -1.6f, (float)gz + 1},
                               {(float)gx, -1.6f, (float)gz + 1}};
            float pxs[4], pys[4];
            bool ok = true;
            for (int i = 0; i < 4 && ok; i++) {
                float nx, ny, d;
                ok = project(pose, fov, corners[i], &nx, &ny, &d);
                if (ok) to_px(nx, ny, &pxs[i], &pys[i]);
            }
            if (!ok) continue;
            const uint8_t shade = (uint8_t)(28 + ((gx + gz) % 3) * 6);
            for (int i = 0; i < 4; i++) {
                const int j = (i + 1) % 4;
                line(img, w, h, (float)pxs[i], (float)pys[i], (float)pxs[j], (float)pys[j], shade, shade,
                     (uint8_t)(shade + 14));
            }
        }
    }

    // régua em z = -2 m (marcas de 10 cm) e alvo em cruz no centro do olhar
    for (int i = -15; i <= 15; i++) {
        const float y = i * 0.1f;
        Vec3 a = {-0.5f, y, -2.0f}, b = {-0.5f + (i % 5 == 0 ? 0.25f : 0.12f), y, -2.0f};
        float nx0, ny0, d0, nx1, ny1, d1;
        if (!project(pose, fov, a, &nx0, &ny0, &d0)) continue;
        if (!project(pose, fov, b, &nx1, &ny1, &d1)) continue;
        int x0, y0, x1, y1;
        to_px(nx0, ny0, &x0, &y0);
        to_px(nx1, ny1, &x1, &y1);
        line(img, w, h, (float)x0, (float)y0, (float)x1, (float)y1, 230, 230, 220);
    }

    // alvo a 2 m
    {
        Vec3 c = {0, 0, -2.0f};
        float nx, ny, d;
        if (project(pose, fov, c, &nx, &ny, &d)) {
            int cx, cy;
            to_px(nx, ny, &cx, &cy);
            const int arm = (int)(h * 0.03f);
            line(img, w, h, (float)(cx - arm), (float)cy, (float)(cx + arm), (float)cy, 255, 106, 19);
            line(img, w, h, (float)cx, (float)(cy - arm), (float)cx, (float)(cy + arm), 255, 106, 19);
        }
    }

    // postes a 1 m para cada lado, na altura dos olhos (checar convergência/IPD)
    for (int s = -1; s <= 1; s += 2) {
        Vec3 a = {(float)s, -1.6f, -1.0f}, b = {(float)s, 0.4f, -1.0f};
        float n0, m0, d0, n1, m1, d1;
        if (!project(pose, fov, a, &n0, &m0, &d0) || !project(pose, fov, b, &n1, &m1, &d1)) continue;
        int x0, y0, x1, y1;
        to_px(n0, m0, &x0, &y0);
        to_px(n1, m1, &x1, &y1);
        line(img, w, h, (float)x0, (float)y0, (float)x1, (float)y1, 120, 200, 255);
    }

    // etiqueta do olho + contador (prova que a imagem muda por olho)
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            const bool on = ((i + j + frame) & 1) == 0;
            if (!on) continue;
            put_px(img, w, h, 20 + i * 3 + (eye ? 90 : 0), 20 + j * 3, 255, 106, 19);
        }

    internal::upload_eye(eye, img.data(), w, h, w * 4);
}

} // namespace

bool selftest(const void *hwnd, int seconds)
{
    if (!start(hwnd)) {
        logf("[vr] autoteste: %s", last_error());
        return false;
    }
    logf("[vr] autoteste: %d s. Olhe em volta: o piso tem 1 m por quadrado, o alvo está a 2 m e a régua "
         "à esquerda marca 10 cm. Aperte Ctrl+C na janela do console para sair antes.",
         seconds);

    const ULONGLONG t0 = GetTickCount64();
    int frame = 0;
    while (GetTickCount64() - t0 < (ULONGLONG)seconds * 1000ull) {
        frame_begin();
        if (active()) {
            render_eye(0, frame);
            render_eye(1, frame);
            frame_end();
        }
        frame++;
        Sleep(active() ? 8 : 100);
    }
    shutdown();
    return true;
}

} // namespace vr
