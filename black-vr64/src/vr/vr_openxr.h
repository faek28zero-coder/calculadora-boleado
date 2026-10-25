// BLACK VR - liga o port de 64 bits (Direct3D 12) ao oculos pelo OpenXR.
//
// Como funciona (primeira etapa, "tela no oculos"):
//   o jogo desenha igual a sempre, na janela dele. Antes de entregar o quadro,
//   esta parte copia o quadro para a imagem da sessao do oculos (uma por olho)
//   e entrega. Os dois olhos recebem a mesma imagem, com campo de visao fixo.
//   Serve para provar o caminho inteiro: sessao + troca de imagens + quadro.
//
// Etapa seguinte (estereo de verdade): desenhar duas vezes, uma por olho, com a
//   camera deslocada. O ponto de entrada continua sendo a funcao GravarCopia.
#pragma once

#include <cstdint>

namespace vr {

// Le a variavel de ambiente BLACK_VR (=1/sim/on); sem ela, tudo fica desligado
void IniciarSePedido();

// Ligada? (sem oculos, sem extensao D3D12 ou sem openxr_loader.dll: desligada)
bool Ativo();

// ANTES do port entregar o comando para a placa: monta a sessao na primeira vez,
// pede as imagens do oculos, copia o quadro do jogo para elas e deixa gravado.
void GravarCopia(void *device, void *fila, void *lista, void *backbuffer, uint32_t largura, uint32_t altura,
                 int formato_dxgi);

// DEPOIS de o port entregar o comando para a placa: devolve as imagens ao oculos
// e entrega o quadro (xrEndFrame). Aqui tambem anda o estado da sessao.
void DepoisDoEnvio();

}  // namespace vr
