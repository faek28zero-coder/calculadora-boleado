// Stubs do lado do PORT, só para o teste de ligação (vr_link_test.cpp).
// No build de verdade quem fornece estes símbolos é o draw.cpp/log.cpp do próprio port,
// depois das emendas — se o contrato de vr_frame.h mudar de assinatura, o link do teste
// quebra aqui em vez de quebrar no MSVC do usuário.
#include <cstdarg>
#include <cstdint>

#include "vr/vr_frame.h"

// log.cpp do port
void logf(const char *fmt, ...) { (void)fmt; }

// image.cpp do port: traduz o endereço da build NTSC-U para o da build em uso.
uint32_t xbe_va(uint32_t ntsc_va) { return 0x10000u + ntsc_va; }

namespace vr_draw {

void capture_begin() {}
void capture_end() {}

void capture_draw(uint32_t type, uint32_t prims, int32_t minv, uint32_t nverts, const uint16_t *indices,
                  const void *verts, uint32_t stride)
{
    (void)type; (void)prims; (void)minv; (void)nverts; (void)indices; (void)verts; (void)stride;
}

void replay() {}

Stats stats()
{
    Stats s{};
    return s;
}

} // namespace vr_draw
