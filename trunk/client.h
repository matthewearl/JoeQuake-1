/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// client.h

#ifdef _WIN32
#include <windows.h>
#endif

typedef struct
{
	vec3_t	viewangles;

// intended velocities
	float	forwardmove;
	float	sidemove;
	float	upmove;
} usercmd_t;

typedef struct
{
	int	length;
	char	map[MAX_STYLESTRING];
} lightstyle_t;

typedef struct
{
	char	name[MAX_SCOREBOARDNAME];
	float	entertime;
	int	frags;
	int	colors;			// two 4 bit fields
	int	ping;
	int	addr;
	byte	translations[VID_GRADES*256];
} scoreboard_t;

typedef struct
{
	int	destcolor[3];
	double	percent;		// 0-256
} cshift_t;

#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define	NUM_CSHIFTS	4


// client_state_t should hold all pieces of the client state

#define	SIGNONS		4		// signon messages to receive before connected

#define	MAX_DLIGHTS	64		// joe: doubled

typedef enum
{
	lt_default, lt_muzzleflash, lt_explosion, lt_rocket,
	lt_red, lt_blue, lt_redblue, lt_green, NUM_DLIGHTTYPES,
	lt_explosion2, lt_explosion3
} dlighttype_t;

extern	float	ExploColor[3];		// joe: for color mapped explosions

typedef struct
{
	int		key;		// so entities can reuse same entry
	vec3_t		origin;
	float		radius;
	float		die;		// stop lighting after this time
	float		decay;		// drop this each second
	float		minlight;	// don't add when contributing less
#ifdef GLQUAKE
	int		type;		// color
#endif
} dlight_t;

#define	MAX_BEAMS	32
typedef struct
{
	int		entity;
	struct model_s	*model;
	float		endtime;
	vec3_t		start, end;
} beam_t;

typedef struct framepos_s
{
	long		baz;
	struct framepos_s *next;
} framepos_t;

extern	framepos_t	*dem_framepos;

#define	MAX_EFRAGS		4096	// joe: was 640

#define	MAX_MAPSTRING	2048
#define	MAX_DEMOS		32
#define	MAX_DEMONAME	64

typedef enum
{
	ca_dedicated, 		// a dedicated server with no ability to start a client
	ca_disconnected, 	// full screen console with no connection
	ca_connected		// valid netcon, talking to a server
} cactive_t;


typedef enum
{
	DEMOCAM_MODE_FIRST_PERSON,
	DEMOCAM_MODE_FREEFLY,
	DEMOCAM_MODE_ORBIT,
	DEMOCAM_MODE_COUNT,
} democam_mode_t;

typedef enum
{
	CL_BBOX_MODE_OFF,
	CL_BBOX_MODE_ON,
	CL_BBOX_MODE_DEMO,
	CL_BBOX_MODE_LIVE,

	NUM_BBOX_MODE
} cl_bbox_mode_t;


// the client_static_t structure is persistant through an arbitrary number
// of server connections
typedef struct
{
	cactive_t	state;

// personalization data sent to server	
	char		mapstring[MAX_QPATH];
	char		spawnparms[MAX_MAPSTRING];	// to restart a level

// demo loop control
	int			demonum;			// -1 = don't play demos
	char		demos[MAX_DEMOS][MAX_DEMONAME];	// when not playing

// demo recording info must be here, because record is started before
// entering a map (and clearing client_state_t)
	qboolean	demorecording;
	qboolean	demoplayback;
	qboolean	timedemo;
	int		forcetrack;			// -1 = use normal cd track
	FILE		*demofile;
	int		td_lastframe;			// to meter out one message a frame
	int		td_startframe;			// host_framecount at start
	float		td_starttime;			// realtime at second frame of timedemo

// connection information
	int		signon;				// 0 to SIGNONS
	struct qsocket_s *netcon;
	sizebuf_t	message;			// writing buffer to send to server
	
	qboolean	capturedemo;
	double		marathon_time;		// joe: adds cl.completed_time at level changes
	int			marathon_level;
} client_static_t;

extern	client_static_t	cls;

// the client_state_t structure is wiped completely at every
// server signon
typedef struct
{
	int		movemessages;		// since connecting to this server
								// throw out the first couple, so the player
								// doesn't accidentally do something the 
								// first frame
	usercmd_t	cmd;			// last command sent to the server

// information for local display
	int		stats[MAX_CL_STATS];	// health, etc
	int		items;			// inventory bit flags
	float		item_gettime[32];	// cl.time of aquiring item, for blinking
	float		faceanimtime;		// use anim frame if cl.time < this

	cshift_t	cshifts[NUM_CSHIFTS];	// color shifts for damage, powerups
	cshift_t	prev_cshifts[NUM_CSHIFTS];	// and content types

// the client maintains its own idea of view angles, which are
// sent to the server each frame. The server sets punchangle when
// the view is temporarliy offset, and an angle reset commands at the start
// of each level and after teleporting.
	vec3_t		mviewangles[2];		// during demo playback viewangles is lerped between these
	vec3_t		viewangles;
	
	vec3_t		mvelocity[2];		// update by server, used for lean+bob (0 is newest)
	vec3_t		velocity;		// lerped between mvelocity[0] and [1]

	vec3_t		punchangle;		// temporary offset
	
// pitch drifting vars
	float		idealpitch;
	float		pitchvel;
	qboolean	nodrift;
	float		driftmove;
	double		laststop;

	float		viewheight;
	float		crouch;			// local amount for smoothing stepups

	qboolean	paused;			// send over by server
	qboolean	onground;
	qboolean	inwater;
	
	int		intermission;		// don't change view angle, full screen, etc
	double	completed_time;		// latched at intermission start

	double		mtime[2];		// the timestamp of last two messages	
	double		time;			// clients view of time, should be between
								// servertime and oldservertime to generate
								// a lerp point for other data
	double		oldtime;		// previous cl.time, time-oldtime is used
								// to decay light values and smooth step ups
	double		ctime;			// joe: copy of cl.time, to avoid incidents caused by rewind


	float		last_received_message;	// (realtime) for net trouble icon

// information that is static for the entire time connected to a server
	struct model_s	*model_precache[MAX_MODELS];
	struct sfx_s	*sound_precache[MAX_SOUNDS];

	char		levelname[128];	// for display on solo scoreboard //johnfitz -- was 40.
	int		viewentity;		// cl_entities[cl.viewentity] = player
	int		maxclients;
	int		gametype;

// refresh related state
	struct model_s	*worldmodel;		// cl_entities[0].model
	struct efrag_s	*free_efrags;
	int			num_efrags;
	int			num_entities;		// held in cl_entities array
	int			num_statics;		// held in cl_staticentities array
	entity_t	viewent;		// the gun model

	int			cdtrack, looptrack;	// cd audio

// frag scoreboard
	scoreboard_t	*scores;		// [cl.maxclients]

	double		last_ping_time;		// last time pings were obtained
	qboolean	console_ping;		// true if the ping came from the console
	double		last_status_time;	// last time status was obtained
	qboolean	console_status;		// true if the status came from the console

	unsigned	protocol; //johnfitz
	unsigned	protocolflags;

// freefly
	democam_mode_t	democam_mode;
	qboolean	democam_freefly_reset;
	double		democam_last_time;
	vec3_t		democam_freefly_origin;
	vec3_t		democam_freefly_angles;
	float		democam_orbit_distance;
	vec3_t		democam_orbit_angles;

	float		zoom;
	float		zoomdir;
} client_state_t;

extern	client_state_t	cl;

// cvars
extern	cvar_t	cl_name;
extern	cvar_t	cl_color;

extern	cvar_t	cl_shownet;
extern	cvar_t	cl_nolerp;

extern	cvar_t	cl_truelightning;
extern	cvar_t	cl_muzzleflash;
extern	cvar_t	cl_sbar;
extern	cvar_t	cl_sbar_offset;
extern	cvar_t	cl_rocket2grenade;
extern	cvar_t	vid_mode;
extern	cvar_t	cl_demorewind;
extern	cvar_t	cl_mapname;
extern	cvar_t	cl_warncmd;

extern	cvar_t	r_explosiontype;
extern	cvar_t	r_explosionlight;
extern	cvar_t	r_rocketlight;
#ifdef GLQUAKE
extern	cvar_t	r_explosionlightcolor;
extern	cvar_t	r_rocketlightcolor;
#endif
extern	cvar_t	r_rockettrail;
extern	cvar_t	r_grenadetrail;
extern	cvar_t	r_powerupglow;
extern	cvar_t	r_coloredpowerupglow;

extern	cvar_t	cl_bobbing;
extern	cvar_t	cl_demospeed;
extern	cvar_t	cl_maxfps;
extern	cvar_t	cl_advancedcompletion;
extern	cvar_t	cl_viewweapons;
extern	cvar_t	cl_autodemo;
extern	cvar_t	cl_deadbodyfilter;
extern	cvar_t	cl_gibfilter;
extern	cvar_t	cl_confirmquit;
extern	cvar_t	cl_bbox;
extern	cvar_t	cl_bboxcolors;

extern	cvar_t	cl_demoui;
extern	cvar_t	cl_demouitimeout;
extern	cvar_t	cl_demouihidespeed;

#define	MAX_TEMP_ENTITIES	256		// lightning bolts, etc
#define	MAX_STATIC_ENTITIES	4096	//ericw -- was 512

// FIXME, allocate dynamically
extern	efrag_t			cl_efrags[MAX_EFRAGS];
extern	entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
extern	lightstyle_t		cl_lightstyle[MAX_LIGHTSTYLES];
extern	dlight_t		cl_dlights[MAX_DLIGHTS];
extern	entity_t		cl_temp_entities[MAX_TEMP_ENTITIES];
extern	beam_t			cl_beams[MAX_BEAMS];

extern	entity_t		*cl_entities; //johnfitz -- was a static array, now on hunk
extern	int				cl_max_edicts; //johnfitz -- only changes when new map loads

//=============================================================================

// cl_main.c
dlight_t *CL_AllocDlight (int key);
void CL_DecayLights (void);

void CL_Init (void);
void CL_Shutdown (void);

void CL_EstablishConnection (char *host);
void CL_Signon1 (void);
void CL_Signon2 (void);
void CL_Signon3 (void);
void CL_Signon4 (void);

void CL_Disconnect (void);
void CL_Disconnect_f (void);
int CL_NextDemo (void);
qboolean Model_isDead (int modelindex, int frame);
qboolean CL_ShowBBoxes(void);

#define			MAX_VISEDICTS	4096	// larger, now we support BSP2 
extern	int		cl_numvisedicts;
extern	entity_t *cl_visedicts[MAX_VISEDICTS];

extern	tagentity_t	q3player_body, q3player_head, q3player_weapon, q3player_weapon_flash;

char *CL_MapName (void);
void CL_CountReconnects (void);

// model indexes
typedef	enum modelindex_s {
	mi_player, mi_q3head, mi_q3torso, mi_q3legs, mi_eyes, mi_rocket, mi_grenade,
	mi_flame0, mi_flame0_md3, mi_flame1, mi_flame2, mi_explo1, mi_explo2, mi_bubble,
	mi_fish, mi_dog, mi_soldier, mi_enforcer, mi_knight, mi_hknight,
	mi_scrag, mi_ogre, mi_fiend, mi_vore, mi_shambler, mi_zombie, mi_spawn,
	mi_h_dog, mi_h_soldier, mi_h_enforcer, mi_h_knight, mi_h_hknight, mi_h_scrag,
	mi_h_ogre, mi_h_fiend, mi_h_vore, mi_h_shambler, mi_h_zombie, mi_h_player,
	mi_gib1, mi_gib2, mi_gib3, mi_vwplayer, mi_vwplayer_md3,
	mi_w_shot, mi_w_shot2, mi_w_nail, mi_w_nail2, mi_w_rock, mi_w_rock2, mi_w_light, 
	mi_q3w_shot, mi_q3w_shot2, mi_q3w_nail, mi_q3w_nail2, mi_q3w_rock, mi_q3w_rock2, mi_q3w_light,
	mi_boss, mi_oldone, mi_i_bh10, mi_i_bh25, mi_i_bh100,
	mi_i_quad, mi_i_invuln, mi_i_suit, mi_i_invis, mi_i_armor,
	mi_i_shell0, mi_i_shell1, mi_i_nail0, mi_i_nail1, mi_i_rock0, mi_i_rock1,
	mi_i_batt0, mi_i_batt1,
	mi_i_shot, mi_i_nail, mi_i_nail2, mi_i_rock, mi_i_rock2, mi_i_light,
	mi_i_wskey, mi_i_mskey, mi_i_wgkey, mi_i_mgkey,
	mi_i_end1, mi_i_end2, mi_i_end3, mi_i_end4,
	mi_i_backpack, mi_explobox,
	mi_k_spike, mi_s_spike, mi_v_spike, mi_w_spike, mi_laser, mi_spike,

	NUM_MODELINDEX
} modelindex_t;

extern	int			cl_modelindex[NUM_MODELINDEX];
extern	char		*cl_modelnames[NUM_MODELINDEX];

// cl_input.c
typedef struct
{
	int		down[2];		// key nums holding it down
	int		state;			// low bit is down state
} kbutton_t;

extern	kbutton_t	in_mlook, in_klook;
extern 	kbutton_t 	in_strafe;
extern 	kbutton_t 	in_speed;

void CL_InitInput (void);
void CL_SendCmd (void);
void CL_SendMove (usercmd_t *cmd);
void CL_SendLagMove (void);	// joe: synthetic lag, from ProQuake

void CL_ClearState (void);

void CL_ReadFromServer (void);
void CL_WriteToServer (usercmd_t *cmd);
void CL_BaseMove (usercmd_t *cmd);

float CL_KeyState (kbutton_t *key);

// demoseekparse.c
#define DSEEK_MAX_MAPS	128
#define DSEEK_MAP_NAME_SIZE  64
typedef struct
{
	long offset;
	char name[DSEEK_MAP_NAME_SIZE];
	float min_time, finish_time, max_time;
} dseek_map_info_t;
typedef struct
{
	dseek_map_info_t maps[DSEEK_MAX_MAPS];
	int num_maps;
} dseek_info_t;
qboolean DSeek_Parse (FILE *demo_file, dseek_info_t *dseek_info);

// cl_demo.c
extern dseek_info_t demo_seek_info;
extern qboolean demo_seek_info_available;
void CL_InitDemo(void);
void CL_ShutdownDemo (void);
void CL_StopPlayback (void);
int CL_GetMessage (void);
void CL_Stop_f (void);
void CL_Record_f (void);
void CL_PlayDemo_f (void);
void CL_DemoSkip_f (void);
void CL_DemoSeek_f (void);
void CL_TimeDemo_f(void);
void CL_KeepDemo_f (void);
int CL_DemoIntermissionState (int old_state, int new_state);
qboolean CL_DemoRewind(void);
dseek_map_info_t *CL_DemoGetCurrentMapInfo (int *map_num_p);
qboolean CL_DemoUIOpen(void);

// cl_demoui.c
typedef struct mouse_state_s mouse_state_t;
extern qboolean demoui_dragging_seek;
extern qboolean demoui_freefly_mlook;
qboolean DemoUI_MouseEvent(const mouse_state_t* ms);
void DemoUI_Draw(void);
qboolean DemoUI_Visible(void);

// cl_parse.c
void CL_ParseServerMessage (void);
void CL_NewTranslation (int slot, qboolean ghost);
void CL_InitModelnames (void);
void CL_SignonReply (void);

// view.c
void V_StartPitchDrift (void);
void V_StopPitchDrift (void);
void V_RenderView (void);
void V_UpdatePalette (void);
void V_Register (void);
void V_ParseDamage (void);
void V_SetContentsColor (int contents);

// cl_tent.c
void CL_InitTEnts (void);
void CL_ClearTEnts (void);
void CL_ParseTEnt (void);
void CL_UpdateTEnts (void);
entity_t *CL_NewTempEntity (void);
qboolean TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);


// cl_dzip.c
typedef enum {
	DZIP_INVALID,
	DZIP_NOT_EXTRACTING,
	DZIP_NO_EXIST,
	DZIP_ALREADY_EXTRACTING,
	DZIP_EXTRACT_IN_PROGRESS,
	DZIP_EXTRACT_FAIL,
	DZIP_EXTRACT_SUCCESS,
} dzip_status_t;
typedef struct {
	qboolean initialized;

    // If true files will be extracted into a temporary directory.  If false,
    // files will be extracted into the same directory as the dzip file.
    qboolean use_temp_dir;

	// Directory into which dzip files will be extracted.
	char extract_dir[1024];

	// Full path of the extracted demo file.
	char dem_path[1024];

	// When opened, file pointer will be put here.
	FILE **demo_file_p;

#ifdef _WIN32
	HANDLE proc;
#else
	qboolean proc;
#endif
} dzip_context_t;
void DZip_Init (dzip_context_t *ctx, const char *prefix);
dzip_status_t DZip_StartExtract (dzip_context_t *ctx, const char *name, FILE **demo_file_p);
dzip_status_t DZip_CheckCompletion (dzip_context_t *ctx);
dzip_status_t DZip_Open(dzip_context_t *ctx, const char *name, FILE **demo_file_p);
void DZip_Cleanup(dzip_context_t *ctx);


// democam.c
void DemoCam_Init (void);
void DemoCam_InitClient (void);
void DemoCam_UpdateOrigin (void);
void DemoCam_MouseMove (double x, double y);
void DemoCam_SetRefdef (void);
qboolean DemoCam_MLook (void);
void DemoCam_DrawPos (void);

#ifdef GLQUAKE
dlighttype_t SetDlightColor (float f, dlighttype_t def, qboolean random);
#endif

void R_TranslatePlayerSkin (int playernum, qboolean ghost);
extern	int	player_fb_skins[MAX_SCOREBOARD];
extern	int	ghost_fb_skins[MAX_SCOREBOARD];
