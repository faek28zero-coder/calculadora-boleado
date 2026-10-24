# Roteiro

Ordem de trabalho, do que destrava o resto para o que é conforto. Cada item diz o que precisa
acontecer para ser considerado pronto.

## Fase 0 — o primeiro build que roda (bloqueia tudo)

- [ ] `tools/aplicar-no-black-pc.sh` + CMake/MSVC x86 sem erro de compilação.
  Critério: `blackrt.dll` e `black.exe` gerados.
- [ ] Ajustar os pontos de contato do módulo com o port (`HostRT`, `host3d::window()`,
  assinaturas do `draw.cpp`) conforme o compilador reclamar.
- [ ] `black.exe --vr-selftest 20`: o cenário de calibração aparece nos dois olhos.
  Critério: piso de 1 m, régua de 10 cm e alvo a 2 m batem com a fita métrica.
- [ ] `black.exe` sem `--vr`: o jogo roda em tela plana exatamente como antes.
  Critério: nenhuma mudança de comportamento com o módulo presente.

## Fase 1 — o jogo no headset

- [ ] `black.exe --vr` com o menu e o primeiro nível desenhando nos dois olhos.
- [ ] Pose: olhar em volta gira a visão; agachar move a cabeça; a mira acompanha.
  Critério: sem deriva, sem oscilação, corpo (yaw) separado da cabeça (pitch/roll).
- [ ] Entrada: andar, atirar, recarregar e girar com os controles do headset.
- [ ] Estabilidade: 30 minutos de jogo sem crash e sem vazamento de memória na gravação.

## Fase 2 — qualidade de imagem

- [ ] Caminho rápido da ponte D3D9→D3D11 (D3D9Ex + textura compartilhada + `OpenSharedResource`).
  Critério: `submit` deixa de aparecer no perfil (`perf.cpp`).
- [ ] O módulo pede ao port o tamanho de render por olho, no aspecto do olho.
- [ ] FOV do jogo = FOV do headset automaticamente (`vr_fov = 0`), e o viewmodel (arma em primeira
  pessoa, c200) conferido nesse FOV.

## Fase 3 — o que faz parecer VR e não "tela colada na cara"

- [ ] HUD em profundidade fixa por olho (~1,5 m), não reprojetado no plano da tela.
- [ ] Passes intermediários por olho (efeitos, sombras) quando doer.
- [ ] Cutscenes e vídeos num plano no espaço.
- [ ] Conforto: vinheta ao girar, altura do jogador, recentralizar, IPD ao vivo.
- [ ] Página de VR no PC SETTINGS do port (hoje é `black.ini`/linha de comando).

## Fase 4 — publicar

- [ ] Repositório público `black-vr` com este conteúdo e um release com instruções de build.
- [ ] Vídeo/roteiro de instalação que começa dizendo: **você precisa da sua cópia do BLACK**.
