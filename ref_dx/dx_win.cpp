#include "dx_win.h"
#include "../win32/winquake.h"

#include <directxmath.h>
#include <d3dcompiler.h>

D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*           g_pd3dDevice = 0;
ID3D11Device1*          g_pd3dDevice1 = 0;
ID3D11DeviceContext*    g_pImmediateContext = 0;
ID3D11DeviceContext1*   g_pImmediateContext1 = 0;
IDXGISwapChain*         g_pSwapChain = 0;
IDXGISwapChain1*        g_pSwapChain1 = 0;
ID3D11RenderTargetView* g_pRenderTargetView = 0;

dxwstate_t dxw_state;

#define	WINDOW_CLASS_NAME	L"Quake 2"

HRESULT InitDevice();
void DXimp_Shutdown(void);

qboolean VID_CreateWindow(int width, int height, qboolean fullscreen)
{
	WNDCLASS		wc;
	RECT			r;
	cvar_t			*vid_xpos, *vid_ypos;
	int				stylebits;
	int				x, y, w, h;
	int				exstyle;

	/* Register the frame class */
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)dxw_state.wndproc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = dxw_state.hInstance;
	wc.hIcon = 0;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_GRAYTEXT;
	wc.lpszMenuName = 0;
	wc.lpszClassName = WINDOW_CLASS_NAME;

	if (!RegisterClass(&wc))
		ri.Sys_Error(ERR_FATAL, "Couldn't register window class");

	if (fullscreen)
	{
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP | WS_VISIBLE;
	}
	else
	{
		exstyle = 0;
		stylebits = WINDOW_STYLE;
	}

	r.left = 0;
	r.top = 0;
	r.right = width;
	r.bottom = height;

	AdjustWindowRect(&r, stylebits, FALSE);

	w = r.right - r.left;
	h = r.bottom - r.top;

	if (fullscreen)
	{
		x = 0;
		y = 0;
	}
	else
	{
		vid_xpos = ri.Cvar_Get("vid_xpos", "0", 0);
		vid_ypos = ri.Cvar_Get("vid_ypos", "0", 0);
		x = vid_xpos->value;
		y = vid_ypos->value;
	}

	dxw_state.hWnd = CreateWindowEx(
		exstyle,
		WINDOW_CLASS_NAME,
		L"Quake 2",
		stylebits,
		x, y, w, h,
		NULL,
		NULL,
		dxw_state.hInstance,
		NULL);

	if (!dxw_state.hWnd)
		ri.Sys_Error(ERR_FATAL, "Couldn't create window");

	ShowWindow(dxw_state.hWnd, SW_SHOW);
	UpdateWindow(dxw_state.hWnd);

	if (FAILED(InitDevice()))
	{
		ri.Con_Printf(PRINT_ALL, "VID_CreateWindow() - InitDevice failed\n");
		return False;
	}

	SetForegroundWindow(dxw_state.hWnd);
	SetFocus(dxw_state.hWnd);

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow(width, height);

	return True;
}

rserr_t DXimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen)
{
	int width, height;
	const char *win_fs[] = { "W", "FS" };

	if (!ri.Vid_GetModeInfo(&width, &height, mode))
	{
		ri.Con_Printf(PRINT_ALL, " invalid mode\n");
		return rserr_invalid_mode;
	}

	ri.Con_Printf(PRINT_ALL, " %d %d %s\n", width, height, win_fs[fullscreen]);

	if (dxw_state.hWnd)
	{
		DXimp_Shutdown();
	}

	// do a CDS if needed
	if (fullscreen)
	{
		DEVMODE dm;

		ri.Con_Printf(PRINT_ALL, "...attempting fullscreen\n");

		memset(&dm, 0, sizeof(dm));

		dm.dmSize = sizeof(dm);

		dm.dmPelsWidth = width;
		dm.dmPelsHeight = height;
		dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

		if (dx_bitdepth->value != 0)
		{
			dm.dmBitsPerPel = dx_bitdepth->value;
			dm.dmFields |= DM_BITSPERPEL;
			ri.Con_Printf(PRINT_ALL, "...using dx_bitdepth of %d\n", (int)dx_bitdepth->value);
		}
		else
		{
			HDC hdc = GetDC(NULL);
			int bitspixel = GetDeviceCaps(hdc, BITSPIXEL);

			ri.Con_Printf(PRINT_ALL, "...using desktop display depth of %d\n", bitspixel);

			ReleaseDC(0, hdc);
		}

		ri.Con_Printf(PRINT_ALL, "...calling CDS: ");
		if (ChangeDisplaySettings(&dm, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
		{
			*pwidth = width;
			*pheight = height;

			fullscreen = True;

			ri.Con_Printf(PRINT_ALL, "ok\n");

			if (!VID_CreateWindow(width, height, True))
				return rserr_invalid_mode;

			return rserr_ok;
		}
		else
		{
			*pwidth = width;
			*pheight = height;

			ri.Con_Printf(PRINT_ALL, "failed\n");

			ri.Con_Printf(PRINT_ALL, "...calling CDS assuming dual monitors:");

			dm.dmPelsWidth = width * 2;
			dm.dmPelsHeight = height;
			dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

			if (dx_bitdepth->value != 0)
			{
				dm.dmBitsPerPel = dx_bitdepth->value;
				dm.dmFields |= DM_BITSPERPEL;
			}

			/*
			** our first CDS failed, so maybe we're running on some weird dual monitor
			** system
			*/
			if (ChangeDisplaySettings(&dm, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				ri.Con_Printf(PRINT_ALL, " failed\n");

				ri.Con_Printf(PRINT_ALL, "...setting windowed mode\n");

				ChangeDisplaySettings(0, 0);

				*pwidth = width;
				*pheight = height;
				fullscreen = False;
				if (!VID_CreateWindow(width, height, False))
					return rserr_invalid_mode;
				return rserr_invalid_fullscreen;
			}
			else
			{
				ri.Con_Printf(PRINT_ALL, " ok\n");
				if (!VID_CreateWindow(width, height, True))
					return rserr_invalid_mode;

				fullscreen = True;
				return rserr_ok;
			}
		}
	}
	else
	{
		ri.Con_Printf(PRINT_ALL, "...setting windowed mode\n");

		ChangeDisplaySettings(0, 0);

		*pwidth = width;
		*pheight = height;
		fullscreen = False;
		if (!VID_CreateWindow(width, height, False))
			return rserr_invalid_mode;
	}

	return rserr_ok;
}

void DXimp_Shutdown(void)
{
	if (dxw_state.hDC)
	{
		if (!ReleaseDC(dxw_state.hWnd, dxw_state.hDC))
			ri.Con_Printf(PRINT_ALL, "ref_dx::R_Shutdown() - ReleaseDC failed\n");
		dxw_state.hDC = NULL;
	}
	if (dxw_state.hWnd)
	{
		DestroyWindow(dxw_state.hWnd);
		dxw_state.hWnd = NULL;
	}

	if (g_pImmediateContext) g_pImmediateContext->ClearState();

	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain1) g_pSwapChain1->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext1) g_pImmediateContext1->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice1) g_pd3dDevice1->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();

	UnregisterClass(L"Quake 2", dxw_state.hInstance);

	if (fullscreen)
	{
		ChangeDisplaySettings(0, 0);
		fullscreen = False;
	}
}


int DXimp_Init(void *hinstance, void *wndproc)
{
	allow_cds = True;

	dxw_state.allowdisplaydepthchange = True;
	dxw_state.hInstance = (HINSTANCE)hinstance;
	dxw_state.wndproc = wndproc;

	return 1;
}

void DXimp_EndFrame(void) { ; }

void DXimp_AppActivate(qboolean active)
{
	if (active)
	{
		SetForegroundWindow(dxw_state.hWnd);
		ShowWindow(dxw_state.hWnd, SW_RESTORE);
	}
	else
	{
		if (vid_fullscreen->value)
			ShowWindow(dxw_state.hWnd, SW_MINIMIZE);
	}
}

HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(dxw_state.hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);

		if (hr == E_INVALIDARG)
		{
			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			hr = D3D11CreateDevice(NULL, g_driverType, NULL, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
				D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		}

		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;

	// Obtain DXGI factory from device (since we used NULL for pAdapter above)
	IDXGIFactory1* dxgiFactory = NULL;
	{
		IDXGIDevice* dxgiDevice = NULL;
		hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = NULL;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		return hr;

	// Create swap chain
	IDXGIFactory2* dxgiFactory2 = NULL;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	if (dxgiFactory2)
	{
		// DirectX 11.1 or later
		hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_pd3dDevice1));
		if (SUCCEEDED(hr))
		{
			(void)g_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1));
		}

		DXGI_SWAP_CHAIN_DESC1 sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.Width = width;
		sd.Height = height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;

		hr = dxgiFactory2->CreateSwapChainForHwnd(g_pd3dDevice, dxw_state.hWnd, &sd, NULL, NULL, &g_pSwapChain1);
		if (SUCCEEDED(hr))
		{
			hr = g_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain));
		}

		dxgiFactory2->Release();
	}
	else
	{
		// DirectX 11.0 systems
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 1;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = dxw_state.hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;

		hr = dxgiFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);
	}

	// Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
	dxgiFactory->MakeWindowAssociation(dxw_state.hWnd, DXGI_MWA_NO_ALT_ENTER);

	dxgiFactory->Release();

	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = NULL;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	return S_OK;
}
