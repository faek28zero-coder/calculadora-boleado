# Estado desta pasta (VR no port de 64 bits)

- Fonte: `TGP482/BLACK-XBERecomp` (clone no build) + `TGP482/XboxXBE-Recomp` (SDK).
- Emenda: `aplicar-emenda.py` — põe a chamada da VR no `Present` do `src/d3d/d3d12.cpp`
  por âncora (o port recebe commits quase todo dia; patch de contexto quebrava).
- Camada de VR: `src/vr/vr_openxr.{h,cpp}` — OpenXR + Direct3D 12.
  Etapa 1: copia o quadro do jogo para as imagens do óculos (mesma imagem nos dois olhos).
- Build: `.github/workflows/compilar-black-vr64.yml`.
  Precisa do segredo `XBE_URL` (link direto do `default.xbe` do jogador), porque o
  CMake do port exige o XBE na hora de compilar.
- Sem `XBE_URL` o build para depois da emenda, com mensagem clara no log.

- 25/10: build de 64 bits passa a devolver os binarios para o branch (o artifact do GitHub nao e alcancavel daqui); o zip vai pro Drive.

## 25/10 - primeiro build de 64 bits COM a camada de VR (sucesso)
- Erros reais do compilador corrigidos: nomes `XR_TYPE_*_D3D12_KHR` e o LUID
  (agora por `IDXGIAdapter::GetDesc`, sem `GetDesc1`).
- `BLACK.exe` (5,8 MB) saiu com a VR dentro: `grep -a "xrCreateSession"` acha o
  codigo no exe. `Black.dll` (9,1 MB) e o jogo recompilado do XBE do jogador.
- Entrega: `VR-64BITS.zip` (71 MB) na pasta BLACK VR do Drive
  (id 1vNQcdFJlc2mrnZnaUlsnkNG9lB5wFhR8) com BLACK.exe, Black.dll, compiler/,
  openxr_loader.dll, JOGAR-VR.bat, LEIA-ISTO.txt e assets/ vazia.
- O artifact do GitHub Actions NAO e alcancavel daqui (host bloqueado); por isso
  o CI devolve o `dist/` num commit [skip ci] no proprio branch.
