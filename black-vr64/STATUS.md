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
