// ============================================================
//  BLACK VR — módulo de realidade virtual para o port nativo de BLACK
//  (Spark198rus/black-pc, GPL-2.0)
//
//  Este cabeçalho é a única coisa que o port precisa conhecer do módulo. Tudo o que
//  fala com o runtime OpenXR, com o Direct3D 11 do headset e com o Direct3D 9 do jogo
//  fica atrás desta API.
//
//  Ciclo de vida
//  -------------
//    vr::install()      uma vez, depois de mapear o XBE (patches de código do jogo)
//    vr::start()        quando o jogo começa (cria instância/sessão/swapchain)
//    vr::frame_begin()  a cada frame, antes de desenhar
//    vr::frame_end()    a cada frame, no lugar do present (faz o render do olho direito
//                       e envia os dois olhos ao runtime)
//    vr::shutdown()     no exit
//
//  Se o runtime não existir, não houver headset ou nada der certo, `available()`
//  fica falso e o jogo segue em tela plana exatamente como antes.
// ============================================================
#pragma once

#include <d3d9.h>
#include <cstdint>

namespace vr {

// ---------------------------------------------------------------- estado

/** O usuário pediu VR (black.ini `vr=1` / `--vr`) e o runtime respondeu. */
bool enabled();

/** A sessão está rodando e focada: é ela que troca o present pelo envio ao headset. */
bool active();

/**
 * O frame do runtime está aberto e esperando as duas imagens (entre frame_begin e frame_end).
 * É a condição para o Swap entregar os olhos: `active()` só diz que a sessão está de pé.
 */
bool frame_ready();

/** Última mensagem de erro legível (para o log e para a tela de configurações). */
const char *last_error();

// ---------------------------------------------------------------- ciclo de vida

/** Instala os patches no código do jogo (view matrix / eye height). Chamar depois de image_xbe(). */
void install();

/** Cria instância, sistema, sessão, swapchain e ações. Devolve false e loga o motivo. */
bool start(const void *hwnd);

/**
 * Início preguiçoso para o port: tenta `start()` uma única vez (guardado por `enabled()`).
 * Chamado no Swap; devolve true quando a sessão já está de pé.
 */
bool start_once(const void *hwnd);

void shutdown();

// ---------------------------------------------------------------- frame

/** Espera o frame do runtime, localiza as views (pose + FOV por olho) e adianta a pose. */
void frame_begin();

/** Envia os dois olhos ao runtime. O conteúdo de cada olho vem de submit_eye(). */
void frame_end();

/**
 * Envia a imagem do jogo (textura do render target principal, A8R8G8B8, `w` x `h`) para o
 * olho `eye` (0 esquerdo, 1 direito). É este o caminho usado pelo port: a textura do jogo
 * tem o aspecto do jogo (4:3 ou 16:9 anamórfico), sem as barras da janela.
 */
bool submit_frame(IDirect3DTexture9 *frame, int eye);

/** Alternativa de diagnóstico: lê o back buffer da janela (com barras) e envia ao olho. */
bool submit_eye(IDirect3DDevice9 *dev, int eye);

/**
 * O jogo terminou de desenhar o frame: fecha a gravação, reexecuta os draws com a matriz do olho
 * direito e envia ao runtime. `frame` é a textura do render target principal do jogo (a mesma que
 * foi para o olho esquerdo em `submit_frame`). Implementado em vr.cpp + vr_frame.h.
 */
void render_second_eye(IDirect3DDevice9 *dev, IDirect3DTexture9 *frame);

// ---------------------------------------------------------------- pose

/**
 * Recalcula as matrizes de correção dos dois olhos para este frame.
 * `game_fov_deg` é o FOV horizontal que o jogo usou (o port sabe: PC SETTINGS/zoom) e
 * `aspect` é largura/altura do render target. Chamar uma vez por frame, antes de desenhar.
 */
void pose_frame_update(float game_fov_deg, float aspect);

/**
 * Matriz (16 floats, convenção de linha) que leva o clip space mono do jogo ao clip space
 * daquele olho: (V_mono·P_mono)⁻¹ · (V_olho·P_olho). Vai para a constante c201 do vertex
 * shader traduzido, então nenhum vértice do jogo precisa ser tocado.
 */
const float *clip_matrix(int eye);

/** Olho que os próximos draws representam (0 esquerdo, 1 direito). Padrão: 0. */
void set_eye(int eye);
int eye();

/** Gira o corpo do jogador (snap turn de conforto), em graus. */
void snap_turn(float degrees);

/**
 * FOV horizontal do jogo neste frame, em graus (o port sabe: é o `fov` do PC SETTINGS / zoom).
 * Este é o valor que a correção estéreo assume ao inverter a projeção mono.
 */
float game_fov();

/**
 * FOV horizontal que o headset cobre (maior dos dois olhos), em graus. Serve para o port
 * escolher o FOV do jogo: em VR os dois devem ser iguais, senão as bordas do quadro do jogo
 * aparecem dentro do campo de visão do headset.
 */
float headset_fov();

/** Contador que muda a cada `frame_begin()`: diz ao port quando reenviar a constante c201. */
unsigned pose_serial();

/** Largura/altura atuais do render target do jogo em pixels. */
void game_render_size(int *w, int *h);

// ---------------------------------------------------------------- configuração

/** Valores da seção `[vr]` do black.ini (preenchidos pelo port em settings.cpp). */
struct Settings {
    float ipd_mm = 63.0f;
    float move_scale = 1.0f;
    float snap_turn_deg = 0.0f; // 0 = desligado
    float game_fov = 0.0f;      // 0 = usar o FOV do headset
    uint16_t reload_button = 0x4000;
    bool input = true;
    bool hud_in_headset = true;
    int fps = 60;
};

/** Aplica a configuração. Chamar antes de `start()`; pode ser chamada de novo em tempo real. */
void configure(const Settings &settings);

// ---------------------------------------------------------------- entrada

struct Pad {
    int16_t lx, ly, rx, ry; // -32768..32767
    uint16_t buttons;       // máscara Xbox: 0x1000 A, 0x2000 B, 0x4000 X, 0x8000 Y, ...
    uint8_t lt, rt;         // gatilhos
};

/** Estado dos controles VR traduzido para o pad Xbox que o jogo lê (xinput.cpp). */
void pad_merge(Pad *pad);

/** Há headset com controles rastreados? (para escolher o layout de HUD no headset) */
bool has_tracked_controllers();

/**
 * Teste do headset sem o jogo: desenha um cenário de calibração (grade, escala de IPD,
 * alvo central) direto na swapchain. `--vr-selftest` no loader chama isto e sai.
 * Devolve false se o VR não estiver disponível.
 */
bool selftest(const void *hwnd, int seconds);

} // namespace vr
