// ============================================================
//  BLACK VR — rastro de execução (vr-trace.txt)
//
//  Por que existe: com --vr-selftest o programa morre antes de qualquer linha do
//  black.log (o banner do port é escrito depois do teste), então um crash deixa o
//  log vazio e não diz nada. Este arquivo escreve e DESCARGA no disco a cada passo,
//  e instala um capturador de falha de acesso que anota o endereço que estourou e
//  em qual DLL ele está — ou seja, diz se o problema é nosso, do conector ou do
//  runtime do headset.
// ============================================================
#pragma once

namespace vr {
namespace trace {

/** Abre vr-trace.txt ao lado do blackrt.dll e instala o capturador de falha. */
void init();

/** Escreve uma linha e descarrega no disco na hora. */
void line(const char *fmt, ...);

/**
 * Anota no rastro as DLLs de VR/carga gráfica já carregadas no processo.
 * Serve para saber, sem depender do crash, qual runtime e qual driver entraram.
 */
void modules();

} // namespace trace
} // namespace vr
