// ============================================================
//  BLACK VR — tests/stubs/windows.h (+ os outros headers de stub)
//
//  Stubs mínimos de Win32/D3D9/D3D11 para COMPILAR (não rodar) os trechos do port que o
//  módulo de VR toca, num compilador que não é o MSVC: é assim que se descobre um erro de
//  sintaxe ou uma assinatura trocada antes de abrir o Windows.
//
//  O que é verificado com isto:
//    - draw.cpp: o bloco do c201..c204 e o gravador/replay (gramática e tipos);
//    - nv2a_vsh.cpp: o trecho de shader gerado;
//    - vr_math/vr_pose: a matemática.
//  O que NÃO é verificado: comportamento em tempo de execução, asm inline da MSVC (o thunk de
//  0x1DFFEC é `__declspec(naked)` e só a MSVC compila) e as chamadas reais de D3D.
// ============================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ---- tipos básicos ----
typedef unsigned long DWORD;
typedef long LONG;
typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef float FLOAT;
typedef void *HANDLE;
typedef void *HWND;
typedef void *HMODULE;
typedef int HRESULT;
typedef unsigned long long ULONGLONG;
typedef intptr_t INT_PTR;
#define __cdecl
#define __declspec(x)
#define __uuidof(x) IID_##x
struct GUID;
inline const void *IID_IDXGIFactory1() { return nullptr; }
typedef uintptr_t DWORD_PTR;
typedef uint32_t UINT32;
typedef int32_t INT32;
typedef uint16_t WORD;
typedef uint8_t BYTE;
struct LARGE_INTEGER { long long QuadPart; };
struct LUID { DWORD LowPart; LONG HighPart; };
typedef enum D3D_FEATURE_LEVEL { D3D_FEATURE_LEVEL_9_1 = 0x9100, D3D_FEATURE_LEVEL_10_0 = 0xa000, D3D_FEATURE_LEVEL_10_1 = 0xa100, D3D_FEATURE_LEVEL_11_0 = 0xb000 } D3D_FEATURE_LEVEL;
struct IUnknown { virtual unsigned long Release() = 0; virtual HRESULT QueryInterface(const void *, void **) = 0; };
struct GUID;
enum D3D_DRIVER_TYPE { D3D_DRIVER_TYPE_UNKNOWN = 0, D3D_DRIVER_TYPE_HARDWARE = 1 };
struct SECURITY_ATTRIBUTES;
struct OVERLAPPED;
struct EXCEPTION_RECORD {
    unsigned long ExceptionCode;
    void *ExceptionAddress;
    unsigned long NumberParameters;
    unsigned long long ExceptionInformation[15];
};
struct _EXCEPTION_POINTERS { EXCEPTION_RECORD *ExceptionRecord; void *ContextRecord; };
using EXCEPTION_POINTERS = _EXCEPTION_POINTERS;
struct CONTEXT;
struct CRITICAL_SECTION;
inline long InterlockedCompareExchange(volatile long *, long, long) { return 0; }
inline long InterlockedExchange(volatile long *, long) { return 0; }
struct NT_TIB { void *ExceptionList, *StackBase, *StackLimit; };
#define GetCurrentThread() ((void*)0)
#define GetCurrentProcess() ((void*)0)
#define GetCurrentProcessId() 0ul
inline int CloseHandle(void *) { return 1; }
#define CP_ACP 0
inline int WideCharToMultiByte(int, unsigned long, const wchar_t *, int, char *, int, const char *, int *) { return 0; }
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((void*)-1)
#endif
struct CONTEXT { unsigned long Eip, Esp, Ebp, Eax, Ebx, Ecx, Edx, Esi, Edi; };
inline int SetThreadPriority(void*, int) { return 1; }
typedef void *FARPROC;
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0
inline HMODULE LoadLibraryA(const char *) { return nullptr; }
inline HMODULE LoadLibraryW(const wchar_t *) { return nullptr; }
inline HMODULE LoadLibraryExA(const char *, void *, DWORD) { return nullptr; }
inline FARPROC GetProcAddress(HMODULE, const char *) { return nullptr; }
inline BOOL FreeLibrary(HMODULE) { return 1; }
inline DWORD GetLastError() { return 0; }
struct MODULEINFO;
inline unsigned int GetModuleFileNameA(HMODULE, char *, unsigned int) { return 0; }
struct MEMORY_BASIC_INFORMATION { void *BaseAddress; void *AllocationBase; unsigned long State; };
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_FREE 0x10000
inline unsigned long VirtualQuery(const void *, MEMORY_BASIC_INFORMATION *, unsigned long) { return 0; }
using PVECTORED_EXCEPTION_HANDLER = long (*)(struct _EXCEPTION_POINTERS *);
inline void *AddVectoredExceptionHandler(unsigned long, PVECTORED_EXCEPTION_HANDLER) { return nullptr; }
#define EXCEPTION_ACCESS_VIOLATION 0xC0000005ul
#define EXCEPTION_ILLEGAL_INSTRUCTION 0xC000001Dul
#define EXCEPTION_STACK_OVERFLOW 0xC00000FDul
#define EXCEPTION_INT_DIVIDE_BY_ZERO 0xC0000094ul
#define EXCEPTION_CONTINUE_SEARCH 0
using LONG = long;
using ULONG_PTR = unsigned long long;
#define CALLBACK
inline int MessageBoxA(void *, const char *, const char *, unsigned int) { return 0; }
struct HKEY;
struct FILETIME { DWORD dwLowDateTime, dwHighDateTime; };
struct SYSTEMTIME { WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; };
struct SYSTEM_INFO { DWORD dwNumberOfProcessors; };
inline void GetSystemInfo(SYSTEM_INFO *) {}
inline int timeBeginPeriod(unsigned) { return 0; }
#define THREAD_PRIORITY_TIME_CRITICAL 15
#define CONTEXT_DEBUG_REGISTERS 0
#define CONTEXT_FULL 0
inline int GetThreadContext(void*, void*) { return 1; }
inline int SetThreadContext(void*, void*) { return 1; }
inline int QueryPerformanceCounter(LARGE_INTEGER *c) { if (c) c->QuadPart = 0; return 1; }
inline int QueryPerformanceFrequency(LARGE_INTEGER *c) { if (c) c->QuadPart = 1000000; return 1; }

#define WINAPI
#define __stdcall
#define TRUE 1
#define FALSE 0
#define S_OK 0
#define E_FAIL ((HRESULT)0x80004005L)
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#define FAILED(hr) ((HRESULT)(hr) < 0)
#define D3D_OK 0
#define D3DERR_INVALIDCALL ((HRESULT)0x8876086CL)
#define D3DERR_DEVICELOST ((HRESULT)0x88760868L)
#define INFINITE 0xFFFFFFFF
#define MAX_PATH 260
#define WIN32_LEAN_AND_MEAN

// ---- D3D11 (só o que o envio do olho usa) ----
struct ID3D11Texture2D;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct DXGI_ADAPTER_DESC1;

// ---- utilidades que o código do port espera ----
inline DWORD GetTickCount64() { return 0; }
inline void Sleep(DWORD) {}
inline HWND GetForegroundWindow() { return nullptr; }
