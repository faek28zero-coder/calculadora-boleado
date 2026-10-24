# Roadmap: levar BLACK de verdade para VR

Este documento separa **o que já está feito aqui** (protótipo jogável) do **que falta** para colocar
o BLACK real em um headset. É um plano técnico, não uma promessa.

## Estado atual

| Etapa | Status |
|---|---|
| Protótipo WebXR jogável (three.js) | ✅ feito |
| Variante declarativa (A-Frame) | ✅ feito |
| Scripts Unity/OpenXR para build nativo | ✅ feito |
| Pesquisa dos ports de PC | ✅ feito (`PORTS-DO-BLACK.md`) |
| Mod de VR no BLACK real | ⛔ não iniciado |

O protótipo não é "o jogo": é o **laboratório de escala e conforto**. Antes de mexer em um renderer
de 2006, você precisa saber se o seu estômago aceita locomoção contínua a 3,4 m/s, se o tamanho dos
corredores funciona em metros reais e se o feedback de tiro fica bom com a arma na mão.

## Fase 1 — Fundação (feito)

- Escala 1:1 em metros, olho a 1,68 m.
- Locomoção contínua + **snap turn** de 30° (menos náusea que giro suave).
- Tiro hitscan com confirmação imediata (hitmarker + som).
- Zona crítica central (2,5×) para dar profundidade de habilidade.
- Round curto de 75 s: partidas curtas = menos tempo de exposição = menos enjoo no começo.

## Fase 2 — Validação com o jogo real (sem VR)

Objetivo: **medir** o que o BLACK real faz, sem alterar nada.

1. Rode o `black-pc` com mouse e teclado.
2. Capture a pose da câmera e a posição do jogador por alguns minutos (overlay ou log).
3. Meça: velocidade de caminhada, altura dos olhos, FOV horizontal em uso, amplitude de recuo da arma,
   tempo de recarga, sensibilidade do stick.
4. Reproduza esses números no protótipo web (`player.js` e `weapon.js`). Se o protótipo ficar
   "errado" em relação ao original, ajuste antes de pensar em mod.

**Entregável:** tabela `docs/metricas-do-black.md` com os valores medidos.

## Fase 3 — Fork do renderer (`black-pc`, GPL-2.0)

Este é o núcleo do trabalho. O `black-pc` substitui o Direct3D 8 por um backend **D3D9**, o que dá um
ponto de estrangulamento único e limpo para interceptar a renderização.

### 3.1 Estereoscopia
- Criar dois render targets (olho E/D) por frame.
- Renderizar a cena duas vezes, com `viewMatrix` deslocada por **IPD** (~63 mm) perpendicular ao
  forward e `projectionMatrix` com **off-axis frustum** (obrigatório: sem isso os olhos convergem
  errado e a escala quebra).
- Enviar as duas texturas ao runtime OpenXR como `XrSwapchainImageD3D11` (ou D3D12, conforme o
  backend escolhido).
- **UI/HUD**: renderizar em cada olho com profundidade fixa (1,5–2 m) ou como camada de overlay.

### 3.2 Pose do headset
- Substituir a matriz de câmera do jogo pela pose do HMD **sem** mexer no "corpo" do jogador:
  `finalView = inverse(hmdPose) * inverse(bodyYaw)`.
- **Pitch e roll vêm do headset**; yaw do jogo continua no stick (com snap turn) — é assim que se evita
  brigar com o rastreamento.
- Câmeras de cutscene e de morte precisam de um caminho separado (câmera "cinema" projetada em um plano
  virtual, como fazem os mods de UEVR/RE4).

### 3.3 Arma e mãos
- O BLACK já tem viewmodel de arma com ADS (aim down sights) e recuo. Em VR:
  - viewmodel **parentado ao controlador**, com FOV de viewmodel separado (o XBERecomp já expõe esse
    parâmetro — bom sinal de que é viável).
  - recuo aplicado como deslocamento local da arma, não como kick de câmera (kick de câmera em VR
    causa náusea imediata).
  - mira: alinhar o eixo do cano com a direção do controlador e **manter a mira do jogo** como
    retículo suave, não como precisão absoluta.
- Recarga: mapear os estados da animação de recarga do jogo para eventos (soltar carregador,
  encaixar, puxar ferrolho) e deixar o gesto do controle disparar o início — sem exigir gesto físico
  completo no começo.

### 3.4 Conforto (não negociável)
| Recurso | Por quê |
|---|---|
| Snap turn 30°/45° | giro suave é a maior fonte de náusea |
| Vignette dinâmica ao mover | reduz fluxo periférico |
| Teleporte opcional | para quem não tolera locomoção contínua |
| Modo sentado | altura do rig ajustável, sem exigir ficar em pé |
| Travamento de horizonte | evita rolagem indesejada em rampas |
| Sem screen shake | explosões do BLACK sacodem a câmera; em VR isso machuca |
| 90 fps estáveis | frame drop em VR é pior que gráfico feio |

### 3.5 Entrada
- OpenXR actions: `trigger` (atirar), `grip` (recarregar), `stick` (mover), `stick click` (agachar).
- Háptica por evento (tiro, impacto recebido, recarga).
- Fallback total para gamepad: o jogo precisa continuar jogável sem VR.

## Fase 4 — Distribuição legal

O que **pode** ser distribuído:
- Código do fork (obrigação do GPL-2.0: publicar o fonte).
- Instruções e scripts de build.

O que **não pode**:
- `default.xbe`, texturas, sons, vídeos, mapas do jogo.
- Binários que embutam código do jogo.
- Builds "prontos para jogar".

Modelo recomendado: **patch + build local**, como o `Black-2006-Recomp` já faz (o usuário aponta o
launcher para o próprio disco e o jogo é compilado na máquina dele).

## Fase 5 — Extras

- **Hand tracking** no Quest: pegar a arma com a mão, sem controle.
- **Modo range**: reaproveitar este protótipo como "campo de treino" importado para dentro do jogo.
- **Passthrough** para pausas: ver a sala real sem tirar o headset.
- **Legendas e HUD diegético** (relógio de pulso, contador de munição na arma).

## Riscos conhecidos

| Risco | Impacto | Mitigação |
|---|---|---|
| Engine de 2006 com pré-cálculos de culling por câmera única | objetos somem na visão periférica | alargar frustum de culling por olho + folga de margem |
| Shaders com posição de tela embutida (bloom, blur, distorção) | efeitos quebram em estéreo | aplicar pós-processamento só no olho esquerdo e reprojetar, ou desativar |
| Cutscenes com câmera scriptada | náusea/desorientação | renderizar em tela virtual com ambiente escurecido |
| Sem código-fonte do jogo | não dá para "só trocar a câmera" | trabalhar no runtime (HLE) e não no jogo |
| Licença do XBERecomp ausente | fork inviável | usar o `black-pc` (GPL-2.0) como base |

## Ordem prática de ataque

1. Fork do `black-pc` rodando com o seu disco. (1 dia)
2. Capturar e fixar a matriz de câmera por um frame (log). (1–2 dias)
3. Renderizar o mesmo frame duas vezes com offset de IPD **sem** OpenXR, lado a lado. (2–4 dias)
4. Trocar lado a lado por OpenXR + headset. (3–5 dias)
5. Pose do headset no lugar do mouse look. (2–3 dias)
6. Arma no controle, recarga por botão. (1 semana)
7. Conforto, tuning e testes com pessoas diferentes. (contínuo)

Vaivém esperado: é um projeto de semanas a meses, não de um fim de semana. O protótipo deste repo
serve para você chegar na etapa 4 já sabendo como o jogo deve **parecer** em VR.
