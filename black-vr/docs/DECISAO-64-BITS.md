# Só 64 bits

Decisão do dono do projeto (25/10/2026): o caminho de **VR** de 32 bits está
descartado. Ele estoura sempre no mesmo ponto (`xrCreateSession`, endereço
solto na memória) em todas as configurações testadas, com e sem o jogo.

## O que muda

- **VR = programa de 64 bits.** O código da sonda vive em `src/sonda/sonda.cpp`
  e compila como `vr-sonda64.exe` (o mesmo arquivo também compila em 32 bits,
  mas isso não é mais usado nem distribuído).
- **Entrega:** os arquivos vão soltos na pasta do Drive do usuário
  (`vr-sonda64.exe`, `openxr_loader.dll`, `TESTAR-64.bat`, `LEIA-ISTO.txt`),
  sem zip, sem substituir nada.
- **O jogo continua de 32 bits.** O `default.xbe` é código x86 de 2006 e o
  `blackrt.dll` tem de ser 32 bits (o CMake do port recusa outro tamanho).
  Por isso a VR que vai funcionar é um **programa separado de 64 bits** que
  conversa com o jogo: o lado de 32 bits só entrega a imagem, quem fala com o
  óculos é o de 64 bits.

## O que já sabemos do lado de 64 bits

- O aplicativo do óculos de 64 bits **carrega, conecta** ("RuntimeIPC:
  establishConnection SUCCESS") e só então reclama:
  - `VirtualLock failed ... 1453` (ERROR_WORKING_SET_QUOTA)
  - `StartThread: JobPoolWorker: std::thread construction FAILED`

  Os dois sintomas são de **falta de memória/commit** ou de quota de memória
  do processo. Por isso a sonda v11 grava `GlobalMemoryStatusEx` e
  `GetProcessMemoryInfo` no relatório, antes de cada passo.

## Modos da sonda

| modo | o que faz |
|------|-----------|
| 1 | caminho do jogo (adaptador do óculos, BGRA, API atual, mensageiro) |
| 2 | adaptador padrão do Windows |
| 3 | adaptador do óculos sem BGRA |
| 4 | para antes da sessão |
| 5 | como 1, com COM nesta thread |
| 6 | API 1.0.0 sem mensageiro |
| 7 | como 6 com janela + fila de mensagens |
| 8 | como 7 em thread própria (pilha 8 MB) |
| 9 | API 1.0.0 com mensageiro |
| 10 | caminho inteiro + quadro colorido no óculos por 20 s |
| 11 | como 10, tentando a sessão 3 vezes |
| 12 | como 10, com o mensageiro do runtime ligado |
