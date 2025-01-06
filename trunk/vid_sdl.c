/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// vid_sdl.c -- SDL 2 driver
//
// Most of this module and in_sdl.c was taken from ironwail, so credit goes to
// the developers of ironwail, QuakeSpasm, and other ancestor ports for much of
// the code here.

#include <SDL.h>

#include "quakedef.h"
#include "bgmusic.h"
#ifdef _WIN32
#include "winquake.h"
#endif


#define MAX_MODE_LIST 600
#define MAX_MENU_MODES  45
#define MAX_REFRESH_RATES   8
#define	WARP_WIDTH	320
#define	WARP_HEIGHT	200
#define WINDOW_TITLE_STRING "JoeQuake"
#define DEFAULT_REFRESHRATE	60


typedef struct
{
	int			width;
	int			height;
	int			bpp;
	int			refreshrate;
} sdlmode_t;

typedef struct
{
    int         width;
    int         height;
    int         refreshrates[MAX_REFRESH_RATES];
    int         numrefreshrates;
} menumode_t;


static SDL_Window* draw_context = NULL;
static SDL_GLContext* gl_context = NULL;
static sdlmode_t	modelist[MAX_MODE_LIST];  // Modes for showing in `vid_describemodes`
static int		nummodes;
static menumode_t menumodelist[MAX_MENU_MODES];
static int		nummenumodes;
static int		displayindex;

static qboolean	vid_initialized = false;
static cvar_t	vid_width = {"vid_width", "", CVAR_ARCHIVE};
static cvar_t	vid_height = {"vid_height", "", CVAR_ARCHIVE};
static cvar_t	vid_refreshrate = {"vid_refreshrate", "", CVAR_ARCHIVE};
static cvar_t	vid_fullscreen = {"vid_fullscreen", "", CVAR_ARCHIVE};
static cvar_t	vid_desktopfullscreen = {"vid_desktopfullscreen", "0", CVAR_ARCHIVE};
static cvar_t	vid_ignoreerrors = {"vid_ignoreerrors", "0", CVAR_ARCHIVE};
static qboolean OnChange_windowed_mouse(struct cvar_s *var, char *value);
cvar_t	_windowed_mouse = {"_windowed_mouse", "1", 0, OnChange_windowed_mouse};

// Stubs that are used externally.
qboolean vid_hwgamma_enabled = false;
qboolean fullsbardraw = false;
cvar_t vid_mode;
qboolean gl_have_stencil = false;
#ifdef _WIN32
modestate_t   modestate;
qboolean    DDActive = false;
#endif
qboolean	scr_skipupdate;

// Sphere --- TODO: Only copied these from in_win.c for now to get a first
// compiling Linux version. Has to be implemented properly to support these
// features.
int GetCurrentBpp(void)
{
	return 32;
}
int GetCurrentFreq(void)
{
	return 60;
}
int menu_bpp, menu_display_freq;
cvar_t vid_vsync;
float menu_vsync;

//========================================================
// Video menu stuff
//========================================================

typedef enum
{
	VID_MENU_ROW_FULLSCREEN,
	VID_MENU_ROW_REFRESH_RATE,
	VID_MENU_ROW_APPLY,
	VID_MENU_ROW_SPACE1,
	VID_MENU_ROW_RESOLUTION_SCALE,
	VID_MENU_NUM_ITEMS,
} vid_menu_rows_t;


#define VID_ROW_SIZE		3  // number of columns for modes
#define VID_MENU_SPACING	2  // rows between video options and modes

#define VID_MENU_IS_SPACE(row)	( \
	(row) == VID_MENU_ROW_SPACE1 \
	|| ((row) >= VID_MENU_NUM_ITEMS && ((row) < VID_MENU_NUM_ITEMS + VID_MENU_SPACING)) \
)

static int	video_cursor_row = 0;
static int	video_cursor_column = 0;
static int video_mode_rows = 0;

static menu_window_t video_window;
static menu_window_t video_slider_resscale_window;
static void VID_MenuDraw (void);
static void VID_MenuKey (int key);

//===========================================

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

void VID_LockBuffer (void)
{
}

void VID_UnlockBuffer (void)
{
}

void VID_ShiftPalette (unsigned char *p)
{
}

void VID_SetDeviceGammaRamp (unsigned short *ramps)
{
}

/*
=================
GL_BeginRendering
=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = vid.width;
	*height = vid.height;
}

/*
=================
GL_EndRendering
=================
*/
void GL_EndRendering (void)
{
	GLenum glerr;
	const char *sdlerr;

	SDL_GL_SwapWindow(draw_context);

	if (vid_ignoreerrors.value == 0.0f)
	{
		while ((glerr = glGetError()))
			Con_Printf("OpenGL error, please report: %d\n", glerr);

		sdlerr = SDL_GetError();
		if (sdlerr[0])
		{
			Con_Printf("SDL error, please report: %s\n", sdlerr);
			SDL_ClearError();
		}
	}

#ifdef _WIN32
	{
		Uint32 flags = SDL_GetWindowFlags(draw_context);
		Minimized = (flags & SDL_WINDOW_SHOWN) == 0;
		ActiveApp = (flags & (SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_INPUT_FOCUS)) != 0;
	}
#endif
}

void VID_SetDefaultMode(void)
{
	// Do we need to do anything here?
}

void VID_Shutdown (void)
{
	if (vid_initialized)
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static SDL_DisplayMode *VID_SDL2_GetDisplayMode(int width, int height, int refreshrate)
{
	static SDL_DisplayMode mode;
	const int sdlmodes = SDL_GetNumDisplayModes(displayindex);
	int i;

	for (i = 0; i < sdlmodes; i++)
	{
		if (SDL_GetDisplayMode(displayindex, i, &mode) < 0)
		{
			SDL_ClearError();
			continue;
		}

		if (mode.w == width && mode.h == height
			&& SDL_BITSPERPIXEL(mode.format) >= 24
			&& mode.refresh_rate == refreshrate)
		{
			return &mode;
		}
	}
	return NULL;
}

static int VID_GetCurrentWidth (void)
{
	int w = 0, h = 0;
	SDL_GetWindowSize(draw_context, &w, &h);
	return w;
}

static int VID_GetCurrentHeight (void)
{
	int w = 0, h = 0;
	SDL_GetWindowSize(draw_context, &w, &h);
	return h;
}

static int VID_GetCurrentBPP (void)
{
	const Uint32 pixelFormat = SDL_GetWindowPixelFormat(draw_context);
	return SDL_BITSPERPIXEL(pixelFormat);
}

static int VID_GetCurrentRefreshRate (void)
{
	SDL_DisplayMode mode;

	if (0 != SDL_GetCurrentDisplayMode(displayindex, &mode))
		return DEFAULT_REFRESHRATE;

	return mode.refresh_rate;
}

static qboolean VID_GetFullscreen (void)
{
	return (SDL_GetWindowFlags(draw_context) & SDL_WINDOW_FULLSCREEN) != 0;
}

static void ClearAllStates (void)
{
	int	i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
		Key_Event (i, false);

	Key_ClearStates ();
	IN_ClearStates ();
}

static qboolean VID_ValidMode (int width, int height, int refreshrate, qboolean fullscreen)
{
// ignore width / height if vid_desktopfullscreen is enabled
	if (fullscreen && vid_desktopfullscreen.value)
		return true;

	if (width < 320)
		return false;

	if (height < 200)
		return false;

	if (fullscreen && VID_SDL2_GetDisplayMode(width, height, refreshrate) == NULL)
		return false;

	return true;
}

extern void GL_Init (void);
extern GLuint r_gamma_texture;
static void SetMode (int width, int height, int refreshrate, qboolean fullscreen)
{
	int		temp;
	Uint32	flags;
	char		caption[50];
	int		depthbits;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();
	BGM_Pause ();
	// TODO: Stop non-CD audio

	/* z-buffer depth */
	depthbits = 24;
	if (SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depthbits) < 0)
		Sys_Error ("Couldn't set GL depth attribute: %s", SDL_GetError());

	snprintf(caption, sizeof(caption), "%s", WINDOW_TITLE_STRING);

	/* Create the window if needed, hidden */
	if (!draw_context)
	{
		flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;

		if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2) < 0)
		Sys_Error ("Couldn't set GL major version attribute: %s", SDL_GetError());
		if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1) < 0)
		Sys_Error ("Couldn't set GL minor version attribute: %s", SDL_GetError());
		draw_context = SDL_CreateWindow (caption, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
		if (!draw_context)
			Sys_Error ("Couldn't create window: %s", SDL_GetError());

		SDL_SetWindowMinimumSize (draw_context, 320, 240);
	}

	/* Ensure the window is not fullscreen */
	if (VID_GetFullscreen ())
	{
		if (SDL_SetWindowFullscreen (draw_context, 0) < 0)
			Sys_Error("Couldn't set fullscreen state mode: %s", SDL_GetError());
	}

	/* Set window size and display mode */
	SDL_SetWindowSize (draw_context, width, height);
	SDL_SetWindowPosition (draw_context, SDL_WINDOWPOS_CENTERED_DISPLAY(displayindex), SDL_WINDOWPOS_CENTERED_DISPLAY(displayindex));
	if (SDL_SetWindowDisplayMode (draw_context, VID_SDL2_GetDisplayMode(width, height, refreshrate)) < 0)
		Sys_Error ("Couldn't set window display mode: %s", SDL_GetError());
	SDL_SetWindowBordered (draw_context, SDL_TRUE);

	/* Make window fullscreen if needed, and show the window */

	if (fullscreen) {
		const Uint32 flag = vid_desktopfullscreen.value ?
				SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
		if (SDL_SetWindowFullscreen (draw_context, flag) < 0)
			Sys_Error ("Couldn't set fullscreen state mode: %s", SDL_GetError());
	}

	SDL_ShowWindow (draw_context);
	SDL_RaiseWindow (draw_context);

	/* Create GL context if needed */
	if (!gl_context) {
		gl_context = SDL_GL_CreateContext(draw_context);
		if (!gl_context)
		{
			SDL_GL_ResetAttributes();
			Sys_Error("Couldn't create GL context: %s", SDL_GetError());
		}
		GL_Init ();
		GL_SetupState ();
	}

	vid.width = vid.conwidth = VID_GetCurrentWidth();
	vid.height = vid.conheight = VID_GetCurrentHeight();
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	vid.numpages = 2;
	vid.recalc_refdef = 1;
	r_gamma_texture = 0;
	Draw_AdjustConback();

// read the obtained z-buffer depth
	if (SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depthbits) < 0)
	{
		depthbits = 0;
		SDL_ClearError();
	}

	if (SDL_SetRelativeMouseMode(VID_GetFullscreen() || _windowed_mouse.value != 0.0f) < 0)
		Sys_Error("Couldn't set mouse mode: %s", SDL_GetError());

	CDAudio_Resume ();
	BGM_Resume ();
	// TODO: Start non-CD audio
	scr_disabled_for_loading = temp;

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	Con_Printf ("Video mode: %dx%dx%d Z%d %dHz\n",
				VID_GetCurrentWidth(),
				VID_GetCurrentHeight(),
				VID_GetCurrentBPP(),
				depthbits,
				VID_GetCurrentRefreshRate());
}

static void VID_DescribeModes_f (void)
{
	int	i;
	int	lastwidth, lastheight, lastbpp, count;

	lastwidth = lastheight = lastbpp = count = 0;

	Con_Printf("List of fullscreen modes:\n");
	for (i = 0; i < nummodes; i++)
	{
		if (lastwidth != modelist[i].width || lastheight != modelist[i].height || lastbpp != modelist[i].bpp)
		{
			if (count > 0)
				Con_Printf ("\n");
			Con_Printf ("%4i x %4i x %i bpp : %i Hz", modelist[i].width, modelist[i].height, modelist[i].bpp, modelist[i].refreshrate);
			lastwidth = modelist[i].width;
			lastheight = modelist[i].height;
			lastbpp = modelist[i].bpp;
			count++;
		}
	}
	Con_Printf ("\n%i modes.\n", count);
}

static void VID_TestMode_f (void)
{
	Con_Printf("vid_testmode not supported, use vid_forcemode instead.\n");
}

static void VID_ForceMode_f (void)
{
	int		i;
	int		width, height, refreshrate;
	qboolean	fullscreen;

	// If this is the `vid_forcemode` that is made after config.cfg is executed
	// then read overrides from command line.
	if (Cmd_Argc() == 2 && strcmp(Cmd_Argv(1), "post-config") == 0)
	{
		if (COM_CheckParm("-fullscreen"))
			Cvar_SetValue(&vid_fullscreen, 1);

		if (COM_CheckParm("-window") || COM_CheckParm("-startwindowed"))
			Cvar_SetValue(&vid_fullscreen, 0);

		if ((i = COM_CheckParm("-width")) && i + 1 < com_argc)
			Cvar_SetValue(&vid_width, Q_atoi(com_argv[i+1]));

		if ((i = COM_CheckParm("-height")) && i + 1 < com_argc)
			Cvar_SetValue(&vid_height, Q_atoi(com_argv[i+1]));

		if ((i = COM_CheckParm("-refreshrate")) && i + 1 < com_argc)
			Cvar_SetValue(&vid_refreshrate, Q_atoi(com_argv[i+1]));
	}

	// Decide what mode to set based on cvars, using defaults if not set.
	if (vid_width.string[0] == '\0')
		Cvar_SetValue(&vid_width, 800);
	width = (int)vid_width.value;

	if (vid_height.string[0] == '\0')
		Cvar_SetValue(&vid_height, 600);
	height = (int)vid_height.value;

	if (vid_refreshrate.string[0] == '\0')
		Cvar_SetValue(&vid_refreshrate, VID_GetCurrentRefreshRate());
	refreshrate = (int)vid_refreshrate.value;

	if (vid_fullscreen.string[0] == '\0')
		Cvar_SetValue(&vid_fullscreen, 0);
	fullscreen = (int)vid_fullscreen.value;

	// Check the mode set above is valid.
	if (!VID_ValidMode(width, height, refreshrate, fullscreen))
	{
		Con_Printf("Invalid mode %dx%d %d Hz %s\n",
					width, height, refreshrate,
					fullscreen ? "fullscreen" : "windowed");
	} else {
		SetMode(width, height, refreshrate, fullscreen);
	}
}

static int CmpModes(sdlmode_t *m1, sdlmode_t *m2)
{
	// Sort lexicographically by (width, height, refreshrate, bpp)
	if (m1->width < m2->width)
		return -1;
	if (m1->width > m2->width)
		return 1;
	if (m1->height < m2->height)
		return -1;
	if (m1->height > m2->height)
		return 1;
	if (m1->refreshrate < m2->refreshrate)
		return -1;
	if (m1->refreshrate > m2->refreshrate)
		return 1;
	if (m1->bpp < m2->bpp)
		return -1;
	if (m1->bpp > m2->bpp)
		return 1;
	return 0;
}

static void SwapModes(sdlmode_t *m1, sdlmode_t *m2)
{
	static sdlmode_t temp_mode;

	temp_mode = *m1;
	*m1 = *m2;
	*m2 = temp_mode;
}

static void VID_InitModelist (void)
{
	const int sdlmodes = SDL_GetNumDisplayModes(displayindex);
	int i, j;
	sdlmode_t *m1, *m2, *mode;
	menumode_t *menumode;

	// Fetch modes from SDL which have a valid pixel format.
	nummodes = 0;
	for (i = 0; i < sdlmodes; i++)
	{
		SDL_DisplayMode mode;

		if (nummodes >= MAX_MODE_LIST)
			break;
		if (SDL_GetDisplayMode(displayindex, i, &mode) == 0 && SDL_BITSPERPIXEL (mode.format) >= 24)
		{
			modelist[nummodes].width = mode.w;
			modelist[nummodes].height = mode.h;
			modelist[nummodes].bpp = SDL_BITSPERPIXEL(mode.format);
			modelist[nummodes].refreshrate = mode.refresh_rate;
			nummodes++;
		}
	}

	if (nummodes == 0)
		Sys_Error("No valid modes");

	// Sort this array, putting the larger resolutions first.
	for (i = 0; i < nummodes - 1; i++)
	{
		for (j = i + 1; j < nummodes; j++)
		{
			m1 = &modelist[i];
			m2 = &modelist[j];
			if (CmpModes(m1, m2) < 0)
				SwapModes(m1, m2);
		}
	}

	// Build the menu mode list.
	nummenumodes = 0;
	menumode = NULL;		// invariant:  menumode == menumodelist[nummenumodes - 1]
	for (i = 0; i < nummodes && nummenumodes < MAX_MENU_MODES; i++)
	{
		mode = &modelist[i];

		if (menumode == NULL || menumode->width != mode->width || menumode->height != mode->height)
		{
			// This is a resolution we haven't seen, so add a new menu mode.
			menumode = &menumodelist[nummenumodes++];

			menumode->width = mode->width;
			menumode->height = mode->height;
			menumode->refreshrates[0] = mode->refreshrate;
			menumode->numrefreshrates = 1;
		}
		else if (menumode->refreshrates[menumode->numrefreshrates - 1] != mode->refreshrate
					&& menumode->numrefreshrates < MAX_REFRESH_RATES)
		{
			// This is a refresh rate we haven't seen, add to the current menu mode.
			menumode->refreshrates[menumode->numrefreshrates++] = mode->refreshrate;
		}
	}
}

static qboolean OnChange_windowed_mouse (struct cvar_s *var, char *value)
{
	if (SDL_SetRelativeMouseMode(VID_GetFullscreen() || Q_atof (value) != 0.0f) < 0)
		Sys_Error("Couldn't set mouse mode: %s", SDL_GetError());
	return false;
}

void VID_Init (unsigned char *palette)
{
	int i;
	int numdisplays;

	Cvar_Register (&vid_width);
	Cvar_Register (&vid_height);
	Cvar_Register (&vid_refreshrate);
	Cvar_Register (&vid_fullscreen);
	Cvar_Register (&vid_desktopfullscreen);
	Cvar_Register (&vid_ignoreerrors);
	Cvar_Register (&_windowed_mouse);

	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);
	Cmd_AddCommand ("vid_forcemode", VID_ForceMode_f);
	Cmd_AddCommand ("vid_testmode", VID_TestMode_f);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		Sys_Error("Couldn't init SDL video: %s", SDL_GetError());

	numdisplays = SDL_GetNumVideoDisplays();
	if (numdisplays < 0)
		Sys_Error("Couldn't get number of displays: %s", SDL_GetError());

	if ((i = COM_CheckParm("-display")) && i + 1 < com_argc)
		displayindex = Q_atoi(com_argv[i+1]);
	else
		displayindex = 0;
	if (displayindex >= numdisplays || displayindex < 0)
		Sys_Error("Invalid display index: %d, there are %d displays", displayindex, numdisplays);

	VID_InitModelist();

	// Set a mode for maximum compatibility.  The real mode will be set once
	// cvars have been set, via an automatic call to `vid_forcemode`.  This
	// initial mode will only be visible fleetingly, as long as there's no
	// problem setting the configured mode.
	SetMode(800, 600, VID_GetCurrentRefreshRate(), false);

	if (SDL_GL_SetSwapInterval (0) < 0)
		Sys_Error("Couldn't disable vsync: %s", SDL_GetError());

	VID_SetPalette (palette);

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	vid_initialized = true;
}

static menumode_t *GetMenuMode(void)
{
	menumode_t *menumode;
	int i, width, height;

	width = (int)vid_width.value;
	height = (int)vid_height.value;

	for (i = 0; i < nummenumodes; i++)
	{
		menumode = &menumodelist[i];
		if (width == menumode->width && height == menumode->height)
			return menumode;
	}

	return NULL;
}

static void MenuSelectPrevRefreshRate (void)
{
	int i;
	menumode_t *menumode;
	int refreshrate;

	menumode = GetMenuMode();
	if (menumode == NULL)
		return;  // Current video mode not in SDL list (eg. windowed mode)

	refreshrate = (int)vid_refreshrate.value;

	// refresh rates are in descending order
	for (i = 0; i < menumode->numrefreshrates; i ++)
	{
		if (menumode->refreshrates[i] < refreshrate)
			break;
	}
	if (i == menumode->numrefreshrates)
		i = 0;		// wrapped around

	Cvar_SetValue(&vid_refreshrate, menumode->refreshrates[i]);
}

static void MenuSelectNextRefreshRate (void)
{
	int i;
	menumode_t *menumode;
	int refreshrate;

	menumode = GetMenuMode();
	if (menumode == NULL)
		return;  // Current video mode not in SDL list (eg. windowed mode)

	refreshrate = (int)vid_refreshrate.value;

	// refresh rates are in descending order
	for (i = 0; i < menumode->numrefreshrates; i ++)
	{
		if (menumode->refreshrates[i] <= refreshrate)
			break;
	}
	if (i == 0)
		i = menumode->numrefreshrates;
	i--;

	Cvar_SetValue(&vid_refreshrate, menumode->refreshrates[i]);
}

static void MenuSelectNearestRefreshRate (void)
{
	int i;
	menumode_t *menumode;
	int refreshrate;

	menumode = GetMenuMode();
	if (menumode == NULL)
		return;  // Current video mode not in SDL list (eg. windowed mode)

	refreshrate = (int)vid_refreshrate.value;

	// refresh rates are in descending order
	for (i = 0; i < menumode->numrefreshrates; i ++)
	{
		if (menumode->refreshrates[i] <= refreshrate)
			break;
	}

	if (i == menumode->numrefreshrates)
		i = menumode->numrefreshrates - 1;	// smaller than all, pick smallest
	else if (i > 0
			 && abs(menumode->refreshrates[i - 1] - refreshrate)
				< abs(menumode->refreshrates[i] - refreshrate))
		i--;  // either larger than all, or prev value is closer.

	Cvar_SetValue(&vid_refreshrate, menumode->refreshrates[i]);
}

static void VID_MenuDraw (void)
{
	int i, row, x, y, lx, ly;
	menumode_t *menumode;
	char mode_desc[14], refresh_rate_desc[8];
	qboolean red;
	mpic_t		*p;

	// title graphic
	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ((320-p->width)/2, 4, p);

	// general settings
	row = VID_MENU_ROW_FULLSCREEN; y = 32 + 8 * row;
	M_Print_GetPoint(16, y, &video_window.x, &video_window.y, "        Fullscreen", video_cursor_row == row);
	video_window.x -= 16;	// adjust it slightly to the left due to the larger, 3 columns vid modes list
	M_DrawCheckbox(188, y, vid_fullscreen.value != 0);

	row = VID_MENU_ROW_REFRESH_RATE; y = 32 + 8 * row;
	M_Print_GetPoint(16, y, &lx, &ly, "      Refresh rate", video_cursor_row == row);
	snprintf(refresh_rate_desc, sizeof(refresh_rate_desc), "%i Hz", (int)vid_refreshrate.value);
	M_Print(188, y, refresh_rate_desc);

	row = VID_MENU_ROW_RESOLUTION_SCALE; y = 32 + 8 * row;
	M_Print_GetPoint(16, y, &lx, &ly, "  Resolution scale", video_cursor_row == row);
	M_DrawSliderFloat2(188, y, (r_scale.value - 0.25) / 0.75, r_scale.value, &video_slider_resscale_window);

	row = VID_MENU_ROW_APPLY; y = 32 + 8 * row;
	M_Print_GetPoint(16, y, &lx, &ly, "     Apply changes", video_cursor_row == row);

	// resolutions
	x = 0;
	y = 32 + 8 * (VID_MENU_NUM_ITEMS + VID_MENU_SPACING);
	video_mode_rows = 0;
	for (i = 0 ; i < nummenumodes ; i++)
	{
		menumode = &menumodelist[i];
		red = (menumode->width == (int)vid_width.value && menumode->height == (int)vid_height.value);
		snprintf(mode_desc, sizeof(mode_desc), "%dx%d", menumode->width, menumode->height);
		M_Print_GetPoint(x, y, &lx, &ly, mode_desc, red);

		x += 14 * 8;

		// if we are at the end of the curent row (last column), prepare for next row
		if (((i + 1) % VID_ROW_SIZE) == 0)
		{
			x = 0;
			y += 8;
		}

		// if we just started a new row, increment row counter
		if (((i + 1) % VID_ROW_SIZE) == 1)
		{
			video_mode_rows++;
		}
	}

	video_window.w = (24 + 17) * 8; // presume 8 pixels for each letter
	video_window.h = ly - video_window.y + 8;

	// help text
	switch (video_cursor_row)
	{
		case VID_MENU_ROW_FULLSCREEN:
			break;
		case VID_MENU_ROW_REFRESH_RATE:
			M_Print(8 * 8, y + 16, "  Choose a refresh rate");
			M_Print(8 * 8, y + 24, "after selecting resolution");
			break;
		case VID_MENU_ROW_RESOLUTION_SCALE:
			break;
		case VID_MENU_ROW_APPLY:
			M_Print(8 * 8, y + 16, "Apply selected settings");
		default:
			if (video_cursor_row >= VID_MENU_NUM_ITEMS + VID_MENU_SPACING)
			{
				M_Print(10 * 8, y + 16, "Select a resolution");
				M_Print(10 * 8, y + 24, "then apply changes");
			}
			break;
	}

	// cursor
	if (video_cursor_row < VID_MENU_NUM_ITEMS && !VID_MENU_IS_SPACE(video_cursor_row))
		M_DrawCharacter(168, 32 + video_cursor_row * 8, 12 + ((int)(realtime * 4) & 1));
	else if (video_cursor_row >= VID_MENU_NUM_ITEMS + VID_MENU_SPACING
			&& (video_cursor_row - VID_MENU_NUM_ITEMS - VID_MENU_SPACING) * VID_ROW_SIZE + video_cursor_column < nummenumodes) // we are in the resolutions region
		M_DrawCharacter(-8 + video_cursor_column * 14 * 8, 32 + video_cursor_row * 8, 12 + ((int)(realtime * 4) & 1));
}

void M_Video_KeyboardSlider(int dir)
{
	S_LocalSound("misc/menu3.wav");

	switch (video_cursor_row)
	{
	case VID_MENU_ROW_RESOLUTION_SCALE:
		r_scale.value += dir * 0.05;
		r_scale.value = bound(0.25, r_scale.value, 1);
		Cvar_SetValue(&r_scale, r_scale.value);
		break;

	default:
		break;
	}
}

static void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound("misc/menu1.wav");
		video_cursor_row--;
		while (VID_MENU_IS_SPACE(video_cursor_row))
			video_cursor_row--;
		if (video_cursor_row < 0)
		{
			video_cursor_row = (VID_MENU_NUM_ITEMS + video_mode_rows + VID_MENU_SPACING) - 1;
			// if we cycle from the top to the bottom row, check if we have an item in the appropriate column
			if (nummenumodes % VID_ROW_SIZE == 1 || (nummenumodes % VID_ROW_SIZE == 2 && video_cursor_column == 2))
				video_cursor_column = 0;
		}
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound("misc/menu1.wav");
		video_cursor_row++;
		while (VID_MENU_IS_SPACE(video_cursor_row))
			video_cursor_row++;

		if (video_cursor_row >= (VID_MENU_NUM_ITEMS + video_mode_rows + VID_MENU_SPACING))
			video_cursor_row = 0;
		else if (video_cursor_row == ((VID_MENU_NUM_ITEMS + video_mode_rows + VID_MENU_SPACING) - 1)) // if we step down to the last row, check if we have an item below in the appropriate column
		{
			if (nummenumodes % VID_ROW_SIZE == 1 || (nummenumodes % VID_ROW_SIZE == 2 && video_cursor_column == 2))
				video_cursor_column = 0;
		}
		break;
	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound("misc/menu2.wav");
		switch (video_cursor_row)
		{
			case VID_MENU_ROW_FULLSCREEN:
				Cvar_SetValue(&vid_fullscreen, vid_fullscreen.value == 0);
				break;
            case VID_MENU_ROW_REFRESH_RATE:
				MenuSelectNextRefreshRate();
                break;
			case VID_MENU_ROW_APPLY:
				Cbuf_AddText ("vid_forcemode\n");
				break;
            default:
                if (video_cursor_row >= VID_MENU_NUM_ITEMS)
                {
					int i = (video_cursor_row - VID_MENU_NUM_ITEMS - VID_MENU_SPACING)
										* VID_ROW_SIZE + video_cursor_column;
					if (i < nummenumodes)
					{
						Cvar_SetValue(&vid_width, menumodelist[i].width);
						Cvar_SetValue(&vid_height, menumodelist[i].height);
						MenuSelectNearestRefreshRate();
					}
                }
                break;
		}
		break;

	case K_LEFTARROW:
		S_LocalSound("misc/menu1.wav");
		if (video_cursor_row == VID_MENU_ROW_REFRESH_RATE)
		{
			MenuSelectPrevRefreshRate();
		}
		else if (video_cursor_row == VID_MENU_ROW_RESOLUTION_SCALE)
		{
			M_Video_KeyboardSlider(-1);
		}
		else if (video_cursor_row >= VID_MENU_NUM_ITEMS)
		{ 
			video_cursor_column--;
			if (video_cursor_column < 0)
			{
				if (video_cursor_row >= ((VID_MENU_NUM_ITEMS + video_mode_rows + VID_MENU_SPACING) - 1)) // if we stand on the last row, check how many items we have
				{
					if (nummenumodes % VID_ROW_SIZE == 1)
						video_cursor_column = 0;
					else if (nummenumodes % VID_ROW_SIZE == 2)
						video_cursor_column = 1;
					else
						video_cursor_column = 2;
				}
				else
				{
					video_cursor_column = VID_ROW_SIZE - 1;
				}
			}
		}
		break;

	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav");
		if (video_cursor_row == VID_MENU_ROW_REFRESH_RATE) {
			MenuSelectNextRefreshRate();
		}
		else if (video_cursor_row == VID_MENU_ROW_RESOLUTION_SCALE)
		{
			M_Video_KeyboardSlider(1);
		}
		else if (video_cursor_row >= VID_MENU_NUM_ITEMS)
		{
			video_cursor_column++;
			if (video_cursor_column >= VID_ROW_SIZE || ((video_cursor_row - VID_MENU_NUM_ITEMS - VID_MENU_SPACING) * VID_ROW_SIZE + (video_cursor_column + 1)) > nummenumodes)
				video_cursor_column = 0;
		}
		break;
	}
}

extern qboolean M_Mouse_Select_RowColumn(const menu_window_t *uw, const mouse_state_t *m, int row_entries, int *newentry_row, int col_entries, int *newentry_col);
extern qboolean M_Mouse_Select_Column(const menu_window_t *uw, const mouse_state_t *m, int entries, int *newentry);

void M_Video_MouseSlider(int k, const mouse_state_t *ms)
{
	int slider_pos;

	switch (k)
	{
	case K_MOUSE2:
		break;

	case K_MOUSE1:
		switch (video_cursor_row)
		{
		case VID_MENU_ROW_RESOLUTION_SCALE:
			M_Mouse_Select_Column(&video_slider_resscale_window, ms, 16, &slider_pos);
			r_scale.value = bound(0.25, 0.25 + (slider_pos * 0.05), 1);
			Cvar_SetValue(&r_scale, r_scale.value);
			break;

		default:
			break;
		}
		return;
	}
}

qboolean M_Video_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select_RowColumn(&video_window, ms, VID_MENU_NUM_ITEMS + video_mode_rows + VID_MENU_SPACING, &video_cursor_row, VID_ROW_SIZE, &video_cursor_column);

	if (ms->button_down == 1) VID_MenuKey(K_MOUSE1);
	if (ms->button_down == 2) VID_MenuKey(K_MOUSE2);

	if (ms->buttons[1]) M_Video_MouseSlider(K_MOUSE1, ms);

	return true;
}
