#ifdef _WIN32
#  include <windows.h>
#endif

#include <stdio.h>
#include <math.h>
#include <d3d11_1.h>

#include "../client/ref.h"
#include "dx_win.h"

#ifndef __VIDDEF_T
#define __VIDDEF_T
typedef struct
{
	unsigned		width, height;			// coordinates from main game
} viddef_t;
#endif

typedef enum
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

typedef struct image_s
{
	char	name[MAX_QPATH];			// game path, including extension
	imagetype_t	type;
	int		width, height;				// source image
	int		upload_width, upload_height;	// after power of two and picmip
	int		registration_sequence;		// 0 = free
	struct msurface_s	*texturechain;	// for sort-by-texture world drawing
	int		texnum;						// dx texture binding
	float	sl, tl, sh, th;				// 0,0 - 1,1 unless part of the scrap
	qboolean	scrap;
	qboolean	has_alpha;

	qboolean paletted;
} image_t;

//===================================================================

//extern refimport_t ri;

void Swap_Init(void);

rserr_t DXimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen);
int DXimp_Init(void *hinstance, void *wndproc);
void DXimp_AppActivate(qboolean active);
void DXimp_Shutdown(void);
void DXimp_EndFrame(void);