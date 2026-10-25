// stub de d3d11.h: só os tipos que o envio do olho usa (checagem de sintaxe fora do Windows).
#pragma once
#define D3D11_SDK_VERSION 7
#define D3D11_CREATE_DEVICE_BGRA_SUPPORT 0x20
#include <windows.h>

struct D3D11_BOX { UINT left, top, front, right, bottom, back; };
struct D3D11_TEXTURE2D_DESC { UINT Width, Height, MipLevels, ArraySize; UINT Format; struct { UINT Count; UINT Quality; } SampleDesc; UINT Usage, BindFlags, CPUAccessFlags, MiscFlags; };
enum D3D11_USAGE { D3D11_USAGE_DEFAULT = 0 };

struct ID3D11Texture2D {
    virtual void GetDesc(D3D11_TEXTURE2D_DESC *) = 0;
    virtual unsigned long Release() = 0;
};
struct ID3D11DeviceContext {
    virtual void UpdateSubresource(ID3D11Texture2D *, UINT, const D3D11_BOX *, const void *, UINT, UINT) = 0;
    virtual void ClearState() = 0;
    virtual unsigned long Release() = 0;
};
struct ID3D11Device {
    virtual HRESULT CreateTexture2D(const D3D11_TEXTURE2D_DESC *, const void *, ID3D11Texture2D **) = 0;
    virtual void GetImmediateContext(ID3D11DeviceContext **) = 0;
    virtual unsigned long Release() = 0;
};
typedef HRESULT(WINAPI *PFN_D3D11_CREATE_DEVICE)(void *, int, void *, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
inline HRESULT D3D11CreateDevice(void *, int, void *, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **)
{
    return E_FAIL;
}
