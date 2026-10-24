# BLACK VR

**BLACK (Criterion, 2006) rodando no PC com headset — o jogo original, não uma recriação.**

Este repositório é um **módulo de realidade virtual para o port nativo de BLACK** e os patches que o
ligam ao port. O jogo continua sendo o jogo: o mesmo XBE do Xbox que você já roda no PC, a mesma
campanha, a mesma IA, as mesmas armas. O que muda é onde a imagem aparece (nos dois olhos de um
headset) e de onde vem a câmera (do seu pescoço).

```
black-pc (port nativo, GPL-2.0)  +  este módulo de VR (OpenXR)  =  BLACK em PCVR
```

Não há engine nova, não há modelo novo, não há "inspirado em". Se o BLACK não estiver rodando no seu
PC pelo port, este módulo não tem o que mostrar — e é assim de propósito.

---

## O que funciona (e o que é trabalho em aberto)

| Peça | Estado |
|---|---|
| Sessão OpenXR + swapchain D3D11 por olho | escrito |
| Estéreo por correção de clip no vertex shader (c201) | escrito |
| Pose do headset na câmera do jogo (patch em `0x1DFFEC`, o mesmo ponto que o lean usa) | escrito |
| Segundo olho por replay dos draws do frame (`vr_frame.h`) | escrito |
| Controles do headset → pad Xbox (`XInputGetState`) | escrito |
| Teste do headset sem o jogo (`--vr-selftest`) | escrito |
| Matemática do estéreo e shader gerado | **testados aqui** (`./tests/run.sh`, 4 grupos, 3300+ verificações) |
| Build completo no MSVC | **não** — sem Windows aqui; veja *Estado do build* |

## Testes (rodam sem Windows e sem o jogo)

```bash
./tests/run.sh
```

1. **matemática do estéreo** — `vr_math.h` contra o caminho direto do olho (o mesmo ponto do mundo
   pelos dois caminhos tem de dar o mesmo NDC). Foi este teste que achou dois erros reais na
   primeira versão: o fator `zn` errado na projeção assimétrica e o sinal trocado do deslocamento
   da IPD.
2. **contrato do shader** — os registradores `c201..c204`, a forma por `dot` (sem `float4x4`, que
   dependeria do empacotamento de matriz do HLSL) e o upload de **quatro** registradores.
3. **sintaxe do módulo** — os cinco `.cpp` de `src/vr/` compilados com stubs de Win32/D3D9/D3D11.
   Achou um `float*` onde a lambda esperava `int*`, bindings do OpenXR com nome errado e `strcpy_s`
   (que é MSVC-only).
4. **patches + gerador de shader** — aplica os seis patches no `black-pc` e compila/executa o
   `nv2a_vsh.cpp` do port para olhar o HLSL que o jogo vai compilar de verdade.

O que ainda não está resolvido e está documentado sem enfeite em
[`docs/IMPLEMENTACAO.md`](docs/IMPLEMENTACAO.md): leitura do back buffer pela CPU (é o caminho lento,
existe o caminho rápido com D3D9Ex), passes intermediários não são reexecutados por olho, e o FOV do
jogo em VR é uma aproximação calibrada (o `--vr-selftest` existe justamente para você ver se ela
fecha na sua máquina).

---

## Como montar

**Se você não mexe em código:** olhe o [diagrama](docs/COMO-JOGAR.png) e siga o [`PASSO-A-PASSO.md`](PASSO-A-PASSO.md) — são dois duplos
cliques (`compilar.bat`) e a sua cópia do jogo. O resto desta seção é a versão manual.

Pré-requisitos no Windows:

- Visual Studio 2022 com **C++ para x86** (o loader é 32 bits e usa asm inline da MSVC);
- CMake 3.24+;
- um runtime OpenXR instalado — **SteamVR**, **Windows Mixed Reality**, **Oculus/Meta Link**, **Virtual
  Desktop** ou **Monado**. Sem runtime, o jogo roda em tela plana e avisa no log;
- um headset que o runtime reconheça.

```bash
# 1. monta o fork: clona o black-pc no commit de base, copia este módulo e aplica os patches
./tools/aplicar-no-black-pc.sh            # ou: ./tools/aplicar-no-black-pc.sh /caminho/de/trabalho

# 2. build (o script imprime o comando). Resumo:
cd ~/black-vr-build/black-pc-vr/loader
cmake -S . -B build -A Win32 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 3. jogo: a SUA cópia de default.xbe (não vem no repositório)
cp -r /caminho/da/sua/copia/do/BLACK/* ~/black-vr-build/black-pc-vr/game/
```

Rodar:

```bat
black.exe --vr                       :: no headset
black.exe --vr-selftest 20           :: sem o jogo: cenário de calibração por 20 s
black.exe                            :: sem VR (tela plana, o port original)
```

Ajustes (também gravados no `black.ini`):

```ini
vr = 1
vr_ipd = 63              ; distância entre as pupilas, em mm
vr_move_scale = 1.0      ; quanto do movimento real da cabeça move o corpo
vr_snap_turn = 30        ; giro por toque lateral no analógico direito (0 = desligado)
vr_fov = 0               ; FOV do jogo em VR; 0 = seguir o headset (recomendado)
vr_input = 1
vr_hud = 1
vr_reload_button = 16384 ; 0x4000 = X
```

---

## Controles

| No headset | No jogo |
|---|---|
| gatilho direito | atirar (RT) |
| squeeze direito | recarregar (X, configurável) |
| analógico esquerdo | andar |
| analógico direito | girar o corpo |
| toque lateral no analógico direito | snap turn (30°, configurável) |
| A / B / menu | A / B / Start |
| sua cabeça | a mira, a visão e a altura dos olhos |

Teclado, mouse e controle continuam funcionando ao mesmo tempo.

---

## Como o estéreo funciona (resumo)

Nenhum shader do jogo foi alterado. O port já traduz o microcódigo do vertex shader do Xbox para
HLSL (`src/d3d/nv2a_vsh.cpp`); o módulo só acrescenta uma linha no fim do shader traduzido:

```hlsl
o.pos = mul(o.pos, c201);   // fora do VR, c201 = identidade
```

`c201` é `(V_mono · P_mono)⁻¹ · (V_olho · P_olho)`: leva a posição de clip que o jogo produziu para o
clip daquele olho, com a projeção assimétrica do headset e a metade da IPD. A câmera do jogo recebe a
pose do headset no ponto em que ela grava a altura dos olhos (`0x1DFFEC`, o mesmo que o lean usa), o
que mantém mira, arma e corpo coerentes.

Para o olho direito, o frame não é recalculado pelo jogo (isso estragaria animação, mira e IA): os
draws são gravados e reemitidos com o `c201` do olho direito. O detalhe está em
[`docs/IMPLEMENTACAO.md`](docs/IMPLEMENTACAO.md).

---

## Estado do build

Este código foi escrito e revisado, mas **não foi compilado nesta árvore**: o ambiente onde ele foi
montado não tem Windows nem MSVC, e o loader é 32 bits com asm inline da MSVC. O que está verificado:

- os seis patches aplicam limpo sobre `Spark198rus/black-pc` @ `b826153`;
- todos os símbolos prometidos por `src/vr/vr.h` têm implementação (checagem automática);
- os arquivos estão sintaticamente equilibrados (chaves/parênteses).

O que só o build vai dizer: erros de tipo/assinatura nos pontos de contato com o port (por exemplo
`HostRT`, que é interno do `device.cpp`) e o comportamento do runtime. Se o primeiro build falhar,
o começo do log `[vr]` é o lugar de olhar.

---

## Licença e conteúdo

- O port (`Spark198rus/black-pc`) é **GPL-2.0**; este fork também é (`LICENSE`).
- Os headers OpenXR em `vendor/openxr/` são da Khronos, `Apache-2.0 OR MIT`.
- **BLACK é © Criterion Software / Electronic Arts.** Este projeto não distribui o jogo, nem
  arquivos dele, nem dados extraídos dele: você usa a sua cópia (`default.xbe`) e nada derivado do
  jogo entra no repositório (veja `.gitignore`). Não há código do XDK da Microsoft aqui.
