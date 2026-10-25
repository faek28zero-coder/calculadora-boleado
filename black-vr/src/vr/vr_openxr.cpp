// ============================================================
//  BLACK VR — vr_openxr.cpp
//
//  Toda a conversa com o headset: carrega o loader do OpenXR em tempo de execução
//  (nenhuma import library, nenhuma DLL obrigatória), cria instância/sistema/sessão,
//  swapchain por olho (D3D11), localiza as views e envia o frame.
//
//  Por que carregar o loader na mão: blackrt.dll é 32-bit e continua rodando em tela
//  plana numa máquina sem runtime de VR. Com LoadLibrary + xrGetInstanceProcAddr,
//  "sem OpenXR" é um caminho de erro, não um crash de carga.
//
//  Por que D3D11: o port desenha em D3D9; o OpenXR no Windows fala D3D11/12/Vulkan.
//  A ponte é explícita: o back buffer do host é lido (GetRenderTargetData) e enviado
//  por UpdateSubresource para a textura da swapchain. O caminho rápido (D3D9Ex +
//  textura compartilhada) está descrito em docs/IMPLEMENTACAO.md.
// ============================================================
#include "vr/vr.h"

#include "common.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

#ifndef XR_USE_PLATFORM_WIN32
#define XR_USE_PLATFORM_WIN32 1
#endif
#ifndef XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D11 1
#endif
#include "openxr.h"
#include "openxr_platform.h"
#include "vr/vr_internal.h"
#include "vr/vr_trace.h"
#include "vr/vr_math.h"

namespace vr {
namespace internal {

Runtime g_rt;
std::atomic<bool> g_requested{false};
std::atomic<bool> g_active{false};

} // namespace internal

namespace {

using internal::g_rt;

// ---------------------------------------------------------------- loader dinâmico

HMODULE g_loader;
HMODULE g_d3d11_dll;

PFN_xrGetInstanceProcAddr p_xrGetInstanceProcAddr;

#define XR_FN(name) PFN_##name p_##name
XR_FN(xrEnumerateInstanceExtensionProperties);
XR_FN(xrCreateInstance);
XR_FN(xrDestroyInstance);
XR_FN(xrGetSystem);
XR_FN(xrGetSystemProperties);
XR_FN(xrEnumerateViewConfigurationViews);
XR_FN(xrEnumerateEnvironmentBlendModes);
XR_FN(xrCreateSession);
XR_FN(xrDestroySession);
XR_FN(xrBeginSession);
XR_FN(xrEndSession);
XR_FN(xrPollEvent);
XR_FN(xrStringToPath);
XR_FN(xrCreateReferenceSpace);
XR_FN(xrLocateViews);
XR_FN(xrWaitFrame);
XR_FN(xrBeginFrame);
XR_FN(xrEndFrame);
XR_FN(xrCreateActionSet);
XR_FN(xrCreateAction);
XR_FN(xrSuggestInteractionProfileBindings);
XR_FN(xrAttachSessionActionSets);
XR_FN(xrSyncActions);
XR_FN(xrCreateActionSpace);
XR_FN(xrEnumerateSwapchainFormats);
XR_FN(xrCreateSwapchain);
XR_FN(xrEnumerateSwapchainImages);
XR_FN(xrAcquireSwapchainImage);
XR_FN(xrWaitSwapchainImage);
XR_FN(xrReleaseSwapchainImage);
XR_FN(xrGetD3D11GraphicsRequirementsKHR);
XR_FN(xrGetInstanceProperties);
XR_FN(xrGetActionStateFloat);
XR_FN(xrGetActionStateBoolean);
XR_FN(xrGetActionStateVector2f);
XR_FN(xrLocateSpace);
XR_FN(xrApplyHapticFeedback);
#undef XR_FN

/** Copia um nome para um campo de tamanho fixo do OpenXR (o strcpy_s é MSVC-only). */
void copy_name(char *dst, size_t size, const char *src)
{
    if (!dst || !size) return;
    size_t n = 0;
    while (n + 1 < size && src && src[n]) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = 0;
}

template <typename T>
bool bind(T &fn, const char *name)
{
    PFN_xrVoidFunction p = nullptr;
    if (XR_FAILED(p_xrGetInstanceProcAddr(g_rt.instance, name, &p)) || !p) {
        logf("[vr] o runtime não expõe %s", name);
        return false;
    }
    fn = reinterpret_cast<T>(p);
    return true;
}

// ---------------------------------------------------------------- estado local

char g_error[512];
XrSessionState g_state = XR_SESSION_STATE_UNKNOWN;

// Mensageiro do próprio runtime (XR_EXT_debug_utils). Quando o runtime reclama de algo,
// a frase dele vai para o rastro — é a diferença entre "0xc0000005" e "device inválido".
PFN_xrCreateDebugUtilsMessengerEXT p_xrCreateDebugUtilsMessengerEXT;
PFN_xrDestroyDebugUtilsMessengerEXT p_xrDestroyDebugUtilsMessengerEXT;
XrDebugUtilsMessengerEXT g_messenger = XR_NULL_HANDLE;

XrBool32 XRAPI_CALL on_runtime_message(XrDebugUtilsMessageSeverityFlagsEXT severity,
                                       XrDebugUtilsMessageTypeFlagsEXT /*types*/,
                                       const XrDebugUtilsMessengerCallbackDataEXT *data, void * /*user*/)
{
    const char *level = (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)      ? "erro"
                        : (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)  ? "aviso"
                                                                                        : "info";
    trace::line("  runtime[%s] %s: %s", level, data && data->functionName ? data->functionName : "",
                data && data->message ? data->message : "");
    return XR_FALSE;
}

XrViewConfigurationView g_view_cfg[internal::kEyes];
XrViewLocateInfo g_view_locate;
XrCompositionLayerProjectionView g_layer_views[internal::kEyes];
XrCompositionLayerProjection g_layer;
const XrCompositionLayerBaseHeader *g_layers[1];
XrTime g_last_frame_time;

// leitura D3D9 -> D3D11
IDirect3DSurface9 *g_readback;
int g_readback_w, g_readback_h;
std::vector<uint8_t> g_scale_buf; // quando a swapchain não tem o tamanho do back buffer

PFN_D3D11_CREATE_DEVICE p_D3D11CreateDevice;

// pose do centro dos olhos, para vr_pose.cpp
struct HeadPose {
    float pos[3];
    float quat[4];
    bool valid;
};
HeadPose g_head;

void set_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_error, sizeof g_error, fmt, ap);
    va_end(ap);
    logf("[vr] %s", g_error);
}

bool check(XrResult r, const char *what)
{
    if (XR_SUCCEEDED(r)) return true;
    set_error("%s falhou (XrResult %d)", what, (int)r);
    return false;
}

bool load_loader()
{
    if (g_loader) return true;
    g_loader = LoadLibraryA("openxr_loader.dll");
    if (!g_loader) {
        set_error("openxr_loader.dll não encontrado ao lado do black.exe. Este arquivo (versão de 32 bits) "
                  "vem junto com o port — se ele sumiu, extraia o pacote de novo sem apagar nada. O app de VR "
                  "do headset (Meta Quest Link, SteamVR) é outra coisa e sozinho não resolve.");
        return false;
    }
    p_xrGetInstanceProcAddr = (PFN_xrGetInstanceProcAddr)GetProcAddress(g_loader, "xrGetInstanceProcAddr");
    if (!p_xrGetInstanceProcAddr) {
        set_error("openxr_loader.dll sem xrGetInstanceProcAddr — provavelmente é a DLL de 64 bits; o port é 32 bits");
        return false;
    }
    // Estas duas existem ANTES da instância: pede-se com instância nula, direto ao conector.
    // (Sem isto, eram ponteiros nulos e a primeira chamada estourava em 0x0 — foi o que o
    // vr-trace.txt mostrou em 25/10.)
    struct Early { PFN_xrVoidFunction *slot; const char *name; };
    const Early early[] = {
        {(PFN_xrVoidFunction *)&p_xrEnumerateInstanceExtensionProperties, "xrEnumerateInstanceExtensionProperties"},
        {(PFN_xrVoidFunction *)&p_xrCreateInstance, "xrCreateInstance"},
    };
    for (const Early &e : early) {
        PFN_xrVoidFunction p = nullptr;
        if (XR_FAILED(p_xrGetInstanceProcAddr(XR_NULL_HANDLE, e.name, &p)) || !p) {
            set_error("o conector não expõe %s", e.name);
            return false;
        }
        *e.slot = p;
    }
    return true;
}

bool create_d3d11(const XrGraphicsRequirementsD3D11KHR &req)
{
    if (g_rt.dev11) return true;
    if (!g_d3d11_dll) g_d3d11_dll = LoadLibraryA("d3d11.dll");
    if (!g_d3d11_dll) {
        set_error("d3d11.dll não encontrado");
        return false;
    }
    p_D3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(g_d3d11_dll, "D3D11CreateDevice");
    if (!p_D3D11CreateDevice) {
        set_error("d3d11.dll sem D3D11CreateDevice");
        return false;
    }

    // O runtime exige um adaptador com o mesmo LUID (o headset está pendurado nele).
    const LUID &adapter = req.adapterLuid;
    IDXGIFactory1 *factory = nullptr;
    IDXGIAdapter1 *use = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&factory))) {
        for (UINT i = 0;; i++) {
            IDXGIAdapter1 *cand = nullptr;
            if (factory->EnumAdapters1(i, &cand) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC1 d = {};
            cand->GetDesc1(&d);
            if (d.AdapterLuid.LowPart == adapter.LowPart && d.AdapterLuid.HighPart == adapter.HighPart) {
                use = cand;
                char name[128] = {};
                for (int k = 0; k < 64 && d.Description[k]; k++) name[k] = (char)d.Description[k];
                trace::line("  3c-1: adaptador do headset: %s (luid %08lx:%08lx)", name,
                            (unsigned long)adapter.HighPart, (unsigned long)adapter.LowPart);
                break;
            }
            cand->Release();
        }
        factory->Release();
    }
    if (!use) trace::line("  3c-1: AVISO - nenhum adaptador com o luid do headset; tentando o padrao");

    // O QUE A REFERÊNCIA FAZ E NÓS NÃO FAZÍAMOS: o runtime manda o feature level mínimo em
    // req.minFeatureLevel e o dispositivo TEM de nascer exatamente nesse nível. Antes pedíamos
    // uma lista fixa (11_0/10_1/10_0) e podíamos entregar ao runtime um dispositivo mais fraco
    // do que ele pediu — o runtime então quebrava lá dentro, na criação da sessão.
    D3D_FEATURE_LEVEL want = req.minFeatureLevel ? req.minFeatureLevel : D3D_FEATURE_LEVEL_11_0;
    trace::line("  3c-2: feature level exigido 0x%x, flags BGRA", (unsigned)want);

    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_9_1;
    HRESULT hr = p_D3D11CreateDevice(use, use ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                     D3D11_CREATE_DEVICE_BGRA_SUPPORT, &want, 1, D3D11_SDK_VERSION, &g_rt.dev11,
                                     &got, &g_rt.ctx11);
    if (FAILED(hr) && (want == D3D_FEATURE_LEVEL_11_0)) {
        // segunda tentativa: alguns drivers recusam BGRA_SUPPORT em placas antigas
        trace::line("  3c-3: falhou com BGRA (0x%08lx); tentando sem a flag", (unsigned long)hr);
        hr = p_D3D11CreateDevice(use, use ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &want,
                                 1, D3D11_SDK_VERSION, &g_rt.dev11, &got, &g_rt.ctx11);
    }
    if (use) use->Release();
    if (FAILED(hr)) {
        set_error("D3D11CreateDevice falhou (0x%08lx) no adaptador do headset", (unsigned long)hr);
        return false;
    }
    logf("[vr] dispositivo D3D11 criado (feature level 0x%x)", (unsigned)got);
    return true;
}

bool create_instance()
{
    uint32_t n = 0;
    trace::line("  2a: enumerando extensoes do runtime");
    if (XR_FAILED(p_xrEnumerateInstanceExtensionProperties(nullptr, 0, &n, nullptr))) {
        set_error("nenhum runtime OpenXR ativo (o headset está ligado? o runtime está instalado?)");
        return false;
    }
    std::vector<XrExtensionProperties> ext(n ? n : 1, {XR_TYPE_EXTENSION_PROPERTIES});
    if (n) p_xrEnumerateInstanceExtensionProperties(nullptr, n, &n, ext.data());
    bool d3d11 = false, debug_utils = false;
    for (uint32_t i = 0; i < n; i++) {
        if (!strcmp(ext[i].extensionName, "XR_KHR_D3D11_enable")) d3d11 = true;
        if (!strcmp(ext[i].extensionName, "XR_EXT_debug_utils")) debug_utils = true;
    }
    if (!d3d11) {
        set_error("o runtime não oferece XR_KHR_D3D11_enable");
        return false;
    }

    trace::line("  2b: %u extensoes, D3D11 presente, debug_utils %s", n, debug_utils ? "presente" : "ausente");
    // XR_EXT_debug_utils é opcional: com ela o runtime escreve as reclamações dele no rastro.
    const char *wanted[2];
    uint32_t wanted_n = 0;
    wanted[wanted_n++] = "XR_KHR_D3D11_enable";
    if (debug_utils) wanted[wanted_n++] = "XR_EXT_debug_utils";
    XrInstanceCreateInfo ci = {XR_TYPE_INSTANCE_CREATE_INFO};
    copy_name(ci.applicationInfo.applicationName, sizeof ci.applicationInfo.applicationName, "BLACK VR (black-vr)");
    ci.applicationInfo.applicationVersion = 1;
    copy_name(ci.applicationInfo.engineName, sizeof ci.applicationInfo.engineName, "black-pc");
    ci.applicationInfo.engineVersion = 1;
    ci.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    ci.enabledExtensionCount = wanted_n;
    ci.enabledExtensionNames = wanted;
    if (!check(p_xrCreateInstance(&ci, &g_rt.instance), "xrCreateInstance")) return false;

    // xrDestroyInstance fica no instante da instância (o shutdown a chama).
    if (g_rt.instance && !p_xrDestroyInstance) bind(p_xrDestroyInstance, "xrDestroyInstance");

    // Quem respondeu? Sem isso, "runtime do headset" é um nome vago: pode ser o Meta, o SteamVR
    // ou um runtime fantasma de uma instalação antiga. O nome e a versão vão para o rastro.
    if (bind(p_xrGetInstanceProperties, "xrGetInstanceProperties")) {
        XrInstanceProperties ip = {XR_TYPE_INSTANCE_PROPERTIES};
        if (XR_SUCCEEDED(p_xrGetInstanceProperties(g_rt.instance, &ip)))
            trace::line("  2c: runtime = %s %u.%u.%u", ip.runtimeName,
                        (unsigned)XR_VERSION_MAJOR(ip.runtimeVersion), (unsigned)XR_VERSION_MINOR(ip.runtimeVersion),
                        (unsigned)XR_VERSION_PATCH(ip.runtimeVersion));
    }

    if (debug_utils && bind(p_xrCreateDebugUtilsMessengerEXT, "xrCreateDebugUtilsMessengerEXT")) {
        XrDebugUtilsMessengerCreateInfoEXT dci = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        dci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        dci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dci.userCallback = on_runtime_message;
        if (XR_SUCCEEDED(p_xrCreateDebugUtilsMessengerEXT(g_rt.instance, &dci, &g_messenger)))
            bind(p_xrDestroyDebugUtilsMessengerEXT, "xrDestroyDebugUtilsMessengerEXT");
    }
    return bind(p_xrGetSystem, "xrGetSystem") && bind(p_xrGetSystemProperties, "xrGetSystemProperties") &&
           bind(p_xrEnumerateViewConfigurationViews, "xrEnumerateViewConfigurationViews") &&
           bind(p_xrEnumerateEnvironmentBlendModes, "xrEnumerateEnvironmentBlendModes") &&
           bind(p_xrCreateSession, "xrCreateSession") && bind(p_xrDestroySession, "xrDestroySession") &&
           bind(p_xrBeginSession, "xrBeginSession") && bind(p_xrEndSession, "xrEndSession") &&
           bind(p_xrPollEvent, "xrPollEvent") && bind(p_xrStringToPath, "xrStringToPath") &&
           bind(p_xrCreateReferenceSpace, "xrCreateReferenceSpace") && bind(p_xrLocateViews, "xrLocateViews") &&
           bind(p_xrWaitFrame, "xrWaitFrame") && bind(p_xrBeginFrame, "xrBeginFrame") && bind(p_xrEndFrame, "xrEndFrame") &&
           bind(p_xrCreateActionSet, "xrCreateActionSet") && bind(p_xrCreateAction, "xrCreateAction") &&
           bind(p_xrSuggestInteractionProfileBindings, "xrSuggestInteractionProfileBindings") &&
           bind(p_xrAttachSessionActionSets, "xrAttachSessionActionSets") && bind(p_xrSyncActions, "xrSyncActions") &&
           bind(p_xrGetActionStateFloat, "xrGetActionStateFloat") &&
           bind(p_xrGetActionStateBoolean, "xrGetActionStateBoolean") &&
           bind(p_xrGetActionStateVector2f, "xrGetActionStateVector2f") && bind(p_xrLocateSpace, "xrLocateSpace") &&
           bind(p_xrApplyHapticFeedback, "xrApplyHapticFeedback") &&
           bind(p_xrCreateActionSpace, "xrCreateActionSpace") && bind(p_xrEnumerateSwapchainFormats, "xrEnumerateSwapchainFormats") &&
           bind(p_xrCreateSwapchain, "xrCreateSwapchain") && bind(p_xrEnumerateSwapchainImages, "xrEnumerateSwapchainImages") &&
           bind(p_xrAcquireSwapchainImage, "xrAcquireSwapchainImage") && bind(p_xrWaitSwapchainImage, "xrWaitSwapchainImage") &&
           bind(p_xrReleaseSwapchainImage, "xrReleaseSwapchainImage") &&
           bind(p_xrGetD3D11GraphicsRequirementsKHR, "xrGetD3D11GraphicsRequirementsKHR");
}

bool create_session()
{
    trace::line("  3a: xrGetSystem");
    XrSystemGetInfo sgi = {XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (!check(p_xrGetSystem(g_rt.instance, &sgi, &g_rt.system), "xrGetSystem (headset conectado?)")) return false;

    trace::line("  3b: xrGetD3D11GraphicsRequirementsKHR (%p)", (void *)p_xrGetD3D11GraphicsRequirementsKHR);
    XrGraphicsRequirementsD3D11KHR req = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    if (!check(p_xrGetD3D11GraphicsRequirementsKHR(g_rt.instance, g_rt.system, &req),
               "xrGetD3D11GraphicsRequirementsKHR"))
        return false;
    trace::line("  3c: D3D11CreateDevice (%p)", (void *)p_D3D11CreateDevice);
    if (!create_d3d11(req)) return false;
    trace::line("  3d: dispositivo D3D11 ok (%p)", (void *)g_rt.dev11);

    trace::line("  3e: xrGetSystemProperties (%p)", (void *)p_xrGetSystemProperties);
    XrSystemProperties props = {XR_TYPE_SYSTEM_PROPERTIES};
    if (!check(p_xrGetSystemProperties(g_rt.instance, g_rt.system, &props), "xrGetSystemProperties")) return false;
    logf("[vr] sistema: %s (vendor 0x%04x)", props.systemName, props.vendorId);
    trace::line("  3e-1: %s, vendor 0x%x, orientacao %s", props.systemName, props.vendorId,
                props.trackingProperties.orientationTracking ? "ok" : "nao");

    trace::line("  3f: xrEnumerateViewConfigurationViews (%p)", (void *)p_xrEnumerateViewConfigurationViews);
    uint32_t n = internal::kEyes;
    if (!check(p_xrEnumerateViewConfigurationViews(g_rt.instance, g_rt.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                                                   &n, nullptr),
               "xrEnumerateViewConfigurationViews"))
        return false;
    n = internal::kEyes;
    for (auto &vc : g_view_cfg) vc = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
    if (!check(p_xrEnumerateViewConfigurationViews(g_rt.instance, g_rt.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n,
                                                   &n, g_view_cfg),
               "xrEnumerateViewConfigurationViews"))
        return false;

    trace::line("  3g: xrEnumerateEnvironmentBlendModes (%p)", (void *)p_xrEnumerateEnvironmentBlendModes);
    uint32_t blends = 0;
    if (!check(p_xrEnumerateEnvironmentBlendModes(g_rt.instance, g_rt.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                  0, &blends, nullptr),
               "xrEnumerateEnvironmentBlendModes"))
        return false;
    if (blends) {
        std::vector<XrEnvironmentBlendMode> modes(blends);
        p_xrEnumerateEnvironmentBlendModes(g_rt.instance, g_rt.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, blends,
                                           &blends, modes.data());
        g_rt.blend = modes[0]; // OPAQUE quando existe
        for (auto m : modes)
            if (m == XR_ENVIRONMENT_BLEND_MODE_OPAQUE) g_rt.blend = m;
    }
    trace::line("  3g-1: painel %ux%u por olho, blend %d", g_view_cfg[0].recommendedImageRectWidth,
                g_view_cfg[0].recommendedImageRectHeight, (int)g_rt.blend);

    trace::line("  3h: xrCreateSession (%p)", (void *)p_xrCreateSession);
    XrGraphicsBindingD3D11KHR binding = {XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device = g_rt.dev11;
    XrSessionCreateInfo sci = {XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &binding;
    sci.systemId = g_rt.system;
    if (!check(p_xrCreateSession(g_rt.instance, &sci, &g_rt.session), "xrCreateSession")) return false;
    trace::line("  3i: sessao criada");

    // Espaço: prefere LOCAL_FLOOR (chão); cai para LOCAL se o runtime não der.
    bool floor = props.trackingProperties.orientationTracking && false;
    (void)floor;
    XrReferenceSpaceCreateInfo rs = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rs.poseInReferenceSpace.orientation.w = 1.0f;
    if (!check(p_xrCreateReferenceSpace(g_rt.session, &rs, &g_rt.stage), "xrCreateReferenceSpace")) return false;
    return true;
}

bool create_swapchains()
{
    uint32_t n = 0;
    if (!check(p_xrEnumerateSwapchainFormats(g_rt.session, 0, &n, nullptr), "xrEnumerateSwapchainFormats")) return false;
    std::vector<int64_t> formats(n ? n : 1);
    if (n) p_xrEnumerateSwapchainFormats(g_rt.session, n, &n, formats.data());
    // O jogo já sai com gamma aplicado (present_backbuffer): UNORM, nunca _SRGB (gamma em dobro).
    g_rt.format = formats.empty() ? (int64_t)DXGI_FORMAT_B8G8R8A8_UNORM : (int64_t)formats[0];
    for (int64_t f : formats)
        if (f == DXGI_FORMAT_B8G8R8A8_UNORM) g_rt.format = f;

    for (int e = 0; e < internal::kEyes; e++) {
        internal::EyeImage &ei = g_rt.eye[e];
        ei.width = g_view_cfg[e].recommendedImageRectWidth;
        ei.height = g_view_cfg[e].recommendedImageRectHeight;
        XrSwapchainCreateInfo ci = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        ci.format = g_rt.format;
        ci.sampleCount = 1;
        ci.width = ei.width;
        ci.height = ei.height;
        ci.faceCount = 1;
        ci.arraySize = 1;
        ci.mipCount = 1;
        if (!check(p_xrCreateSwapchain(g_rt.session, &ci, &ei.chain), "xrCreateSwapchain")) return false;

        uint32_t count = 0;
        p_xrEnumerateSwapchainImages(ei.chain, 0, &count, nullptr);
        ei.images.assign(count ? count : 1, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        if (!check(p_xrEnumerateSwapchainImages(ei.chain, count, &count,
                                                reinterpret_cast<XrSwapchainImageBaseHeader *>(ei.images.data())),
                   "xrEnumerateSwapchainImages"))
            return false;
        ei.images.resize(count);
        logf("[vr] olho %d: swapchain %ux%u, %u imagens", e, ei.width, ei.height, count);
    }
    return true;
}

bool create_actions()
{
    internal::Actions &a = g_rt.actions;
    XrActionSetCreateInfo asci = {XR_TYPE_ACTION_SET_CREATE_INFO};
    copy_name(asci.actionSetName, sizeof asci.actionSetName, "gameplay");
    copy_name(asci.localizedActionSetName, sizeof asci.localizedActionSetName, "Gameplay");
    if (!check(p_xrCreateActionSet(g_rt.instance, &asci, &a.set), "xrCreateActionSet")) return false;

    auto make = [&](XrAction &act, XrActionType t, const char *name, const char *localized) {
        XrActionCreateInfo ci = {XR_TYPE_ACTION_CREATE_INFO};
        copy_name(ci.actionName, sizeof ci.actionName, name);
        ci.actionType = t;
        copy_name(ci.localizedActionName, sizeof ci.localizedActionName, localized);
        return check(p_xrCreateAction(a.set, &ci, &act), "xrCreateAction");
    };
    if (!make(a.trigger, XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Atirar") ||
        !make(a.grip, XR_ACTION_TYPE_FLOAT_INPUT, "grip", "Recarregar") ||
        !make(a.move, XR_ACTION_TYPE_VECTOR2F_INPUT, "move", "Mover") ||
        !make(a.turn, XR_ACTION_TYPE_VECTOR2F_INPUT, "turn", "Olhar") ||
        !make(a.a, XR_ACTION_TYPE_BOOLEAN_INPUT, "a", "A") ||
        !make(a.b, XR_ACTION_TYPE_BOOLEAN_INPUT, "b", "B") ||
        !make(a.menu, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", "Menu") ||
        !make(a.haptic, XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Vibração") ||
        !make(a.aim, XR_ACTION_TYPE_POSE_INPUT, "aim", "Mira"))
        return false;

    auto path = [](const char *s) {
        XrPath p = XR_NULL_PATH;
        p_xrStringToPath(g_rt.instance, s, &p);
        return p;
    };

    struct Profile {
        const char *name;
        const char *trigger, *grip, *stick, *stick_click, *a, *b, *menu, *haptic;
    };
    const Profile profiles[] = {
        {"/interaction_profiles/oculus/touch_controller", "/user/hand/right/input/trigger/value",
         "/user/hand/right/input/squeeze/value", "/user/hand/left/input/thumbstick",
         "/user/hand/left/input/thumbstick/click", "/user/hand/right/input/a/click",
         "/user/hand/right/input/b/click", "/user/hand/left/input/menu/click", "/user/hand/right/output/haptic"},
        {"/interaction_profiles/valve/index_controller", "/user/hand/right/input/trigger/value",
         "/user/hand/right/input/squeeze/value", "/user/hand/left/input/thumbstick",
         "/user/hand/left/input/thumbstick/click", "/user/hand/right/input/a/click",
         "/user/hand/right/input/b/click", "/user/hand/left/input/system/click", "/user/hand/right/output/haptic"},
        {"/interaction_profiles/htc/vive_controller", "/user/hand/right/input/trigger/value",
         "/user/hand/right/input/squeeze/click", "/user/hand/left/input/trackpad",
         "/user/hand/left/input/trackpad/click", "/user/hand/right/input/menu/click",
         "/user/hand/right/input/menu/click", "/user/hand/right/output/haptic"},
        {"/interaction_profiles/microsoft/motion_controller", "/user/hand/right/input/trigger/value",
         "/user/hand/right/input/squeeze/click", "/user/hand/left/input/thumbstick",
         "/user/hand/left/input/thumbstick/click", "/user/hand/right/input/menu/click",
         "/user/hand/right/input/menu/click", "/user/hand/right/output/haptic"},
        {"/interaction_profiles/khr/simple_controller", "/user/hand/right/input/select/click",
         "/user/hand/left/input/select/click", nullptr, nullptr, nullptr, nullptr,
         "/user/hand/right/input/menu/click", nullptr},
    };

    bool any = false;
    for (const Profile &pr : profiles) {
        std::vector<XrActionSuggestedBinding> binds;
        auto add = [&](XrAction action, const char *p) {
            if (action != XR_NULL_HANDLE && p && *p) binds.push_back({action, path(p)});
        };
        add(a.trigger, pr.trigger);
        add(a.grip, pr.grip);
        add(a.move, pr.stick);
        add(a.turn, "/user/hand/right/input/thumbstick");
        add(a.a, pr.a);
        add(a.b, pr.b);
        add(a.menu, pr.menu);
        add(a.haptic, pr.haptic);
        add(a.aim, "/user/hand/right/input/aim/pose");
        if (binds.empty()) continue;
        XrInteractionProfileSuggestedBinding s = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        s.interactionProfile = path(pr.name);
        s.countSuggestedBindings = (uint32_t)binds.size();
        s.suggestedBindings = binds.data();
        if (XR_SUCCEEDED(p_xrSuggestInteractionProfileBindings(g_rt.instance, &s))) any = true;
    }
    if (!any) {
        set_error("nenhum perfil de controle aceito pelo runtime (seguindo sem controles)");
    }

    a.hand_path[0] = path("/user/hand/left");
    a.hand_path[1] = path("/user/hand/right");
    for (int i = 0; i < 2; i++) {
        XrActionSpaceCreateInfo si = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
        si.action = a.aim;
        si.subactionPath = a.hand_path[i];
        si.poseInActionSpace.orientation.w = 1.0f;
        p_xrCreateActionSpace(g_rt.session, &si, &a.hand[i]);
    }

    const XrActionSet set = a.set;
    XrSessionActionSetsAttachInfo attach = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach.countActionSets = 1;
    attach.actionSets = &set;
    return check(p_xrAttachSessionActionSets(g_rt.session, &attach), "xrAttachSessionActionSets");
}

void poll_events()
{
    XrEventDataBuffer ev = {XR_TYPE_EVENT_DATA_BUFFER};
    while (p_xrPollEvent(g_rt.instance, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto *sc = reinterpret_cast<XrEventDataSessionStateChanged *>(&ev);
            g_state = sc->state;
            switch (g_state) {
            case XR_SESSION_STATE_READY: {
                XrSessionBeginInfo bi = {XR_TYPE_SESSION_BEGIN_INFO};
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (XR_SUCCEEDED(p_xrBeginSession(g_rt.session, &bi))) {
                    g_rt.session_running = true;
                    g_rt.active = true;
                    internal::g_active = true;
                    logf("[vr] sessão iniciada");
                }
                break;
            }
            case XR_SESSION_STATE_STOPPING:
                p_xrEndSession(g_rt.session);
                g_rt.session_running = false;
                g_rt.active = false;
                internal::g_active = false;
                break;
            case XR_SESSION_STATE_EXITING:
            case XR_SESSION_STATE_LOSS_PENDING:
                g_rt.active = false;
                internal::g_active = false;
                break;
            default:
                break;
            }
        }
        ev = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

void update_head()
{
    g_head.valid = false;
    if (g_rt.view_count < (uint32_t)internal::kEyes) return;
    if (!(g_rt.view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT)) return;
    if (!(g_rt.view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) return;
    const XrPosef &a = g_rt.views[0].pose, &b = g_rt.views[1].pose;
    g_head.pos[0] = (a.position.x + b.position.x) * 0.5f;
    g_head.pos[1] = (a.position.y + b.position.y) * 0.5f;
    g_head.pos[2] = (a.position.z + b.position.z) * 0.5f;
    g_head.quat[0] = a.orientation.x;
    g_head.quat[1] = a.orientation.y;
    g_head.quat[2] = a.orientation.z;
    g_head.quat[3] = a.orientation.w;
    g_head.valid = true;
}

float read_action_float(XrAction a)
{
    if (!g_rt.session || a == XR_NULL_HANDLE) return 0.0f;
    XrActionStateFloat s = {XR_TYPE_ACTION_STATE_FLOAT};
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = a;
    if (XR_FAILED(p_xrGetActionStateFloat(g_rt.session, &gi, &s))) return 0.0f;
    return s.isActive ? s.currentState : 0.0f;
}

bool read_action_bool(XrAction a)
{
    if (!g_rt.session || a == XR_NULL_HANDLE) return false;
    XrActionStateBoolean s = {XR_TYPE_ACTION_STATE_BOOLEAN};
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = a;
    if (XR_FAILED(p_xrGetActionStateBoolean(g_rt.session, &gi, &s))) return false;
    return s.isActive && s.currentState != 0;
}

void read_action_vector2(XrAction a, float out[2])
{
    out[0] = out[1] = 0.0f;
    if (!g_rt.session || a == XR_NULL_HANDLE) return;
    XrActionStateVector2f s = {XR_TYPE_ACTION_STATE_VECTOR2F};
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = a;
    if (XR_FAILED(p_xrGetActionStateVector2f(g_rt.session, &gi, &s))) return;
    if (!s.isActive) return;
    out[0] = s.currentState.x;
    out[1] = s.currentState.y;
}

/** Envia ao runtime o que foi escrito nas duas imagens e termina o frame. */
void finish_frame()
{
    if (!g_rt.frame_open) return; // sem BeginFrame não há EndFrame
    g_rt.frame_open = false;
    for (int e = 0; e < internal::kEyes; e++) {
        internal::EyeImage &ei = g_rt.eye[e];
        if (!ei.acquired) continue;
        XrSwapchainImageReleaseInfo ri = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        p_xrReleaseSwapchainImage(ei.chain, &ri);
        ei.acquired = false;

        g_layer_views[e].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        g_layer_views[e].next = nullptr;
        g_layer_views[e].pose = g_rt.views[e].pose;
        g_layer_views[e].fov = g_rt.views[e].fov;
        g_layer_views[e].subImage.swapchain = ei.chain;
        g_layer_views[e].subImage.imageRect.offset = {0, 0};
        g_layer_views[e].subImage.imageRect.extent = {(int32_t)ei.width, (int32_t)ei.height};
        g_layer_views[e].subImage.imageArrayIndex = 0;
    }

    g_layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    g_layer.space = g_rt.stage;
    g_layer.viewCount = internal::kEyes;
    g_layer.views = g_layer_views;
    g_layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader *>(&g_layer);

    XrFrameEndInfo end = {XR_TYPE_FRAME_END_INFO};
    end.displayTime = g_rt.display_time;
    end.environmentBlendMode = g_rt.blend;
    end.layerCount = g_rt.should_render ? 1u : 0u;
    end.layers = g_layers;
    p_xrEndFrame(g_rt.session, &end);
}

} // namespace

// ================================================================
//  API pública (vr.h)
// ================================================================

namespace internal {

bool upload_eye(int eye, const uint8_t *bgra, uint32_t width, uint32_t height, uint32_t pitch)
{
    internal::EyeImage &ei = g_rt.eye[eye];
    if (!ei.acquired || !ei.texture || !g_rt.ctx11) return false;

    D3D11_TEXTURE2D_DESC desc = {};
    ei.texture->GetDesc(&desc);

    if (width == desc.Width && height == desc.Height) {
        D3D11_BOX box = {0, 0, 0, width, height, 1};
        g_rt.ctx11->UpdateSubresource(ei.texture, 0, &box, bgra, pitch, 0);
        return true;
    }

    // Tamanhos diferentes (o jogo pode renderizar em outra resolução): reescala no CPU,
    // PRESERVANDO O ASPECTO (faixas pretas). Esticar o quadro 4:3 no olho quadrado do headset
    // deformaria o mundo; o caminho certo é o port renderizar no aspecto do olho, mas se alguém
    // mexer no --resolution, a imagem continua correta.
    const float src_aspect = (float)width / (float)height;
    const float dst_aspect = (float)desc.Width / (float)desc.Height;
    uint32_t fit_w = desc.Width, fit_h = desc.Height;
    if (src_aspect > dst_aspect)
        fit_h = (uint32_t)(desc.Width / src_aspect);
    else if (src_aspect < dst_aspect)
        fit_w = (uint32_t)(desc.Height * src_aspect);
    if (!fit_w) fit_w = 1;
    if (!fit_h) fit_h = 1;
    const uint32_t off_x = (desc.Width - fit_w) / 2, off_y = (desc.Height - fit_h) / 2;

    g_scale_buf.assign((size_t)desc.Width * desc.Height * 4, 0);
    for (uint32_t y = 0; y < fit_h; y++) {
        const uint32_t sy0 = std::min(height - 1, (uint32_t)((uint64_t)y * height / fit_h));
        const uint32_t *src = reinterpret_cast<const uint32_t *>(bgra + (size_t)sy0 * pitch);
        uint32_t *dst = reinterpret_cast<uint32_t *>(&g_scale_buf[(size_t)(y + off_y) * desc.Width * 4]) + off_x;
        for (uint32_t x = 0; x < fit_w; x++) dst[x] = src[std::min(width - 1, (uint32_t)((uint64_t)x * width / fit_w))];
    }
    g_rt.ctx11->UpdateSubresource(ei.texture, 0, nullptr, g_scale_buf.data(), desc.Width * 4, 0);
    return true;
}

float action_trigger() { return read_action_float(g_rt.actions.trigger); }
float action_grip() { return read_action_float(g_rt.actions.grip); }

void action_move(float out[2]) { read_action_vector2(g_rt.actions.move, out); }
void action_turn(float out[2]) { read_action_vector2(g_rt.actions.turn, out); }

bool action_a() { return read_action_bool(g_rt.actions.a); }
bool action_b() { return read_action_bool(g_rt.actions.b); }
bool action_menu() { return read_action_bool(g_rt.actions.menu); }

bool controllers_seen()
{
    static bool seen;
    if (seen) return true;
    if (!g_rt.session) return false;
    XrActionStateFloat s = {XR_TYPE_ACTION_STATE_FLOAT};
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = g_rt.actions.trigger;
    if (XR_SUCCEEDED(p_xrGetActionStateFloat(g_rt.session, &gi, &s)) && s.isActive) seen = true;
    return seen;
}

bool head_pose(float pos[3], float quat[4])
{
    if (!g_head.valid) return false;
    memcpy(pos, g_head.pos, sizeof g_head.pos);
    memcpy(quat, g_head.quat, sizeof g_head.quat);
    return true;
}

void sync_actions()
{
    if (!g_rt.session || !g_rt.actions.set) return;
    XrActiveActionSet active = {g_rt.actions.set, XR_NULL_PATH};
    XrActionsSyncInfo si = {XR_TYPE_ACTIONS_SYNC_INFO};
    si.countActiveActionSets = 1;
    si.activeActionSets = &active;
    p_xrSyncActions(g_rt.session, &si);
}

} // namespace internal

namespace {

std::atomic<unsigned> g_pose_serial{0};

} // namespace

unsigned pose_serial() { return g_pose_serial.load(); }

float headset_fov()
{
    if (g_rt.view_count < 1) return 90.0f;
    float best = 0.0f;
    for (unsigned e = 0; e < g_rt.view_count && e < (unsigned)internal::kEyes; e++) {
        const XrFovf &f = g_rt.views[e].fov;
        const float deg = (std::atan(std::fabs(std::tan(f.angleRight))) + std::atan(std::fabs(std::tan(f.angleLeft)))) *
                          57.2957795f;
        if (deg > best) best = deg;
    }
    return best > 1.0f ? best : 90.0f;
}

bool enabled() { return internal::g_requested.load(); }

bool active() { return internal::g_active.load() && g_rt.session_running; }

bool frame_ready() { return g_rt.frame_open && g_rt.session_running; }

const char *last_error() { return g_error; }

bool has_tracked_controllers() { return internal::controllers_seen(); }

bool start(const void *hwnd)
{
    (void)hwnd;
    if (g_rt.session) return true;
    trace::init();
    trace::line("cpu: %d bits do programa", (int)(sizeof(void *) * 8));
    trace::line("[1] carregando openxr_loader.dll");
    if (!load_loader()) return false;
    trace::line("[1] ok: %p", (void *)g_loader);
    trace::line("[2] xrCreateInstance");
    if (!create_instance()) {
        shutdown();
        return false;
    }
    trace::line("[2] ok: instance %p", (void *)g_rt.instance);
    trace::modules(); // com a instância de pé, as DLLs do runtime já estão carregadas: ficam no rastro
    trace::line("[3] sessao D3D11 (xrCreateSession)");
    if (!create_session()) {
        shutdown();
        return false;
    }
    trace::line("[3] ok: session %p", (void *)g_rt.session);
    trace::line("[4] swapchains + acoes");
    if (!create_swapchains() || !create_actions()) {
        shutdown();
        return false;
    }
    trace::line("[4] ok: swapchain %ux%u por olho", g_rt.eye[0].width, g_rt.eye[0].height);
    trace::line("=== sessao de pe ===");
    logf("[vr] pronto: %ux%u por olho; aproxime o headset e aperte o gatilho", g_rt.eye[0].width, g_rt.eye[0].height);
    return true;
}

bool start_once(const void *hwnd)
{
    static std::atomic<int> state{0}; // 0 = ainda não tentou, 1 = ok, 2 = falhou
    if (state.load() == 1) return true;
    if (state.load() == 2) return false;
    if (!internal::g_requested.load()) return false;
    if (start(hwnd)) {
        state = 1;
        return true;
    }
    state = 2;
    logf("[vr] sem headset: seguindo em tela plana (%s)", last_error());
    return false;
}

void shutdown()
{
    internal::g_active = false;
    if (g_rt.dev11 && g_rt.ctx11) g_rt.ctx11->ClearState();
    if (g_readback) {
        g_readback->Release();
        g_readback = nullptr;
    }
    if (g_rt.session && p_xrDestroySession) p_xrDestroySession(g_rt.session);
    if (g_messenger && p_xrDestroyDebugUtilsMessengerEXT) {
        p_xrDestroyDebugUtilsMessengerEXT(g_messenger);
        g_messenger = XR_NULL_HANDLE;
    }
    if (g_rt.instance && p_xrDestroyInstance) p_xrDestroyInstance(g_rt.instance);
    g_rt = internal::Runtime{};
}

void frame_begin()
{
    if (!g_rt.session) return;
    poll_events();
    if (!g_rt.session_running) return;

    XrFrameWaitInfo wi = {XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState fs = {XR_TYPE_FRAME_STATE};
    if (XR_FAILED(p_xrWaitFrame(g_rt.session, &wi, &fs))) return;

    XrFrameBeginInfo bi = {XR_TYPE_FRAME_BEGIN_INFO};
    if (XR_FAILED(p_xrBeginFrame(g_rt.session, &bi))) return;
    g_rt.frame_open = true;

    g_rt.display_time = fs.predictedDisplayTime;
    g_rt.display_period = fs.predictedDisplayPeriod;
    g_rt.should_render = fs.shouldRender != 0;

    g_view_locate = {XR_TYPE_VIEW_LOCATE_INFO};
    g_view_locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    g_view_locate.displayTime = fs.predictedDisplayTime;
    g_view_locate.space = g_rt.stage;
    g_rt.view_state = {XR_TYPE_VIEW_STATE};
    g_rt.view_count = internal::kEyes;
    for (auto &v : g_rt.views) v = {XR_TYPE_VIEW};
    if (XR_FAILED(p_xrLocateViews(g_rt.session, &g_view_locate, &g_rt.view_state, internal::kEyes, &g_rt.view_count,
                                  g_rt.views)))
        g_rt.view_count = 0;

    update_head();
    g_pose_serial++;

    // Adquire as duas imagens agora: o jogo desenha e o present escreve nelas.
    for (int e = 0; e < internal::kEyes; e++) {
        internal::EyeImage &ei = g_rt.eye[e];
        if (!ei.chain) continue;
        XrSwapchainImageAcquireInfo ai = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if (XR_FAILED(p_xrAcquireSwapchainImage(ei.chain, &ai, &ei.array_index))) {
            ei.acquired = false;
            continue;
        }
        XrSwapchainImageWaitInfo wi2 = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, nullptr, XR_INFINITE_DURATION};
        if (XR_FAILED(p_xrWaitSwapchainImage(ei.chain, &wi2))) {
            XrSwapchainImageReleaseInfo ri = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            p_xrReleaseSwapchainImage(ei.chain, &ri);
            ei.acquired = false;
            continue;
        }
        ei.acquired = true;
        ei.texture = ei.array_index < ei.images.size() ? ei.images[ei.array_index].texture : nullptr;
    }
    internal::sync_actions();
}

void frame_end()
{
    if (!g_rt.session || !g_rt.session_running) return;
    finish_frame();
}

/**
 * Lê uma superfície de render target do D3D9 e envia ao olho `eye`.
 * Caminho de CPU: GetRenderTargetData + LockRect + UpdateSubresource (o caminho rápido, com
 * textura compartilhada D3D9Ex, está descrito em docs/IMPLEMENTACAO.md).
 */
bool submit_surface(IDirect3DDevice9 *dev, IDirect3DSurface9 *surf, int eye)
{
    if (!dev || !surf || eye < 0 || eye >= internal::kEyes) return false;
    internal::EyeImage &ei = g_rt.eye[eye];
    if (!ei.acquired) return false;

    D3DSURFACE_DESC dd = {};
    surf->GetDesc(&dd);
    if (!g_readback || g_readback_w != (int)dd.Width || g_readback_h != (int)dd.Height) {
        if (g_readback) {
            g_readback->Release();
            g_readback = nullptr;
        }
        if (FAILED(dev->CreateOffscreenPlainSurface(dd.Width, dd.Height, dd.Format, D3DPOOL_SYSTEMMEM, &g_readback, nullptr)))
            g_readback = nullptr;
        g_readback_w = (int)dd.Width;
        g_readback_h = (int)dd.Height;
    }
    if (!g_readback) return false;

    bool ok = false;
    if (SUCCEEDED(dev->GetRenderTargetData(surf, g_readback))) {
        D3DLOCKED_RECT lr = {};
        if (SUCCEEDED(g_readback->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            ok = internal::upload_eye(eye, reinterpret_cast<const uint8_t *>(lr.pBits), dd.Width, dd.Height, (uint32_t)lr.Pitch);
            g_readback->UnlockRect();
        }
    }
    return ok;
}

bool submit_frame(IDirect3DTexture9 *frame, int eye)
{
    if (!frame) return false;
    IDirect3DDevice9 *dev = nullptr;
    if (FAILED(frame->GetDevice(&dev)) || !dev) return false;
    IDirect3DSurface9 *src = nullptr;
    if (FAILED(frame->GetSurfaceLevel(0, &src)) || !src) return false;
    const bool ok = submit_surface(dev, src, eye);
    src->Release();
    return ok;
}

bool submit_eye(IDirect3DDevice9 *dev, int eye)
{
    if (!dev) return false;
    IDirect3DSurface9 *bb = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return false;
    const bool ok = submit_surface(dev, bb, eye);
    bb->Release();
    return ok;
}

} // namespace vr
