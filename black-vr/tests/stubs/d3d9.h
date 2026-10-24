// stub de d3d9.h para checagem de sintaxe fora do Windows (ver windows.h)
#pragma once
#include <windows.h>
#define D3DLOCK_READONLY 0x00000010L
// ---- D3D9 ----
enum D3DPRIMITIVETYPE { D3DPT_POINTLIST = 1, D3DPT_LINELIST, D3DPT_LINESTRIP, D3DPT_TRIANGLELIST, D3DPT_TRIANGLESTRIP, D3DPT_TRIANGLEFAN };
enum D3DFORMAT { D3DFMT_INDEX16 = 101, D3DFMT_A8R8G8B8 = 21 };
enum D3DPOOL { D3DPOOL_DEFAULT = 0, D3DPOOL_MANAGED = 1, D3DPOOL_SYSTEMMEM = 2 };
enum D3DBACKBUFFER_TYPE { D3DBACKBUFFER_TYPE_MONO = 0 };
struct D3DVIEWPORT9 { DWORD X, Y, Width, Height; float MinZ, MaxZ; };
struct D3DMATRIX { union { struct { float _11, _12, _13, _14, _21, _22, _23, _24, _31, _32, _33, _34, _41, _42, _43, _44; }; float m[4][4]; }; };
struct D3DSURFACE_DESC { D3DFORMAT Format; UINT Width, Height; };
struct D3DLOCKED_RECT { INT_PTR Pitch; void *pBits; };
struct RECT { LONG left, top, right, bottom; };

struct IDirect3DSurface9 {
    virtual HRESULT GetDesc(D3DSURFACE_DESC *) = 0;
    virtual HRESULT LockRect(D3DLOCKED_RECT *, const RECT *, DWORD) = 0;
    virtual HRESULT UnlockRect() = 0;
    virtual unsigned long Release() = 0;
};

struct IDirect3DDevice9;
struct IDirect3DTexture9 {
    virtual HRESULT GetSurfaceLevel(UINT, IDirect3DSurface9 **) = 0;
    virtual HRESULT GetLevelDesc(UINT, D3DSURFACE_DESC *) = 0;
    virtual HRESULT GetDevice(IDirect3DDevice9 **) = 0;
    virtual unsigned long Release() = 0;
};

struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;
struct IDirect3DVertexDeclaration9;
struct IDirect3DBaseTexture9;

struct IDirect3DDevice9 {
    virtual HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE, UINT, const void *, UINT) = 0;
    virtual HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE, UINT, UINT, UINT, const void *, D3DFORMAT, const void *, UINT) = 0;
    virtual HRESULT SetVertexShaderConstantF(UINT, const float *, UINT) = 0;
    virtual HRESULT SetVertexShader(IDirect3DVertexShader9 *) = 0;
    virtual HRESULT SetPixelShader(IDirect3DPixelShader9 *) = 0;
    virtual HRESULT SetVertexDeclaration(IDirect3DVertexDeclaration9 *) = 0;
    virtual HRESULT SetFVF(DWORD) = 0;
    virtual HRESULT SetRenderState(DWORD, DWORD) = 0;
    virtual HRESULT SetTexture(DWORD, IDirect3DBaseTexture9 *) = 0;
    virtual HRESULT SetViewport(const D3DVIEWPORT9 *) = 0;
    virtual HRESULT SetRenderTarget(UINT, IDirect3DSurface9 *) = 0;
    virtual HRESULT GetBackBuffer(UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9 **) = 0;
    virtual HRESULT CreateOffscreenPlainSurface(UINT, UINT, D3DFORMAT, D3DPOOL, IDirect3DSurface9 **, HANDLE *) = 0;
    virtual HRESULT GetRenderTargetData(IDirect3DSurface9 *, IDirect3DSurface9 *) = 0;
    virtual HRESULT CreateTexture(UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9 **, HANDLE *) = 0;
    virtual HRESULT ColorFill(IDirect3DSurface9 *, const RECT *, DWORD) = 0;
    virtual HRESULT StretchRect(IDirect3DSurface9 *, const RECT *, IDirect3DSurface9 *, const RECT *, int) = 0;
    virtual HRESULT BeginScene() = 0;
    virtual HRESULT EndScene() = 0;
    virtual HRESULT Clear(DWORD, const void *, DWORD, DWORD, float, DWORD) = 0;
};

