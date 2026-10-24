# Como rodar

Tudo aqui é estático (HTML/JS). Nenhum build, nenhum `npm install` obrigatório — o `three.js` e o
A-Frame já estão vendorizados em `vendor/`.

## 1. Servir a pasta (obrigatório)

Não abra o arquivo direto com `file://`: módulos ES e WebXR precisam de HTTP.

```bash
# na raiz do repositório
python3 -m http.server 8080
# ou
npx serve .
```

Depois abra:

| Variante | URL |
|---|---|
| Hub | http://localhost:8080/ |
| three.js (principal) | http://localhost:8080/webxr-three/ |
| A-Frame | http://localhost:8080/webxr-aframe/ |

## 2. Testar no headset (WebXR)

1. O headset precisa alcançar o seu computador na mesma rede. Descubra o IP local:
   ```bash
   hostname -I      # Linux
   ipconfig         # Windows
   ```
2. Abra no navegador do headset: `http://SEU_IP:8080/webxr-three/`.
3. WebXR exige **contexto seguro**. Em `localhost` funciona; por IP na rede local o Chrome do Quest
   aceita `http://` na maioria das versões, mas se recusar, sirva com HTTPS:
   ```bash
   # opção rápida com certificado autoassinado
   npx http-server -S -C cert.pem -K key.pem -p 8443
   ```
   Alternativa recomendada: publique a pasta em qualquer host estático (GitHub Pages, Netlify, Cloudflare
   Pages) — HTTPS de graça e sem configurar nada no headset.
4. No Quest, o botão **ENTER VR** aparece no canto da tela (é o `VRButton` do three.js). No A-Frame, use
   **ENTRAR EM VR** no menu.

### Quest / Pico: dica de teclado
Sem teclado no headset? Use o navegador do Quest (Meta Browser) e o menu já inicia com o gatilho.

## 3. Testar sem headset

Ambas as variantes web têm modo desktop completo:

**three.js**
- `WASD` / setas — mover
- mouse — olhar (clique na tela para capturar o ponteiro)
- clique esquerdo — atirar
- `R` — recarregar
- `Esc` — liberar o ponteiro (pausa)

**A-Frame**
- `WASD` / setas — mover
- arrastar com o mouse — olhar
- clique — atirar no ponto do cursor
- `R` — recarregar

## 4. Unity

Siga `unity/README.md`. Resumo: projeto 3D novo → instalar OpenXR + XR Interaction Toolkit → copiar
`Assets/Scripts` → montar a cena com XR Origin, arma filha do controlador e alvos com o componente
`Target`. Build para Windows x64 (OpenXR) ou Android (Quest).

## 5. Publicar de graça (GitHub Pages)

```bash
# no GitHub: Settings → Pages → Source: Deploy from a branch → main → / (root)
```
Depois de ativar, o hub fica em `https://SEU_USUARIO.github.io/black-vr/`.
O caminho relativo entre hub e demos já está correto — nada a ajustar.

## Problemas comuns

| Sintoma | Causa provável | Solução |
|---|---|---|
| Tela preta / "FALHA: ..." | aberto via `file://` | sirva por HTTP |
| Botão ENTER VR não aparece | navegador sem WebXR ou sem HTTPS | use Chrome/Quest Browser em HTTPS ou localhost |
| AR aparece mas o chão está alto/baixo | `local-floor` indisponível | o demo cai para `local`; ajuste a altura do rig no Unity |
| Sem som | o áudio só inicia após um gesto | clique em **INICIAR TREINO** |
| Trava em 20 fps no Quest | resolução/pixelRatio alto | reduza `Math.min(devicePixelRatio, 2)` para `1.5` em `webxr-three/src/main.js` |
