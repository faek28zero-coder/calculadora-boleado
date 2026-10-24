# Arquitetura

## Visão geral

O projeto existe em três implementações independentes da mesma ideia:

```
                 ┌─────────────────────────────┐
                 │  Conceito: "Protocolo Zero" │
                 │  round de 75 s · alvos pop-up│
                 │  combo · zona crítica        │
                 └──────────────┬──────────────┘
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
  webxr-three/            webxr-aframe/            unity/
  three.js + WebXR        A-Frame 1.7              C# + OpenXR
  (principal)             (declarativo)            (nativo)
```

Cada variante é autossuficiente: você pode apagar as outras duas e continuar. A regra comum é
**zero assets externos** — geometria, texturas e som são gerados em runtime.

## Variante 1 — three.js (principal)

```
webxr-three/
├── index.html          menu, HUD (DOM), tela de resultados, importmap
├── styles.css          HUD/menu em CSS (fora do canvas → nítido em qualquer DPI)
└── src/
    ├── main.js         orquestrador: cena, estado, loop, tiro, pontuação
    ├── world.js        arena procedural + colliders + iluminação + poeira
    ├── targets.js      alvos pop-up, spawn, dificuldade, zona crítica
    ├── weapon.js       fuzil procedural, recuo, recarga, flash, muzzle
    ├── player.js       rig, desktop (WASD/mouse), XR (stick + snap turn)
    ├── effects.js      faíscas, decais de bala, traçantes (com pools)
    ├── textures.js     texturas em canvas (concreto, metal, perigo, alvo…)
    ├── audio.js        WebAudio sintetizado (tiro, impacto, recarga, alvo)
    └── hud.js          ponte entre estado do jogo e elementos do HUD
```

### Fluxo do loop

```
setAnimationLoop
  ├─ dt ← clock.getDelta() (limitado a 50 ms)
  ├─ estado do round .......... tempo, dificuldade, combo, recarga
  ├─ targets.update ............ sobe/desce alvos, agenda novos
  ├─ effects.update ............ faíscas, traçantes
  ├─ world.update .............. poeira, luz de emergência
  ├─ player.update ............. locomoção, colisão, animação da arma
  └─ renderer.render(scene, camera)
```

### Decisões que valem explicar

**HUD em DOM, não em canvas.** Em desktop, texto em DOM é nítido e acessível de graça. Dentro do
headset o HUD de DOM desaparece (`body.in-xr`) — ali a leitura vem da própria mira e do feedback
sonoro, evitando um plano flutuante colado na cara.

**Tiro por raycast, não por projétil.** Um FPS de precisão precisa de resposta imediata. O tiro é
resolvido com `Raycaster` a partir do olho (desktop) ou do controlador (XR), com decal + faíscas
no primeiro impacto. O traçante é puramente visual e dura 70 ms.

**Mira física no headset.** No VR não existe "crosshair": você aponta a arma com a mão. O raio sai
do controlador que segura a arma (`player.getAimRay()`), e a arma é reparentada para a mão esquerda
se o primeiro controle conectado for canhoto (`applyHandedness()`).

**Alvos = placas com material clonado.** O material é clonado por alvo para permitir emissivo
individual e para que esconder a placa (`plate.visible = false`) ao destruir não apague os outros.

**Texturas procedurais.** `textures.js` desenha concreto, metal, listras de perigo, piso xadrez,
silhueta de alvo e buraco de bala em `<canvas>`. Custo: ~30 ms no primeiro frame, zero bytes no repo,
zero questão de licenciamento.

**Áudio sintetizado.** `audio.js` monta o tiro com ruído + oscilador descendente + cauda filtrada.
Recarga são dois cliques metálicos; acerto é um tom em duas vozes. `AudioContext` só nasce no
primeiro clique (política dos navegadores).

**Dificuldade por curva.** `progress = 1 - timeLeft / 75` alimenta ao mesmo tempo o número de alvos
simultâneos (3 → 5) e o intervalo de spawn (1,15 s → 0,32 s). Uma variável, duas consequências.

## Variante 2 — A-Frame

Componentes registrados em um único `index.html`:

| Componente | Responsabilidade |
|---|---|
| `checker-floor` | textura de piso gerada em canvas |
| `canvas-label` | texto nítido em `<canvas>` (sem depender de fonte remota) |
| `vr-locomotion` | WASD no desktop + analógicos e snap turn no XR |
| `popup-target` | máquina de estados do alvo (down → rising → up → falling/dead) |
| `target-range` | quem sobe, quando, e com que dificuldade |
| `shoot` | `triggerdown` (controle) e clique (cursor do desktop) |
| `game-state` | round, pontos, combo, munição, HUD e resultados |

Por que A-Frame mesmo tendo three.js? Porque o cenário é **declarativo**: dá para reposicionar um
alvo editando uma linha de atributo (`position="-5.6 2.6 -9"`), sem mexer em lógica. É a variante
boa para level design rápido e para quem está aprendendo WebXR.

## Variante 3 — Unity/OpenXR

| Script | Papel |
|---|---|
| `VRPlayerRig` | CharacterController, locomoção contínua/snap turn, limites, fallback desktop |
| `XRWeapon` | hitscan, munição, recarga, recuo, traçante, háptica, eventos |
| `Target` | estados do alvo, zona crítica, multiplicador de dano, evento de acerto |
| `TargetRangeSystem` | pool de alvos, ativos simultâneos, curva de dificuldade |
| `GameManagerVR` | round, combo, HUD TMP, menu e resultados |

Eventos em vez de acoplamento: `XRWeapon.OnHit`, `Target.OnTargetDown`, `TargetRangeSystem.TargetDown`
e `GameManagerVR` assina tudo. Isso permite trocar a arma ou o sistema de alvos sem tocar no placar.

## Convenções

- **Unidades**: 1 unidade = 1 metro, tudo em escala humana (olho a 1,68 m).
- **Idioma**: código e comentários em português; identificadores em inglês.
- **Sem dependências de CDN**: `vendor/` guarda three.js r171 e A-Frame 1.7.0.
- **Sem build step**: qualquer mudança é salvar e recarregar. É o que torna o vibecoding viável.
