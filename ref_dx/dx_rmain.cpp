#include "dx_local.h"

cvar_t	*dx_bitdepth;
cvar_t	*dx_mode;

cvar_t	*vid_fullscreen;
cvar_t	*vid_ref;

qboolean allow_cds;
qboolean fullscreen;

viddef_t	vid;

refimport_t	ri;

qboolean toQboolean(float f)
{
	if (f)
		return True;
	return False;
}

void R_Register(void)
{
	dx_bitdepth = ri.Cvar_Get("dx_bitdepth", "0", 0);
	dx_mode = ri.Cvar_Get("dx_mode", "3", CVAR_ARCHIVE);

	vid_fullscreen = ri.Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_ref = ri.Cvar_Get("vid_ref", "soft", CVAR_ARCHIVE);
}

qboolean R_SetMode(void)
{
	rserr_t err;
	qboolean fullscreen;

	if (vid_fullscreen->modified && !allow_cds)
	{
		ri.Con_Printf(PRINT_ALL, "R_SetMode() - CDS not allowed with this driver\n");
		ri.Cvar_SetValue("vid_fullscreen", !vid_fullscreen->value);
		vid_fullscreen->modified = False;
	}

	fullscreen = toQboolean(vid_fullscreen->value);

	vid_fullscreen->modified = False;
	dx_mode->modified = False;

	if ((err = DXimp_SetMode((int *)&vid.width, (int *)&vid.height, dx_mode->value, fullscreen)) == rserr_ok)
	{
		;
	}
	else
	{
		if (err == rserr_invalid_fullscreen)
		{
			ri.Cvar_SetValue("vid_fullscreen", 0);
			vid_fullscreen->modified = False;
			ri.Con_Printf(PRINT_ALL, "ref_dx::R_SetMode() - fullscreen unavailable in this mode\n");
			if ((err = DXimp_SetMode((int *)&vid.width, (int *)&vid.height, dx_mode->value, False)) == rserr_ok)
				return True;
		}
		else if (err == rserr_invalid_mode)
		{
			ri.Cvar_SetValue("dx_mode", 0);
			dx_mode->modified = False;
			ri.Con_Printf(PRINT_ALL, "ref_dx::R_SetMode() - invalid mode\n");
		}

		// try setting it back to something safe
		if ((err = DXimp_SetMode((int *)&vid.width, (int *)&vid.height, 0, False)) != rserr_ok)
		{
			ri.Con_Printf(PRINT_ALL, "ref_dx::R_SetMode() - could not revert to safe mode\n");
			return False;
		}
	}
	return True;
}

qboolean R_Init(void *hinstance, void *hWnd)
{
	int		err;

	R_Register();

	if (!DXimp_Init(hinstance, hWnd))
	{
		return False;
	}

	// create the window and set up the context
	if (!R_SetMode())
	{
		ri.Con_Printf(PRINT_ALL, "ref_dx::R_Init() - could not R_SetMode()\n");
		return False;
	}

	ri.Vid_MenuInit();

	return True;
}

void R_Shutdown(void)
{
	DXimp_Shutdown();
}

void R_BeginFrame(float camera_separation)
{
		if (dx_mode->modified || vid_fullscreen->modified)
	{	// FIXME: only restart if CDS is required
		cvar_t	*ref;

		ref = ri.Cvar_Get("vid_ref", "dx", 0);
		ref->modified = True;
	}

		if (dx_bitdepth->modified)
		{
			if (dx_bitdepth->value != 0 && !dxw_state.allowdisplaydepthchange)
			{
				ri.Cvar_SetValue("dx_bitdepth", 0);
				ri.Con_Printf(PRINT_ALL, "dx_bitdepth requires Win95 OSR2.x or WinNT 4.x\n");
			}
			dx_bitdepth->modified = False;
		}
}

void R_RenderFrame(refdef_t *fd)
{
	float ClearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);

	g_pSwapChain->Present(0, 0);
}

void	R_SetPalette(const unsigned char *palette) { ; }

void	R_BeginRegistration(char *map) { ; }
struct	model_s	*R_RegisterModel(char *name) { return NULL; }
struct	image_s	*R_RegisterSkin(char *name) { return NULL; }
void	R_SetSky(char *name, float rotate, vec3_t axis) { ; }
void	R_EndRegistration(void) { ; }

struct	image_s	*Draw_FindPic(char *name) { return NULL; }

void	Draw_Pic(int x, int y, char *pic) { ; }
void	Draw_Char(int x, int y, int c) { ; }
void	Draw_TileClear(int x, int y, int w, int h, char *pic) { ; }
void	Draw_Fill(int x, int y, int w, int h, int c) { ; };
void	Draw_FadeScreen(void) { ; };
void	Draw_GetPicSize(int *w, int *h, char *pic) { ; }
void	Draw_StretchPic(int x, int y, int w, int h, char *pic) { ; }

void	Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data) { ; }


/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
refexport_t GetRefAPI(refimport_t rimp)
{
	refexport_t	re;

	ri = rimp;

	re.api_version = API_VERSION;

	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_RegisterModel;
	re.RegisterSkin = R_RegisterSkin;
	re.RegisterPic = Draw_FindPic;
	re.SetSky = R_SetSky;
	re.EndRegistration = R_EndRegistration;

	re.RenderFrame = R_RenderFrame;

	re.DrawGetPicSize = Draw_GetPicSize;
	re.DrawPic = Draw_Pic;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawChar = Draw_Char;
	re.DrawTileClear = Draw_TileClear;
	re.DrawFill = Draw_Fill;
	re.DrawFadeScreen = Draw_FadeScreen;

	re.DrawStretchRaw = Draw_StretchRaw;

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;

	re.CinematicSetPalette = R_SetPalette;
	re.BeginFrame = R_BeginFrame;
	re.EndFrame = DXimp_EndFrame;

	re.AppActivate = DXimp_AppActivate;

	Swap_Init();

	return re;
}