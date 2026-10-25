// BLACK VR - OpenXR sobre Direct3D 12 (o mesmo desenho do port de 64 bits).
//
// Etapa 1 ("tela no oculos"): o jogo continua desenhando na janela como sempre;
// antes de mandar o comando para a placa, copiamos o quadro para as imagens da
// sessao do oculos (uma por olho) e, depois do envio, entregamos o quadro.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef XR_USE_PLATFORM_WIN32
#define XR_USE_PLATFORM_WIN32 1
#endif
#ifndef XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_D3D12 1
#endif
#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"

#include "vr/vr_openxr.h"

using Microsoft::WRL::ComPtr;

namespace vr {
namespace {

// ------------------------------------------------------------------ estado
constexpr int kOlhos = 2;
constexpr int kMaxImagens = 8;

struct Gerenciador {
    bool pedido = false;   // BLACK_VR=1: o jogador quer VR
    bool tentado = false;  // já tentamos montar a sessão
    bool ativo = false;    // sessão montada e entregando quadro

    HMODULE loader = nullptr;
    PFN_xrGetInstanceProcAddr gipa = nullptr;
    PFN_xrPollEvent poll = nullptr;
    PFN_xrCreateReferenceSpace criar_espaco = nullptr;
    PFN_xrEnumerateSwapchainFormats formatos = nullptr;
    PFN_xrCreateSwapchain criar_troca = nullptr;
    PFN_xrEnumerateSwapchainImages imagens_da_troca = nullptr;
    PFN_xrAcquireSwapchainImage pegar = nullptr;
    PFN_xrWaitSwapchainImage esperar = nullptr;
    PFN_xrReleaseSwapchainImage soltar = nullptr;
    PFN_xrWaitFrame esperar_quadro = nullptr;
    PFN_xrBeginFrame comecar_quadro = nullptr;
    PFN_xrEndFrame entregar_quadro = nullptr;
    PFN_xrLocateViews localizar = nullptr;
    PFN_xrBeginSession comecar_sessao = nullptr;
    PFN_xrEndSession terminar_sessao = nullptr;
    PFN_xrGetD3D12GraphicsRequirementsKHR requisitos = nullptr;

    XrInstance instancia = XR_NULL_HANDLE;
    XrSystemId sistema = XR_NULL_SYSTEM_ID;
    XrSession sessao = XR_NULL_HANDLE;
    XrSpace espaco = XR_NULL_HANDLE;
    bool sessao_comecada = false;

    ComPtr<ID3D12Device> device;
    ID3D12CommandQueue *queue = nullptr;  // do port, não é nosso

    struct Olho {
        XrSwapchain troca = XR_NULL_HANDLE;
        uint32_t largura = 0, altura = 0;
        uint32_t indice = 0;      // imagem pega neste quadro
        bool pega = false;
        std::vector<ComPtr<ID3D12Resource>> imagens;
    } olho[kOlhos];
    int olhos = 0;

    int64_t formato = 0;
    bool formato_copiavel = false;

    XrFrameState quadro = {XR_TYPE_FRAME_STATE};
    bool quadro_aberto = false;
    uint32_t entregues = 0;
    uint32_t falhas = 0;
};

Gerenciador g;

void Log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char linha[1024];
    vsnprintf(linha, sizeof linha, fmt, ap);
    va_end(ap);
    OutputDebugStringA("[VR] ");
    OutputDebugStringA(linha);
    OutputDebugStringA("\n");
    std::fprintf(stderr, "[VR] %s\n", linha);
    std::fflush(stderr);
}

template <typename Fn>
bool Ligar(Fn &fn, const char *nome) {
    fn = nullptr;
    if (!g.gipa) return false;
    if (XR_FAILED(g.gipa(g.instancia, nome, reinterpret_cast<PFN_xrVoidFunction *>(&fn))) || !fn) {
        Log("o aplicativo do oculos nao entregou %s", nome);
        return false;
    }
    return true;
}

const char *NomeErro(XrResult r) {
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
        case XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING: return "XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING";
        default: return "erro nao catalogado";
    }
}

bool QuerVR() {
    char valor[16] = {};
    DWORD n = GetEnvironmentVariableA("BLACK_VR", valor, sizeof valor);
    if (n == 0 || n >= sizeof valor) return false;
    for (char *p = valor; *p; p++) *p = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
    return !std::strcmp(valor, "1") || !std::strcmp(valor, "sim") || !std::strcmp(valor, "on") ||
           !std::strcmp(valor, "yes");
}

// ------------------------------------------------------------------ sessão
bool MontarSessao(ID3D12Device *dev, ID3D12CommandQueue *fila) {
    if (!g.pedido) return false;
    if (g.tentado) return g.ativo;
    g.tentado = true;
    g.device = dev;
    g.queue = fila;

    g.loader = LoadLibraryA("openxr_loader.dll");
    if (!g.loader) {
        Log("openxr_loader.dll nao carregou (erro %lu): coloque o de 64 bits ao lado do BLACK.exe",
            GetLastError());
        return false;
    }
    g.gipa = reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(g.loader, "xrGetInstanceProcAddr"));
    if (!g.gipa) {
        Log("openxr_loader.dll sem xrGetInstanceProcAddr");
        return false;
    }

    PFN_xrEnumerateInstanceExtensionProperties listar = nullptr;
    PFN_xrCreateInstance criar = nullptr;
    if (XR_FAILED(g.gipa(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties",
                         reinterpret_cast<PFN_xrVoidFunction *>(&listar))) ||
        !listar ||
        XR_FAILED(g.gipa(XR_NULL_HANDLE, "xrCreateInstance", reinterpret_cast<PFN_xrVoidFunction *>(&criar))) ||
        !criar) {
        Log("nao consegui falar com o conector OpenXR");
        return false;
    }

    uint32_t total = 0;
    if (XR_FAILED(listar(nullptr, 0, &total, nullptr)) || !total) {
        Log("nenhuma extensao oferecida (o aplicativo do oculos esta aberto?)");
        return false;
    }
    std::vector<XrExtensionProperties> props(total, {XR_TYPE_EXTENSION_PROPERTIES});
    listar(nullptr, total, &total, props.data());
    bool tem_d3d12 = false;
    for (uint32_t i = 0; i < total; i++)
        if (!std::strcmp(props[i].extensionName, "XR_KHR_D3D12_enable")) tem_d3d12 = true;
    if (!tem_d3d12) {
        Log("o aplicativo do oculos nao oferece XR_KHR_D3D12_enable");
        return false;
    }

    const char *pedidas[1] = {"XR_KHR_D3D12_enable"};
    XrInstanceCreateInfo ci = {XR_TYPE_INSTANCE_CREATE_INFO};
    std::snprintf(ci.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "BLACK VR");
    std::snprintf(ci.applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, "BLACK XBERecomp");
    ci.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    ci.enabledExtensionCount = 1;
    ci.enabledExtensionNames = pedidas;
    XrResult r = criar(&ci, &g.instancia);
    if (XR_FAILED(r)) {
        Log("xrCreateInstance: %s (%d)", NomeErro(r), static_cast<int>(r));
        return false;
    }

    PFN_xrGetSystem pegar_sistema = nullptr;
    PFN_xrGetInstanceProperties props_inst = nullptr;
    if (!Ligar(pegar_sistema, "xrGetSystem") || !Ligar(g.requisitos, "xrGetD3D12GraphicsRequirementsKHR") ||
        !Ligar(g.poll, "xrPollEvent") || !Ligar(g.criar_espaco, "xrCreateReferenceSpace") ||
        !Ligar(g.formatos, "xrEnumerateSwapchainFormats") || !Ligar(g.criar_troca, "xrCreateSwapchain") ||
        !Ligar(g.imagens_da_troca, "xrEnumerateSwapchainImages") || !Ligar(g.pegar, "xrAcquireSwapchainImage") ||
        !Ligar(g.esperar, "xrWaitSwapchainImage") || !Ligar(g.soltar, "xrReleaseSwapchainImage") ||
        !Ligar(g.esperar_quadro, "xrWaitFrame") || !Ligar(g.comecar_quadro, "xrBeginFrame") ||
        !Ligar(g.entregar_quadro, "xrEndFrame") || !Ligar(g.localizar, "xrLocateViews") ||
        !Ligar(g.comecar_sessao, "xrBeginSession") || !Ligar(g.terminar_sessao, "xrEndSession")) {
        return false;
    }
    g.gipa(g.instancia, "xrGetInstanceProperties", reinterpret_cast<PFN_xrVoidFunction *>(&props_inst));
    if (props_inst) {
        XrInstanceProperties ip = {XR_TYPE_INSTANCE_PROPERTIES};
        if (XR_SUCCEEDED(props_inst(g.instancia, &ip)))
            Log("runtime: %s %u.%u.%u", ip.runtimeName, XR_VERSION_MAJOR(ip.runtimeVersion),
                XR_VERSION_MINOR(ip.runtimeVersion), XR_VERSION_PATCH(ip.runtimeVersion));
    }

    XrSystemGetInfo sgi = {XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    r = pegar_sistema(g.instancia, &sgi, &g.sistema);
    if (XR_FAILED(r)) {
        Log("xrGetSystem: %s (%d) - oculos ligado no cabo?", NomeErro(r), static_cast<int>(r));
        return false;
    }

    XrGraphicsRequirementsD3D12KHR req = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12KHR};
    r = g.requisitos(g.instancia, g.sistema, &req);
    if (XR_FAILED(r)) {
        Log("requisitos D3D12: %s (%d)", NomeErro(r), static_cast<int>(r));
        return false;
    }
    DXGI_ADAPTER_DESC1 desc{};
    {
        ComPtr<IDXGIDevice> dxgi_dev;
        ComPtr<IDXGIAdapter> adaptador;
        if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&dxgi_dev))) &&
            SUCCEEDED(dxgi_dev->GetAdapter(&adaptador)) &&
            SUCCEEDED(adaptador->GetDesc1(&desc))) {
            Log("placa do jogo: luid %08lx:%08lx | placa pedida: luid %08lx:%08lx",
                static_cast<unsigned long>(desc.AdapterLuid.HighPart), static_cast<unsigned long>(desc.AdapterLuid.LowPart),
                static_cast<unsigned long>(req.adapterLuid.HighPart), static_cast<unsigned long>(req.adapterLuid.LowPart));
            if (desc.AdapterLuid.LowPart != req.adapterLuid.LowPart ||
                desc.AdapterLuid.HighPart != req.adapterLuid.HighPart)
                Log("AVISO: a placa do jogo pode nao ser a que o oculos pede");
        }
    }

    XrGraphicsBindingD3D12KHR binding = {XR_TYPE_GRAPHICS_BINDING_D3D12KHR};
    binding.device = dev;
    binding.queue = fila;
    XrSessionCreateInfo sci = {XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &binding;
    sci.systemId = g.sistema;
    r = XR_ERROR_RUNTIME_FAILURE;
    PFN_xrCreateSession criar_sessao = nullptr;
    if (!Ligar(criar_sessao, "xrCreateSession")) return false;
    Log("pedindo a sessao ao oculos...");
    r = criar_sessao(g.instancia, &sci, &g.sessao);
    if (XR_FAILED(r)) {
        Log("xrCreateSession: %s (%d)", NomeErro(r), static_cast<int>(r));
        return false;
    }
    Log("SESSAO CRIADA");

    XrReferenceSpaceCreateInfo rsci = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rsci.poseInReferenceSpace.orientation.w = 1.0f;
    r = g.criar_espaco(g.sessao, &rsci, &g.espaco);
    if (XR_FAILED(r)) {
        Log("xrCreateReferenceSpace: %s (%d)", NomeErro(r), static_cast<int>(r));
        return false;
    }

    g.ativo = true;
    Log("VR ligada: esperando o oculos ficar pronto");
    return true;
}

// troca de imagens: uma por olho, do tamanho do quadro do jogo
bool CriarTrocas(uint32_t largura, uint32_t altura, int formato_dxgi) {
    uint32_t n = 0;
    if (XR_FAILED(g.formatos(g.sessao, 0, &n, nullptr)) || !n) {
        Log("xrEnumerateSwapchainFormats falhou");
        return false;
    }
    std::vector<int64_t> lista(n);
    g.formatos(g.sessao, n, &n, lista.data());
    bool achou_igual = false;
    for (uint32_t i = 0; i < n; i++)
        if (lista[i] == formato_dxgi) achou_igual = true;
    g.formato = achou_igual ? formato_dxgi : lista[0];
    g.formato_copiavel = achou_igual;
    Log("formato do jogo: %d | formato da troca: %lld | copia direta: %s", formato_dxgi,
        static_cast<long long>(g.formato), achou_igual ? "sim" : "NAO (a imagem vai ficar preta)");

    g.olhos = 2;
    for (int e = 0; e < kOlhos; e++) {
        XrSwapchainCreateInfo sci = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        sci.format = g.formato;
        sci.sampleCount = 1;
        sci.width = largura;
        sci.height = altura;
        sci.faceCount = 1;
        sci.arraySize = 1;
        sci.mipCount = 1;
        XrResult r = g.criar_troca(g.sessao, &sci, &g.olho[e].troca);
        if (XR_FAILED(r)) {
            Log("xrCreateSwapchain (olho %d): %s (%d)", e, NomeErro(r), static_cast<int>(r));
            return false;
        }
        g.olho[e].largura = largura;
        g.olho[e].altura = altura;
        uint32_t count = 0;
        if (XR_FAILED(g.imagens_da_troca(g.olho[e].troca, 0, &count, nullptr)) || !count) {
            Log("xrEnumerateSwapchainImages (olho %d) falhou", e);
            return false;
        }
        if (count > static_cast<uint32_t>(kMaxImagens)) count = kMaxImagens;
        std::vector<XrSwapchainImageD3D12KHR> imgs(count, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12KHR});
        r = g.imagens_da_troca(g.olho[e].troca, count, &count,
                               reinterpret_cast<XrSwapchainImageBaseHeader *>(imgs.data()));
        if (XR_FAILED(r)) {
            Log("xrEnumerateSwapchainImages (olho %d): %s (%d)", e, NomeErro(r), static_cast<int>(r));
            return false;
        }
        for (uint32_t i = 0; i < count; i++) g.olho[e].imagens.push_back(imgs[i].texture);
        Log("olho %d: %ux%u, %u imagens", e, largura, altura, count);
    }
    return true;
}

// estado da sessão: o oculos só aceita quadro depois de "pronto"
void AndarEstado() {
    XrEventDataBuffer ev = {XR_TYPE_EVENT_DATA_BUFFER};
    while (XR_SUCCEEDED(g.poll(g.instancia, &ev))) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            const XrEventDataSessionStateChanged *ch = reinterpret_cast<const XrEventDataSessionStateChanged *>(&ev);
            Log("estado da sessao: %d", static_cast<int>(ch->state));
            if (ch->state == XR_SESSION_STATE_READY && !g.sessao_comecada) {
                XrSessionBeginInfo bi = {XR_TYPE_SESSION_BEGIN_INFO};
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                XrResult r = g.comecar_sessao(g.sessao, &bi);
                if (XR_FAILED(r)) Log("xrBeginSession: %s (%d)", NomeErro(r), static_cast<int>(r));
                else g.sessao_comecada = true;
            } else if (ch->state == XR_SESSION_STATE_EXITING || ch->state == XR_SESSION_STATE_LOSS_PENDING) {
                g.ativo = false;
            }
        }
        std::memset(&ev, 0, sizeof ev);
        ev.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
}

void Transicao(ID3D12GraphicsCommandList *cl, ID3D12Resource *recurso, D3D12_RESOURCE_STATES antes,
               D3D12_RESOURCE_STATES depois) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = recurso;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = antes;
    b.Transition.StateAfter = depois;
    cl->ResourceBarrier(1, &b);
}

}  // namespace

// ------------------------------------------------------------------ público
void IniciarSePedido() {
    g.pedido = QuerVR();
    if (g.pedido) Log("BLACK_VR ligado: a VR vai ser tentada no primeiro quadro");
}

bool Ativo() { return g.ativo; }

void GravarCopia(void *device, void *fila, void *lista, void *backbuffer, uint32_t largura, uint32_t altura,
                 int formato_dxgi) {
    if (!g.pedido) return;
    auto *dev = static_cast<ID3D12Device *>(device);
    auto *queue = static_cast<ID3D12CommandQueue *>(fila);
    auto *cl = static_cast<ID3D12GraphicsCommandList *>(lista);
    auto *origem = static_cast<ID3D12Resource *>(backbuffer);
    if (!dev || !queue || !cl || !origem) return;

    if (!g.ativo) {
        if (!MontarSessao(dev, queue)) return;
        if (!CriarTrocas(largura, altura, formato_dxgi)) {
            g.ativo = false;
            return;
        }
    }
    AndarEstado();
    if (!g.ativo) return;

    if (!g.quadro_aberto) {
        XrFrameWaitInfo fwi = {XR_TYPE_FRAME_WAIT_INFO};
        std::memset(&g.quadro, 0, sizeof g.quadro);
        g.quadro.type = XR_TYPE_FRAME_STATE;
        if (XR_FAILED(g.esperar_quadro(g.sessao, &fwi, &g.quadro))) return;
        XrFrameBeginInfo fbi = {XR_TYPE_FRAME_BEGIN_INFO};
        if (XR_FAILED(g.comecar_quadro(g.sessao, &fbi))) return;
        g.quadro_aberto = true;
    }

    bool copiou = false;
    for (int e = 0; e < g.olhos; e++) {
        uint32_t idx = 0;
        XrSwapchainImageAcquireInfo ai = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if (XR_FAILED(g.pegar(g.olho[e].troca, &ai, &idx))) return;
        XrSwapchainImageWaitInfo wi = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wi.timeout = XR_INFINITE_DURATION;
        if (XR_FAILED(g.esperar(g.olho[e].troca, &wi))) return;
        g.olho[e].indice = idx;
        g.olho[e].pega = true;
        if (idx < g.olho[e].imagens.size() && g.formato_copiavel) {
            ID3D12Resource *destino = g.olho[e].imagens[idx].Get();
            Transicao(cl, destino, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION dst{destino, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
            D3D12_TEXTURE_COPY_LOCATION src{origem, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
            cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            Transicao(cl, destino, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
            copiou = true;
        }
    }
    if (!copiou) g.falhas++;
}

void DepoisDoEnvio() {
    if (!g.pedido || !g.ativo || !g.quadro_aberto) return;

    XrViewLocateInfo vli = {XR_TYPE_VIEW_LOCATE_INFO};
    vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vli.displayTime = g.quadro.predictedDisplayTime;
    vli.space = g.espaco;
    XrViewState vs = {XR_TYPE_VIEW_STATE};
    XrView views[kOlhos] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    uint32_t vistas = 0;
    g.localizar(g.sessao, &vli, &vs, kOlhos, &vistas, views);
    if (vistas == 0) vistas = kOlhos;

    XrCompositionLayerProjectionView camadas[kOlhos] = {{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
                                                        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
    for (int e = 0; e < g.olhos; e++) {
        if (g.olho[e].pega) {
            XrSwapchainImageReleaseInfo ri = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            g.soltar(g.olho[e].troca, &ri);
            g.olho[e].pega = false;
        }
        camadas[e].pose = views[e].pose;
        // campo de visao fixo: a imagem do jogo ocupa o meio da tela do oculos
        camadas[e].fov.angleLeft = -0.62f;
        camadas[e].fov.angleRight = 0.62f;
        camadas[e].fov.angleUp = 0.52f;
        camadas[e].fov.angleDown = -0.52f;
        camadas[e].subImage.swapchain = g.olho[e].troca;
        camadas[e].subImage.imageRect.offset.x = 0;
        camadas[e].subImage.imageRect.offset.y = 0;
        camadas[e].subImage.imageRect.extent.width = static_cast<int32_t>(g.olho[e].largura);
        camadas[e].subImage.imageRect.extent.height = static_cast<int32_t>(g.olho[e].altura);
    }

    XrCompositionLayerProjection projecao = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projecao.space = g.espaco;
    projecao.viewCount = static_cast<uint32_t>(g.olhos);
    projecao.views = camadas;
    const XrCompositionLayerBaseHeader *camadas_finais[1] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader *>(&projecao)};
    XrFrameEndInfo fei = {XR_TYPE_FRAME_END_INFO};
    fei.displayTime = g.quadro.predictedDisplayTime;
    fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    fei.layerCount = 1;
    fei.layers = camadas_finais;
    if (XR_SUCCEEDED(g.entregar_quadro(g.sessao, &fei))) {
        if (g.entregues == 0) Log("primeiro quadro entregue no oculos");
        g.entregues++;
    }
    g.quadro_aberto = false;
    if (g.entregues == 120) Log("120 quadros entregues, %u sem copia", g.falhas);
}

}  // namespace vr
