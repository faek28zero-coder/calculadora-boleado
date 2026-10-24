# Os ports nativos de BLACK para PC (e onde o VR entra)

> Pesquisa feita em **24/10/2026**. Links e números mudam; confira sempre o repositório antes de baixar
> qualquer coisa. Este documento **não** distribui, hospeda nem linka diretamente arquivos do jogo.

## O básico

**BLACK** é um FPS da **Criterion Games** publicado pela **EA** em **2006**, lançado para
**PlayStation 2** e **Xbox original**. Nunca teve versão oficial para PC. O que existe desde outubro de
2026 são projetos da comunidade que fazem o código do jogo rodar em Windows sem emulador tradicional.

Existem duas famílias de técnica:

| Técnica | O que faz | Exemplo |
|---|---|---|
| **HLE** (high-level emulation) | Executa o `default.xbe` original (x86) e substitui só o que conversa com o hardware: kernel, Direct3D 8, DirectSound, XInput | `Spark198rus/black-pc` |
| **Recompilação estática** | Traduz o código do Xbox para C++ uma vez, no seu PC, e compila um executável nativo com um renderer moderno | `TGP482/BLACK-XBERecomp`, `portingpete/Black-2006-Recomp` |

Nos dois casos **o jogo não é incluído**: você precisa extrair o disco que já possui.

## Projetos que existem hoje

### 1. `Spark198rus/black-pc` — HLE, o mais "modável"
- **Licença: GPL-2.0** (importante: é o único dos grandes que permite fork legal).
- Linguagem: C++ · ~330 estrelas na data da pesquisa.
- Aceita **NTSC-U** e **PAL Europa**. O README publica os SHA-256 de `default.xbe`:
  - NTSC-U `DF2739C372D254A90AEECCF5971D12097F22D89FBB90DB021FDA96DD4DEB68F6`
  - PAL `67F35121CCF622E7A293C402219596FE6456CA4C3D27E6F8139E449264DA30F0`
- Requisitos: Windows 10/11, GPU com **Direct3D 9 (Shader Model 3)**. No Linux roda via
  **Distrobox + WineHQ 9 + DXVK** (Vulkan 1.1).
- Recursos: qualquer resolução, 16:9, FOV ajustável, 60 fps (ou 30 original), mouse look,
  reverb de sala, gamma do jogo, auto-aim desligado por padrão, filmes puláveis.
- Organização: `game/default.xbe` + `port/black.exe`. O port grava em `port/savedata`,
  `port/black.ini`, `port/black.log` e `port/cache/menus`.

**Por que importa para VR:** sendo GPL-2.0 e com um backend D3D9 isolado, é o candidato natural para
um fork que renderize **duas vistas** (olho esquerdo/direito) e fale com **OpenXR**.

### 2. `TGP482/BLACK-XBERecomp` — recompilação estática, o mais popular
- ~720 estrelas; **sem licença declarada** no repositório (isso impede fork/redistribuição).
- Construído com a ferramenta `TGP482/XboxXBE-Recomp`.
- Instalação: jogue os arquivos (ISO ou extraídos, USA ou Europa) na pasta `assets` e rode `BLACK.exe`.
  O primeiro boot compila o código para o seu PC.
- Overlay de configurações com **F10**.
- Já traz opções de tela que ajudam um mod de VR: **FOV 45–140**, **FOV do viewmodel**,
  resolução de render 20–200%, MSAA/SMAA, draw distance 2×–16×, SSAO/GTAO, HDR10.
- Entrada: teclado e mouse com raw input, **SDL3** para controles e **gyro aiming**
  (DualSense/DualShock 4/Switch Pro) — ou seja, já existe infraestrutura de rastreio de movimento
  sendo usada para mira.
- Mínimo: Windows 10 64-bit, CPU com SSE4.2 + POPCNT, GPU **Direct3D 12**.

### 3. `portingpete/Black-2006-Recomp`
Recompilação experimental com **renderer Direct3D 11**, launcher próprio, áudio nativo e suporte a
XInput. Roda no Linux via **Wine/Proton**. Distribui um "starter" (`BLACK-0.1.0-alpha.5-starter.zip`)
que **compila o jogo na sua máquina** a partir do seu disco — o que evita redistribuir código.

### 4. Outros que apareceram na pesquisa
| Repositório | Nota |
|---|---|
| `euspring1/BlackDefinitiveHDPC` | HLE com até 240 fps, texturas HD, inglês e **português do Brasil** |
| `LucasRamos96/Black-recomp-PTBR` | Caminho **PS2** (PAL `SLES-54030`) via `ps2-web-recomp` (MIT), com tradução PT-BR |
| `Nubi-Prey/black-pc` | Espelho do `black-pc` com build Linux |
| `PauloVx/Black-2006-Recomp` | Espelho/fork do projeto de recompilação |

## O que isso significa para o VR

1. **Nenhum dos ports tem VR.** Todos são flatscreen com mouse/controle.
2. O caminho de menor atrito técnico é o **`black-pc` (GPL-2.0)**: dá para forkar, ler o código do
   renderer e injetar estereoscopia sem violar licença.
3. Os recompiladores (XBERecomp/Black-2006-Recomp) **não são forkáveis** (sem licença) e misturam
   código do jogo com o executável — o que torna um mod de VR neles bem mais arriscado e limitado.
4. **Gyro aiming já existente** é a prova de que a engine aceita ser dirigida por dados de
   rastreamento externos; o passo seguinte é trocar o "mover a mira" por "mover a câmera".

O plano detalhado está em [`ROADMAP-VR.md`](./ROADMAP-VR.md).

## Como jogar BLACK no PC hoje (resumo honesto)

1. Você precisa de uma **cópia legal** do jogo (Xbox NTSC-U/PAL, ou PS2 PAL no projeto do LucasRamos).
2. Extraia o disco (`extract-xiso` para ISOs de Xbox) e confira o hash do `default.xbe`.
3. Baixe o release do projeto escolhido, coloque a pasta do port **ao lado** de `game/` (ou os arquivos
   em `assets/`, conforme o projeto).
4. Rode com o seu disco no lugar; salve os arquivos em local próprio.

**Não** baixe "BLACK PC grátis" de site de terceiros: os repositórios oficiais deixam claro que não
distribuem dados do jogo, e pacotes prontos fora deles são redistribuição ilegal.

## Em dispositivos móveis

BLACK roda em Android por emulação de PS2 (AetherSX2/NetherSX2 com a sua ISO). No **Quest** é possível
rodar o emulador por sideload, mas o desempenho e a ergonomia de controles não se comparam ao port
nativo no PC. Se o objetivo é VR, o caminho é PC + headset via Link/Air Link, não o emulador no
headset.

## Cronologia (contexto)

- **2006** — lançamento em PS2 e Xbox; nenhuma versão de PC.
- **Set/2026** — TGP482 publica a ferramenta `XboxXBE-Recomp`.
- **03/10/2026** — criação do repositório `BLACK-XBERecomp`; **14/10** o `black-pc` v0.2.0;
  **15/10** o V1 do XBERecomp; **18/10** o V2.
- **24/10/2026** — este documento é escrito; nenhum dos projetos menciona VR no roadmap público.

## Créditos

Todo o mérito do trabalho pesado é dos autores citados: Spark198rus, TGP482, portingpete, euspring1,
LucasRamos96, snuri00 (`ps2-web-recomp`) e a comunidade do XboxDev (`extract-xiso`). Este repositório
não é um port: é a camada de VR que se acopla ao `black-pc` (ver `IMPLEMENTACAO.md`).
