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
// chase.c -- chase camera code

#include "quakedef.h"

cvar_t	cl_thirdperson_back = {"cl_thirdperson_back", "100"};
cvar_t	cl_thirdperson_up = {"cl_thirdperson_up", "16"};
cvar_t	cl_thirdperson_right = {"cl_thirdperson_right", "0"};
cvar_t	cl_thirdperson = {"cl_thirdperson", "0"};

vec3_t	chase_pos;
vec3_t	chase_angles;

vec3_t	chase_dest;
vec3_t	chase_dest_angles;


void Chase_Init (void)
{
	Cvar_Register (&cl_thirdperson_back);
	Cvar_Register (&cl_thirdperson_up);
	Cvar_Register (&cl_thirdperson_right);
	Cvar_Register (&cl_thirdperson);

	Cmd_AddLegacyCommand("chase_active", "cl_thirdperson");
}

void TraceLine (vec3_t start, vec3_t end, vec3_t impact)
{
	trace_t	trace;

	memset (&trace, 0, sizeof(trace));
	SV_RecursiveHullCheck (&cl.worldmodel->hulls[1], 0, 0, 1, start, end, &trace);
	VectorCopy (trace.endpos, impact);
}

void Chase_Update (void)
{
	int	i;
	float	dist;
	vec3_t	forward, up, right, dest, start, stop;

	// if can't see player, reset
	AngleVectors (cl.viewangles, forward, right, up);

	// calc exact destination
	VectorCopy(r_refdef.vieworg, start);
	start[2] -= cl.viewheight;
	for (i=0 ; i<3 ; i++)
		chase_dest[i] = start[i] - forward[i]*cl_thirdperson_back.value - right[i]*cl_thirdperson_right.value;
	chase_dest[2] += cl_thirdperson_up.value;

	TraceLine (start, chase_dest, stop);
	if (stop[0] != 0 || stop[1] != 0 || stop[2] != 0)
		VectorCopy (stop, chase_dest);

	// move towards destination
	VectorCopy (chase_dest, r_refdef.vieworg);
}
