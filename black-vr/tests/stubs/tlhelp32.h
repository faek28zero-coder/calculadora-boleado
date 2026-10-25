// stub de tlhelp32.h: o rastro lista os módulos carregados (só Windows de verdade tem isto).
#pragma once
#include <windows.h>

#define TH32CS_SNAPMODULE 0x00000008
#define TH32CS_SNAPMODULE32 0x00000010

struct MODULEENTRY32W {
    unsigned long dwSize;
    unsigned long th32ModuleID;
    unsigned long th32ProcessID;
    unsigned long GlblcntUsage;
    unsigned long ProccntUsage;
    unsigned char *modBaseAddr;
    unsigned long modBaseSize;
    void *hModule;
    wchar_t szModule[256];
    wchar_t szExePath[260];
};
inline void *CreateToolhelp32Snapshot(unsigned long, unsigned long) { return (void *)-1; }
inline int Module32FirstW(void *, MODULEENTRY32W *) { return 0; }
inline int Module32NextW(void *, MODULEENTRY32W *) { return 0; }
