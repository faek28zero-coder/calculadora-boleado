// ============================================================
//  BLACK VR — sonda (v10) — MESMO CÓDIGO, DUAS VERSÕES
//
//  Programa SEPARADO do jogo que faz só a conversa com o headset: carrega o
//  conector, cria a instância, pega o sistema, cria o dispositivo D3D11 e tenta
//  criar a sessão. Sem jogo, sem gancho, sem D3D9.
//
//  v9 (32 bits) provou que o estouro em xrCreateSession acontece sem o jogo junto.
//  Agora o mesmo código compila em 64 bits (vr-sonda64.exe): o jogo em si é de
//  32 bits (é código de 2006, não dá pra virar 64), mas a parte do óculos pode
//  virar um programa de 64 bits separado — se ele funcionar, esse é o caminho.
//
//  O modo 10 é o teste que decide: monta o caminho inteiro (sessão, espaço,
//  troca de imagens) e desenha um quadro colorido no óculos. Se aparecer imagem
//  no óculos da versão 64, o caminho está aberto.
//
//  Uso:  vr-sonda.exe N     (grava sonda-N.txt ao lado; a de 64 bits grava sonda64-N.txt)
//    1 = adaptador do óculos + BGRA + API atual + debug_utils   (caminho do jogo)
//    2 = adaptador padrão do Windows
//    3 = adaptador do óculos sem BGRA
//    4 = para antes da sessão (só diz qual runtime respondeu)
//    5 = como 1, com COM inicializado nesta thread
//    6 = API 1.0.0 e SEM debug_utils
//    7 = API 1.0.0, sem debug_utils e com janela + fila de mensagens
//    8 = como 7, mas a sessão é criada numa thread própria (pilha de 8 MB)
//    9 = API 1.0.0 com debug_utils (isola a versão da API do mensageiro)
//   10 = caminho inteiro + quadro colorido no óculos por 20 s (use a de 64 bits)
//   11 = como 10, tentando a sessão 3 vezes
//   12 = como 10, com o mensageiro do runtime ligado (ele diz o motivo)
//    0 = roda 4 e depois 1
// ============================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h> // CoInitializeEx (o WIN32_LEAN_AND_MEAN não traz COM pelo windows.h)
#include <winreg.h>  // ActiveRuntime do registro
#include <tlhelp32.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <cstdarg>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <psapi.h>

#ifndef XR_USE_PLATFORM_WIN32
#define XR_USE_PLATFORM_WIN32 1
#endif
#ifndef XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D11 1
#endif
#include "openxr.h"
#include "openxr_platform.h"

namespace {

// ---------------------------------------------------------------- rastro
FILE *g_file;
int g_variant;
const char *g_step = "inicio";

void say(const char *fmt, ...)
{
    char linha[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(linha, sizeof linha, fmt, ap);
    va_end(ap);
    if (g_file) {
        fputs(linha, g_file);
        fputc('\n', g_file);
        fflush(g_file);
    }
    // tambem na tela: se o relatorio se perder, a janela ja mostra tudo
    fputs("[sonda] ", stdout);
    fputs(linha, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

// ---------------------------------------------------------------- módulos / memória
const int kMaxMods = 400;
struct ModInfo {
    uintptr_t base, size;
    char name[MAX_PATH];
};
ModInfo g_mods[kMaxMods];
int g_mod_count;

void load_modules(bool force = false)
{
    if (g_mod_count && !force) return;
    g_mod_count = 0;
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

const ModInfo *find_mod(uintptr_t a)
{
    for (int i = 0; i < g_mod_count; i++)
        if (a >= g_mods[i].base && a < g_mods[i].base + g_mods[i].size) return &g_mods[i];
    return nullptr;
}

void describe(uintptr_t a, char *out, size_t n)
{
    if (const ModInfo *m = find_mod(a)) {
        snprintf(out, n, "%s + 0x%llx", m->name, (unsigned long long)(a - m->base));
        return;
    }
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery((const void *)a, &mbi, sizeof mbi) && mbi.State == MEM_FREE) {
        snprintf(out, n, "memoria livre (nao existe)");
        return;
    }
    snprintf(out, n, "fora de modulo (heap/pilha)");
}

bool readable(uintptr_t a, size_t n)
{
    const uintptr_t end = a + n;
    while (a < end) {
        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery((const void *)a, &mbi, sizeof mbi)) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
        const uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= a) return false;
        a = next;
    }
    return true;
}

LONG CALLBACK on_fault(EXCEPTION_POINTERS *info)
{
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION) return EXCEPTION_CONTINUE_SEARCH;
    load_modules(true);
    char mod[300];
    describe((uintptr_t)info->ExceptionRecord->ExceptionAddress, mod, sizeof mod);
    say("");
    say("*** CAIU (0x%08lx) em %p  (%s)", (unsigned long)code, info->ExceptionRecord->ExceptionAddress, mod);
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        char atmod[300];
        describe((uintptr_t)info->ExceptionRecord->ExceptionInformation[1], atmod, sizeof atmod);
        say("*** acesso %s a 0x%llx  (%s)", info->ExceptionRecord->ExceptionInformation[0] == 0 ? "de leitura" : "de escrita",
            (unsigned long long)info->ExceptionRecord->ExceptionInformation[1], atmod);
    }
    CONTEXT *ctx = (CONTEXT *)info->ContextRecord;
    if (ctx) {
        // 32 e 64 bits guardam os registradores com nomes diferentes.
        const size_t w = sizeof(uintptr_t);
#if defined(_M_X64)
        say("*** thread %lu, rip %p rsp %p rbp %p", (unsigned long)GetCurrentThreadId(), (void *)ctx->Rip,
            (void *)ctx->Rsp, (void *)ctx->Rbp);
        say("*** rax %llx rbx %llx rcx %llx rdx %llx", (unsigned long long)ctx->Rax,
            (unsigned long long)ctx->Rbx, (unsigned long long)ctx->Rcx, (unsigned long long)ctx->Rdx);
        say("*** rsi %llx rdi %llx r8 %llx r9 %llx", (unsigned long long)ctx->Rsi,
            (unsigned long long)ctx->Rdi, (unsigned long long)ctx->R8, (unsigned long long)ctx->R9);
        const uintptr_t sp = (uintptr_t)ctx->Rsp;
#else
        say("*** thread %lu, eip %p esp %p ebp %p", (unsigned long)GetCurrentThreadId(), (void *)ctx->Eip,
            (void *)ctx->Esp, (void *)ctx->Ebp);
        say("*** eax %08lx ebx %08lx ecx %08lx edx %08lx esi %08lx edi %08lx", (unsigned long)ctx->Eax,
            (unsigned long)ctx->Ebx, (unsigned long)ctx->Ecx, (unsigned long)ctx->Edx, (unsigned long)ctx->Esi,
            (unsigned long)ctx->Edi);
        const uintptr_t sp = (uintptr_t)ctx->Esp;
#endif
        {
            char raw[512] = {};
            size_t used = 0;
            for (int i = 0; i < 24; i++) {
                const uintptr_t addr = sp + (uintptr_t)i * w;
                if (!readable(addr, w)) break;
                int n = snprintf(raw + used, sizeof raw - used, "%llx ",
                                 (unsigned long long)*(const uintptr_t *)addr);
                if (n <= 0) break;
                used += (size_t)n;
            }
            say("*** pilha crua: %s", raw);
        }
        int printed = 0;
        for (int i = 0; i < 320 && printed < 16; i++) {
            const uintptr_t addr = sp + (uintptr_t)i * w;
            if (!readable(addr, w)) {
                say("*** pilha: parou em %p (memoria nao mapeada)", (void *)addr);
                break;
            }
            const uintptr_t v = *(const uintptr_t *)addr;
            if (v < 0x10000) continue;
            const ModInfo *m = find_mod(v);
            if (!m) continue;
            say("*** pilha[%d]: %s + 0x%llx", (int)(i * (int)w), m->name, (unsigned long long)(v - m->base));
            printed++;
        }
    }
    say("*** passo em que estava: %s", g_step);
    say("sonda: FALHOU");
    return EXCEPTION_CONTINUE_SEARCH;
}

// ---------------------------------------------------------------- erros do OpenXR
const char *xr_name(XrResult r)
{
    switch (r) {
    case XR_SUCCESS: return "XR_SUCCESS";
    case XR_TIMEOUT_EXPIRED: return "XR_TIMEOUT_EXPIRED";
    case XR_SESSION_LOSS_PENDING: return "XR_SESSION_LOSS_PENDING";
    case XR_EVENT_UNAVAILABLE: return "XR_EVENT_UNAVAILABLE";
    case XR_ERROR_VALIDATION_FAILURE: return "XR_ERROR_VALIDATION_FAILURE";
    case XR_ERROR_RUNTIME_FAILURE: return "XR_ERROR_RUNTIME_FAILURE";
    case XR_ERROR_OUT_OF_MEMORY: return "XR_ERROR_OUT_OF_MEMORY";
    case XR_ERROR_API_VERSION_UNSUPPORTED: return "XR_ERROR_API_VERSION_UNSUPPORTED";
    case XR_ERROR_INITIALIZATION_FAILED: return "XR_ERROR_INITIALIZATION_FAILED";
    case XR_ERROR_FUNCTION_UNSUPPORTED: return "XR_ERROR_FUNCTION_UNSUPPORTED";
    case XR_ERROR_EXTENSION_NOT_PRESENT: return "XR_ERROR_EXTENSION_NOT_PRESENT";
    case XR_ERROR_CALL_ORDER_INVALID: return "XR_ERROR_CALL_ORDER_INVALID";
    case XR_ERROR_GRAPHICS_DEVICE_INVALID: return "XR_ERROR_GRAPHICS_DEVICE_INVALID";
    case XR_ERROR_FORM_FACTOR_UNAVAILABLE: return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
    case XR_ERROR_HANDLE_INVALID: return "XR_ERROR_HANDLE_INVALID";
    case XR_ERROR_INSTANCE_LOST: return "XR_ERROR_INSTANCE_LOST";
    case XR_ERROR_SESSION_LOST: return "XR_ERROR_SESSION_LOST";
    case XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING: return "XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING (-50)";
    default: return "erro nao catalogado";
    }
}

XrBool32 XRAPI_CALL on_message(XrDebugUtilsMessageSeverityFlagsEXT sev, XrDebugUtilsMessageTypeFlagsEXT,
                               const XrDebugUtilsMessengerCallbackDataEXT *d, void *)
{
    if (sev & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        say("  runtime[erro] %s: %s", d && d->functionName ? d->functionName : "", d && d->message ? d->message : "");
    else if (sev & XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        say("  runtime[aviso] %s: %s", d && d->functionName ? d->functionName : "", d && d->message ? d->message : "");
    return XR_FALSE;
}

// ---------------------------------------------------------------- runtime ativo
void dump_folder(const char *dir, const char *what)
{
    char mask[MAX_PATH * 2];
    snprintf(mask, sizeof mask, "%s*.dll", dir);
    WIN32_FIND_DATAA fd = {};
    HANDLE h = FindFirstFileA(mask, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        say("  %s: nao consegui listar (%s)", what, mask);
        return;
    }
    say("  %s (%s):", what, dir);
    do {
        say("    %-40s %8lu bytes", fd.cFileName, (unsigned long)fd.nFileSizeLow);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

void dump_active_runtime()
{
    // Um programa de 32 bits le a chave redirecionada (WOW6432Node); um de 64 bits
    // le a principal. A ordem muda conforme a versao, mas as duas sao mostradas.
#if defined(_M_X64)
    const char *keys[] = {"SOFTWARE\\Khronos\\OpenXR\\1", "SOFTWARE\\WOW6432Node\\Khronos\\OpenXR\\1"};
#else
    const char *keys[] = {"SOFTWARE\\WOW6432Node\\Khronos\\OpenXR\\1", "SOFTWARE\\Khronos\\OpenXR\\1"};
#endif
    say("registro procurado primeiro: %s (%d bits)", keys[0], (int)(sizeof(void *) * 8));
    char folder[MAX_PATH] = {};
    for (const char *key : keys) {
        HKEY h = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &h) != ERROR_SUCCESS) {
            say("registro: %s -> nao existe", key);
            continue;
        }
        char value[MAX_PATH * 2] = {};
        DWORD size = sizeof value, type = 0;
        if (RegQueryValueExA(h, "ActiveRuntime", nullptr, &type, (LPBYTE)value, &size) == ERROR_SUCCESS && size) {
            say("registro: %s -> %s", key, value);
            if (!folder[0]) {
                snprintf(folder, sizeof folder, "%s", value);
                char *slash = strrchr(folder, '\\');
                if (slash) slash[1] = 0;
            }
            HANDLE f = CreateFileA(value, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            if (f == INVALID_HANDLE_VALUE) {
                say("  manifesto: NAO ABRIU (erro %lu) - o runtime apontado nao existe", GetLastError());
            } else {
                char buf[4096] = {};
                DWORD read = 0;
                ReadFile(f, buf, (DWORD)sizeof buf - 1, &read, nullptr);
                CloseHandle(f);
                say("  manifesto (%lu bytes):", (unsigned long)read);
                char *line = buf;
                for (char *p = buf;; p++) {
                    if (*p == '\n' || *p == 0) {
                        bool end = (*p == 0);
                        *p = 0;
                        while (*line == ' ' || *line == '\t') line++;
                        if (*line) say("    %s", line);
                        line = p + 1;
                        if (end) break;
                    }
                }
            }
        } else {
            say("registro: %s -> sem ActiveRuntime", key);
        }
        RegCloseKey(h);
    }
    if (folder[0]) {
        dump_folder(folder, "DLLs na pasta do runtime");
        char sub[MAX_PATH * 2];
        snprintf(sub, sizeof sub, "%sclient-plugins\\x86\\", folder);
        if (GetFileAttributesA(sub) != INVALID_FILE_ATTRIBUTES)
            dump_folder(sub, "DLLs de 32 bits do runtime (client-plugins\\x86)");
        else
            say("  client-plugins\\x86: nao existe nesta instalacao");
        snprintf(sub, sizeof sub, "%sclient-plugins\\amd64\\", folder);
        if (GetFileAttributesA(sub) != INVALID_FILE_ATTRIBUTES)
            dump_folder(sub, "DLLs de 64 bits do runtime (client-plugins\\amd64)");
        // API antiga da Oculus: a de 32 bits seria o plano B do jogo; a de 64 bits
        // mostraria que existe caminho de 64 bits sem OpenXR.
        const char *legacy[] = {"LibOVRRT32_1.dll", "LibOVRRT64_1.dll"};
        for (const char *name : legacy) {
            char path[MAX_PATH * 2];
            snprintf(path, sizeof path, "%s%s", folder, name);
            HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            say("API antiga da Oculus (%s): %s", name, f != INVALID_HANDLE_VALUE ? "EXISTE" : "nao existe");
            if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
        }
    }
}

// O runtime reclamou de "VirtualLock failed (1453)" e de "StartThread failed":
// os dois acontecem quando falta memoria/commit. Isto mostra os numeros.
void dump_memory()
{
    MEMORYSTATUSEX ms = {};
    ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms)) {
        say("memoria do PC: %llu MB no total, %llu MB livres; commit %llu de %llu MB",
            (unsigned long long)(ms.ullTotalPhys >> 20), (unsigned long long)(ms.ullAvailPhys >> 20),
            (unsigned long long)((ms.ullTotalPageFile - ms.ullAvailPageFile) >> 20),
            (unsigned long long)(ms.ullTotalPageFile >> 20));
        say("carga da memoria: %lu%%", (unsigned long)ms.dwMemoryLoad);
        if (ms.ullTotalPhys <= ms.ullTotalPageFile)
            say("  (sem arquivo de paginacao: o commit e igual a memoria fisica)");
    }
    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof pmc)) {
        say("este programa: %llu MB em uso, pico %llu MB",
            (unsigned long long)(pmc.WorkingSetSize >> 20), (unsigned long long)(pmc.PeakWorkingSetSize >> 20));
    }
    DWORD handles = 0;
    if (GetProcessHandleCount(GetCurrentProcess(), &handles)) say("descritores abertos: %lu", (unsigned long)handles);
    int threads = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te = {};
        te.dwSize = sizeof te;
        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID == GetCurrentProcessId()) threads++;
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
    }
    say("threads deste programa: %d", threads);
}

void dump_modules()
{
    load_modules(true);
    const char *keys[] = {"openxr", "oculus", "libovr", "meta", "nvwgf", "nvogl", "d3d11", "dxgi", "d3d12"};
    for (int i = 0; i < g_mod_count; i++) {
        char low[MAX_PATH];
        snprintf(low, sizeof low, "%s", g_mods[i].name);
        for (char *p = low; *p; p++) *p = (char)((*p >= 'A' && *p <= 'Z') ? *p + 32 : *p);
        for (const char *k : keys)
            if (strstr(low, k)) {
                say("  mod: %s base %p tamanho 0x%llx", g_mods[i].name, (void *)g_mods[i].base,
                    (unsigned long long)g_mods[i].size);
                break;
            }
    }
}

// ---------------------------------------------------------------- uma rodada
struct Opt {
    const char *nome;
    bool usar_luid = true;      // adaptador do óculos (senão o padrão do Windows)
    bool bgra = true;           // flag D3D11_CREATE_DEVICE_BGRA_SUPPORT
    bool api_1_0 = false;       // pedir API 1.0.0 em vez da atual (1.1)
    bool debug_utils = true;    // ligar mensageiro do runtime
    bool janela = false;        // criar janela + fila de mensagens antes
    bool ate_instancia = false; // parar antes da sessão
    bool com = false;           // CoInitializeEx nesta thread
    bool desenhar = false;      // modo 10: montar o caminho inteiro e desenhar no óculos
    int tentativas = 1;         // modo 11: quantas vezes tentar criar a sessao
};

struct Funcoes {
    PFN_xrGetInstanceProcAddr gipa = nullptr;
    PFN_xrEnumerateInstanceExtensionProperties enum_ext = nullptr;
    PFN_xrCreateInstance create_instance = nullptr;
};

void make_window()
{
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "blackvr_sonda";
    RegisterClassA(&wc);
    HWND w = CreateWindowExA(0, wc.lpszClassName, "sonda", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr,
                             wc.hInstance, nullptr);
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    say("janela criada (%p) e fila de mensagens drenada", (void *)w);
}


// ---------------------------------------------------------------- modo 10: quadro no óculos
// O caminho completo de um app de VR de verdade: esperar o estado "pronto", criar o
// espaco de referencia, criar a troca de imagens (uma por olho) e entregar quadros.
// Se aparecer imagem no oculos, o caminho esta aberto para o jogo usar.
struct FuncoesDesenho {
    PFN_xrPollEvent poll = nullptr;
    PFN_xrEnumerateViewConfigurationViews enum_views = nullptr;
    PFN_xrCreateReferenceSpace create_space = nullptr;
    PFN_xrEnumerateSwapchainFormats enum_formats = nullptr;
    PFN_xrCreateSwapchain create_chain = nullptr;
    PFN_xrEnumerateSwapchainImages enum_images = nullptr;
    PFN_xrAcquireSwapchainImage acquire = nullptr;
    PFN_xrWaitSwapchainImage wait_img = nullptr;
    PFN_xrReleaseSwapchainImage release = nullptr;
    PFN_xrWaitFrame wait_frame = nullptr;
    PFN_xrBeginFrame begin_frame = nullptr;
    PFN_xrEndFrame end_frame = nullptr;
    PFN_xrLocateViews locate = nullptr;
    PFN_xrBeginSession begin = nullptr;
    PFN_xrEndSession end = nullptr;
};

const int kMaxImagens = 8;
const int kMaxOlhos = 2;

int desenhar_no_oculos(const Funcoes &f, XrInstance inst, XrSession sess, ID3D11Device *dev,
                       ID3D11DeviceContext *ctx, XrSystemId system)
{
    FuncoesDesenho p;
    bool ok = true;
    auto bind = [&](auto &fn, const char *name) {
        fn = nullptr;
        if (XR_FAILED(f.gipa(inst, name, (PFN_xrVoidFunction *)&fn)) || !fn) {
            say("FALHOU: o runtime nao entregou %s", name);
            ok = false;
        }
    };
    bind(p.poll, "xrPollEvent");
    bind(p.enum_views, "xrEnumerateViewConfigurationViews");
    bind(p.create_space, "xrCreateReferenceSpace");
    bind(p.enum_formats, "xrEnumerateSwapchainFormats");
    bind(p.create_chain, "xrCreateSwapchain");
    bind(p.enum_images, "xrEnumerateSwapchainImages");
    bind(p.acquire, "xrAcquireSwapchainImage");
    bind(p.wait_img, "xrWaitSwapchainImage");
    bind(p.release, "xrReleaseSwapchainImage");
    bind(p.wait_frame, "xrWaitFrame");
    bind(p.begin_frame, "xrBeginFrame");
    bind(p.end_frame, "xrEndFrame");
    bind(p.locate, "xrLocateViews");
    bind(p.begin, "xrBeginSession");
    bind(p.end, "xrEndSession");
    if (!ok) return 1;

    g_step = "tamanho dos olhos";
    uint32_t vcount = 0;
    XrViewConfigurationView vcv[kMaxOlhos] = {{XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW}};
    XrResult r = p.enum_views(inst, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, kMaxOlhos, &vcount, vcv);
    if (XR_FAILED(r) || vcount < 1 || vcount > kMaxOlhos) {
        say("FALHOU: xrEnumerateViewConfigurationViews: %s (%d)", xr_name(r), (int)r);
        return 1;
    }
    say("olhos: %u, painel recomendado %ux%u por olho", vcount, (unsigned)vcv[0].recommendedImageRectWidth,
        (unsigned)vcv[0].recommendedImageRectHeight);

    g_step = "esperar estado pronto";
    bool comecou = false;
    const DWORD t_inicio = GetTickCount();
    while (!comecou && GetTickCount() - t_inicio < 15000) {
        XrEventDataBuffer ev = {XR_TYPE_EVENT_DATA_BUFFER};
        while (XR_SUCCEEDED(p.poll(inst, &ev))) {
            if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const XrEventDataSessionStateChanged *ch = (const XrEventDataSessionStateChanged *)&ev;
                say("estado da sessao: %d", (int)ch->state);
                if (ch->state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo bi = {XR_TYPE_SESSION_BEGIN_INFO};
                    bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    r = p.begin(sess, &bi);
                    if (XR_FAILED(r)) {
                        say("FALHOU: xrBeginSession: %s (%d)", xr_name(r), (int)r);
                        return 1;
                    }
                    comecou = true;
                } else if (ch->state == XR_SESSION_STATE_EXITING || ch->state == XR_SESSION_STATE_LOSS_PENDING) {
                    say("FALHOU: o runtime pediu para sair antes de comecar");
                    return 1;
                }
            }
            memset(&ev, 0, sizeof ev);
            ev.type = XR_TYPE_EVENT_DATA_BUFFER;
        }
        Sleep(10);
    }
    if (!comecou) {
        say("FALHOU: a sessao nunca ficou pronta (o oculos esta na cabeca e o Link aberto?)");
        return 1;
    }
    say("sessao comecou (o oculos esta recebendo)");

    g_step = "espaco de referencia";
    XrReferenceSpaceCreateInfo rsci = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rsci.poseInReferenceSpace.orientation.w = 1.f;
    XrSpace espaco = XR_NULL_HANDLE;
    r = p.create_space(sess, &rsci, &espaco);
    if (XR_FAILED(r)) {
        say("FALHOU: xrCreateReferenceSpace: %s (%d)", xr_name(r), (int)r);
        return 1;
    }

    g_step = "formato das imagens";
    uint32_t fmt_count = 0;
    if (XR_FAILED(p.enum_formats(sess, 0, &fmt_count, nullptr)) || !fmt_count) {
        say("FALHOU: xrEnumerateSwapchainFormats");
        return 1;
    }
    std::vector<int64_t> formats(fmt_count);
    p.enum_formats(sess, fmt_count, &fmt_count, formats.data());
    int64_t escolhido = formats[0];
    for (uint32_t i = 0; i < fmt_count; i++)
        if (formats[i] == (int64_t)DXGI_FORMAT_R8G8B8A8_UNORM) escolhido = formats[i];
    say("formato escolhido: %lld (de %u oferecidos)", (long long)escolhido, (unsigned)fmt_count);

    g_step = "troca de imagens";
    struct Olho {
        XrSwapchain chain = XR_NULL_HANDLE;
        uint32_t w = 0, h = 0;
        ID3D11RenderTargetView *rtv[kMaxImagens] = {};
    };
    Olho olho[kMaxOlhos];
    for (uint32_t e = 0; e < vcount; e++) {
        XrSwapchainCreateInfo ci = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        ci.format = escolhido;
        ci.sampleCount = 1;
        ci.width = vcv[e].recommendedImageRectWidth;
        ci.height = vcv[e].recommendedImageRectHeight;
        ci.faceCount = 1;
        ci.arraySize = 1;
        ci.mipCount = 1;
        olho[e].w = ci.width;
        olho[e].h = ci.height;
        r = p.create_chain(sess, &ci, &olho[e].chain);
        if (XR_FAILED(r)) {
            say("FALHOU: xrCreateSwapchain (olho %u): %s (%d)", e, xr_name(r), (int)r);
            return 1;
        }
        uint32_t n_img = 0;
        if (XR_FAILED(p.enum_images(olho[e].chain, 0, &n_img, nullptr)) || !n_img) {
            say("FALHOU: xrEnumerateSwapchainImages (olho %u)", e);
            return 1;
        }
        if (n_img > (uint32_t)kMaxImagens) n_img = kMaxImagens;
        XrSwapchainImageD3D11KHR imgs[kMaxImagens] = {};
        for (uint32_t i = 0; i < n_img; i++) imgs[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
        r = p.enum_images(olho[e].chain, n_img, &n_img, (XrSwapchainImageBaseHeader *)imgs);
        if (XR_FAILED(r)) {
            say("FALHOU: xrEnumerateSwapchainImages (olho %u): %s (%d)", e, xr_name(r), (int)r);
            return 1;
        }
        for (uint32_t i = 0; i < n_img; i++)
            if (imgs[i].texture) dev->CreateRenderTargetView(imgs[i].texture, nullptr, &olho[e].rtv[i]);
        say("olho %u: %ux%u, %u imagens", e, (unsigned)olho[e].w, (unsigned)olho[e].h, (unsigned)n_img);
    }

    g_step = "desenhar quadros";
    say("");
    say("--- 20 segundos desenhando. OLHE PARA O OCULOS: deve aparecer um quadro");
    say("    colorido mudando de cor (vermelho num olho, verde no outro). ---");
    const DWORD t_quadros = GetTickCount();
    int quadros = 0;
    while (GetTickCount() - t_quadros < 20000) {
        XrFrameWaitInfo fwi = {XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState fs = {XR_TYPE_FRAME_STATE};
        if (XR_FAILED(p.wait_frame(sess, &fwi, &fs))) {
            say("FALHOU: xrWaitFrame");
            break;
        }
        XrFrameBeginInfo fbi = {XR_TYPE_FRAME_BEGIN_INFO};
        if (XR_FAILED(p.begin_frame(sess, &fbi))) {
            say("FALHOU: xrBeginFrame");
            break;
        }
        XrViewLocateInfo vli = {XR_TYPE_VIEW_LOCATE_INFO};
        vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        vli.displayTime = fs.predictedDisplayTime;
        vli.space = espaco;
        XrViewState vs = {XR_TYPE_VIEW_STATE};
        XrView views[kMaxOlhos] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
        uint32_t vistas = 0;
        p.locate(sess, &vli, &vs, vcount, &vistas, views);
        if (vistas < vcount) vistas = vcount;

        const float t = (float)(GetTickCount() - t_quadros) / 1000.f;
        const float pulso = 0.5f + 0.5f * sinf(t * 2.f);
        XrCompositionLayerProjectionView lpv[kMaxOlhos] = {{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
                                                           {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
        bool desenhou = true;
        for (uint32_t e = 0; e < vistas; e++) {
            uint32_t idx = 0;
            XrSwapchainImageAcquireInfo ai = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            if (XR_FAILED(p.acquire(olho[e].chain, &ai, &idx))) {
                desenhou = false;
                break;
            }
            XrSwapchainImageWaitInfo wi = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            wi.timeout = XR_INFINITE_DURATION;
            if (XR_FAILED(p.wait_img(olho[e].chain, &wi))) {
                desenhou = false;
                break;
            }
            float cor[4] = {0.05f, 0.05f, 0.05f, 1.f};
            if (e == 0) {
                cor[0] = pulso;
                cor[1] = 0.10f;
            } else {
                cor[1] = pulso;
                cor[2] = 0.10f;
            }
            if (idx < (uint32_t)kMaxImagens && olho[e].rtv[idx]) ctx->ClearRenderTargetView(olho[e].rtv[idx], cor);
            XrSwapchainImageReleaseInfo ri = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            p.release(olho[e].chain, &ri);
            lpv[e].pose = views[e].pose;
            lpv[e].fov = views[e].fov;
            lpv[e].subImage.swapchain = olho[e].chain;
            lpv[e].subImage.imageRect.offset.x = 0;
            lpv[e].subImage.imageRect.offset.y = 0;
            lpv[e].subImage.imageRect.extent.width = (int32_t)olho[e].w;
            lpv[e].subImage.imageRect.extent.height = (int32_t)olho[e].h;
        }
        if (desenhou) {
            XrCompositionLayerProjection camada = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
            camada.space = espaco;
            camada.viewCount = vistas;
            camada.views = lpv;
            const XrCompositionLayerBaseHeader *camadas[1] = {(const XrCompositionLayerBaseHeader *)&camada};
            XrFrameEndInfo fei = {XR_TYPE_FRAME_END_INFO};
            fei.displayTime = fs.predictedDisplayTime;
            fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            fei.layerCount = 1;
            fei.layers = camadas;
            p.end_frame(sess, &fei);
            quadros++;
        }
        Sleep(1);
    }
    say("quadros entregues ao oculos: %d", quadros);
    if (p.end) p.end(sess);
    if (quadros < 30) {
        say("FALHOU: poucos quadros (%d) - a imagem nao chegou no oculos", quadros);
        return 1;
    }
    say("sonda: OK (deu para desenhar no oculos)");
    return 0;
}

int rodada(const Opt &o, const Funcoes &f)
{
    say("");
    say("======== modo: %s ========", o.nome);

    if (o.com) {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        say("COM nesta thread: 0x%08lx", (unsigned long)hr);
    }
    if (o.janela) make_window();

    g_step = "listar extensoes";
    uint32_t n = 0;
    XrResult r = f.enum_ext(nullptr, 0, &n, nullptr);
    if (XR_FAILED(r)) {
        say("FALHOU: extensoes: %s (%d)", xr_name(r), (int)r);
        return 1;
    }
    bool have_d3d11 = false, have_debug = false;
    {
        char exts[4096] = {};
        XrExtensionProperties *props = (XrExtensionProperties *)calloc(n ? n : 1, sizeof(XrExtensionProperties));
        for (uint32_t i = 0; i < n; i++) props[i].type = XR_TYPE_EXTENSION_PROPERTIES;
        f.enum_ext(nullptr, n, &n, props);
        for (uint32_t i = 0; i < n; i++) {
            if (!strcmp(props[i].extensionName, "XR_KHR_D3D11_enable")) have_d3d11 = true;
            if (!strcmp(props[i].extensionName, "XR_EXT_debug_utils")) have_debug = true;
            if (strlen(exts) + strlen(props[i].extensionName) + 2 < sizeof exts) {
                strcat(exts, props[i].extensionName);
                strcat(exts, " ");
            }
        }
        free(props);
        say("extensoes (%u): %s", n, exts);
    }
    if (!have_d3d11) {
        say("FALHOU: o runtime nao oferece XR_KHR_D3D11_enable");
        return 1;
    }
    const bool dbg = o.debug_utils && have_debug;
    say("D3D11: sim; debug_utils pedido: %s, disponivel: %s", o.debug_utils ? "sim" : "nao", have_debug ? "sim" : "nao");

    g_step = "criar instancia";
    const char *wanted[2];
    uint32_t wanted_n = 0;
    wanted[wanted_n++] = "XR_KHR_D3D11_enable";
    if (dbg) wanted[wanted_n++] = "XR_EXT_debug_utils";
    XrInstance instance = XR_NULL_HANDLE;
    XrInstanceCreateInfo ci = {XR_TYPE_INSTANCE_CREATE_INFO};
    snprintf(ci.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "BLACK VR sonda");
    ci.applicationInfo.applicationVersion = 1;
    snprintf(ci.applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, "black-pc");
    ci.applicationInfo.engineVersion = 1;
    ci.applicationInfo.apiVersion = o.api_1_0 ? XR_MAKE_VERSION(1, 0, 0) : XR_CURRENT_API_VERSION;
    ci.enabledExtensionCount = wanted_n;
    ci.enabledExtensionNames = wanted;
    say("API pedida: %u.%u.%u", (unsigned)XR_VERSION_MAJOR(ci.applicationInfo.apiVersion),
        (unsigned)XR_VERSION_MINOR(ci.applicationInfo.apiVersion),
        (unsigned)XR_VERSION_PATCH(ci.applicationInfo.apiVersion));
    r = f.create_instance(&ci, &instance);
    if (XR_FAILED(r)) {
        say("FALHOU: xrCreateInstance: %s (%d)", xr_name(r), (int)r);
        return 1;
    }
    say("instancia criada (%p)", (void *)instance);

    PFN_xrGetInstanceProperties get_props = nullptr;
    f.gipa(instance, "xrGetInstanceProperties", (PFN_xrVoidFunction *)&get_props);
    if (get_props) {
        XrInstanceProperties ip = {XR_TYPE_INSTANCE_PROPERTIES};
        if (XR_SUCCEEDED(get_props(instance, &ip)))
            say("runtime: %s versao %u.%u.%u", ip.runtimeName, (unsigned)XR_VERSION_MAJOR(ip.runtimeVersion),
                (unsigned)XR_VERSION_MINOR(ip.runtimeVersion), (unsigned)XR_VERSION_PATCH(ip.runtimeVersion));
    }

    if (dbg) {
        PFN_xrCreateDebugUtilsMessengerEXT mk = nullptr;
        f.gipa(instance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction *)&mk);
        if (mk) {
            XrDebugUtilsMessengerCreateInfoEXT dci = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            dci.messageSeverities =
                XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            dci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dci.userCallback = on_message;
            XrDebugUtilsMessengerEXT msgr = XR_NULL_HANDLE;
            if (XR_SUCCEEDED(mk(instance, &dci, &msgr))) say("mensageiro do runtime: ligado");
        }
    }

    if (o.ate_instancia) {
        say("parando aqui de proposito (sem sessao).");
        say("sonda: OK ate aqui");
        return 0;
    }

    g_step = "xrGetSystem";
    PFN_xrGetSystem get_system = nullptr;
    PFN_xrGetSystemProperties get_sys_props = nullptr;
    PFN_xrGetD3D11GraphicsRequirementsKHR get_req = nullptr;
    PFN_xrCreateSession create_session = nullptr;
    f.gipa(instance, "xrGetSystem", (PFN_xrVoidFunction *)&get_system);
    f.gipa(instance, "xrGetSystemProperties", (PFN_xrVoidFunction *)&get_sys_props);
    f.gipa(instance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction *)&get_req);
    f.gipa(instance, "xrCreateSession", (PFN_xrVoidFunction *)&create_session);
    if (!get_system || !get_sys_props || !get_req || !create_session) {
        say("FALHOU: o runtime nao entregou as funcoes basicas de sessao");
        return 1;
    }
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrSystemGetInfo sgi = {XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    r = get_system(instance, &sgi, &system);
    if (XR_FAILED(r)) {
        say("FALHOU: xrGetSystem: %s (%d) - headset conectado e Link aberto?", xr_name(r), (int)r);
        return 1;
    }
    say("sistema: id %llu", (unsigned long long)system);

    XrSystemProperties sp = {XR_TYPE_SYSTEM_PROPERTIES};
    r = get_sys_props(instance, system, &sp);
    if (XR_FAILED(r)) {
        say("FALHOU: xrGetSystemProperties: %s (%d)", xr_name(r), (int)r);
        return 1;
    }
    say("headset: %s (vendor 0x%x)", sp.systemName, sp.vendorId);

    g_step = "xrGetD3D11GraphicsRequirementsKHR";
    XrGraphicsRequirementsD3D11KHR req = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    r = get_req(instance, system, &req);
    if (XR_FAILED(r)) {
        say("FALHOU: requisitos D3D11: %s (%d)", xr_name(r), (int)r);
        return 1;
    }
    say("requisitos: luid %08lx:%08lx, feature level minimo 0x%x", (unsigned long)req.adapterLuid.HighPart,
        (unsigned long)req.adapterLuid.LowPart, (unsigned)req.minFeatureLevel);

    g_step = "criar dispositivo D3D11";
    HMODULE d3d11 = LoadLibraryA("d3d11.dll");
    auto create_dev = d3d11 ? (PFN_D3D11_CREATE_DEVICE)GetProcAddress(d3d11, "D3D11CreateDevice") : nullptr;
    if (!create_dev) {
        say("FALHOU: d3d11.dll/D3D11CreateDevice indisponivel");
        return 1;
    }

    IDXGIAdapter1 *adapter = nullptr;
    IDXGIFactory1 *factory = nullptr;
    if (o.usar_luid && SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&factory))) {
        for (UINT i = 0;; i++) {
            IDXGIAdapter1 *cand = nullptr;
            if (factory->EnumAdapters1(i, &cand) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC1 d = {};
            cand->GetDesc1(&d);
            if (d.AdapterLuid.LowPart == req.adapterLuid.LowPart && d.AdapterLuid.HighPart == req.adapterLuid.HighPart) {
                adapter = cand;
                char name[128] = {};
                for (int k = 0; k < 64 && d.Description[k]; k++) name[k] = (char)d.Description[k];
                say("adaptador escolhido: %s", name);
                break;
            }
            cand->Release();
        }
        factory->Release();
        if (!adapter) say("adaptador do luid NAO encontrado; caindo para o padrao");
    }

    D3D_FEATURE_LEVEL want = req.minFeatureLevel ? req.minFeatureLevel : D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_9_1;
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *ctx = nullptr;
    UINT flags = o.bgra ? (UINT)D3D11_CREATE_DEVICE_BGRA_SUPPORT : 0u;
    HRESULT hr = create_dev(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, &want,
                            1, D3D11_SDK_VERSION, &device, &got, &ctx);
    if (adapter) adapter->Release();
    say("D3D11CreateDevice: hr 0x%08lx, feature level 0x%x (pedido 0x%x, flags %u)", (unsigned long)hr, (unsigned)got,
        (unsigned)want, flags);
    if (FAILED(hr)) {
        say("FALHOU: sem dispositivo D3D11");
        return 1;
    }

    dump_modules();
    say("");
    say("--- agora o passo que quebra: xrCreateSession (thread %lu) ---", (unsigned long)GetCurrentThreadId());
    g_step = "xrCreateSession";
    XrGraphicsBindingD3D11KHR binding = {XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device = device;
    XrSessionCreateInfo sci = {XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &binding;
    sci.systemId = system;
    dump_memory();
    XrSession session = XR_NULL_HANDLE;
    for (int tentativa = 1; tentativa <= o.tentativas; tentativa++) {
        if (tentativa > 1) {
            say("tentando de novo (tentativa %d de %d) depois de 2 s...", tentativa, o.tentativas);
            Sleep(2000);
        }
        r = create_session(instance, &sci, &session);
        if (XR_SUCCEEDED(r)) break;
        say("xrCreateSession (tentativa %d): %s (%d)", tentativa, xr_name(r), (int)r);
    }
    if (XR_FAILED(r)) {
        say("sonda: FALHOU");
        return 1;
    }
    say("SESSAO CRIADA (%p)", (void *)session);
    int rc = 0;
    if (o.desenhar) {
        g_step = "desenhar no oculos";
        rc = desenhar_no_oculos(f, instance, session, device, ctx, system);
    } else {
        say("sonda: OK");
    }

    PFN_xrDestroySession destroy_session = nullptr;
    PFN_xrDestroyInstance destroy_instance = nullptr;
    f.gipa(instance, "xrDestroySession", (PFN_xrVoidFunction *)&destroy_session);
    f.gipa(instance, "xrDestroyInstance", (PFN_xrVoidFunction *)&destroy_instance);
    if (destroy_session) destroy_session(session);
    if (destroy_instance) destroy_instance(instance);
    if (ctx) ctx->Release();
    if (device) device->Release();
    return rc;
}

// O modo 8 roda a rodada inteira numa thread própria (pilha de 8 MB), como um app de verdade.
struct ThreadArgs {
    Opt opt;
    Funcoes fun;
    int rc = 99;
};

DWORD WINAPI thread_body(LPVOID p)
{
    ThreadArgs *a = (ThreadArgs *)p;
    a->rc = rodada(a->opt, a->fun);
    return 0;
}

int rodada_em_thread(const Opt &o, const Funcoes &f)
{
    ThreadArgs a;
    a.opt = o;
    a.fun = f;
    HANDLE t = CreateThread(nullptr, 8u * 1024u * 1024u, thread_body, &a, 0, nullptr);
    if (!t) {
        say("FALHOU: nao consegui criar a thread do modo 8");
        return 1;
    }
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
    return a.rc;
}

Opt opt_da_variante(int v)
{
    Opt o;
    switch (v) {
    case 1: o.nome = "1 adaptador do oculos + BGRA + API atual + debug_utils"; break;
    case 2: o.nome = "2 adaptador padrao do Windows"; o.usar_luid = false; break;
    case 3: o.nome = "3 adaptador do oculos sem BGRA"; o.bgra = false; break;
    case 4: o.nome = "4 so ate a instancia (nunca deve cair)"; o.ate_instancia = true; break;
    case 5: o.nome = "5 como 1, com COM inicializado"; o.com = true; break;
    case 6: o.nome = "6 API 1.0.0 e SEM debug_utils"; o.api_1_0 = true; o.debug_utils = false; break;
    case 7:
        o.nome = "7 API 1.0.0, sem debug_utils, com janela e fila de mensagens";
        o.api_1_0 = true;
        o.debug_utils = false;
        o.janela = true;
        break;
    case 8:
        o.nome = "8 como 7, em thread propria com pilha de 8 MB";
        o.api_1_0 = true;
        o.debug_utils = false;
        o.janela = true;
        break;
    case 9: o.nome = "9 API 1.0.0 com debug_utils"; o.api_1_0 = true; break;
    case 10:
        o.nome = "10 caminho inteiro + quadro colorido no oculos por 20 s";
        o.api_1_0 = true;
        o.debug_utils = false;
        o.desenhar = true;
        break;
    case 11:
        o.nome = "11 como 10, mas tenta a sessao 3 vezes";
        o.api_1_0 = true;
        o.debug_utils = false;
        o.desenhar = true;
        o.tentativas = 3;
        break;
    case 12:
        o.nome = "12 como 10, com o mensageiro do runtime ligado (ele diz o motivo)";
        o.api_1_0 = true;
        o.debug_utils = true;
        o.desenhar = true;
        break;
    default: o.nome = "1 (padrao)"; break;
    }
    return o;
}

} // namespace

int main(int argc, char **argv)
{
    g_variant = argc > 1 ? atoi(argv[1]) : 1;

    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    char *slash = strrchr(exe, '\\');
    if (slash) slash[1] = 0;
    char path[MAX_PATH];
    // 32 e 64 bits gravam arquivos diferentes para os dois relatorios conviverem
    const char *tag = sizeof(void *) == 8 ? "sonda64-" : "sonda-";
    snprintf(path, sizeof path, "%s%s%d.txt", exe, tag, g_variant);
    g_file = fopen(path, "w");
    if (!g_file) return 2;

    load_modules(true);
    AddVectoredExceptionHandler(1, on_fault);

    say("=== BLACK VR - sonda %d - programa de %d bits (teste separado do jogo) ===", g_variant,
        (int)(sizeof(void *) * 8));
    say("programa: %d bits, thread %lu", (int)(sizeof(void *) * 8), (unsigned long)GetCurrentThreadId());
    OSVERSIONINFOA vi = {};
    vi.dwOSVersionInfoSize = sizeof vi;
#pragma warning(suppress : 4996)
    GetVersionExA(&vi);
    say("windows: %lu.%lu build %lu", (unsigned long)vi.dwMajorVersion, (unsigned long)vi.dwMinorVersion,
        (unsigned long)vi.dwBuildNumber);
    dump_memory();
    dump_active_runtime();

    g_step = "carregar openxr_loader.dll";
    HMODULE loader = LoadLibraryA("openxr_loader.dll");
    if (!loader) {
        say("FALHOU: openxr_loader.dll nao carregou (erro %lu) - ele tem de estar na mesma pasta", GetLastError());
        say("sonda: FALHOU");
        return 2;
    }
    say("conector carregado em %p", (void *)loader);

    Funcoes f;
    f.gipa = (PFN_xrGetInstanceProcAddr)GetProcAddress(loader, "xrGetInstanceProcAddr");
    if (!f.gipa) {
        say("FALHOU: o conector nao expoe xrGetInstanceProcAddr (DLL de 64 bits?)");
        return 2;
    }
    f.gipa(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties", (PFN_xrVoidFunction *)&f.enum_ext);
    f.gipa(XR_NULL_HANDLE, "xrCreateInstance", (PFN_xrVoidFunction *)&f.create_instance);
    if (!f.enum_ext || !f.create_instance) {
        say("FALHOU: conector incompleto");
        return 2;
    }

    if (g_variant == 0) {
        Opt o4 = opt_da_variante(4);
        int rc = rodada(o4, f);
        if (rc != 0) return rc;
        Opt o1 = opt_da_variante(1);
        return rodada(o1, f);
    }

    Opt o = opt_da_variante(g_variant);
    if (g_variant == 8) return rodada_em_thread(o, f);
    return rodada(o, f);
}
