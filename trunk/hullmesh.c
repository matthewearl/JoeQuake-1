#include <assert.h>

#include "quakedef.h"

#define MAX_POINTS_ON_WINDING	64
#define	BOGUS_RANGE	18000
#define	SIDESPACE	24

typedef struct
{
	int		numpoints;
	vec3_t	points[8];			// variable sized
} winding_t;

// Per-node information specific to portals.
// One element per node.
typedef struct node_s
{
// information for decision nodes	
	mplane_t		*plane;
	struct node_s	*children[2];	// only valid for decision nodes
	
// information for leafs
	int				contents;		// leaf nodes (0 for decision nodes)
	struct portal_s	*portals;
} node_t;

typedef struct portal_s
{
	mplane_t	*plane;
	node_t		*nodes[2];		// [0] = front side of planenum
	struct portal_s	*next[2];	
	winding_t	*winding;
} portal_t;

typedef struct
{
	vec3_t	position;
	vec3_t	normal;
} hull_vertex_t;

static node_t	outside_node;		// portals outside the world face this


static winding_t *NewWinding (int points);
static void FreeWinding (winding_t *w);
static mplane_t new_planes[6];  // bounding box planes

/*
 * The below is adapted from qutils/qbsp/bsp.c in quake-tools
 */

/*
=================
BaseWindingForPlane
=================
*/
static winding_t *BaseWindingForPlane (mplane_t *p)
{
	int		i, x;
	vec_t	max, v;
	vec3_t	org, vright, vup;
	winding_t	*w;
	
// find the major axis

	max = -BOGUS_RANGE;
	x = -1;
	for (i=0 ; i<3; i++)
	{
		v = fabs(p->normal[i]);
		if (v > max)
		{
			x = i;
			max = v;
		}
	}
	if (x==-1)
		Sys_Error ("BaseWindingForPlane: no axis found");
		
	VectorCopy (vec3_origin, vup);	
	switch (x)
	{
	case 0:
	case 1:
		vup[2] = 1;
		break;		
	case 2:
		vup[0] = 1;
		break;		
	}

	v = DotProduct (vup, p->normal);
	VectorMA (vup, -v, p->normal, vup);
	VectorNormalize (vup);
		
	VectorScale (p->normal, p->dist, org);
	
	CrossProduct (vup, p->normal, vright);
	
	VectorScale (vup, 8192, vup);
	VectorScale (vright, 8192, vright);

// project a really big	axis aligned box onto the plane
	w = NewWinding (4);
	
	VectorSubtract (org, vright, w->points[0]);
	VectorAdd (w->points[0], vup, w->points[0]);
	
	VectorAdd (org, vright, w->points[1]);
	VectorAdd (w->points[1], vup, w->points[1]);
	
	VectorAdd (org, vright, w->points[2]);
	VectorSubtract (w->points[2], vup, w->points[2]);
	
	VectorSubtract (org, vright, w->points[3]);
	VectorSubtract (w->points[3], vup, w->points[3]);
	
	w->numpoints = 4;
	
	return w;	
}

/*
==================
ClipWinding

Clips the winding to the plane, returning the new winding on the positive side
Frees the input winding.
If keepon is true, an exactly on-plane winding will be saved, otherwise
it will be clipped away.
==================
*/
static winding_t *ClipWinding (winding_t *in, mplane_t *split, qboolean keepon)
{
	vec_t	dists[MAX_POINTS_ON_WINDING];
	int		sides[MAX_POINTS_ON_WINDING];
	int		counts[3];
	vec_t	dot;
	int		i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
	winding_t	*neww;
	int		maxpts;
	
	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for (i=0 ; i<in->numpoints ; i++)
	{
		dot = DotProduct (in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	
	if (keepon && !counts[0] && !counts[1])
		return in;
		
	if (!counts[0])
	{
		FreeWinding (in);
		return NULL;
	}
	if (!counts[1])
		return in;
	
	maxpts = in->numpoints+4;	// can't use counts[0]+2 because
								// of fp grouping errors
	neww = NewWinding (maxpts);
		
	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = in->points[i];
		
		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
		}
		
		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
	// generate a split point
		p2 = in->points[(i+1)%in->numpoints];
		
		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{	// avoid round off error when possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}
			
		VectorCopy (mid, neww->points[neww->numpoints]);
		neww->numpoints++;
	}
	
	if (neww->numpoints > maxpts)
		Sys_Error ("ClipWinding: points exceeded estimate");
		
// free the original winding
	FreeWinding (in);
	
	return neww;
}


/*
==================
DivideWinding

Divides a winding by a plane, producing one or two windings.  The
original winding is not damaged or freed.  If only on one side, the
returned winding will be the input winding.  If on both sides, two
new windings will be created.
==================
*/
static void	DivideWinding (winding_t *in, mplane_t *split, winding_t **front, winding_t **back)
{
	vec_t	dists[MAX_POINTS_ON_WINDING];
	int		sides[MAX_POINTS_ON_WINDING];
	int		counts[3];
	vec_t	dot;
	int		i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
	winding_t	*f, *b;
	int		maxpts;
	
	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for (i=0 ; i<in->numpoints ; i++)
	{
		dot = DotProduct (in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	
	*front = *back = NULL;

	if (!counts[0])
	{
		*back = in;
		return;
	}
	if (!counts[1])
	{
		*front = in;
		return;
	}

	maxpts = in->numpoints+4;	// can't use counts[0]+2 because
								// of fp grouping errors

	*front = f = NewWinding (maxpts);
	*back = b = NewWinding (maxpts);
		
	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = in->points[i];
		
		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, f->points[f->numpoints]);
			f->numpoints++;
			VectorCopy (p1, b->points[b->numpoints]);
			b->numpoints++;
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, f->points[f->numpoints]);
			f->numpoints++;
		}
		if (sides[i] == SIDE_BACK)
		{
			VectorCopy (p1, b->points[b->numpoints]);
			b->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
	// generate a split point
		p2 = in->points[(i+1)%in->numpoints];
		
		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{	// avoid round off error when possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}
			
		VectorCopy (mid, f->points[f->numpoints]);
		f->numpoints++;
		VectorCopy (mid, b->points[b->numpoints]);
		b->numpoints++;
	}
	
	if (f->numpoints > maxpts || b->numpoints > maxpts)
		Sys_Error ("ClipWinding: points exceeded estimate");
}

/*
==================
NewWinding
==================
*/
static winding_t *NewWinding (int points)
{
	winding_t	*w;
	int			size;
	
	if (points > MAX_POINTS_ON_WINDING)
		Sys_Error ("NewWinding: %i points", points);
	
	size = (int)((void *)((winding_t *)0)->points[points] - (void *)0);
	w = malloc (size);
	memset (w, 0, size);
	
	return w;
}

static void FreeWinding (winding_t *w)
{
	free (w);
}

/*
===========
AllocPortal
===========
*/
static portal_t *AllocPortal (void)
{
	portal_t	*p;
	
	p = malloc (sizeof(portal_t));
	memset (p, 0, sizeof(portal_t));
	
	return p;
}

static void FreePortal (portal_t *p)
{
	free (p);
}

/*
 * The below is adapted from qutils/qbsp/portals.c in quake-tools
 */

/*
=============
AddPortalToNodes
=============
*/
static void AddPortalToNodes (portal_t *p, node_t *front, node_t *back)
{
	if (p->nodes[0] || p->nodes[1])
		Sys_Error ("AddPortalToNode: allready included");

	p->nodes[0] = front;
	p->next[0] = front->portals;
	front->portals = p;
	
	p->nodes[1] = back;
	p->next[1] = back->portals;
	back->portals = p;
}

/*
=============
RemovePortalFromNode
=============
*/
static void RemovePortalFromNode (portal_t *portal, node_t *l)
{
	portal_t	**pp, *t;
	
// remove reference to the current portal
	pp = &l->portals;
	while (1)
	{
		t = *pp;
		if (!t)
			Sys_Error ("RemovePortalFromNode: portal not in leaf");	

		if ( t == portal )
			break;

		if (t->nodes[0] == l)
			pp = &t->next[0];
		else if (t->nodes[1] == l)
			pp = &t->next[1];
		else
			Sys_Error ("RemovePortalFromNode: portal not bounding leaf");
	}
	
	if (portal->nodes[0] == l)
	{
		*pp = portal->next[0];
		portal->nodes[0] = NULL;
	}
	else if (portal->nodes[1] == l)
	{
		*pp = portal->next[1];	
		portal->nodes[1] = NULL;
	}
}

/*
================
MakeHeadnodePortals

The created portals will face the global outside_node
================
*/
static void MakeHeadnodePortals (node_t *node, vec3_t mins, vec3_t maxs)
{
	vec3_t		bounds[2];
	int			i, j, n;
	portal_t	*p, *portals[6];
	mplane_t	*pl;
	
// pad with some space so there will never be null volume leafs
	for (i=0 ; i<3 ; i++)
	{
		bounds[0][i] = mins[i] - SIDESPACE;
		bounds[1][i] = maxs[i] + SIDESPACE;
	}
	
	outside_node.contents = CONTENTS_SOLID;
	outside_node.portals = NULL;

	for (i=0 ; i<3 ; i++)
		for (j=0 ; j<2 ; j++)
		{
			n = j*3 + i;

			p = AllocPortal ();
			portals[n] = p;
			
			pl = &new_planes[n];
			memset (pl, 0, sizeof(*pl));
			if (j)
			{
				pl->normal[i] = -1;
				pl->dist = -bounds[j][i];
			}
			else
			{
				pl->normal[i] = 1;
				pl->dist = bounds[j][i];
			}
			p->plane = pl;
	
			p->winding = BaseWindingForPlane (pl);
			AddPortalToNodes (p, node, &outside_node);
		}
		
// clip the basewindings by all the other planes
	for (i=0 ; i<6 ; i++)
	{
		for (j=0 ; j<6 ; j++)
		{
			if (j == i)
				continue;
			portals[i]->winding = ClipWinding (portals[i]->winding, &new_planes[j], true);
		}
	}
}

/*
================
CutNodePortals_r

================
*/
static void CutNodePortals_r (node_t *node)
{
	mplane_t 	*plane, clipplane;
	node_t		*f, *b, *other_node;
	portal_t	*p, *new_portal, *next_portal;
	winding_t	*w, *frontwinding, *backwinding;
	int			side;

//	CheckLeafPortalConsistancy (node);

//
// seperate the portals on node into it's children	
//
	if (node->contents)
	{
		return;			// at a leaf, no more dividing
	}

	plane = node->plane;

	f = node->children[0];
	b = node->children[1];

//
// create the new portal by taking the full plane winding for the cutting plane
// and clipping it by all of the planes from the other portals
//
	new_portal = AllocPortal ();
	new_portal->plane = node->plane;
	
	w = BaseWindingForPlane (node->plane);
	side = 0;	// shut up compiler warning
	for (p = node->portals ; p ; p = p->next[side])	
	{
		clipplane = *p->plane;
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
		{
			clipplane.dist = -clipplane.dist;
			VectorSubtract (vec3_origin, clipplane.normal, clipplane.normal);
			side = 1;
		}
		else
			Sys_Error ("CutNodePortals_r: mislinked portal");

		w = ClipWinding (w, &clipplane, true);
		if (!w)
		{
			printf ("WARNING: CutNodePortals_r:new portal was clipped away\n");
			break;
		}
	}
	
	if (w)
	{
	// if the plane was not clipped on all sides, there was an error
		new_portal->winding = w;	
		AddPortalToNodes (new_portal, f, b);
	}

//
// partition the portals
//
	for (p = node->portals ; p ; p = next_portal)	
	{
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
			side = 1;
		else
			Sys_Error ("CutNodePortals_r: mislinked portal");
		next_portal = p->next[side];

		other_node = p->nodes[!side];
		RemovePortalFromNode (p, p->nodes[0]);
		RemovePortalFromNode (p, p->nodes[1]);

//
// cut the portal into two portals, one on each side of the cut plane
//
		DivideWinding (p->winding, plane, &frontwinding, &backwinding);
		
		if (!frontwinding)
		{
			if (side == 0)
				AddPortalToNodes (p, b, other_node);
			else
				AddPortalToNodes (p, other_node, b);
			continue;
		}
		if (!backwinding)
		{
			if (side == 0)
				AddPortalToNodes (p, f, other_node);
			else
				AddPortalToNodes (p, other_node, f);
			continue;
		}
		
	// the winding is split
		new_portal = AllocPortal ();
		*new_portal = *p;
		new_portal->winding = backwinding;
		FreeWinding (p->winding);
		p->winding = frontwinding;

		if (side == 0)
		{
			AddPortalToNodes (p, f, other_node);
			AddPortalToNodes (new_portal, b, other_node);
		}
		else
		{
			AddPortalToNodes (p, other_node, f);
			AddPortalToNodes (new_portal, other_node, b);
		}
	}
	
	CutNodePortals_r (f);	
	CutNodePortals_r (b);	
}

/*
==================
FreeAllPortals

==================
*/
void FreeAllPortals (node_t *node)
{
	portal_t	*p, *nextp;
	
	if (!node->contents)
	{
		FreeAllPortals (node->children[0]);
		FreeAllPortals (node->children[1]);
	}
	
	for (p=node->portals ; p ; p=nextp)
	{
		if (p->nodes[0] == node)
			nextp = p->next[0];
		else
			nextp = p->next[1];
		RemovePortalFromNode (p, p->nodes[0]);
		RemovePortalFromNode (p, p->nodes[1]);
		FreeWinding (p->winding);
		FreePortal (p);
	}
}

/*
 * The below is (largely) new code not adapted from quake-tools.
 */


static node_t *ConvertNodes (hull_t *hull, int idx)
{
	mclipnode_t *clipnode;
	int child_idx;
	node_t *node;

	node = malloc(sizeof(node_t));
	if (node == NULL)
		Sys_Error("Could not allocate node");
	memset(node, 0, sizeof(node_t));

	node->portals = NULL;

	if (idx >= 0)
	{
		clipnode = &hull->clipnodes[idx];
		node->plane = &hull->planes[clipnode->planenum];
		node->contents = 0;
		for (child_idx = 0; child_idx < 2; child_idx++)
			node->children[child_idx] = ConvertNodes(hull,
													 clipnode->children[child_idx]);
	} else {
		node->contents = idx;
		for (child_idx = 0; child_idx < 2; child_idx++)
			node->children[child_idx] = NULL;
	}

	return node;
}

static void FreeNode (node_t *node)
{
	int child_idx;

	for (child_idx = 0; child_idx < 2; child_idx++)
		if (node->children[child_idx] != NULL)
			FreeNode(node->children[child_idx]);
	free(node);
}

typedef void (*visit_windings_cb_t)(portal_t *p, winding_t *w, void *ctx);

static void VisitWindings (node_t *node, visit_windings_cb_t cb, void *ctx)
{
	portal_t *p;
	winding_t *w;

	if (!node->contents)
	{
		VisitWindings (node->children[0], cb, ctx);
		VisitWindings (node->children[1], cb, ctx);
		return;
	}

	if (node->contents != CONTENTS_SOLID)
		return;

	for (p = node->portals ; p ; )
	{
		w = p->winding;
		if (w && (
				(p->nodes[0] == node && p->nodes[1]->contents != CONTENTS_SOLID)
				|| (p->nodes[1] == node && p->nodes[0]->contents != CONTENTS_SOLID)
			))
		{
			cb(p, w, ctx);
		}
		
		if (p->nodes[0] == node)
			p = p->next[0];
		else
			p = p->next[1];
	}

}

typedef struct
{
	FILE *f;
	int vertex_count;
} write_face_to_file_ctx_t;

static void WriteFaceToFile (portal_t *p, winding_t *w, void *ctx)
{
	int i;
	write_face_to_file_ctx_t *wfctx = ctx;

	for (i=0 ; i<w->numpoints ; i++)
	{
		fprintf(wfctx->f, "v %f %f %f\n",
					w->points[i][0],
					w->points[i][2],
					-w->points[i][1]);
	}

	fprintf(wfctx->f, "f ");
	for (i=0 ; i<w->numpoints; i++)
	{
		fprintf(wfctx->f, "%d", wfctx->vertex_count + 1);
		if (i < w->numpoints - 1)
			fprintf(wfctx->f, " ");
		wfctx->vertex_count += 1;
	}
	fprintf(wfctx->f, "\n");
}

static void ExtractSurfaceTris (node_t *node)
{
	write_face_to_file_ctx_t wfctx;

	wfctx.vertex_count = 0;
	wfctx.f = fopen("hull.obj", "w");
	if (!wfctx.f)
		Sys_Error("Could not open obj file for writing");

	VisitWindings(node, WriteFaceToFile, &wfctx);

	fclose(wfctx.f);
}

typedef struct
{
	int num_faces;
	int num_vertices;
} count_face_ctx_t;

static void CountFace (portal_t *p, winding_t *w, void *ctx)
{
	count_face_ctx_t *cfctx = ctx;

	cfctx->num_faces += 1;
	cfctx->num_vertices += w->numpoints;
}

typedef struct
{
	int num_vertices;
	int num_indices;

	hull_vertex_t *vertices;
	int *indices;
} write_face_ctx_t;

static void WriteFace (portal_t *p, winding_t *w, void *ctx)
{
	int i;
	write_face_ctx_t *wfctx = ctx;

	for (i=0 ; i<w->numpoints ; i++)
	{
		VectorCopy(w->points[i], wfctx->vertices[wfctx->num_vertices + i].position);
		VectorCopy(p->plane->normal, wfctx->vertices[wfctx->num_vertices + i].normal);
	}

	for (i=0; i<w->numpoints - 2; i++)
	{
		wfctx->indices[wfctx->num_indices++] = wfctx->num_vertices;
		wfctx->indices[wfctx->num_indices++] = wfctx->num_vertices + i + 1;
		wfctx->indices[wfctx->num_indices++] = wfctx->num_vertices + i + 2;
	}

	wfctx->num_vertices += w->numpoints;
}

static void MakeVertexArray (node_t *root_node,
							 hull_vertex_t **vertices,
							 int *num_vertices,
							 int **indices,
							 int *num_indices)
{
	count_face_ctx_t cfctx = {};
	write_face_ctx_t wfctx = {};

	// Allocate memory for vertices and indices.
	VisitWindings(root_node, CountFace, &cfctx);
	*num_vertices = cfctx.num_vertices;
	*num_indices = 3 * (cfctx.num_vertices - 2 * cfctx.num_faces);

	*vertices = malloc(*num_vertices * sizeof(hull_vertex_t));
	if (!*vertices)
		Sys_Error("Could not allocate %d vertices", *num_vertices);
	memset(*vertices, 0, *num_vertices * sizeof(hull_vertex_t));

	*indices = malloc(*num_indices * sizeof(int));
	if (!*indices)
		Sys_Error("Could not allocate %d indices", *num_indices);
	memset(*indices, 0, *num_indices * sizeof(int));

	// Write the vertices and indices.
	wfctx.vertices = *vertices;
	wfctx.indices = *indices;
	VisitWindings(root_node, WriteFace, &wfctx);
	assert(wfctx.num_indices == *num_indices);
	assert(wfctx.num_vertices == *num_vertices);
}

void TriangulateHull (hull_t *hull, vec3_t mins, vec3_t maxs)
{
	node_t *root_node;

	root_node = ConvertNodes(hull, 0);
	MakeHeadnodePortals(root_node, mins, maxs);
	CutNodePortals_r(root_node);

	ExtractSurfaceTris(root_node);

	FreeAllPortals(root_node);
	FreeNode(root_node);
}
