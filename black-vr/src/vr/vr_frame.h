// ============================================================
//  BLACK VR — vr_frame.h
//
//  Contrato entre o módulo de VR e o `draw.cpp` do port.
//
//  O porquê: para mostrar estéreo, o frame que o jogo desenhou (já com a cabeça aplicada na
//  view e com o clip do olho esquerdo) precisa ser reexecutado uma vez com o clip do olho
//  direito. O único lugar que sabe reexecutar um draw do port — porque tem `g_st`,
//  `begin_draw()`, o estado enviado ao D3D9 e a geometria final — é o próprio draw.cpp.
//  Por isso o gravador e o replay vivem lá (patch 0001) e este cabeçalho é o contrato.
//
//  Fluxo dentro de um frame (ver D3DDevice_Swap, patch 0003):
//      jogo desenha            -> capture_draw() por draw (chamado pelo draw.cpp)
//      Swap: submit_frame(0)   -> o quadro pronto vai para o olho esquerdo
//            capture_end()     -> fecha a gravação
//            set_eye(1) + replay() + submit_frame(1)
//                              -> o mesmo frame, com o clip do olho direito
//            frame_end()       -> xrEndFrame com os dois olhos
//            capture_begin()   -> grava o próximo frame
//
//  O replay reaplica estado e constantes por draw (deduplicados na gravação), então ele não
//  depende de nada que o jogo tenha deixado no device.
// ============================================================
#pragma once

#include <cstdint>

namespace vr_draw {

/** Esquece o frame gravado e começa a gravar do zero. */
void capture_begin();

/** O frame acabou de ser desenhado: nada mais será gravado até o próximo capture_begin(). */
void capture_end();

/**
 * Grava um draw para o replay do segundo olho.
 *
 * Os argumentos são exatamente os que foram para o D3D9, depois das conversões do port:
 *   `type`   D3DPRIMITIVETYPE já convertido (triangle list, strip, fan)
 *   `prims`  número de primitivas
 *   `minv`   MinVertexIndex (o port rebaseia os índices, então é 0 nas duas chamadas)
 *   `nverts` vértices referenciados
 *   `indices` índices de 16 bits, ou nullptr para DrawPrimitiveUP
 *   `verts`  geometria que o device leu
 *   `stride` tamanho de um vértice em bytes
 *
 * A implementação copia a geometria e, na primeira vez que o estado muda, tira um retrato de
 * `g_st` + render states + texture stages + constantes do vertex shader.
 */
void capture_draw(uint32_t type, uint32_t prims, int32_t minv, uint32_t nverts, const uint16_t *indices,
                  const void *verts, uint32_t stride);

/** Desenha de novo o frame gravado, com o clip do olho atual (vr::set_eye antes). */
void replay();

/** Estatísticas para o log (draws gravados, bytes, tempo do replay). */
struct Stats {
    uint32_t draws;
    uint32_t bytes;
    float last_replay_ms;
};
Stats stats();

} // namespace vr_draw
