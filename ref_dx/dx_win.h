#ifdef _WIN32
#  include <windows.h>
#endif

#include <d3d11_1.h>
#include "../client/ref.h"

#ifndef __DXW_WIN_H__
#define __DXW_WIN_H__

typedef struct
{
	HINSTANCE	hInstance;
	void	*wndproc;

	HDC     hDC;			// handle to device context
	HWND    hWnd;			// handle to window

	qboolean allowdisplaydepthchange;

} dxwstate_t;

#endif

extern dxwstate_t dxw_state;

typedef enum
{
	rserr_ok,

	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
} rserr_t;

extern refimport_t ri;

extern qboolean allow_cds;
extern qboolean fullscreen;

extern cvar_t	*dx_bitdepth;

extern cvar_t	*vid_fullscreen;


extern D3D_DRIVER_TYPE         g_driverType;
extern D3D_FEATURE_LEVEL       g_featureLevel;
extern ID3D11Device*           g_pd3dDevice;
extern ID3D11Device1*          g_pd3dDevice1;
extern ID3D11DeviceContext*    g_pImmediateContext;
extern ID3D11DeviceContext1*   g_pImmediateContext1;
extern IDXGISwapChain*         g_pSwapChain;
extern IDXGISwapChain1*        g_pSwapChain1;
extern ID3D11RenderTargetView* g_pRenderTargetView;