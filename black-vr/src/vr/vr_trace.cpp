// Rastro de execução e captura de falha de acesso. Ver vr_trace.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdarg>
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

// Em qual módulo está um endereço? É o que separa "erro nosso" de "erro do runtime".
void module_of(const void *addr, char *out, size_t n)
{
    out[0] = 0;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (!VirtualQuery(addr, &mbi, sizeof mbi)) return;
    char path[MAX_PATH] = {};
    if (GetModuleFileNameA((HMODULE)mbi.AllocationBase, path, MAX_PATH)) {
        const char *base = strrchr(path, '\\');
        snprintf(out, n, "%s + 0x%llx", base ? base + 1 : path,
                    (unsigned long long)((const char *)addr - (const char *)mbi.AllocationBase));
    }
}

LONG CALLBACK on_exception(EXCEPTION_POINTERS *info)
{
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_INT_DIVIDE_BY_ZERO)
        return EXCEPTION_CONTINUE_SEARCH;

    char mod[280];
    module_of(info->ExceptionRecord->ExceptionAddress, mod, sizeof mod);
    const ULONG_PTR rw = info->ExceptionRecord->ExceptionInformation[0];
    const ULONG_PTR at = info->ExceptionRecord->ExceptionInformation[1];
    line("");
    line("*** FALHA 0x%08lx em %p  (%s)", (unsigned long)code,
         info->ExceptionRecord->ExceptionAddress, mod[0] ? mod : "modulo desconhecido");
    if (code == EXCEPTION_ACCESS_VIOLATION)
        line("*** acesso %s ao endereco 0x%llx", rw == 0 ? "de leitura" : rw == 1 ? "de escrita" : "de execucao",
             (unsigned long long)at);
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

} // namespace trace
} // namespace vr
