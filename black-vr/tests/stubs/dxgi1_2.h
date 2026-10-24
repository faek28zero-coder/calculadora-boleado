// stub de dxgi1_2.h (checagem de sintaxe fora do Windows).
#pragma once
#include <windows.h>
#include <d3d11.h>

enum DXGI_FORMAT { DXGI_FORMAT_UNKNOWN = 0, DXGI_FORMAT_B8G8R8A8_UNORM = 87, DXGI_FORMAT_R8G8B8A8_UNORM = 28, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29 };
#define DXGI_ERROR_NOT_FOUND ((HRESULT)0x887A0002L)
struct DXGI_ADAPTER_DESC1 { wchar_t Description[128]; UINT VendorId, DeviceId, SubSysId, Revision; size_t DedicatedVideoMemory; LUID AdapterLuid; UINT Flags; };
struct IDXGIAdapter1;
struct IDXGIAdapter1 {
    virtual HRESULT GetDesc1(DXGI_ADAPTER_DESC1 *) = 0;
    virtual unsigned long Release() = 0;
};
struct IDXGIFactory1 {
    virtual HRESULT EnumAdapters1(UINT, IDXGIAdapter1 **) = 0;
    virtual unsigned long Release() = 0;
};
inline HRESULT CreateDXGIFactory1(const void *(&)(), void **) { return E_FAIL; }
struct IDXGIDevice;
