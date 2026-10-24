# Como o estéreo é feito (implementação)

Documento técnico do módulo. Se você só quer jogar, o `README.md` basta. Aqui está o porquê de cada
decisão, o que já está resolvido e o que ainda não está.

Arquivos:

```
src/vr/vr.h            API que o port conhece (nada mais)
src/vr/vr_internal.h   estado compartilhado entre os arquivos do módulo
src/vr/vr_openxr.cpp   instância, sessão, swapchain D3D11, ações, envio do frame
src/vr/vr_pose.cpp     pose do headset na câmera do jogo + matriz de clip por olho
src/vr/vr_frame.h      contrato do gravador/replay de draws (implementado no draw.cpp do port)
src/vr/vr_input.cpp    controles do headset -> pad Xbox
src/vr/vr.cpp          configuração e o caminho do olho direito
src/vr/vr_selftest.cpp cenário de calibração (--vr-selftest)
patches/000*.patch     as emendas no port, por arquivo
```

---

## 1. Por que fork do `black-pc`

O BLACK é um jogo de Xbox (PowerPC + NV2A) e não existe versão de PC. As opções eram:

| caminho | veredito |
|---|---|
| `Spark198rus/black-pc` — port HLE nativo Win32 (32 bits), D3D9 + HLSL, GPL-2.0 | **escolhido**: roda no PC, tem código-fonte, tem licença que permite fork |
| `TGP482/BLACK-XBERecomp`, `portingpete/Black-2006-Recomp` — recompilações | sem licença publicada: não dá para fork distribuir |
| emulador (Xemu/PCSX2) + camada VR | fora do escopo: o problema vira "VR para emulador", não "BLACK em VR" |

Nenhum port de BLACK tem VR. Este módulo é a camada que faltava.

---

## 2. Estéreo sem tocar no jogo

### O problema

O jogo desenha **um** frame por vez, com uma câmera só. Para estéreo é preciso a mesma cena vista de
dois pontos (os dois olhos), e não dá para simplesmente chamar o renderizador do jogo duas vezes: a
câmera, a mira, a animação e a IA são atualizadas uma vez por frame no código do jogo; chamar de novo
no meio do frame quebra a lógica (e o `Swap` do jogo é o único ponto em que o frame está completo).

### A solução

1. **A câmera recebe a cabeça.** O port já documenta que a matriz de visão do jogador é montada em
   `FUN_001dfd90` e que a altura dos olhos é gravada pela instrução `movss [esi+0x104], xmm0` em
   `0x1DFFEC` (é o mesmo ponto que o *tactical lean* do port usa). O módulo troca essa instrução por
   um thunk: grava a altura (o que o original fazia) e aplica por cima a pose do headset — rotação e
   deslocamento. Como essa matriz é a fonte da mira e da posição da arma, tudo acompanha.
2. **Duas projeções, um só frame.** O jogo desenha com a projeção "mono". No fim do vertex shader
   traduzido (que o próprio port gera, `nv2a_vsh.cpp`) entra uma linha:

   ```hlsl
   o.pos = mul(o.pos, c201);
   ```

   com

   ```
   C = (V_mono · P_mono)⁻¹ · (V_olho · P_olho)
   ```

   `P_olho` é a projeção **assimétrica** do headset (`XrFovf`: ângulos diferentes para esquerda,
   direita, cima, baixo, como todo headset real tem) com deslocamento de ±IPD/2, e `V_mono`/`P_mono`
   são as mesmas matrizes com IPD zero. Aplicar `C` é reprojetar o frame pronto no ponto de vista
   daquele olho — a técnica de *reprojection* usada em VR, só que exata para o caso estático.
3. **O segundo olho é o mesmo frame redesenhado.** `c201` é uma constante de vertex shader
   (registro 201): o port envia c0..c200 (`upload_vs_constants`), então o 201 estava livre. O olho
   esquerdo usa `C_esq` desde o começo do frame; o direito usa `C_dir` no replay.

   Fora do VR, `c201` recebe a identidade uma vez, então o shader é o mesmo nos dois casos.

### Por que `o.pos = mul(...)` e não uma matriz de viewport

Porque o port desfaz a transformação de viewport do NV2A no fim do shader (`o.pos.xyz = (oPos -
vpOffset) / vpScale * w`), então `o.pos` naquele ponto é o clip space de verdade do D3D9. Multiplicar
ali é a única posição em que a correção vale para todos os shaders (os do jogo, os de efeito, os 2D) e
também para a interpolação de textura e o `oFog`.

---

## 3. Gravação e replay (`vr_frame.h`, patch 0001)

O replay vive no `draw.cpp` do port porque é o único lugar que tem:

- o estado (`g_st`, `render_state()`, `texture_state()`), que é o que o `begin_draw()` traduz para o
  D3D9 a cada draw;
- a geometria **final** que foi para o device (já decodificada e com índices rebaseados).

O que é gravado por draw:

| dado | tamanho | deduplicação |
|---|---|---|
| `HleState` + 166 render states + 4 texture stages | ~2 KB | contra o snapshot anterior |
| 192 vec4 de constantes do vertex shader | 3 KB | contra o snapshot anterior |
| geometria (vértices) + índices | variável | não |

Teto de 64 MB por frame; se estourar, o replay para naquele ponto e loga (é um sintoma de que o
`--resolution` está alto demais para o caminho de CPU).

O replay, por draw: restaura estado e constantes, re-liga o render target principal, aplica o
viewport, invalida o cache de "o que já foi enviado" (`forget_sent_state`), chama o `begin_draw()` do
port e reemite a chamada D3D9 exata. No fim, devolve ao jogo **o estado que ele havia deixado**
(senão o próximo frame do jogo desenharia errado).

Limitações conhecidas da v1:

- só entram os draws cujo render target é o back buffer; passes em targets próprios (buffers de
  efeito, sombras) já estão resolvidos dentro do back buffer quando o replay acontece;
- se o jogo liberar um recurso (textura/vertex buffer) entre o draw e o replay do mesmo frame, o
  ponteiro gravado pode ficar pendurado. Na prática o BLACK não faz isso dentro de um frame, mas é o
  tipo de coisa que um crash report vai apontar.

---

## 4. Ponte D3D9 → D3D11 (o ponto mais caro hoje)

O OpenXR no Windows fala D3D11/D3D12/Vulkan; o port desenha em D3D9. O caminho atual (v1, CPU):

```
back buffer do jogo (textura D3D9)
   -> GetRenderTargetData            (cópia GPU -> superfície SYSTEMMEM)
   -> LockRect                       (leitura pela CPU)
   -> ID3D11DeviceContext::UpdateSubresource   (para a textura da swapchain)
```

Custo: ~2 leituras de 720p/1080p por frame + upload. Funciona, mas é o gargalo.

Caminho rápido (próximo passo, já desenhado): criar o device D3D9 como **D3D9Ex**, criar o back
buffer com `D3DUSAGE_RENDERTARGET` + `D3DPOOL_DEFAULT` como textura compartilhada
(`CreateTexture(..., &g_shared_handle)` com `D3D9EX_TEXTURE_SHARING`), e no D3D11 fazer
`OpenSharedResource` **uma vez**. Depois disso, cada frame é só um `StretchRect` para a textura
compartilhada — sem CPU no caminho. Trocar o device para Ex mexe no `device.cpp` inteiro (formato de
recursos, `D3DPOOL_MANAGED` deixa de existir), por isso ficou para depois do primeiro build que roda.

---

## 5. Pose, olhos e o FOV do jogo

O cálculo de `C` (`vr_pose.cpp`) assume que a projeção mono do jogo é a de um FOV horizontal
`game_fov_deg` com o aspecto do render target, e constrói a projeção do olho a partir do `XrFovf` do
runtime. Isso é **uma aproximação**: o jogo tem o seu próprio near/far (o módulo usa 0.05 m / 5000 m).

O que reduz o erro:

- em VR, `--vr-fov` deve ficar em `0`: o port passa a usar o FOV horizontal do headset (o `fov.cpp`
  já permite trocar o FOV em tempo real). FOVs iguais ⇒ a borda do quadro do jogo cai na borda do
  campo de visão do headset, sem "janela" no meio;
- o render target do jogo deve ter o aspecto do olho. Sem isso, `upload_eye` encaixa a imagem com
  faixas pretas (preserva o aspecto, mas perde resolução).

O `--vr-selftest` existe para fechar essa conta na prática: ele desenha piso de 1 m, régua de 10 cm e
um alvo a 2 m **com a mesma projeção por olho** que o jogo vai usar. Se a régua medir 10 cm com a
fita métrica e o alvo parecer estar a 2 m, a matemática está certa e o problema (se houver) é do
acoplamento com o jogo.

Ordem dentro do frame (`D3DDevice_Swap`, patch 0003):

```
Swap N-1  : frame_begin()  -> xrWaitFrame, xrLocateViews, adquire as 2 imagens
            (a pose daqui é a que a câmera do jogo vai usar no frame N)
frame N   : o jogo desenha (gravação acontece dentro do draw.cpp)
Swap N    : present_backbuffer()      -> janela espelhada
            submit_frame(bb, 0)       -> olho esquerdo
            render_second_eye()       -> capture_end, set_eye(1), replay, submit_frame(...,1),
                                         set_eye(0), capture_begin
            frame_end()               -> xrEndFrame
            frame_begin()             -> próximo frame
```

---

## 6. Entrada

O port consome o pad do Xbox em `XInputGetState` (`src/input/xinput.cpp`). Em vez de criar um
caminho novo, o módulo escreve no mesmo `XGamepad` depois do pad físico, das teclas e dos atalhos
(patch 0004). O jogo não sabe que existe VR:

| ação OpenXR | vira |
|---|---|
| `trigger` (float) | `bAnalogButtons[7]` (RT), 0..255 |
| `grip` > 0.6 (borda de subida) | `bAnalogButtons[2]` (X = recarregar) |
| `move` (vec2) | `sThumbLX/LY` |
| `turn` (vec2) | `sThumbRX/RY` + snap turn por frame |
| `a` / `b` / `menu` | A / B / Start |

Se o runtime não reportar controles (`isActive` falso), o módulo não escreve nada e o
teclado/controle seguem normais.

---

## 7. O que ainda falta

1. **Compilar e rodar** (o item que domina todo o resto): ajustar os pontos de contato com o port e
   medir. Nada aqui substitui um `black.log` de uma sessão real.
2. Caminho rápido D3D9Ex (seção 4).
3. Renderização do jogo no aspecto/resolução do olho (hoje depende de `--resolution`; o certo é o
   módulo pedir `render_w×render_h` ao port em `start()`).
4. Passes intermediários por olho (seção 3): hoje só o frame principal é reprojetado.
5. HUD: hoje é reprojetado junto e fica "colado" no plano da tela. O certo é desenhá-lo em
   profundidade fixa (1,5–2 m) por olho.
6. Conforto: snap turn já existe; falta vinheta ao girar e altura do jogador (sentado/em pé).
7. Vídeos/cutscenes: em VR, projetar o vídeo num plano no espaço em vez de encher o olho.
