#!/usr/bin/env bash
# Clona o black-pc no commit de base, aplica os patches e COMPILA o arquivo do shader
# (d3d/nv2a_vsh.cpp) com os stubs, executando o gerador de HLSL de verdade.
set -uo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
base_commit="b826153"
work="${TMPDIR:-/tmp}/black-vr-patchtest"
src="$work/black-pc"
out="${TMPDIR:-/tmp}/black-vr-tests"
mkdir -p "$out"

if [ ! -d "$src/.git" ]; then
    mkdir -p "$work"
    git clone --quiet https://github.com/Spark198rus/black-pc.git "$src" || { echo "  sem rede: pulando"; exit 0; }
fi
git -C "$src" checkout --quiet "$base_commit" 2>/dev/null || echo "  (aviso: base $base_commit não encontrado)"
git -C "$src" reset --hard --quiet

fails=0
for p in "$here"/patches/*.patch; do
    if git -C "$src" apply --check --whitespace=nowarn "$p" 2>/dev/null; then
        git -C "$src" apply --whitespace=nowarn "$p"
        echo "  ok      $(basename "$p")"
    else
        echo "  FALHOU  $(basename "$p")"
        fails=$((fails + 1))
    fi
done
[ "$fails" -eq 0 ] || { echo "$fails patch(es) não aplicaram"; exit 1; }

echo "  -- gerador de shader do port (nv2a_vsh.cpp) compilado e executado:"
if g++ -std=c++20 -O2 -DXR_USE_PLATFORM_WIN32 -DXR_USE_GRAPHICS_API_D3D11 \
    -I "$here/tests/stubs" -I "$src/loader/src" -I "$here/vendor/openxr" \
    "$here/tests/vr_shader_gen_test.cpp" "$src/loader/src/d3d/nv2a_vsh.cpp" -o "$out/vr_shader_gen_test" 2>"$out/gen_build.log"; then
    "$out/vr_shader_gen_test" | sed 's/^/     /'
else
    echo "     FALHOU ao compilar o gerador:"
    head -10 "$out/gen_build.log"
    exit 1
fi
