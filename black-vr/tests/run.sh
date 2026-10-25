#!/usr/bin/env bash
# Testes que não precisam de Windows, do MSVC nem do jogo:  ./tests/run.sh
#   1. matemática do estéreo (vr_math.h) contra o caminho direto
#   2. contrato do shader nos patches (registradores c201..c204, ordem, upload de 4)
#   3. sintaxe do módulo (g++ + stubs de Win32/D3D)
#   3b. LIGAÇÃO do módulo: símbolo declarado e não definido, assinatura trocada
#   4. patches aplicam sobre o black-pc e o gerador de HLSL do port roda e é verificado
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="${TMPDIR:-/tmp}/black-vr-tests"
mkdir -p "$out"

echo "=== 1/5 matemática do estéreo ==="
g++ -std=c++20 -O2 -Wall -Wextra -I "$here/src" "$here/tests/vr_math_test.cpp" -o "$out/vr_math_test"
"$out/vr_math_test"

echo
echo "=== 2/5 contrato do shader ==="
g++ -std=c++20 -O2 -Wall -Wextra "$here/tests/vr_shader_test.cpp" -o "$out/vr_shader_test"
"$out/vr_shader_test" "$here"

echo
echo "=== 3/5 sintaxe do módulo (stubs) ==="
"$here/tests/syntax_check.sh"

echo
echo "=== 3b/5 funções OpenXR ligadas antes do uso ==="
python3 "$here/tests/vr_bindings_test.py"

echo
echo "=== 4/5 ligação do módulo (todas as funções públicas de vr.h) ==="
g++ -std=c++20 -Wno-unused-parameter -I "$here/tests/stubs" -I "$here/src" -I "$here/vendor/openxr" \
    -o "$out/vr_link_test" "$here/tests/vr_link_test.cpp" "$here/tests/vr_link_stubs.cpp" "$here"/src/vr/*.cpp
"$out/vr_link_test"

echo
echo "=== 5/5 patches + gerador de shader do port ==="
if [ "${SKIP_PATCH_TEST:-0}" = "1" ]; then
    echo "  (pulado: SKIP_PATCH_TEST=1)"
else
    "$here/tests/patch_test.sh"
fi

echo
echo "Tudo verde."
