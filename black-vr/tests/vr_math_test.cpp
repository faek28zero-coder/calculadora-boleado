// ============================================================
//  BLACK VR — tests/vr_math_test.cpp
//
//  Testa a matemática do estéreo sem D3D, sem OpenXR e sem o jogo:
//
//      g++ -std=c++20 -O2 -I ../src tests/vr_math_test.cpp -o vr_math_test && ./vr_math_test
//      (ou: tests/run.sh)
//
//  O teste que importa é `clip_matrix_matches_direct_path`: ele pega um ponto do mundo, leva
//  pela projeção mono do jogo (o que o shader produz) e aplica C; em paralelo leva o MESMO ponto
//  direto pela view+projeção do olho. Os dois têm de dar o mesmo NDC. Se a convenção de matriz
//  estiver invertida (coluna em vez de linha), se a inversa estiver errada ou se a ordem do
//  produto estiver trocada, este teste falha.
// ============================================================
#include "vr/vr_math.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>

using namespace vr_math;

namespace {

int g_failures;
int g_checks;

void check(bool ok, const char *what, float got = 0.0f, float want = 0.0f, float tol = 0.0f)
{
    g_checks++;
    if (ok) return;
    g_failures++;
    if (tol > 0.0f) std::printf("  FALHOU: %s (obtido %.6f, esperado %.6f, tolerância %.6f)\n", what, got, want, tol);
    else std::printf("  FALHOU: %s\n", what);
}

void check_near(float got, float want, float tol, const char *what)
{
    check(std::fabs(got - want) <= tol, what, got, want, tol);
}

/** NDC do mesmo ponto pelos dois caminhos: via C (como o shader faz) e direto (view+proj do olho). */
bool same_ndc(const float p[4], const float mono_fov[4], const float eye_fov[4], float ipd_half, int eye, float zn,
              float zf, float out_via_c[3], float out_direct[3])
{
    float mono[16], eye_vp[16], c[16];
    eye_view_projection(0.0f, 0, mono_fov, zn, zf, mono);
    eye_view_projection(ipd_half, eye, eye_fov, zn, zf, eye_vp);
    if (!eye_clip_matrix(ipd_half, eye, mono_fov, eye_fov, zn, zf, c)) return false;

    float clip_mono[4], clip_via_c[4], clip_direct[4];
    transform(p, mono, clip_mono);   // o que o jogo já produziu (clip mono)
    transform(clip_mono, c, clip_via_c); // o que o shader faz com c201..c204
    transform(p, eye_vp, clip_direct);   // o caminho direto
    return to_ndc(clip_via_c, out_via_c) && to_ndc(clip_direct, out_direct);
}

// --- um headset típico: FOV assimétrico e olhos deslocados ---
constexpr float kZn = 0.05f, kZf = 5000.0f;

void test_identity_inverse()
{
    std::printf("identidade e inversa\n");
    float i[16];
    identity(i);
    float p[4] = {1.5f, -2.0f, 3.0f, 1.0f}, q[4];
    transform(p, i, q);
    for (int k = 0; k < 4; k++) check_near(q[k], p[k], 1e-6f, "identidade não muda o vetor");

    // inversa de matrizes aleatórias: M · M⁻¹ = I
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> dist(-2.0f, 2.0f);
    for (int n = 0; n < 200; n++) {
        float m[16];
        for (float &v : m) v = dist(rng);
        m[3] = m[7] = m[11] = 0.0f; // linha afim, como as que usamos
        m[15] = 1.0f;
        float inv[16], prod[16];
        if (!invert(m, inv)) continue;
        mul(m, inv, prod);
        for (int r = 0; r < 4; r++)
            for (int col = 0; col < 4; col++) check_near(prod[r * 4 + col], r == col ? 1.0f : 0.0f, 1e-4f, "M·M⁻¹ = I");
    }
}

void test_projection_basics()
{
    std::printf("projeção perspectiva\n");
    float fov[4];
    game_fov_tangents(70.0f, 16.0f / 9.0f, fov);
    float p[16];
    projection(fov[0], fov[1], fov[2], fov[3], kZn, kZf, p);

    // um ponto no centro do olhar e uma unidade à frente cai no centro do NDC
    float center[4] = {0.0f, 0.0f, 1.0f, 1.0f}, clip[4], ndc[3];
    transform(center, p, clip);
    check(to_ndc(clip, ndc), "divisão por w");
    check_near(ndc[0], 0.0f, 1e-6f, "centro em x");
    check_near(ndc[1], 0.0f, 1e-6f, "centro em y");

    // a borda direita do FOV cai em x = 1; a borda esquerda em -1
    const float tx = tan_half_h(70.0f);
    float right_edge[4] = {tx * 2.0f, 0.0f, 2.0f, 1.0f};
    transform(right_edge, p, clip);
    to_ndc(clip, ndc);
    check_near(ndc[0], 1.0f, 1e-4f, "borda direita do FOV em NDC x = 1");
    float left_edge[4] = {-tx * 2.0f, 0.0f, 2.0f, 1.0f};
    transform(left_edge, p, clip);
    to_ndc(clip, ndc);
    check_near(ndc[0], -1.0f, 1e-4f, "borda esquerda do FOV em NDC x = -1");

    // borda de cima do FOV (y = tan(fov/2)/aspecto)
    const float ty = tx / (16.0f / 9.0f);
    float top_edge[4] = {0.0f, ty * 2.0f, 2.0f, 1.0f};
    transform(top_edge, p, clip);
    to_ndc(clip, ndc);
    check_near(ndc[1], 1.0f, 1e-4f, "borda de cima do FOV em NDC y = 1");

    // profundidade: near -> 0, far -> 1 (convenção do D3D)
    float at_near[4] = {0.0f, 0.0f, kZn, 1.0f}, at_far[4] = {0.0f, 0.0f, kZf, 1.0f};
    transform(at_near, p, clip);
    to_ndc(clip, ndc);
    check_near(ndc[2], 0.0f, 1e-4f, "z do near em 0");
    transform(at_far, p, clip);
    to_ndc(clip, ndc);
    check_near(ndc[2], 1.0f, 1e-3f, "z do far em 1");
}

/** O teste central: C tem de reproduzir exatamente o caminho direto. */
void test_clip_matrix_matches_direct_path()
{
    std::printf("matriz de clip por olho x caminho direto\n");
    float mono_fov[4];
    game_fov_tangents(70.0f, 16.0f / 9.0f, mono_fov);

    // headset realista: cada olho vê mais para fora do que para dentro, e o centro do olho
    // está deslocado do centro do nariz
    const float eye_fov[2][4] = {
        {-52.0f * kDegToRad, 40.0f * kDegToRad, -44.0f * kDegToRad, 38.0f * kDegToRad}, // esquerdo
        {-40.0f * kDegToRad, 52.0f * kDegToRad, -44.0f * kDegToRad, 38.0f * kDegToRad}, // direito
    };
    float eye_tan[2][4];
    for (int e = 0; e < 2; e++)
        for (int k = 0; k < 4; k++) eye_tan[e][k] = std::tan(eye_fov[e][k]);

    const float ipd_half = 0.0315f; // 63 mm

    const float points[][4] = {
        {0.0f, 0.0f, 2.0f, 1.0f},     // centro do olhar a 2 m
        {0.0f, 0.0f, 30.0f, 1.0f},    // longe
        {1.0f, 0.0f, 2.0f, 1.0f},     // à direita
        {-0.8f, 0.4f, 3.0f, 1.0f},    // à esquerda e acima
        {0.3f, -1.5f, 1.2f, 1.0f},    // perto, abaixo (chão aos pés)
        {6.0f, 0.0f, 40.0f, 1.0f},    // quase no limite do FOV
        {0.0f, 0.0f, 0.2f, 1.0f},     // muito perto do rosto
    };

    for (int e = 0; e < 2; e++) {
        for (const auto &p : points) {
            float via_c[3], direct[3];
            if (!same_ndc(p, mono_fov, eye_tan[e], ipd_half, e, kZn, kZf, via_c, direct)) {
                check(false, "eye_clip_matrix falhou");
                continue;
            }
            char label[128];
            std::snprintf(label, sizeof label, "olho %d, ponto (%.1f, %.1f, %.1f): NDC x", e, p[0], p[1], p[2]);
            check_near(via_c[0], direct[0], 2e-4f, label);
            std::snprintf(label, sizeof label, "olho %d, ponto (%.1f, %.1f, %.1f): NDC y", e, p[0], p[1], p[2]);
            check_near(via_c[1], direct[1], 2e-4f, label);
            std::snprintf(label, sizeof label, "olho %d, ponto (%.1f, %.1f, %.1f): NDC z", e, p[0], p[1], p[2]);
            check_near(via_c[2], direct[2], 2e-4f, label);
        }
    }
}

/** Um objeto no centro do olhar tem de aparecer deslocado para o lado oposto em cada olho. */
void test_parallax_sign_and_size()
{
    std::printf("paralaxe: sinal e tamanho\n");
    float mono_fov[4];
    game_fov_tangents(70.0f, 16.0f / 9.0f, mono_fov);
    // cada olho tem o seu FOV assimétrico: quem está à esquerda vê mais para a esquerda
    const float eye_fov[2][4] = {
        {-52.0f * kDegToRad, 40.0f * kDegToRad, -44.0f * kDegToRad, 38.0f * kDegToRad},
        {-40.0f * kDegToRad, 52.0f * kDegToRad, -44.0f * kDegToRad, 38.0f * kDegToRad},
    };
    float eye_tan[2][4];
    for (int e = 0; e < 2; e++)
        for (int k = 0; k < 4; k++) eye_tan[e][k] = std::tan(eye_fov[e][k]);

    const float ipd_half = 0.0315f;
    float point[4] = {0.0f, 0.0f, 2.0f, 1.0f}; // 2 m à frente, no meio
    float ndc[2][3] = {{0, 0, 0}, {0, 0, 0}};

    for (int e = 0; e < 2; e++) {
        float direct[3];
        float via_c[3];
        if (!same_ndc(point, mono_fov, eye_tan[e], ipd_half, e, kZn, kZf, via_c, direct)) continue;
        std::memcpy(ndc[e], direct, sizeof direct);
    }
    // o olho esquerdo está em x = -ipd/2: o ponto no meio fica à DIREITA dele -> NDC x > 0
    check(ndc[0][0] > 0.0f, "olho esquerdo vê o ponto do centro à direita");
    check(ndc[1][0] < 0.0f, "olho direito vê o ponto do centro à esquerda");

    // tamanho: deslocamento angular = atan(ipd/2 / d); em NDC é aproximadamente
    // tan(atan(x/d) + centro da assimetria)... comparamos com o cálculo direto do ângulo:
    const float angle = std::atan(ipd_half / 2.0f); // ipd_half já é ipd/2; d = 2 m
    const float tan_l = eye_tan[0][0], tan_r = eye_tan[0][1];
    const float cx = (tan_r + tan_l) * 0.5f; // centro da projeção assimétrica
    const float half_w = (tan_r - tan_l) * 0.5f;
    const float want = (std::tan(angle) - cx) / half_w; // NDC x do ponto para o olho esquerdo
    check_near(ndc[0][0], want, 5e-3f, "paralaxe do olho esquerdo na distância de 2 m");

    // A separação entre os olhos em NDC tem dois termos:
    //     sep(z) = A + B/z,  B = 2·(ipd/2) / meia_largura
    // `A` vem do FOV assimétrico (cada tela cobre um intervalo angular deslocado para fora; em
    // objeto no infinito os dois NDC ainda diferem, e isso é o correto), `B/z` é a paralaxe de
    // verdade, a que dá profundidade. Os dois termos são verificados aqui.

    // (a) com FOV simétrico nos dois olhos, A = 0 e a separação cai com 1/z
    const float sym[4] = {-0.85f, 0.85f, -0.55f, 0.55f};
    const float half_w_sym = sym[1];
    for (float d : {1.0f, 2.0f, 10.0f}) {
        float p[4] = {0.0f, 0.0f, d, 1.0f};
        float a[3], b[3], ca[3], cb[3];
        if (!same_ndc(p, mono_fov, sym, ipd_half, 0, kZn, kZf, ca, a)) continue;
        if (!same_ndc(p, mono_fov, sym, ipd_half, 1, kZn, kZf, cb, b)) continue;
        const float want = 2.0f * ipd_half / (d * half_w_sym);
        check_near(std::fabs(a[0] - b[0]), want, 3e-4f, "separação com FOV simétrico = ipd / (z · meia-largura)");
    }

    // (b) com FOV assimétrico por olho, o termo constante A existe e B continua sendo a paralaxe
    auto sep_at = [&](float d) {
        float p[4] = {0.0f, 0.0f, d, 1.0f};
        float a[3], b[3], ca[3], cb[3];
        if (!same_ndc(p, mono_fov, eye_tan[0], ipd_half, 0, kZn, kZf, ca, a)) return 0.0f;
        if (!same_ndc(p, mono_fov, eye_tan[1], ipd_half, 1, kZn, kZf, cb, b)) return 0.0f;
        return a[0] - b[0];
    };
    const float s2 = sep_at(2.0f), s20 = sep_at(20.0f);
    const float b_fit = (s2 - s20) / (0.5f - 0.05f); // B de sep = A + B/z
    const float half_w_asym = (eye_tan[0][1] - eye_tan[0][0]) * 0.5f;
    check_near(b_fit, 2.0f * ipd_half / half_w_asym, 2e-3f, "termo 1/z da separação (paralaxe) com FOV assimétrico");
    const float a_fit = s2 - b_fit / 2.0f;
    check(a_fit > 0.0f, "termo constante da separação (o FOV assimétrico desloca as telas para fora)");
}

/** Sem IPD e com o mesmo FOV do jogo, a correção tem de ser a identidade (não estragar nada). */
void test_identity_when_flat()
{
    std::printf("identidade quando não há estéreo\n");
    float fov[4];
    game_fov_tangents(70.0f, 4.0f / 3.0f, fov);
    float c[16];
    check(eye_clip_matrix(0.0f, 0, fov, fov, kZn, kZf, c), "matriz calculada");
    for (int r = 0; r < 4; r++)
        for (int col = 0; col < 4; col++)
            check_near(c[r * 4 + col], r == col ? 1.0f : 0.0f, 1e-5f, "C = I com ipd 0 e mesmo FOV");
}

/** O olho direito tem de ser o espelho do esquerdo quando o FOV também é. */
void test_mirror_symmetry()
{
    std::printf("simetria entre os olhos\n");
    float mono_fov[4];
    game_fov_tangents(70.0f, 16.0f / 9.0f, mono_fov);
    const float sym[4] = {-0.6f, 0.6f, -0.45f, 0.45f};
    const float ipd_half = 0.032f;

    float c0[16], c1[16];
    eye_clip_matrix(ipd_half, 0, mono_fov, sym, kZn, kZf, c0);
    eye_clip_matrix(ipd_half, 1, mono_fov, sym, kZn, kZf, c1);

    float mono[16];
    eye_view_projection(0.0f, 0, mono_fov, kZn, kZf, mono);

    auto ndc_of = [&](const float p[4], const float c[16], float out[3]) {
        float cm[4], clip[4];
        transform(p, mono, cm);
        transform(cm, c, clip);
        to_ndc(clip, out);
    };

    // (a) um ponto no eixo central: os dois olhos o veem espelhados
    float center[4] = {0.0f, -0.2f, 4.0f, 1.0f};
    float n0[3], n1[3];
    ndc_of(center, c0, n0);
    ndc_of(center, c1, n1);
    check_near(n0[0], -n1[0], 2e-4f, "ponto no eixo: NDC x espelhado entre os olhos");
    check_near(n0[1], n1[1], 1e-5f, "NDC y igual nos dois olhos");
    check_near(n0[2], n1[2], 1e-5f, "profundidade igual nos dois olhos");

    // (b) fora do eixo: a diferença entre os olhos é a paralaxe 2·ipd/2 / (z · meia-largura),
    // exatamente a mesma que um par estereoscópico real tem (não é espelho, é deslocamento)
    float side[4] = {0.7f, 0.0f, 4.0f, 1.0f};
    float s0[3], s1[3];
    ndc_of(side, c0, s0);
    ndc_of(side, c1, s1);
    const float half_w = (sym[1] - sym[0]) * 0.5f;
    const float want_parallax = 2.0f * ipd_half / (side[2] * half_w); // olho esquerdo (+h) menos o direito (−h)
    check_near(std::fabs(s0[0] - s1[0]), want_parallax, 2e-4f, "paralaxe fora do eixo = ipd/2 / (z · meia-largura)");
}

/** A matriz de clip preserva a profundidade (não pode mexer no z-buffer). */
void test_depth_preserved()
{
    std::printf("profundidade preservada pela correção\n");
    float mono_fov[4];
    game_fov_tangents(70.0f, 16.0f / 9.0f, mono_fov);
    float eye_fov[4] = {-52.0f * kDegToRad, 40.0f * kDegToRad, -44.0f * kDegToRad, 38.0f * kDegToRad};
    float eye_tan[4];
    for (int k = 0; k < 4; k++) eye_tan[k] = std::tan(eye_fov[k]);

    for (float d : {0.5f, 2.0f, 10.0f, 100.0f}) {
        float p[4] = {0.4f, 0.1f, d, 1.0f};
        float mono[16], clip[4], c[16], out[4];
        float ndc[3] = {0, 0, 0}, ndc_mono[3] = {0, 0, 0};
        eye_view_projection(0.0f, 0, mono_fov, kZn, kZf, mono);
        transform(p, mono, clip);
        to_ndc(clip, ndc_mono);
        eye_clip_matrix(0.0315f, 1, mono_fov, eye_tan, kZn, kZf, c);
        transform(clip, c, out);
        to_ndc(out, ndc);
        check_near(ndc[2], ndc_mono[2], 2e-4f, "z em NDC igual depois da correção");
    }
}

} // namespace

int main()
{
    std::printf("== testes da matemática do estéreo (vr_math.h) ==\n\n");
    test_identity_inverse();
    test_projection_basics();
    test_clip_matrix_matches_direct_path();
    test_parallax_sign_and_size();
    test_identity_when_flat();
    test_mirror_symmetry();
    test_depth_preserved();

    std::printf("\n%d verificações, %d falha(s)\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
