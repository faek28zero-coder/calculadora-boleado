#!/usr/bin/env bash
# ============================================================
#  BLACK VR — tools/aplicar-no-black-pc.sh
#
#  Monta um fork do port nativo de BLACK (Spark198rus/black-pc, GPL-2.0) com o módulo de VR
#  deste repositório:
#
#      ./tools/aplicar-no-black-pc.sh [/caminho/de/trabalho]
#
#  O que ele faz:
#    1. clona o black-pc no commit de base fixado abaixo (o patch é diff contra ele);
#    2. copia src/vr -> loader/src/vr e vendor/openxr -> vendor/openxr;
#    3. aplica patches/000*.patch em ordem;
#    4. mostra o comando de build (MSVC x86; o loader é 32 bits e usa asm inline da MSVC).
#
#  O jogo NÃO vem junto: coloque a sua cópia de default.xbe do BLACK em <fork>/game/.
#  Nada derivado do jogo entra no repositório.
# ============================================================
set -euo pipefail

BASE_REPO="https://github.com/Spark198rus/black-pc"
BASE_COMMIT="b826153"           # "Release 0.3.0", o commit contra o qual os patches foram gerados
WORK="${1:-$HOME/black-vr-build}"

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fork="$WORK/black-pc-vr"

echo "==> base: $BASE_REPO @ $BASE_COMMIT"
echo "==> destino: $fork"

if [ -d "$fork/.git" ]; then
    echo "==> reutilizando o clone existente"
else
    mkdir -p "$WORK"
    git clone "$BASE_REPO" "$fork"
    git -C "$fork" checkout "$BASE_COMMIT"
fi

echo "==> copiando o módulo de VR"
mkdir -p "$fork/loader/src/vr" "$fork/vendor/openxr"
cp "$here"/src/vr/*.h "$here"/src/vr/*.cpp "$fork/loader/src/vr/"
cp "$here"/vendor/openxr/*.h "$fork/vendor/openxr/"

echo "==> aplicando patches"
for p in "$here"/patches/*.patch; do
    echo "    $(basename "$p")"
    git -C "$fork" apply --whitespace=nowarn "$p"
done

cat <<EOF

==> pronto.

Build (Windows, Visual Studio 2022 com C++ x86 instalado):

    cd "$fork/loader"
    cmake -S . -B build -A Win32 -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

Depois:

    "$fork/loader/build/Release/blackrt.dll"  ->  copie para a pasta do black.exe
    <fork>/game/default.xbe                   ->  a SUA cópia do jogo

Rodar com headset:   black.exe --vr
Testar só o headset: black.exe --vr-selftest 20
Sem headset:         black.exe            (tela plana, como o port original)
EOF
