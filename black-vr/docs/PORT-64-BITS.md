# Mudança de base: port de 64 bits (BLACK-XBERecomp)

Decisão do dono do projeto (25/10/2026): a VR vai ser feita no port **de 64 bits**,
não no `black-pc` de 32 bits. Motivo: em 64 bits a sessão do óculos **abre**
(`SESSAO CRIADA` no modo 1 da sonda) — o runtime da Meta estoura só no lado de 32 bits.

## O que existe hoje (verificado em 25/10/2026)

| projeto | o que é | base | desenho | roda onde |
|---|---|---|---|---|
| `Spark198rus/black-pc` | port "clássico" | HLE + código x86 do XBE rodando nativo | Direct3D 9 | **só 32 bits** (o CMake recusa outro) |
| `TGP482/BLACK-XBERecomp` | recompilação estática | x86 → C++ → clang | **Direct3D 12** | PC **64 bits**, Windows 10+ |
| `RanggaCasper/Black-Android` | port de celular | mesma recompilação | Vulkan + SDL3 | Android arm64 |

- O port de 64 bits tem release pronta: `TGP482/BLACK-XBERecomp` release **V2**
  (`BLACK-XBERecomp.zip`, 70,9 MB). O jogo é compilado **na primeira execução**, na
  própria máquina do jogador — não precisa de Visual Studio.
- Fonte: 63 arquivos C++ em `src/`, desenho em `src/d3d/d3d12.cpp` (180 KB),
  backend com `Present(const Surface& front)` (`D3D12Backend::Present`, linha ~3797)
  e `CreateSwapChainForHwnd` (linha ~1253). **Este é o ponto de encaixe da VR.**
- O runtime do óculos do usuário oferece `XR_KHR_D3D12_enable` (aparece na lista de
  36 extensões do relatório de 25/10), então a sessão pode usar D3D12 — o mesmo
  backend do port.

## Requisitos que o usuário já tem

- Quest 2 no cabo, Meta Horizon Link em `Z:\Meta Horizon\Support\oculus-runtime\`
  (`oculus_openxr_64.json` → `LibOVRRTImpl64_1.dll`).
- `oculus_openxr_64.json` responde: instância, sistema (Quest2), D3D11 e **sessão**
  funcionam em 64 bits.
- GTX 1650 (Direct3D 12 ok), Windows 10.
- Jogo: `default.xbe` (release PT-BR/Europa) na pasta dele.

## Plano

1. **Validar a base** (usuário, 5 min): baixar a release V2, pôr os arquivos do jogo
   em `assets`, rodar `BLACK.exe` no monitor. Se o jogo abrir, a base está boa.
2. **Primeira VR = tela no óculos** (modo cinema): em `Present`, quando `--vr`
   estiver ligado, usar o quadro já desenhado como textura de um plano na frente do
   jogador, numa sessão OpenXR **D3D12**. Isso já dá imagem no óculos, sem mexer na
   câmera do jogo.
3. **VR de verdade (estéreo)**: desenhar duas vezes, uma por olho, com as matrizes
   de visão/projeção do jogo trocadas por olho (o port já mexe em FOV e viewmodel),
   mais o `xrLocateViews`.

## Build automático: o que falta

- O CMake deste port exige `assets/default.xbe` na hora de **compilar**
  (`add_xbe_image(...)`: `DEPENDS`, `file(SHA1 ...)`). Sem o XBE não sai binário, e
  o XBE não entra em repositório (é dado do jogo).
- Saídas possíveis: (a) o XBE do usuário é baixado no build (link do Drive dele) e
  o binário vai só pra ele; (b) o build roda na máquina dele.
- A release oficial já resolve o primeiro teste sem build nenhum.
