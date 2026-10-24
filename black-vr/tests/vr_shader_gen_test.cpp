// ============================================================
//  BLACK VR — tests/vr_shader_gen_test.cpp
//
//  Compila e EXECUTA o gerador de shader do port (loader/src/d3d/nv2a_vsh.cpp) fora do
//  Windows, para olhar o HLSL de verdade que o jogo vai compilar: o shader gerado tem de
//  conter as declarações de c201..c204 e a linha final que leva o clip mono ao clip do olho,
//  e ela tem de vir DEPOIS de `o.pos.xy *= vmScale.xy`.
//
//  É o teste mais próximo do runtime que dá para fazer sem Windows: o mesmo código que gera
//  o shader do jogo é o que gera o texto aqui.
//
//      ./tests/run.sh   (compila junto com os outros)
// ============================================================
#include "d3d/nv2a_vsh.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace {

int g_failures, g_checks;

void check(bool ok, const char *what)
{
    g_checks++;
    if (ok) return;
    g_failures++;
    std::printf("  FALHOU: %s\n", what);
}

bool contains(const std::string &hay, const std::string &needle, size_t *where = nullptr)
{
    const size_t at = hay.find(needle);
    if (where) *where = at;
    return at != std::string::npos;
}

} // namespace

int main()
{
    std::printf("== shader gerado pelo port (nv2a_vsh.cpp) ==\n\n");

    // Uma instrução "final" (bit 0 do dword 3): o gerador fecha o main() logo depois do prólogo,
    // que é o suficiente para ver o tail do VR no texto.
    std::vector<uint32_t> program(4, 0);
    program[3] = 1; // final

    const std::string hlsl = xd3d::vsh_to_hlsl(program.data(), 1, 0);
    if (hlsl.empty()) {
        std::printf("  o gerador não produziu texto\n");
        return 1;
    }

    // 1) as declarações, na forma que o HLSL compila (register(cNNN))
    for (int i = 0; i < 4; i++) {
        char reg[64];
        std::snprintf(reg, sizeof reg, "float4 vrRow%d : register(c%d);", i, 201 + i);
        check(contains(hlsl, reg), reg);
    }

    // 2) a aplicação por dots
    const char *mul = "o.pos = float4(dot(o.pos, vrRow0), dot(o.pos, vrRow1), dot(o.pos, vrRow2), dot(o.pos, vrRow3));";
    size_t at_mul = 0;
    check(contains(hlsl, mul, &at_mul), "a linha de correção do olho está no shader gerado");

    // 3) ordem: depois do clip space do jogo
    size_t at_vm = 0;
    check(contains(hlsl, "o.pos.xy *= vmScale.xy;", &at_vm), "o clip space do jogo continua no shader");
    check(at_mul > at_vm, "a correção vem depois do clip space");

    // 4) nada de matriz do HLSL (packing) e o viewmodel (c200) intacto
    check(!contains(hlsl, "mul(o.pos"), "sem mul() com matriz no shader gerado");
    check(!contains(hlsl, "float4x4"), "sem float4x4 no shader gerado");
    check(contains(hlsl, "vmScale : register(c200);"), "c200 (viewmodel) declarado como antes");

    // 5) o shader tem main() e a saída padrão do port (d0/t0/fog) — o tail não pode quebrar isso
    check(contains(hlsl, "VSOut main(VSIn I)"), "main() presente");
    check(contains(hlsl, "o.pos.w = w;"), "a divisão por w do port continua");
    check(contains(hlsl, "o.d0 = oD0; o.d1 = oD1;"), "cores de saída continuam");

    if (std::getenv("VR_SHOW_SHADER")) std::printf("\n--- HLSL gerado ---\n%s\n", hlsl.c_str());

    std::printf("\n%d verificações, %d falha(s)\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
