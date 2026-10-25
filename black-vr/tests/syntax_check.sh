#!/usr/bin/env bash
# Checagem de sintaxe do módulo de VR fora do Windows: g++ + stubs de Win32/D3D9/D3D11.
# Não substitui o build no MSVC (o thunk de asm inline é MSVC-only e fica num #if), mas pega
# erro de gramática, nome errado e assinatura trocada — foi assim que apareceram, antes de
# qualquer build: um float* passado como int*, bindings com nome errado e strcpy_s (MSVC-only).
set -uo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fails=0
for f in "$here"/src/vr/*.cpp; do
    printf "  %-20s " "$(basename "$f")"
    out=$(g++ -std=c++20 -fsyntax-only -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
        \
        -I "$here/tests/stubs" -I "$here/src" -I "$here/vendor/openxr" "$f" 2>&1)
    if [ -z "$out" ]; then
        echo "ok"
    else
        echo "FALHOU"
        echo "$out" | head -12
        fails=$((fails + 1))
    fi
done
[ "$fails" -eq 0 ] || exit 1
