// ============================================================
//  Teste de LIGAÇÃO do módulo de VR.
//
//  A checagem de sintaxe (syntax_check.sh) compila cada .cpp sozinho: ela não vê
//  função declarada no cabeçalho e nunca definida, nem assinatura trocada entre
//  arquivos. Foi assim que passou, meses de teste verdes, um
//  `vr::set_game_render_size()` que o port chamava e o cabeçalho não declarava —
//  o MSVC só reclamou no build de verdade.
//
//  Este arquivo pega o endereço de CADA função pública de vr.h e é ligado junto
//  com todos os src/vr/*.cpp: símbolo faltando ou assinatura diferente vira erro
//  de ligação aqui, no Linux, antes de qualquer build no Windows.
// ============================================================
#include <cstdio>

#include "vr/vr.h"

namespace {

// Uma referência por função pública: se alguma desaparecer, o link quebra.
volatile void *keep[] = {
    reinterpret_cast<void *>(&vr::enabled),
    reinterpret_cast<void *>(&vr::active),
    reinterpret_cast<void *>(&vr::frame_ready),
    reinterpret_cast<void *>(&vr::last_error),
    reinterpret_cast<void *>(&vr::install),
    reinterpret_cast<void *>(&vr::start),
    reinterpret_cast<void *>(&vr::start_once),
    reinterpret_cast<void *>(&vr::shutdown),
    reinterpret_cast<void *>(&vr::frame_begin),
    reinterpret_cast<void *>(&vr::frame_end),
    reinterpret_cast<void *>(&vr::submit_frame),
    reinterpret_cast<void *>(&vr::submit_eye),
    reinterpret_cast<void *>(&vr::render_second_eye),
    reinterpret_cast<void *>(&vr::pose_frame_update),
    reinterpret_cast<void *>(&vr::clip_matrix),
    reinterpret_cast<void *>(&vr::set_eye),
    reinterpret_cast<void *>(&vr::eye),
    reinterpret_cast<void *>(&vr::snap_turn),
    reinterpret_cast<void *>(&vr::game_fov),
    reinterpret_cast<void *>(&vr::headset_fov),
    reinterpret_cast<void *>(&vr::pose_serial),
    reinterpret_cast<void *>(&vr::game_render_size),
    reinterpret_cast<void *>(&vr::set_game_render_size), // o que faltava em 24/10
    reinterpret_cast<void *>(&vr::configure),
    reinterpret_cast<void *>(&vr::pad_merge),
    reinterpret_cast<void *>(&vr::has_tracked_controllers),
    reinterpret_cast<void *>(&vr::selftest),
};

} // namespace

int main()
{
    // Confere também o contrato de tipos nas assinaturas que o port usa direto.
    void (*set_size)(int, int) = &vr::set_game_render_size;
    void (*get_size)(int *, int *) = &vr::game_render_size;
    const float *(*clip)(int) = &vr::clip_matrix;
    void (*pose)(float, float) = &vr::pose_frame_update;
    void (*pad)(vr::Pad *) = &vr::pad_merge;
    (void)set_size; (void)get_size; (void)clip; (void)pose; (void)pad;

    int w = 0, h = 0;
    vr::set_game_render_size(640, 480);
    vr::game_render_size(&w, &h);
    if (w != 640 || h != 480) {
        std::printf("  set_game_render_size nao guardou o tamanho (%d x %d)\n", w, h);
        return 1;
    }
    std::printf("  %zu funcoes publicas ligadas, round-trip de tamanho ok\n", sizeof(keep) / sizeof(keep[0]));
    return 0;
}
