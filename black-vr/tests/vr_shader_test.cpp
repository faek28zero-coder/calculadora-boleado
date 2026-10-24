// ============================================================
//  BLACK VR — tests/vr_shader_test.cpp
//
//  O outro lado do contrato: o trecho de shader que o módulo acrescenta ao vertex shader
//  traduzido pelo port (patch 0002, nv2a_vsh.cpp) tem de ser exatamente
//
//      o.pos = float4(dot(o.pos, vrRow0), dot(o.pos, vrRow1), dot(o.pos, vrRow2), dot(o.pos, vrRow3));
//
//  com vrRow0..3 declarados em c201..c204 — e o draw.cpp tem de enviar QUATRO registradores
//  a partir de 201 (um só deixaria vrRow1..3 com lixo). O upload é feito com
//  SetVertexShaderConstantF(201, m, 4), e como a matriz é de linha (`m[linha*4+coluna]`), a
//  linha i da matriz é justamente o registrador c(201+i): nada de packing do HLSL no caminho.
//
//  Este teste lê o FONTE do patch (não precisa de Windows, nem de shader compiler):
//
//      g++ -std=c++20 -O2 tests/vr_shader_test.cpp -o vr_shader_test && ./vr_shader_test
// ============================================================
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_failures;
int g_checks;

void check(bool ok, const std::string &what)
{
    g_checks++;
    if (ok) return;
    g_failures++;
    std::printf("  FALHOU: %s\n", what.c_str());
}

std::string read_file(const std::string &path)
{
    std::ifstream f(path);
    if (!f) {
        std::printf("  não consegui abrir %s\n", path.c_str());
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool contains(const std::string &hay, const std::string &needle, size_t *where = nullptr)
{
    const size_t at = hay.find(needle);
    if (where) *where = at;
    return at != std::string::npos;
}

/** Todas as linhas do patch que ADICIONAM texto (começam com um '+'). */
std::string added_lines(const std::string &patch)
{
    std::string out;
    std::istringstream in(patch);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line[0] == '+' && line.rfind("+++", 0) != 0) out += line.substr(1) + "\n";
    }
    return out;
}

} // namespace

int main(int argc, char **argv)
{
    const std::string root = argc > 1 ? argv[1] : "..";
    std::printf("== teste do contrato do shader (patch 0002 e draw.cpp) ==\n\n");

    const std::string patch_vsh = root + "/patches/0002-vertex-shader-c201.patch";
    const std::string patch_draw = root + "/patches/0001-draw-capture-e-replay.patch";

    const std::string pv = added_lines(read_file(patch_vsh));
    const std::string pd = added_lines(read_file(patch_draw));
    check(!pv.empty(), "patch 0002 encontrado e com adições");
    check(!pd.empty(), "patch 0001 encontrado e com adições");

    // 1) os quatro registradores, nas posições certas
    for (int i = 0; i < 4; i++) {
        std::ostringstream reg;
        reg << "float4 vrRow" << i << " : register(c" << (201 + i) << ");";
        check(contains(pv, reg.str()), "declaração " + reg.str());
    }

    // 2) a multiplicação por dots, na forma exata (sem float4x4/mul, que dependeria do packing)
    const char *want = "o.pos = float4(dot(o.pos, vrRow0), dot(o.pos, vrRow1), dot(o.pos, vrRow2), dot(o.pos, vrRow3));";
    check(contains(pv, want), "a linha de aplicação da matriz (quatro dots)");
    check(!contains(pv, "mul(o.pos"), "nada de mul() com matriz (packing column-major silencioso)");
    // o comentário do próprio patch cita float4x4 para explicar por que NÃO se usa; o que não pode
    // existir é uma declaração de shader com float4x4
    check(!contains(pv, "float4x4 ") && !contains(pv, "float4x4;"),
          "nada de float4x4 declarado: este shader nunca usou matriz, o packing seria ambíguo");

    // 3) ORDEM: a aplicação tem de vir depois do `o.pos` final. O patch 0001 (draw.cpp) não
    //    toca no `upload_vs_constants` além do c201; o 0002 só insere no fim do main(). O que dá
    //    para afirmar por leitura: (a) o trecho do shader termina em `o.pos.xy *= vmScale.xy;`
    //    antes da correção, e (b) o c200 continua sendo o viewmodel (na aba do draw.cpp).
    const std::string pv_all = read_file(patch_vsh);
    // a função vr_clip_tail() é definida no topo do arquivo (antes do vsh_to_hlsl), então a ordem
    // que importa é a do PONTO DE INSERÇÃO: a chamada tem de vir depois do clip space do jogo.
    size_t at_vm = 0, at_call = 0;
    const bool has_vm = contains(pv_all, "o.pos.xy *= vmScale.xy", &at_vm);
    const bool has_call = contains(pv_all, "+ vr_clip_tail() +", &at_call);
    check(has_vm, "o patch é inserido depois do clip space do jogo (o.pos.xy *= vmScale.xy)");
    check(has_call, "o patch chama o trecho de VR no fim do main() do shader");
    check(has_vm && has_call && at_call > at_vm, "a correção vem depois do clip space (ordem certa)");

    // 4) o c200 (viewmodel) continua no upload do port, intocado
    // o patch 0001 não reescreve o upload de c192..c200; ele só acrescenta o c201 depois. As linhas
    // de contexto trazem o c200 (o viewmodel), então a checagem certa é: nenhuma linha REMOVIDA
    // do bloco do c200.
    const std::string pd_all = read_file(patch_draw);
    bool removed_c200 = false;
    {
        std::istringstream in(pd_all);
        std::string line;
        while (std::getline(in, line))
            if (!line.empty() && line[0] == '-' && line.rfind("---", 0) != 0 && line.find("c200") != std::string::npos) {
                removed_c200 = true;
                break;
            }
    }
    check(!removed_c200, "o patch do draw.cpp não remove nada do c200 (viewmodel)");

    // 5) o draw.cpp envia QUATRO registradores a partir de 201
    check(contains(pd, "SetVertexShaderConstantF(201, vr::clip_matrix(vr::eye()), 4)"),
          "upload dos 4 registradores para o olho atual");
    check(contains(pd, "SetVertexShaderConstantF(201, identity, 4)"), "upload da identidade (4) fora do VR");
    check(!contains(pd, "SetVertexShaderConstantF(201, vr::clip_matrix(vr::eye()), 1)"),
          "nenhum upload de 1 registrador (deixaria vrRow1..3 com lixo)");

    // 6) a identidade está escrita em ordem de linha (diagonal principal)
    check(contains(pd, "{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}"), "identidade em ordem de linha");


    std::printf("\n%d verificações, %d falha(s)\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
