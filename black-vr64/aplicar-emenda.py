#!/usr/bin/env python3
# Poe a chamada da VR dentro do src/d3d/d3d12.cpp do port (BLACK-XBERecomp).
#
# Por que um script e nao um .patch: o port recebe commits quase todo dia e a
# emenda de contexto quebraria. Aqui procuramos por trechos estaveis do codigo
# (o fim do Present) e inserimos por ancora, com mensagem clara se mudar.
#
# Uso:  python3 aplicar-emenda.py <pasta-do-port>
import io
import sys

INCLUDE_ANCORA = '#include "gpu/vertex.h"'
INCLUDE_NOVO = '#include "gpu/vertex.h"\n#include "vr/vr_openxr.h"'

ANTES_ANCORA = "    uint32_t bi = swap_->GetCurrentBackBufferIndex();\n    DrawFrame(t, bi);\n"

CALL_NOVO = """    uint32_t bi = swap_->GetCurrentBackBufferIndex();
    DrawFrame(t, bi);
    // BLACK VR: copia o quadro que acabou de ser desenhado para as imagens do oculos.
    // Com BLACK_VR desligado, GravarCopia volta na hora e nada muda.
    {
        DXGI_SWAP_CHAIN_DESC1 vrd{};
        if (SUCCEEDED(swap_->GetDesc1(&vrd))) {
            vr::GravarCopia(dev_.Get(), queue_.Get(), cl_.Get(), bb_[bi].Get(), vrd.Width, vrd.Height,
                            static_cast<int>(vrd.Format));
        }
    }
"""

DEPOIS_ANCORA = "    DrawFrame(t, bi);\n    Submit(false);\n"
DEPOIS_NOVO = """    DrawFrame(t, bi);
    {
        DXGI_SWAP_CHAIN_DESC1 vrd{};
        if (SUCCEEDED(swap_->GetDesc1(&vrd))) {
            vr::GravarCopia(dev_.Get(), queue_.Get(), cl_.Get(), bb_[bi].Get(), vrd.Width, vrd.Height,
                            static_cast<int>(vrd.Format));
        }
    }
    Submit(false);
    // BLACK VR: agora que os comandos foram enviados, entrega o quadro ao oculos
    vr::DepoisDoEnvio();
"""


def main() -> int:
    if len(sys.argv) < 2:
        print("uso: aplicar-emenda.py <pasta-do-port>")
        return 2
    caminho = f"{sys.argv[1].rstrip('/')}/src/d3d/d3d12.cpp"
    try:
        with io.open(caminho, encoding="utf-8") as f:
            texto = f.read()
    except OSError as e:
        print(f"ERRO: nao abri {caminho}: {e}")
        return 1

    if "#include \"vr/vr_openxr.h\"" in texto:
        print("ja estava emendado")
        return 0

    if INCLUDE_ANCORA not in texto:
        print(f"ERRO: nao achei o include {INCLUDE_ANCORA!r} em d3d12.cpp")
        return 1
    texto = texto.replace(INCLUDE_ANCORA, INCLUDE_NOVO, 1)

    # o Present: uma chamada antes do Submit (copia) e uma depois (entrega)
    if DEPOIS_ANCORA not in texto:
        print("ERRO: nao achei o trecho 'DrawFrame(t, bi); Submit(false);' em d3d12.cpp")
        return 1
    texto = texto.replace(DEPOIS_ANCORA, DEPOIS_NOVO, 1)

    if "vr::IniciarSePedido();" not in texto:
        texto = texto.replace(
            "    uint32_t bi = swap_->GetCurrentBackBufferIndex();",
            "    vr::IniciarSePedido();\n    uint32_t bi = swap_->GetCurrentBackBufferIndex();",
            1,
        )

    with io.open(caminho, "w", encoding="utf-8", newline="") as f:
        f.write(texto)
    print("emenda aplicada em src/d3d/d3d12.cpp")
    return 0


if __name__ == "__main__":
    sys.exit(main())
