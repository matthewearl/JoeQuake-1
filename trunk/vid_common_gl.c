/*
Copyright (C) 2002-2003 A Nourai

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
// vid_common_gl.c -- Common code for vid_wgl.c and vid_glx.c

#include "quakedef.h"

#ifdef _WIN32
#define qglGetProcAddress wglGetProcAddress
#else
void *glXGetProcAddress (const GLubyte *procName);
#define qglGetProcAddress glXGetProcAddress
#endif

const	char	*gl_vendor;
const	char	*gl_renderer;
const	char	*gl_version;
static	int		gl_version_major;
static	int		gl_version_minor;
const	char	*gl_extensions;

qboolean	gl_mtexable = false;
int			gl_textureunits = 1;
qboolean	gl_anisotropy_able = false; //johnfitz
float		gl_max_anisotropy; //johnfitz
qboolean	gl_vbo_able = false;
qboolean	gl_glsl_able = false;
qboolean	gl_glsl_gamma_able = false;
qboolean	gl_glsl_alias_able = false; //ericw
qboolean	gl_packed_pixels = false;
qboolean	gl_nv_depth_clamp = false;

lpGenerateMipmapFUNC qglGenerateMipmap = NULL;

lpMTexFUNC	qglMultiTexCoord2f = NULL;
lpSelTexFUNC qglActiveTexture = NULL;
lpBindBufFUNC qglBindBuffer = NULL;
lpBufferDataFUNC qglBufferData = NULL;
lpBufferSubDataFUNC qglBufferSubData = NULL;
lpDeleteBuffersFUNC qglDeleteBuffers = NULL;
lpGenBuffersFUNC qglGenBuffers = NULL;

lpCreateShaderFUNC qglCreateShader = NULL; //ericw
lpDeleteShaderFUNC qglDeleteShader = NULL; //ericw
lpDeleteProgramFUNC qglDeleteProgram = NULL; //ericw
lpShaderSourceFUNC qglShaderSource = NULL; //ericw
lpCompileShaderFUNC qglCompileShader = NULL; //ericw
lpGetShaderivFUNC qglGetShaderiv = NULL; //ericw
lpGetShaderInfoLogFUNC qglGetShaderInfoLog = NULL; //ericw
lpGetProgramivFUNC qglGetProgramiv = NULL; //ericw
lpGetProgramInfoLogFUNC qglGetProgramInfoLog = NULL; //ericw
lpCreateProgramFUNC qglCreateProgram = NULL; //ericw
lpAttachShaderFUNC qglAttachShader = NULL; //ericw
lpLinkProgramFUNC qglLinkProgram = NULL; //ericw
lpBindAttribLocationFUNC qglBindAttribLocation = NULL; //ericw
lpUseProgramFUNC qglUseProgram = NULL; //ericw
lpGetAttribLocationFUNC qglGetAttribLocation = NULL; //ericw
lpVertexAttribPointerFUNC qglVertexAttribPointer = NULL; //ericw
lpEnableVertexAttribArrayFUNC qglEnableVertexAttribArray = NULL; //ericw
lpDisableVertexAttribArrayFUNC qglDisableVertexAttribArray = NULL; //ericw
lpGetUniformLocationFUNC qglGetUniformLocation = NULL; //ericw
lpUniform1iFUNC qglUniform1i = NULL; //ericw
lpUniform1ivFUNC qglUniform1iv = NULL;
lpUniform1fFUNC qglUniform1f = NULL; //ericw
lpUniform3fFUNC qglUniform3f = NULL; //ericw
lpUniform4fFUNC qglUniform4f = NULL; //ericw
lpUniformMatrix4fvFUNC qglUniformMatrix4fv = NULL;
lpTexBufferFUNC qglTexBuffer = NULL;
lpBindBufferBaseFUNC qglBindBufferBase = NULL;
lpGetUniformBlockIndexFUNC qglGetUniformBlockIndex = NULL;
lpUniformBlockBindingFUNC qglUniformBlockBinding = NULL;

qboolean	gl_add_ext = false;

float		vid_gamma = 1.0;
byte		vid_gamma_table[256];

unsigned	d_8to24table[256];

byte		color_white[4] = {255, 255, 255, 0};
byte		color_black[4] = {0, 0, 0, 0};

extern qboolean	fullsbardraw;

qboolean CheckExtension (const char *extension)
{
	char		*where, *terminator;
	const	char	*start;

	if (!gl_extensions && !(gl_extensions = glGetString(GL_EXTENSIONS)))
		return false;

	if (!extension || *extension == 0 || strchr(extension, ' '))
		return false;

	for (start = gl_extensions ; where = strstr(start, extension) ; start = terminator)
	{
		terminator = where + strlen(extension);
		if ((where == start || *(where - 1) == ' ') && (*terminator == 0 || *terminator == ' '))
			return true;
	}

	return false;
}

void CheckGenerateMipmapExtension (void)
{
	if (gl_version_major >= 3)
	{
		qglGenerateMipmap = (void *)qglGetProcAddress("glGenerateMipmap");
	}
}

void CheckMultiTextureExtensions (void)
{
	if (!COM_CheckParm("-nomtex") && CheckExtension("GL_ARB_multitexture"))
	{
		if (strstr(gl_renderer, "Savage"))
			return;
		qglMultiTexCoord2f = (void *)qglGetProcAddress ("glMultiTexCoord2fARB");
		qglActiveTexture = (void *)qglGetProcAddress ("glActiveTextureARB");
		if (!qglMultiTexCoord2f || !qglActiveTexture)
			return;
		Con_Printf ("Multitexture extensions found\n");
		gl_mtexable = true;
	}

	glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS, &gl_textureunits);
	gl_textureunits = min(gl_textureunits, 8);

	if (COM_CheckParm("-maxtmu2"))
		gl_textureunits = min(gl_textureunits, 2);

	if (gl_textureunits < 2)
		gl_mtexable = false;

	if (!gl_mtexable)
		gl_textureunits = 1;
	else
		Con_Printf ("Enabled %i texture units on hardware\n", gl_textureunits);
}

void CheckAnisotropicFilteringExtensions(void)
{
	if (CheckExtension("GL_EXT_texture_filter_anisotropic"))
	{
		float test1, test2;
		GLuint tex;

		// test to make sure we really have control over it
		// 1.0 and 2.0 should always be legal values
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
		glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &test1);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.0f);
		glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &test2);
		glDeleteTextures(1, &tex);

		if (test1 == 1 && test2 == 2)
		{
			Con_Printf("Anisotropic filter extensions found\n");
			gl_anisotropy_able = true;
		}
		else
		{
			Con_Printf("Anisotropic filtering locked by driver. Current driver setting is %.0f\n", test1);
		}

		//get max value either way, so the menu and stuff know it
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_max_anisotropy);
		if (gl_max_anisotropy < 2)
		{
			gl_anisotropy_able = false;
			gl_max_anisotropy = 1;
			Con_Printf("Anisotropic filtering broken: disabled\n");
		}
	}
	else
	{
		gl_max_anisotropy = 1;
	}
}

void CheckVertexBufferExtensions(void)
{
	if (!COM_CheckParm("-novbo") && 
		((gl_version_major == 1 && gl_version_minor >= 5) || gl_version_major > 1))
	{
		qglBindBuffer = (void *)qglGetProcAddress("glBindBufferARB");
		qglBufferData = (void *)qglGetProcAddress("glBufferDataARB");
		qglBufferSubData = (void *)qglGetProcAddress("glBufferSubDataARB");
		qglDeleteBuffers = (void *)qglGetProcAddress("glDeleteBuffersARB");
		qglGenBuffers = (void *)qglGetProcAddress("glGenBuffersARB");
		if (!qglBindBuffer || !qglBufferData || !qglBufferSubData || !qglDeleteBuffers || !qglGenBuffers)
			return;
		Con_Printf("Vertex buffer extensions found\n");
		gl_vbo_able = true;
	}
}

void CheckGLSLExtensions(void)
{
	if (!COM_CheckParm("-noglsl") && gl_version_major >= 2)
	{
		qglCreateShader = (void *)qglGetProcAddress("glCreateShader");
		qglDeleteShader = (void *)qglGetProcAddress("glDeleteShader");
		qglDeleteProgram = (void *)qglGetProcAddress("glDeleteProgram");
		qglShaderSource = (void *)qglGetProcAddress("glShaderSource");
		qglCompileShader = (void *)qglGetProcAddress("glCompileShader");
		qglGetShaderiv = (void *)qglGetProcAddress("glGetShaderiv");
		qglGetShaderInfoLog = (void *)qglGetProcAddress("glGetShaderInfoLog");
		qglGetProgramiv = (void *)qglGetProcAddress("glGetProgramiv");
		qglGetProgramInfoLog = (void *)qglGetProcAddress("glGetProgramInfoLog");
		qglCreateProgram = (void *)qglGetProcAddress("glCreateProgram");
		qglAttachShader = (void *)qglGetProcAddress("glAttachShader");
		qglLinkProgram = (void *)qglGetProcAddress("glLinkProgram");
		qglBindAttribLocation = (void *)qglGetProcAddress("glBindAttribLocation");
		qglUseProgram = (void *)qglGetProcAddress("glUseProgram");
		qglGetAttribLocation = (void *)qglGetProcAddress("glGetAttribLocation");
		qglVertexAttribPointer = (void *)qglGetProcAddress("glVertexAttribPointer");
		qglEnableVertexAttribArray = (void *)qglGetProcAddress("glEnableVertexAttribArray");
		qglDisableVertexAttribArray = (void *)qglGetProcAddress("glDisableVertexAttribArray");
		qglGetUniformLocation = (void *)qglGetProcAddress("glGetUniformLocation");
		qglUniform1i = (void *)qglGetProcAddress("glUniform1i");
		qglUniform1iv = (void *)qglGetProcAddress("glUniform1iv");
		qglUniform1f = (void *)qglGetProcAddress("glUniform1f");
		qglUniform3f = (void *)qglGetProcAddress("glUniform3f");
		qglUniform4f = (void *)qglGetProcAddress("glUniform4f");
		qglUniformMatrix4fv = (void *)qglGetProcAddress("glUniformMatrix4fv");
		qglTexBuffer = (void *)qglGetProcAddress("glTexBuffer");
		qglBindBufferBase = (void *)qglGetProcAddress("glBindBufferBase");
		qglGetUniformBlockIndex = (void *)qglGetProcAddress("glGetUniformBlockIndex");
		qglUniformBlockBinding = (void *)qglGetProcAddress("glUniformBlockBinding");

		if (qglCreateShader &&
			qglDeleteShader &&
			qglDeleteProgram &&
			qglShaderSource &&
			qglCompileShader &&
			qglGetShaderiv &&
			qglGetShaderInfoLog &&
			qglGetProgramiv &&
			qglGetProgramInfoLog &&
			qglCreateProgram &&
			qglAttachShader &&
			qglLinkProgram &&
			qglBindAttribLocation &&
			qglUseProgram &&
			qglGetAttribLocation &&
			qglVertexAttribPointer &&
			qglEnableVertexAttribArray &&
			qglDisableVertexAttribArray &&
			qglGetUniformLocation &&
			qglUniform1i &&
			qglUniform1iv &&
			qglUniform1f &&
			qglUniform3f &&
			qglUniform4f &&
			qglUniformMatrix4fv &&
			qglTexBuffer &&
			qglBindBufferBase &&
			qglGetUniformBlockIndex &&
			qglUniformBlockBinding)
		{
			Con_Printf("GLSL extensions found\n");
			gl_glsl_able = true;
		}
	}

	// GLSL gamma
	if (!COM_CheckParm("-noglslgamma") && gl_glsl_able)
		gl_glsl_gamma_able = true;

	// GLSL alias model rendering
	if (!COM_CheckParm("-noglslalias") && gl_glsl_able && gl_vbo_able && gl_textureunits >= 4)
	{
		gl_glsl_alias_able = true;
	}
}

void CheckPackedPixelsExtensions(void)
{
	// 10bit color buffer
	if (!COM_CheckParm("-nopackedpixels") && gl_glsl_alias_able &&
		(CheckExtension("GL_APPLE_packed_pixels") || CheckExtension("GL_EXT_packed_pixels")))
	{
		Con_Printf("Packed pixels extensions found\n");
		gl_packed_pixels = true;
	}
}

void CheckDepthClampExtensions(void)
{
	// ARB_depth_clamp
	if (!COM_CheckParm("-nodepthclamp") && 
		(CheckExtension("GL_ARB_depth_clamp") || CheckExtension("GL_NV_depth_clamp")))
	{
		Con_Printf("Depth clamp extensions found\n");
		gl_nv_depth_clamp = true;
	}
}

/*
===============
GL_SetupState -- johnfitz

does all the stuff from GL_Init that needs to be done every time a new GL render context is created
===============
*/
void GL_SetupState (void)
{
	glClearColor (0.15, 0.15, 0.15, 0);
	glCullFace(GL_BACK); //johnfitz -- glquake used CCW with backwards culling -- let's do it right
	glFrontFace(GL_CW); //johnfitz -- glquake used CCW with backwards culling -- let's do it right
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);
	glAlphaFunc (GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);
	glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); //johnfitz

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glDepthRange (0, 1); //johnfitz -- moved here becuase gl_ztrick is gone.
	glDepthFunc (GL_LEQUAL); //johnfitz -- moved here becuase gl_ztrick is gone.
#if 0	//joe: removed due to causing issues in Re:Mobilize mod
	if (gl_nv_depth_clamp)
		glEnable(GL_DEPTH_CLAMP_NV);
#endif
}

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
	gl_vendor = glGetString (GL_VENDOR);
	gl_renderer = glGetString (GL_RENDERER);
	gl_version = glGetString (GL_VERSION);
	gl_extensions = glGetString (GL_EXTENSIONS);

	Con_Printf("GL_VENDOR: %s\n", gl_vendor);
	Con_Printf("GL_RENDERER: %s\n", gl_renderer);
	Con_Printf("GL_VERSION: %s\n", gl_version);
	if (gl_version == NULL || sscanf(gl_version, "%d.%d", &gl_version_major, &gl_version_minor) < 2)
	{
		gl_version_major = 0;
		gl_version_minor = 0;
	}
	if (COM_CheckParm("-gl_ext"))
		Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	if (!Q_strncasecmp((char *)gl_renderer, "PowerVR", 7))
		fullsbardraw = true;

	gl_add_ext = CheckExtension("GL_ARB_texture_env_add");
	CheckGenerateMipmapExtension();
	CheckMultiTextureExtensions ();
	CheckAnisotropicFilteringExtensions();
	CheckVertexBufferExtensions();
	CheckGLSLExtensions();
	CheckPackedPixelsExtensions();
#if 0	//joe: removed due to causing issues in Re:Mobilize mod
	CheckDepthClampExtensions();
#endif

	GLAlias_CreateShaders();
	GLWorld_CreateShaders();
	GL_ClearBufferBindings();
}

void Check_Gamma (unsigned char *pal)
{
	int		i;
	float	inf;
	unsigned char palette[768];

	if ((i = COM_CheckParm("-gamma")) && i + 1 < com_argc)
		vid_gamma = bound(0.3, Q_atof(com_argv[i+1]), 1);
	else
		vid_gamma = 1;

	//Cvar_SetDefault (&v_gamma, vid_gamma);

	if (vid_gamma != 1)
	{
		for (i = 0 ; i < 256 ; i++)
		{
			inf = min(255 * pow((i + 0.5) / 255.5, vid_gamma) + 0.5, 255);
			vid_gamma_table[i] = inf;
		}
	}
	else
	{
		for (i = 0 ; i < 256 ; i++)
			vid_gamma_table[i] = i;
	}

	for (i = 0 ; i < 768 ; i++)
		palette[i] = vid_gamma_table[pal[i]];

	memcpy (pal, palette, sizeof(palette));
}

void VID_SetPalette (unsigned char *palette)
{
	int		i;
	byte	*pal;
	unsigned r, g, b, *table;

// 8 8 8 encoding
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		*table++ = (255<<24) + (r<<0) + (g<<8) + (b<<16);
	}
	d_8to24table[255] = 0;	// 255 is transparent
}
