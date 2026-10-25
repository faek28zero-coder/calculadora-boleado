// Rastro de execução e captura de falha de acesso. Ver vr_trace.h.
//
// Regra de ouro: quando o programa estoura dentro do runtime do headset, o único dado que
// vale é ONDE. Por isso aqui saem três coisas a mais do que só "falhou em X":
//   1. o nome da DLL dona do endereço (antes o cálculo dava "black.exe + 0xffffffff..." porque
//      o endereço estava em memória livre — pista falsa);
//   2. a lista dos módulos de VR/carga gráfica presentes no processo;
//   3. um "backtrace de pobre": os endereços de código que estão na pilha no momento da falha,
//      cada um com a DLL e o deslocamento. Isso mostra quem chamou quem.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "vr/vr_trace.h"

namespace vr {
namespace trace {
namespace {

FILE *g_file;
// O rastro roda num processo de 32 bits e só é chamado da thread principal e do capturador
// de falha; uma trava simples de flag basta e não depende de struct do Windows.
volatile long g_busy;

void lock()
{
    while (InterlockedCompareExchange(&g_busy, 1, 0) != 0) Sleep(0);
}
void unlock() { InterlockedExchange(&g_busy, 0); }

// black.exe e blackrt.dll vivem na mesma pasta (port), onde o resto do programa escreve.
void trace_path(char *out, size_t n)
{
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    char *slash = strrchr(exe, '\\');
    if (slash) slash[1] = 0;
    snprintf(out, n, "%svr-trace.txt", exe);
}

// ---------------------------------------------------------------- módulos carregados
// Lista fixa (sem alocação dinâmica): o capturador de falha pode rodar com o heap quebrado.
const int kMaxMods = 320;
struct ModInfo {
    uintptr_t base;
    uintptr_t size;
    char name[MAX_PATH];
};
ModInfo g_mods[kMaxMods];
int g_mod_count;

void load_modules()
{
    if (g_mod_count) return;
    // Só existe a versão "W" no SDK moderno (e o port compila com UNICODE): pede W e converte.
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return;
    MODULEENTRY32W me = {};
    me.dwSize = sizeof me;
    if (Module32FirstW(snap, &me)) {
        do {
            if (g_mod_count >= kMaxMods) break;
            ModInfo &m = g_mods[g_mod_count++];
            m.base = (uintptr_t)me.modBaseAddr;
            m.size = (uintptr_t)me.modBaseSize;
            char path[MAX_PATH] = {};
            WideCharToMultiByte(CP_ACP, 0, me.szExePath, -1, path, (int)sizeof path - 1, nullptr, nullptr);
            const char *base = strrchr(path, '\\');
            snprintf(m.name, sizeof m.name, "%s", base ? base + 1 : path);
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
}

const ModInfo *find_mod(uintptr_t addr)
{
    for (int i = 0; i < g_mod_count; i++)
        if (addr >= g_mods[i].base && addr < g_mods[i].base + g_mods[i].size) return &g_mods[i];
    return nullptr;
}

/** Em qual módulo está um endereço? É o que separa "erro nosso" de "erro do runtime". */
void describe(uintptr_t addr, char *out, size_t n)
{
    out[0] = 0;
    if (const ModInfo *m = find_mod(addr)) {
        snprintf(out, n, "%s + 0x%llx", m->name, (unsigned long long)(addr - m->base));
        return;
    }
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery((const void *)addr, &mbi, sizeof mbi)) {
        switch (mbi.State) {
        case MEM_FREE: snprintf(out, n, "memoria livre (nao existe)"); return;
        case MEM_RESERVE: snprintf(out, n, "memoria reservada (nao e codigo)"); return;
        default: snprintf(out, n, "heap/pilha (nao e codigo)"); return;
        }
    }
    snprintf(out, n, "desconhecido");
}

/** Módulos que interessam para VR: runtime do headset, carga gráfica e o nosso. */
void dump_relevant_modules()
{
    load_modules();
    const char *keys[] = {"openxr", "oculus", "libovr", "meta", "nvwgf", "nvogl", "d3d11", "dxgi",
                          "amdvlk", "atio", "black", "vrclient", "steamvr"};
    for (int i = 0; i < g_mod_count; i++) {
        char low[MAX_PATH];
        snprintf(low, sizeof low, "%s", g_mods[i].name);
        for (char *p = low; *p; p++) *p = (char)((*p >= 'A' && *p <= 'Z') ? *p + 32 : *p);
        for (const char *k : keys) {
            if (strstr(low, k)) {
                line("  mod: %s base %p tamanho 0x%llx", g_mods[i].name, (void *)g_mods[i].base,
                     (unsigned long long)g_mods[i].size);
                break;
            }
        }
    }
}

/** "Backtrace de pobre": percorre a pilha e anota o que aponta para dentro de um módulo. */
void dump_stack(EXCEPTION_POINTERS *info)
{
    load_modules();
    CONTEXT *ctx = (CONTEXT *)info->ContextRecord;
    if (!ctx) return;
    line("*** registradores: eip %p esp %p ebp %p eax %p ebx %p ecx %p edx %p esi %p edi %p",
         (void *)ctx->Eip, (void *)ctx->Esp, (void *)ctx->Ebp, (void *)ctx->Eax, (void *)ctx->Ebx,
         (void *)ctx->Ecx, (void *)ctx->Edx, (void *)ctx->Esi, (void *)ctx->Edi);

    // Endereços na pilha que caem em código: os primeiros são as chamadas mais recentes.
    const uintptr_t *sp = (const uintptr_t *)ctx->Esp;
    int printed = 0;
    uintptr_t last_mod_base = 0;
    for (int i = 0; i < 512 && printed < 14; i++) {
        uintptr_t v = sp[i];
        if (v < 0x10000 || v > 0x7FFFFFFF) continue;
        const ModInfo *m = find_mod(v);
        if (!m) continue;
        if (m->base == last_mod_base && printed > 0 && i > 0 && (v & 0xFFF) == 0) continue;
        last_mod_base = m->base;
        line("*** pilha[%d]: %s + 0x%llx", i * 4, m->name, (unsigned long long)(v - m->base));
        printed++;
    }
}

LONG CALLBACK on_exception(EXCEPTION_POINTERS *info)
{
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_INT_DIVIDE_BY_ZERO)
        return EXCEPTION_CONTINUE_SEARCH;

    char mod[300];
    describe((uintptr_t)info->ExceptionRecord->ExceptionAddress, mod, sizeof mod);
    const ULONG_PTR rw = info->ExceptionRecord->ExceptionInformation[0];
    const ULONG_PTR at = info->ExceptionRecord->ExceptionInformation[1];
    line("");
    line("*** FALHA 0x%08lx em %p  (%s)", (unsigned long)code,
         info->ExceptionRecord->ExceptionAddress, mod);
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        char atmod[300];
        describe((uintptr_t)at, atmod, sizeof atmod);
        line("*** acesso %s ao endereco 0x%llx  (%s)", rw == 0 ? "de leitura" : rw == 1 ? "de escrita" : "de execucao",
             (unsigned long long)at, atmod);
    }
    dump_stack(info);
    line("*** modulos de VR/carga grafica carregados:");
    dump_relevant_modules();
    line("*** este rastro para aqui: o passo anterior é o que estourou");
    return EXCEPTION_CONTINUE_SEARCH; // deixa o Windows tratar (queremos o mesmo codigo de saida)
}

} // namespace

void init()
{
    if (g_file) return;
    char path[MAX_PATH];
    trace_path(path, sizeof path);
    g_file = fopen(path, "w");
    if (!g_file) return;
    setvbuf(g_file, nullptr, _IONBF, 0); // sem buffer: em crash nada se perde
    load_modules();
    AddVectoredExceptionHandler(1, on_exception);
    line("=== BLACK VR - rastro (este arquivo diz onde o programa para) ===");
}

void line(const char *fmt, ...)
{
    if (!g_file) return;
    lock();
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_file, fmt, ap);
    va_end(ap);
    fputc('\n', g_file);
    fflush(g_file);
    unlock();
}

void modules() { dump_relevant_modules(); }

} // namespace trace
} // namespace vr
