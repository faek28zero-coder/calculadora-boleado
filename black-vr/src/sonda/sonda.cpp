// ============================================================
//  BLACK VR — vr-sonda.exe
//
//  Um programa SEPARADO do jogo, de 32 bits, que faz só a conversa com o headset:
//  carrega o conector, cria a instância, pega o sistema, cria o dispositivo D3D11 e
//  tenta criar a sessão. Nada de jogo, nenhum gancho, nenhum D3D9.
//
//  Por que existe: o jogo quebra dentro do xrCreateSession e não há como saber, de lá
//  de dentro, se a culpa é do runtime do headset, do driver de vídeo ou do processo do
//  jogo (ganchos, threads, D3D9). Aqui não tem jogo: se ESTA sonda também cair, o
//  problema é de fora do jogo. Se ela passar, o problema é do processo do jogo.
//
//  Uso:  vr-sonda.exe 1     (grava sonda-1.txt ao lado)
//        1 = adaptador do headset + flag BGRA (o caminho novo do jogo)
//        2 = adaptador padrão do Windows
//        3 = adaptador do headset sem a flag BGRA (o caminho antigo do jogo)
//        4 = só até "qual runtime respondeu" (não cria sessão: nunca deve cair)
//        5 = como 1, mas com COM inicializado nesta thread
//        0 = roda tudo que é seguro e tenta a sessão no fim
//
//  Código de saida: 0 = chegou ao fim, 1 = o runtime devolveu erro no passo indicado,
//  2 = não achou o conector, 0xFFFFFFFF = caiu (o .bat mostra o código).
// ============================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h> // CoInitializeEx (o WIN32_LEAN_AND_MEAN não traz COM pelo windows.h)
#include <winreg.h>  // leitura do registro (idem: WIN32_LEAN_AND_MEAN corta winreg.h)
#include <tlhelp32.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef XR_USE_PLATFORM_WIN32
#define XR_USE_PLATFORM_WIN32 1
#endif
#ifndef XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D11 1
#endif
#include "openxr.h"
#include "openxr_platform.h"

// ---------------------------------------------------------------- rastro
namespace {

FILE *g_file;
int g_variant;
const char *g_step = "inicio";

void say(const char *fmt, ...)
{
    if (!g_file) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_file, fmt, ap);
    va_end(ap);
    fputc('\n', g_file);
    fflush(g_file);
}

// ---------------------------------------------------------------- módulos da falha
const int kMaxMods = 320;
struct ModInfo {
    uintptr_t base, size;
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

LONG CALLBACK on_fault(EXCEPTION_POINTERS *info)
{
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION) return EXCEPTION_CONTINUE_SEARCH;
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
        say("*** eip %p esp %p ebp %p", (void *)ctx->Eip, (void *)ctx->Esp, (void *)ctx->Ebp);
        const uintptr_t sp = (uintptr_t)ctx->Esp;
        int printed = 0;
        for (int i = 0; i < 320 && printed < 14; i++) {
            const uintptr_t addr = sp + (uintptr_t)i * 4;
            MEMORY_BASIC_INFORMATION mbi = {};
            if (!VirtualQuery((const void *)addr, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT ||
                (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                say("*** pilha: parou em %p (memoria nao mapeada)", (void *)addr);
                break;
            }
            const uintptr_t v = *(const uintptr_t *)addr;
            if (v < 0x10000) continue;
            const ModInfo *m = find_mod(v);
            if (!m) continue;
            say("*** pilha[%d]: %s + 0x%llx", i * 4, m->name, (unsigned long long)(v - m->base));
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

// ---------------------------------------------------------------- runtime ativo (registro + manifesto)
void dump_active_runtime()
{
    const char *keys[] = {"SOFTWARE\\WOW6432Node\\Khronos\\OpenXR\\1", "SOFTWARE\\Khronos\\OpenXR\\1"};
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
}

void dump_modules()
{
    load_modules();
    const char *keys[] = {"openxr", "oculus", "libovr", "meta", "nvwgf", "nvogl", "d3d11", "dxgi"};
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

XrBool32 XRAPI_CALL on_message(XrDebugUtilsMessageSeverityFlagsEXT sev, XrDebugUtilsMessageTypeFlagsEXT,
                               const XrDebugUtilsMessengerCallbackDataEXT *d, void *)
{
    if (sev & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        say("  runtime[erro] %s: %s", d && d->functionName ? d->functionName : "", d && d->message ? d->message : "");
    else if (sev & XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        say("  runtime[aviso] %s: %s", d && d->functionName ? d->functionName : "", d && d->message ? d->message : "");
    return XR_FALSE;
}

} // namespace

// ---------------------------------------------------------------- main
int main(int argc, char **argv)
{
    g_variant = argc > 1 ? atoi(argv[1]) : 1;

    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    char *slash = strrchr(exe, '\\');
    if (slash) slash[1] = 0;
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%ssonda-%d.txt", exe, g_variant);
    g_file = fopen(path, "w");
    if (!g_file) return 2;

    load_modules();
    AddVectoredExceptionHandler(1, on_fault);

    say("=== BLACK VR - sonda %d (teste separado do jogo) ===", g_variant);
    say("programa: %d bits", (int)(sizeof(void *) * 8));
    OSVERSIONINFOA vi = {};
    vi.dwOSVersionInfoSize = sizeof vi;
#pragma warning(suppress : 4996)
    GetVersionExA(&vi);
    say("windows: %lu.%lu build %lu", (unsigned long)vi.dwMajorVersion, (unsigned long)vi.dwMinorVersion,
        (unsigned long)vi.dwBuildNumber);

    dump_active_runtime();

    if (g_variant == 5) {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        say("COM nesta thread: 0x%08lx", (unsigned long)hr);
    }

    g_step = "carregar openxr_loader.dll";
    HMODULE loader = LoadLibraryA("openxr_loader.dll");
    if (!loader) {
        say("FALHOU: openxr_loader.dll nao carregou (erro %lu) - ele tem de estar na mesma pasta",
            GetLastError());
        say("sonda: FALHOU");
        return 2;
    }
    say("conector carregado em %p", (void *)loader);
    dump_modules();
    auto gipa = (PFN_xrGetInstanceProcAddr)GetProcAddress(loader, "xrGetInstanceProcAddr");
    if (!gipa) {
        say("FALHOU: o conector nao expoe xrGetInstanceProcAddr (DLL de 64 bits?)");
        return 2;
    }

    PFN_xrEnumerateInstanceExtensionProperties enum_ext = nullptr;
    PFN_xrCreateInstance create_instance = nullptr;
    gipa(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties", (PFN_xrVoidFunction *)&enum_ext);
    gipa(XR_NULL_HANDLE, "xrCreateInstance", (PFN_xrVoidFunction *)&create_instance);
    if (!enum_ext || !create_instance) {
        say("FALHOU: conector incompleto");
        return 2;
    }

    g_step = "listar extensoes";
    uint32_t n = 0;
    XrResult r = enum_ext(nullptr, 0, &n, nullptr);
    if (XR_FAILED(r)) {
        say("FALHOU: extensoes: %s (%d)", xr_name(r), (int)r);
        return 1;
    }
    bool have_d3d11 = false, have_debug = false;
    {
        char exts[4096] = {};
        XrExtensionProperties *props = (XrExtensionProperties *)calloc(n ? n : 1, sizeof(XrExtensionProperties));
        for (uint32_t i = 0; i < n; i++) props[i].type = XR_TYPE_EXTENSION_PROPERTIES;
        enum_ext(nullptr, n, &n, props);
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
    say("D3D11: sim; debug_utils: %s", have_debug ? "sim" : "nao");

    g_step = "criar instancia";
    const char *wanted[2];
    uint32_t wanted_n = 0;
    wanted[wanted_n++] = "XR_KHR_D3D11_enable";
    if (have_debug) wanted[wanted_n++] = "XR_EXT_debug_utils";
    XrInstance instance = XR_NULL_HANDLE;
    XrInstanceCreateInfo ci = {XR_TYPE_INSTANCE_CREATE_INFO};
    snprintf(ci.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "BLACK VR sonda");
    ci.applicationInfo.applicationVersion = 1;
    snprintf(ci.applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, "black-pc");
    ci.applicationInfo.engineVersion = 1;
    ci.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    ci.enabledExtensionCount = wanted_n;
    ci.enabledExtensionNames = wanted;
    r = create_instance(&ci, &instance);
    if (XR_FAILED(r)) {
        say("FALHOU: xrCreateInstance: %s (%d)", xr_name(r), (int)r);
        return 1;
    }
    say("instancia criada (%p)", (void *)instance);

    // Quem esta respondendo (meta/steamvr/...) e qual versao.
    PFN_xrGetInstanceProperties get_props = nullptr;
    gipa(instance, "xrGetInstanceProperties", (PFN_xrVoidFunction *)&get_props);
    if (get_props) {
        XrInstanceProperties ip = {XR_TYPE_INSTANCE_PROPERTIES};
        if (XR_SUCCEEDED(get_props(instance, &ip)))
            say("runtime: %s versao %u.%u.%u", ip.runtimeName, (unsigned)XR_VERSION_MAJOR(ip.runtimeVersion),
                (unsigned)XR_VERSION_MINOR(ip.runtimeVersion), (unsigned)XR_VERSION_PATCH(ip.runtimeVersion));
    }

    if (have_debug) {
        PFN_xrCreateDebugUtilsMessengerEXT mk = nullptr;
        gipa(instance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction *)&mk);
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

    if (g_variant == 4) {
        say("modo 4: parando aqui de proposito (sem sessao).");
        say("sonda: OK ate aqui");
        return 0;
    }

    g_step = "xrGetSystem";
    PFN_xrGetSystem get_system = nullptr;
    PFN_xrGetSystemProperties get_sys_props = nullptr;
    PFN_xrGetD3D11GraphicsRequirementsKHR get_req = nullptr;
    PFN_xrCreateSession create_session = nullptr;
    gipa(instance, "xrGetSystem", (PFN_xrVoidFunction *)&get_system);
    gipa(instance, "xrGetSystemProperties", (PFN_xrVoidFunction *)&get_sys_props);
    gipa(instance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction *)&get_req);
    gipa(instance, "xrCreateSession", (PFN_xrVoidFunction *)&create_session);
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
    if (g_variant != 2 && SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&factory))) {
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
    } else if (g_variant == 2) {
        say("modo 2: usando o adaptador padrao do Windows");
    }

    D3D_FEATURE_LEVEL want = req.minFeatureLevel ? req.minFeatureLevel : D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_9_1;
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *ctx = nullptr;
    UINT flags = g_variant == 3 ? 0u : (UINT)D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = create_dev(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, &want,
                            1, D3D11_SDK_VERSION, &device, &got, &ctx);
    if (adapter) adapter->Release();
    say("D3D11CreateDevice: hr 0x%08lx, feature level 0x%x (pedido 0x%x, flags %u)", (unsigned long)hr,
        (unsigned)got, (unsigned)want, flags);
    if (FAILED(hr)) {
        say("FALHOU: sem dispositivo D3D11");
        return 1;
    }

    say("");
    say("--- agora o passo que quebra o jogo: xrCreateSession ---");
    g_step = "xrCreateSession";
    XrGraphicsBindingD3D11KHR binding = {XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device = device;
    XrSessionCreateInfo sci = {XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &binding;
    sci.systemId = system;
    XrSession session = XR_NULL_HANDLE;
    r = create_session(instance, &sci, &session);
    if (XR_FAILED(r)) {
        say("xrCreateSession: %s (%d)", xr_name(r), (int)r);
        say("sonda: FALHOU");
        return 1;
    }
    say("SESSAO CRIADA (%p)", (void *)session);
    say("sonda: OK");

    PFN_xrDestroySession destroy_session = nullptr;
    PFN_xrDestroyInstance destroy_instance = nullptr;
    gipa(instance, "xrDestroySession", (PFN_xrVoidFunction *)&destroy_session);
    gipa(instance, "xrDestroyInstance", (PFN_xrVoidFunction *)&destroy_instance);
    if (destroy_session) destroy_session(session);
    if (destroy_instance) destroy_instance(instance);
    if (ctx) ctx->Release();
    if (device) device->Release();
    return 0;
}
