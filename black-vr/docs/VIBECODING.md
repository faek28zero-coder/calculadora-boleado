# Como isso foi construído (vibecoding)

Este projeto nasceu de uma sessão de **vibecoding**: descrever o objetivo em linguagem natural, deixar
o agente escrever código de verdade e iterar em cima do resultado executável. Este documento registra
o método, as decisões e as armadilhas — para você repetir com o seu próprio agente.

## A ideia em uma frase

> "BLACK é um jogo de PS2/Xbox que ganhou port nativo de PC. Quero que uma IA consiga transformar esse
> modelo em VR por conta própria, via vibecoding."

Traduzindo para algo executável: **não dá para pedir um mod de VR de um jogo fechado em um prompt**.
Dá para pedir:

1. a **pesquisa** de como esses ports funcionam (o que é HLE, o que é recompilação, quais têm licença);
2. um **protótipo jogável** que prove escala, mira, conforto e loop de jogo;
3. um **plano técnico** de como acoplar VR ao port real.

Foi exatamente isso que este repositório entrega.

## Regras que mantiveram o projeto honesto

| Regra | Motivo |
|---|---|
| **Zero assets do jogo** | o repositório poderia ser derrubado (DMCA) e o problema viraria jurídico, não técnico |
| **Tudo procedural** | texturas em `<canvas>`, áudio em WebAudio, arma feita de caixas — e o repo fica leve |
| **Sem build step** | salvar e recarregar é o que permite ciclos de 30 segundos |
| **Três stacks em paralelo** | se uma abordagem travar, a outra continua; e dá para comparar de verdade |
| **Medir antes de modar** | o protótipo não substitui o jogo; ele calibra números (FOV, velocidade, altura) |
| **Licença explícita** | MIT no código, CC BY 4.0 na documentação, e um aviso legal em todo lugar |

## Anatomia do prompt que funciona

Ruim:

> "faz um jogo de VR do BLACK"

Bom:

> "Protótipo WebXR de estande de tiro inspirado em BLACK: sala industrial 28×32 m, alvos pop-up com
> zona crítica central, round de 75 s, combo até x5, HUD em DOM, arma procedural com recuo, áudio
> sintetizado, locomoção contínua + snap turn, funciona em desktop com WASD/mouse. Sem assets externos.
> Arquivos separados: world, targets, weapon, player, effects, textures, audio, hud, main."

A diferença: **restrições verificáveis**. Tamanho da sala, duração do round, multiplicador máximo,
teclas, proibição de assets. Sem isso o agente inventa um universo; com isso ele constrói um sistema.

## Ordem que funcionou

1. **Pesquisa primeiro.** Levantar os projetos existentes (`black-pc`, `BLACK-XBERecomp`, forks) antes
   de escrever uma linha de jogo. Sem isso o plano de VR seria fantasia.
2. **Esqueleto do loop.** Cena + renderer + estado (`menu → playing → results`) + HUD. Rodar feio, mas
   rodar.
3. **Sistemas por arquivo.** Um arquivo por responsabilidade. Cada um com uma API pequena
   (`fire()`, `kick()`, `hit()`). Isso deixa o agente substituir uma peça sem quebrar as outras.
4. **Feedback antes de beleza.** Hitmarker, som de acerto, faísca e decal primeiro. Depois iluminação,
   névoa e poeira. Jogo sem feedback não é jogo, é tech demo.
5. **Segunda stack como revisão.** Reescrever em A-Frame força a entender o que é essencial: se algo
   não cabe em 300 linhas declarativas, provavelmente estava complexo demais na primeira versão.
6. **Documentar o limite.** Escrever o que **não** está feito é o que separa um projeto de fã honesto
   de um pacote de promessas.

## Erros que este projeto cometeu (e as lições)

- **Escala errada no primeiro olhar.** Sala grande demais parece vazia em VR; alvo a 16 m de altura não
  é "difícil", é invisível. Corrigir sempre olhando em metros reais.
- **Mira na câmera em vez do controle.** No headset isso não existe. O raio tem que sair da mão que
  segura a arma; a câmera só dá a direção do corpo.
- **Recuo que sacode a câmera.** Em tela plana é gostoso; em VR é náusea. Recuo vai na arma.
- **HUD de DOM tentando existir no headset.** Em XR o DOM some — o feedback tem que estar no som, na
  mira e no mundo.
- **Áudio antes do gesto.** `AudioContext` criado no carregamento fica suspenso; tem que nascer no
  primeiro clique.
- **Achar que "VR do BLACK" é uma flag.** Não é: é estereoscopia com frustum off-axis, pose de HMD,
  viewmodel, conforto e centenas de horas de teste. O roadmap existe para não vender ilusão.

## Como continuar com um agente

```text
Contexto: repositório black-vr, variante three.js (webxr-three/src).
Tarefa: adicionar modo "relaxado" com teleporte por arco e vignette dinâmica.
Restrições: sem dependências novas, sem assets externos, manter API atual de player.js,
             funcionar em desktop e XR, e não alterar a pontuação.
Critério de aceite: alternar com a tecla T, teleporte carregando o gatilho, 90 fps no Quest 2.
```

Boa prática: peça **um sistema por vez**, exija **o que não pode mudar** e defina **aceite mensurável**.
Vibecoding funciona igual a dirigir um time: quanto melhor o briefing, menos retrabalho.

## O que ficou para você

- Rodar o demo em um headset de verdade e anotar o que incomoda (a lista sempre começa por conforto).
- Medir os números do BLACK original com o port e preencher `docs/metricas-do-black.md`.
- Decidir a stack que vai evoluir: three.js para iterar rápido, Unity para build nativo, A-Frame para
  level design declarativo.
