# Compilar sem instalar nada no seu PC

Existem dois caminhos para o programa de VR ficar pronto. Escolha **um**.

| | Caminho A: GitHub compila | Caminho B: seu PC compila |
|---|---|---|
| O que você instala | **nada** | Visual Studio + Git + CMake (~10 GB) |
| O que você clica | 1 interruptor no GitHub | 1 instalador 3 vezes + 1 `.bat` |
| Quem compila | a nuvem do GitHub (Windows) | o seu computador |
| Tempo | ~15 min, sem você olhar | ~20 min, sem você olhar |
| Se der erro | eu vejo o log e corrijo | você me manda o print |

---

## Caminho A — o jeito rápido (recomendado)

Eu já escrevi o trabalho de compilação: ele está em `.github/workflows/compilar-black-vr.yml`,
no repositório. Ele baixa o port, coloca o módulo de VR, aplica as emendas, compila no Windows da
nuvem e devolve `black.exe` + `blackrt.dll` prontos.

**Falta só ligar o interruptor**, porque o GitHub desliga isso por padrão em alguns repositórios:

1. Abra: **https://github.com/faek28zero-coder/calculadora-boleado/settings/actions**
2. Em **Actions permissions**, marque **Allow all actions and reusable workflows**.
3. Clique **Save**.
4. Me avise aqui: *"liguei"*.

A partir daí é comigo: eu envio o código, a compilação acontece sozinha, e os dois arquivos voltam
para o repositório. Eu te entrego num zip — você baixa, copia para a pasta `port` e joga.

> Se a página mostrar *"Actions is disabled for this repository"* ou pedir para habilitar workflows
> primeiro, clique no botão verde que aparecer. Se aparecer cobrança ou bloqueio de conta, use o
> Caminho B — repositório público normalmente não cobra.

---

## Caminho B — no seu PC (já está tudo pronto)

1. Instale: **Visual Studio 2022** (marcando *Desenvolvimento para desktop com C++*), **Git** e **CMake**.
2. Duplo clique em **`compilar.bat`**.
3. Arraste a sua pasta do BLACK para a janela preta e aperte Enter.

Ele compila e instala na sua própria pasta, guardando o programa antigo em `_port-antigo\` e levando
seus saves junto. No fim aparecem `TESTAR-OCULOS.bat` e `JOGAR-VR.bat`.

---

## Por que não dá para eu compilar aqui

O programa é 32 bits e usa montagem x86 do compilador da Microsoft. Isso não existe no Linux onde eu
rodo, e não há como instalar: a Microsoft não distribui esse compilador fora do Windows. Compilar
exige uma máquina Windows de verdade — a sua, ou a da nuvem do GitHub.

O que **não** depende de Windows (e já está pronto e testado aqui):

- a matemática do estéreo (3287 verificações, zero falhas);
- o contrato do shader que monta os dois olhos (17 verificações);
- os 6 patches que ligam o módulo no port (aplicam limpos sobre o `b826153` / v0.3.0);
- a sintaxe dos 5 arquivos do módulo, compilada contra os stubs do Windows.
