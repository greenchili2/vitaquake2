/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// r_main.c

#include "r_local.h"

viddef_t	vid;

unsigned	d_refsoft_8to24table[256];

entity_t	r_worldentity;

static char		skyname[MAX_QPATH];
static float		skyrotate;
static vec3_t		skyaxis;
static image_t		*sky_images[6];

refdef_t	r_refsoft_newrefdef;
model_t		*refsoft_currentmodel;

model_t		*r_refsoft_worldmodel;

byte		r_warpbuffer[WARP_WIDTH * WARP_HEIGHT];

swstate_t sw_state;

void		*colormap;
vec3_t		viewlightvec;
alight_t	r_viewlighting = {128, 192, viewlightvec};
float		r_time1;
int			r_numallocatededges;
float		r_aliasuvscale = 1.0;
int			r_outofsurfaces;
int			r_outofedges;

qboolean	r_dowarp;

mvertex_t	*r_pcurrentvertbase;

int			c_surf;
int			r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
qboolean	r_surfsonstack;
int			r_clipflags;

/* forward declarations */
static void Draw_GetPalette(void);
static void SWR_BeginFrame( float camera_separation );
static void R_DrawBeam( entity_t *e );

//
// view origin
//
vec3_t	vup, base_vup;
vec3_t	refsoft_vpn, base_vpn;
vec3_t	vright, base_vright;
vec3_t	r_refsoft_origin;

//
// screen size info
//
oldrefdef_t	r_refdef;
float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int		r_screenwidth;

float	verticalFieldOfView;
float	xOrigin, yOrigin;

mplane_t	screenedge[4];

//
// refresh flags
//
int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount;
int		d_spanpixcount;
int		r_polycount;
int		r_drawnpolycount;
int		r_wholepolycount;

int			*pfrustum_indexes[4];
int			r_frustum_indexes[4*6];

mleaf_t		*r_viewleaf;
int			r_refsoft_viewcluster, r_refsoft_oldviewcluster;

image_t  	*r_notexture_mip;

float	da_time1, da_time2, dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float	se_time1, se_time2, de_time1, de_time2;

static void SWR_MarkLeaves (void);

cvar_t	*r_refsoft_lefthand;
cvar_t	*sw_aliasstats;
cvar_t	*sw_allow_modex;
cvar_t	*sw_clearcolor;
cvar_t	*sw_drawflat;
cvar_t	*sw_draworder;
cvar_t	*sw_maxedges;
cvar_t	*sw_maxsurfs;
cvar_t  *sw_mode;
cvar_t	*sw_reportedgeout;
cvar_t	*sw_reportsurfout;
cvar_t  *sw_stipplealpha;
cvar_t	*sw_surfcacheoverride;
cvar_t	*sw_waterwarp;

cvar_t	*r_refsoft_drawworld;
static cvar_t	*r_drawentities;
cvar_t	*r_dspeeds;
cvar_t	*r_fullbright;
cvar_t  *r_refsoft_lerpmodels;
static cvar_t  *r_novis;

cvar_t	*r_speeds;
cvar_t	*r_refsoft_lightlevel;	//FIXME HACK

extern cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;

//PGM
cvar_t	*sw_lockpvs;
//PGM

cvar_t	*sw_texfilt;

#define	STRINGER(x) "x"

// r_vars.c

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver


// d_vars.c

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver

float	d_sdivzstepu, d_tdivzstepu, d_zistepu;
float	d_sdivzstepv, d_tdivzstepv, d_zistepv;
float	d_sdivzorigin, d_tdivzorigin, d_ziorigin;

fixed16_t	sadjust, tadjust, bbextents, bbextentt;

pixel_t			*cacheblock;
int				cachewidth;
pixel_t			*d_viewbuffer;
short			*d_pzbuffer;
unsigned int	d_zrowbytes;
unsigned int	d_zwidth;


byte	r_notexture_buffer[1024];

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	int		x,y, m;
	byte	*dest;
	
// create a simple checkerboard texture for the default
	r_notexture_mip = (image_t *)&r_notexture_buffer;
	
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->pixels[0] = &r_notexture_buffer[sizeof(image_t)];
	r_notexture_mip->pixels[1] = r_notexture_mip->pixels[0] + 16*16;
	r_notexture_mip->pixels[2] = r_notexture_mip->pixels[1] + 8*8;
	r_notexture_mip->pixels[3] = r_notexture_mip->pixels[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = r_notexture_mip->pixels[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )

					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
}


/*
================
R_InitTurb
================
*/
void R_InitTurb (void)
{
   int		i;
   int w = vid.width*2;

	if (sintable != NULL)
	{
		free(sintable);
		free(intsintable);
		free(blanktable);
	}
	
   if (w == 0)
   {
      sintable = NULL;
      intsintable = NULL;
      blanktable = NULL;
      return;
   }
	sintable = malloc(sizeof(int)*w);
	intsintable = malloc(sizeof(int)*w);
	blanktable = malloc(sizeof(int)*w);

	for (i=0 ; i<w ; i++)
	{
		sintable[i] = AMP + sin(i*3.14159*2/CYCLE)*AMP;
		intsintable[i] = AMP2 + sin(i*3.14159*2/CYCLE)*AMP2;	// AMP2, not 20
		blanktable[i] = 0;			//PGM
	}
}

void R_UninitTurb(void)
{
	if (sintable != NULL)
	{
		free(sintable);
		free(intsintable);
		free(blanktable);
	}
}

void R_ImageList_f( void );

#ifdef HAVE_OPENGL
extern cvar_t *gl_xflip;
#else
cvar_t *gl_xflip;
#endif
extern float libretro_gamma;

void SWR_Register (void)
{
	sw_aliasstats = ri.Cvar_Get ("sw_polymodelstats", "0", 0);
	sw_allow_modex = ri.Cvar_Get( "sw_allow_modex", "1", CVAR_ARCHIVE );
	sw_clearcolor = ri.Cvar_Get ("sw_clearcolor", "2", 0);
	sw_drawflat = ri.Cvar_Get ("sw_drawflat", "0", 0);
	sw_draworder = ri.Cvar_Get ("sw_draworder", "0", 0);
	sw_maxedges = ri.Cvar_Get ("sw_maxedges", STRINGER(MAXSTACKSURFACES), 0);
	sw_maxsurfs = ri.Cvar_Get ("sw_maxsurfs", "0", 0);
	sw_mipcap = ri.Cvar_Get ("sw_mipcap", "0", CVAR_ARCHIVE);
	sw_mipscale = ri.Cvar_Get ("sw_mipscale", "1", 0);
	sw_reportedgeout = ri.Cvar_Get ("sw_reportedgeout", "0", 0);
	sw_reportsurfout = ri.Cvar_Get ("sw_reportsurfout", "0", 0);
	sw_stipplealpha = ri.Cvar_Get( "sw_stipplealpha", "0", CVAR_ARCHIVE );
	sw_surfcacheoverride = ri.Cvar_Get ("sw_surfcacheoverride", "0", 0);
	sw_waterwarp = ri.Cvar_Get ("sw_waterwarp", "1", 0);
	sw_mode = ri.Cvar_Get( "sw_mode", "0", CVAR_ARCHIVE );
	gl_xflip = ri.Cvar_Get( "gl_xflip", "0", CVAR_ARCHIVE);
	
	r_refsoft_lefthand = ri.Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_speeds = ri.Cvar_Get ("r_speeds", "0", 0);
	r_fullbright = ri.Cvar_Get ("r_fullbright", "0", 0);
	r_drawentities = ri.Cvar_Get ("r_drawentities", "1", 0);
	r_refsoft_drawworld = ri.Cvar_Get ("r_drawworld", "1", 0);
	r_dspeeds = ri.Cvar_Get ("r_dspeeds", "0", 0);
	r_refsoft_lightlevel = ri.Cvar_Get ("r_lightlevel", "0", 0);
	r_refsoft_lerpmodels = ri.Cvar_Get( "r_lerpmodels", "1", 0 );
	r_novis = ri.Cvar_Get( "r_novis", "0", 0 );

	vid_fullscreen = ri.Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );

	vid_gamma = ri.Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );
	ri.Cvar_SetValue( "vid_gamma", libretro_gamma );

	ri.Cmd_AddCommand ("modellist", SWR_Mod_Modellist_f);
	ri.Cmd_AddCommand( "screenshot", R_ScreenShot_f );
	ri.Cmd_AddCommand( "imagelist", R_ImageList_f );

	sw_mode->modified = true; // force us to do mode specific stuff later
	vid_gamma->modified = true; // force us to rebuild the gamma table later

//PGM
	sw_lockpvs = ri.Cvar_Get ("sw_lockpvs", "0", 0);
//PGM

   sw_texfilt = ri.Cvar_Get ("sw_texfilt", "0", 0);
}

static void SWR_UnRegister (void)
{
	ri.Cmd_RemoveCommand( "screenshot" );
	ri.Cmd_RemoveCommand ("modellist");
	ri.Cmd_RemoveCommand( "imagelist" );
}

/*
===============
R_Init
===============
*/
qboolean SWR_Init( void *hInstance, void *wndProc )
{
	R_InitImages ();
	SWR_Mod_Init ();
	SWR_Draw_InitLocal ();
	R_InitTextures ();

	R_InitTurb ();

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
			view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
			view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

	r_aliasuvscale = 1.0;

	SWR_Register ();
	Draw_GetPalette ();
	SWimp_Init( hInstance, wndProc );

	// create the window
	SWR_BeginFrame( 0 );

	ri.Con_Printf (PRINT_ALL, "ref_soft version: "REF_VERSION"\n");

	return true;
}

/*
===============
SWR_Shutdown
===============
*/
static void SWR_Shutdown (void)
{
	/* free z buffer */
	if (d_pzbuffer)
	{
		free (d_pzbuffer);
		d_pzbuffer = NULL;
	}
	/* free surface cache */
	if (sc_base)
	{
		D_FlushCaches ();
		free (sc_base);
		sc_base = NULL;
	}

	/* free colormap */
	if (vid.colormap)
	{
		free (vid.colormap);
		vid.colormap = NULL;
	}
   R_UninitTurb ();
	SWR_UnRegister ();
	SWR_Mod_FreeAll ();
	R_ShutdownImages ();

	SWimp_Shutdown();
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	r_refsoft_viewcluster = -1;

	r_cnumsurfs = sw_maxsurfs->value;

	if (r_cnumsurfs <= MINSURFACES)
		r_cnumsurfs = MINSURFACES;

	if (r_cnumsurfs > NUMSTACKSURFACES)
	{
		surfaces = malloc (r_cnumsurfs * sizeof(surf_t));
		surface_p = surfaces;
		surf_max = &surfaces[r_cnumsurfs];
		r_surfsonstack = false;
      /* surface 0 doesn't really exist; it's just a dummy because index 0
       * is used to indicate no edge attached to surface */
		surfaces--;
		SWR_SurfacePatch ();
	}
	else
	{
		r_surfsonstack = true;
	}

	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;

	r_numallocatededges = sw_maxedges->value;

	if (r_numallocatededges < MINEDGES)
		r_numallocatededges = MINEDGES;

	if (r_numallocatededges <= NUMSTACKEDGES)
	{
		auxedges = NULL;
	}
	else
	{
		auxedges = malloc (r_numallocatededges * sizeof(edge_t));
	}
}


/*
===============
SWR_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
static void SWR_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int		i;
	mleaf_t	*leaf;
	int		cluster;

	if (r_refsoft_oldviewcluster == r_refsoft_viewcluster && !r_novis->value && r_refsoft_viewcluster != -1)
		return;
	
	// development aid to let you run around and see exactly where
	// the pvs ends
	if (sw_lockpvs->value)
		return;

	r_visframecount++;
	r_refsoft_oldviewcluster = r_refsoft_viewcluster;

	if (r_novis->value || r_refsoft_viewcluster == -1 || !r_refsoft_worldmodel->vis)
	{
		// mark everything
		for (i=0 ; i<r_refsoft_worldmodel->numleafs ; i++)
			r_refsoft_worldmodel->leafs[i].visframe = r_visframecount;
		for (i=0 ; i<r_refsoft_worldmodel->numnodes ; i++)
			r_refsoft_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = SWR_Mod_ClusterPVS (r_refsoft_viewcluster, r_refsoft_worldmodel);
	
	for (i=0,leaf=r_refsoft_worldmodel->leafs ; i<r_refsoft_worldmodel->numleafs ; i++, leaf++)
	{
		cluster = leaf->cluster;
		if (cluster == -1)
			continue;
		if (vis[cluster>>3] & (1<<(cluster&7)))
		{
			node = (mnode_t *)leaf;
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}

#if 0
	for (i=0 ; i<r_refsoft_worldmodel->vis->numclusters ; i++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			node = (mnode_t *)&r_refsoft_worldmodel->leafs[i];	// FIXME: cluster
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
#endif
}

/*
** SWR_DrawNullModel
**
** IMPLEMENT THIS!
*/
static void SWR_DrawNullModel( void )
{
}

/*
=============
SWR_DrawEntitiesOnList
=============
*/
static void SWR_DrawEntitiesOnList (void)
{
	int			i;
	qboolean	translucent_entities = false;

	if (!r_drawentities->value)
		return;

	// all bmodels have already been drawn by the edge list
	for (i=0 ; i<r_refsoft_newrefdef.num_entities ; i++)
	{
		refsoft_currententity = &r_refsoft_newrefdef.entities[i];

		if ( refsoft_currententity->flags & RF_TRANSLUCENT )
		{
			translucent_entities = true;
			continue;
		}

		if ( refsoft_currententity->flags & RF_BEAM )
		{
			modelorg[0] = -r_refsoft_origin[0];
			modelorg[1] = -r_refsoft_origin[1];
			modelorg[2] = -r_refsoft_origin[2];
			VectorCopy( vec3_origin, r_entorigin );
			R_DrawBeam( refsoft_currententity );
		}
		else
		{
			refsoft_currentmodel = refsoft_currententity->model;
			if (!refsoft_currentmodel)
			{
				SWR_DrawNullModel();
				continue;
			}
			VectorCopy (refsoft_currententity->origin, r_entorigin);
			VectorSubtract (r_refsoft_origin, r_entorigin, modelorg);

			switch (refsoft_currentmodel->type)
			{
			case mod_sprite:
				R_DrawSprite ();
				break;

			case mod_alias:
				R_AliasDrawModel ();
				break;

			default:
				break;
			}
		}
	}

	if ( !translucent_entities )
		return;

	for (i=0 ; i<r_refsoft_newrefdef.num_entities ; i++)
	{
		refsoft_currententity = &r_refsoft_newrefdef.entities[i];

		if ( !( refsoft_currententity->flags & RF_TRANSLUCENT ) )
			continue;

		if ( refsoft_currententity->flags & RF_BEAM )
		{
			modelorg[0] = -r_refsoft_origin[0];
			modelorg[1] = -r_refsoft_origin[1];
			modelorg[2] = -r_refsoft_origin[2];
			VectorCopy( vec3_origin, r_entorigin );
			R_DrawBeam( refsoft_currententity );
		}
		else
		{
			refsoft_currentmodel = refsoft_currententity->model;
			if (!refsoft_currentmodel)
			{
				SWR_DrawNullModel();
				continue;
			}
			VectorCopy (refsoft_currententity->origin, r_entorigin);
			VectorSubtract (r_refsoft_origin, r_entorigin, modelorg);

			switch (refsoft_currentmodel->type)
			{
			case mod_sprite:
				R_DrawSprite ();
				break;

			case mod_alias:
				R_AliasDrawModel ();
				break;

			default:
				break;
			}
		}
	}
}


/*
=============
R_BmodelCheckBBox
=============
*/
int R_BmodelCheckBBox (float *minmaxs)
{
	int			i, *pindex, clipflags;
	vec3_t		acceptpt, rejectpt;
	float		d;

	clipflags = 0;

	for (i=0 ; i<4 ; i++)
	{
	// generate accept and reject points
	// FIXME: do with fast look-ups or integer tests based on the sign bit
	// of the floating point values

		pindex = pfrustum_indexes[i];

		rejectpt[0] = minmaxs[pindex[0]];
		rejectpt[1] = minmaxs[pindex[1]];
		rejectpt[2] = minmaxs[pindex[2]];
		
		d = DotProduct (rejectpt, view_clipplanes[i].normal);
		d -= view_clipplanes[i].dist;

		if (d <= 0)
			return BMODEL_FULLY_CLIPPED;

		acceptpt[0] = minmaxs[pindex[3+0]];
		acceptpt[1] = minmaxs[pindex[3+1]];
		acceptpt[2] = minmaxs[pindex[3+2]];

		d = DotProduct (acceptpt, view_clipplanes[i].normal);
		d -= view_clipplanes[i].dist;

		if (d <= 0)
			clipflags |= (1<<i);
	}

	return clipflags;
}


/*
===================
R_FindTopnode

Find the first node that splits the given box
===================
*/
mnode_t *R_FindTopnode (vec3_t mins, vec3_t maxs)
{
	mplane_t	*splitplane;
	int			sides;
	mnode_t *node;

	node = r_refsoft_worldmodel->nodes;

	while (1)
	{
		if (node->visframe != r_visframecount)
			return NULL;		// not visible at all
		
		if (node->contents != CONTENTS_NODE)
		{
			if (node->contents != CONTENTS_SOLID)
				return	node; // we've reached a non-solid leaf, so it's
							//  visible and not BSP clipped
			return NULL;	// in solid, so not visible
		}
		
		splitplane = node->plane;
		sides = BOX_ON_PLANE_SIDE(mins, maxs, (cplane_t *)splitplane);
		
		if (sides == 3)
			return node;	// this is the splitter
		
	// not split yet; recurse down the contacted side
		if (sides & 1)
			node = node->children[0];
		else
			node = node->children[1];
	}
}


/*
=============
RotatedBBox

Returns an axially aligned box that contains the input box at the given rotation
=============
*/
void RotatedBBox (vec3_t mins, vec3_t maxs, vec3_t angles, vec3_t tmins, vec3_t tmaxs)
{
	vec3_t	tmp, v;
	int		i, j;
	vec3_t	forward, right, up;

	if (!angles[0] && !angles[1] && !angles[2])
	{
		VectorCopy (mins, tmins);
		VectorCopy (maxs, tmaxs);
		return;
	}

	for (i=0 ; i<3 ; i++)
	{
		tmins[i] = 99999;
		tmaxs[i] = -99999;
	}

	AngleVectors (angles, forward, right, up);

	for ( i = 0; i < 8; i++ )
	{
		if ( i & 1 )
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if ( i & 2 )
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if ( i & 4 )
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];


		VectorScale (forward, tmp[0], v);
		VectorMA (v, -tmp[1], right, v);
		VectorMA (v, tmp[2], up, v);

		for (j=0 ; j<3 ; j++)
		{
			if (v[j] < tmins[j])
				tmins[j] = v[j];
			if (v[j] > tmaxs[j])
				tmaxs[j] = v[j];
		}
	}
}

/*
=============
R_DrawBEntitiesOnList
=============
*/
void R_DrawBEntitiesOnList (void)
{
	int			i, clipflags;
	vec3_t		oldorigin;
	vec3_t		mins, maxs;
	float		minmaxs[6];
	mnode_t		*topnode;

	if (!r_drawentities->value)
		return;

	VectorCopy (modelorg, oldorigin);
	insubmodel = true;
	r_refsoft_dlightframecount = r_framecount;

	for (i=0 ; i<r_refsoft_newrefdef.num_entities ; i++)
	{
		refsoft_currententity = &r_refsoft_newrefdef.entities[i];
		refsoft_currentmodel = refsoft_currententity->model;
		if (!refsoft_currentmodel)
			continue;
		if (refsoft_currentmodel->nummodelsurfaces == 0)
			continue;	// clip brush only
		if ( refsoft_currententity->flags & RF_BEAM )
			continue;
		if (refsoft_currentmodel->type != mod_brush)
			continue;
	// see if the bounding box lets us trivially reject, also sets
	// trivial accept status
		RotatedBBox (refsoft_currentmodel->mins, refsoft_currentmodel->maxs,
			refsoft_currententity->angles, mins, maxs);
		VectorAdd (mins, refsoft_currententity->origin, minmaxs);
		VectorAdd (maxs, refsoft_currententity->origin, (minmaxs+3));

		clipflags = R_BmodelCheckBBox (minmaxs);
		if (clipflags == BMODEL_FULLY_CLIPPED)
			continue;	// off the edge of the screen

		topnode = R_FindTopnode (minmaxs, minmaxs+3);
		if (!topnode)
			continue;	// no part in a visible leaf

		VectorCopy (refsoft_currententity->origin, r_entorigin);
		VectorSubtract (r_refsoft_origin, r_entorigin, modelorg);

		r_pcurrentvertbase = refsoft_currentmodel->vertexes;

      /* FIXME: stop transforming twice */
		R_RotateBmodel ();

      /* calculate dynamic lighting for bmodel */
		SWR_PushDlights (refsoft_currentmodel);

		if (topnode->contents == CONTENTS_NODE)
		{
		// not a leaf; has to be clipped to the world BSP
			r_clipflags = clipflags;
			R_DrawSolidClippedSubmodelPolygons (refsoft_currentmodel, topnode);
		}
		else
		{
		// falls entirely in one leaf, so we just put all the
		// edges in the edge list and let 1/z sorting handle
		// drawing order
			R_DrawSubmodelPolygons (refsoft_currentmodel, clipflags, topnode);
		}

	// put back world rotation and frustum clipping		
	// FIXME: R_RotateBmodel should just work off base_vxx
		VectorCopy (base_vpn, refsoft_vpn);
		VectorCopy (base_vup, vup);
		VectorCopy (base_vright, vright);
		VectorCopy (oldorigin, modelorg);
		R_TransformFrustum ();
	}

	insubmodel = false;
}


/*
================
R_EdgeDrawing
================
*/
void R_EdgeDrawing (void)
{
	edge_t	ledges[NUMSTACKEDGES +
				((CACHE_SIZE - 1) / sizeof(edge_t)) + 1];
	surf_t	lsurfs[NUMSTACKSURFACES +
				((CACHE_SIZE - 1) / sizeof(surf_t)) + 1];

	if ( r_refsoft_newrefdef.rdflags & RDF_NOWORLDMODEL )
		return;

	if (auxedges)
	{
		r_edges = auxedges;
	}
	else
	{
		r_edges =  (edge_t *)
				(((uintptr_t)&ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	}

	if (r_surfsonstack)
	{
		surfaces =  (surf_t *)
				(((uintptr_t)&lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
		surf_max = &surfaces[r_cnumsurfs];
      /* surface 0 doesn't really exist; it's just a dummy because index 0
       * is used to indicate no edge attached to surface */
		surfaces--;
		SWR_SurfacePatch ();
	}

	R_BeginEdgeFrame ();

	if (r_dspeeds->value)
	{
		rw_time1 = Sys_Milliseconds ();
	}

	R_RenderWorld ();

	if (r_dspeeds->value)
	{
		rw_time2 = Sys_Milliseconds ();
		db_time1 = rw_time2;
	}

	R_DrawBEntitiesOnList ();

	if (r_dspeeds->value)
	{
		db_time2 = Sys_Milliseconds ();
		se_time1 = db_time2;
	}

	R_ScanEdges ();
}

//=======================================================================


/*
=============
R_CalcPalette

=============
*/
void R_CalcPalette (void)
{
	static qboolean modified;
	byte	palette[256][4], *in, *out;
	int		i, j;
	float	alpha, one_minus_alpha;
	vec3_t	premult;
	int		v;

	alpha = r_refsoft_newrefdef.blend[3];
	if (alpha <= 0)
	{
		if (modified)
		{	// set back to default
			modified = false;
			R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_refsoft_8to24table );
			return;
		}
		return;
	}

	modified = true;
	if (alpha > 1)
		alpha = 1;

	premult[0] = r_refsoft_newrefdef.blend[0]*alpha*255;
	premult[1] = r_refsoft_newrefdef.blend[1]*alpha*255;
	premult[2] = r_refsoft_newrefdef.blend[2]*alpha*255;

	one_minus_alpha = (1.0 - alpha);

	in = (byte *)d_refsoft_8to24table;
	out = palette[0];
	for (i=0 ; i<256 ; i++, in+=4, out+=4)
	{
		for (j=0 ; j<3 ; j++)
		{
			v = premult[j] + one_minus_alpha * in[j];
			if (v > 255)
				v = 255;
			out[j] = v;
		}
		out[3] = 255;
	}

	R_GammaCorrectAndSetPalette( ( const unsigned char * ) palette[0] );
//	SWimp_SetPalette( palette[0] );
}

//=======================================================================

static void SWR_SetLightLevel (void)
{
	vec3_t		light;

	if ((r_refsoft_newrefdef.rdflags & RDF_NOWORLDMODEL) || (!r_drawentities->value) || (!refsoft_currententity))
	{
		r_refsoft_lightlevel->value = 150.0;
		return;
	}

	/* save off light value for server to look at (BIG HACK!) */
	SWR_LightPoint (r_refsoft_newrefdef.vieworg, light);
	r_refsoft_lightlevel->value = 150.0 * light[0];
}


/*
@@@@@@@@@@@@@@@@
SWR_RenderFrame

@@@@@@@@@@@@@@@@
*/
static void SWR_RenderFrame (refdef_t *fd)
{
	r_refsoft_newrefdef = *fd;

	if (!r_refsoft_worldmodel && !( r_refsoft_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		ri.Sys_Error (ERR_FATAL,"R_RenderView: NULL worldmodel");

	VectorCopy (fd->vieworg, r_refdef.vieworg);
	VectorCopy (fd->viewangles, r_refdef.viewangles);

	if (r_speeds->value || r_dspeeds->value)
		r_time1 = Sys_Milliseconds ();

	SWR_SetupFrame ();

	SWR_MarkLeaves ();	// done here so we know if we're in water

	SWR_PushDlights (r_refsoft_worldmodel);

	R_EdgeDrawing ();

	if (r_dspeeds->value)
	{
		se_time2 = Sys_Milliseconds ();
		de_time1 = se_time2;
	}

	SWR_DrawEntitiesOnList ();

	if (r_dspeeds->value)
	{
		de_time2 = Sys_Milliseconds ();
		dp_time1 = Sys_Milliseconds ();
	}

	SWR_DrawParticles ();

	if (r_dspeeds->value)
		dp_time2 = Sys_Milliseconds ();

	SWR_DrawAlphaSurfaces();

	SWR_SetLightLevel ();

	if (r_dowarp)
		D_WarpScreen ();

	if (r_dspeeds->value)
		da_time1 = Sys_Milliseconds ();

	if (r_dspeeds->value)
		da_time2 = Sys_Milliseconds ();

	R_CalcPalette ();

	if (sw_aliasstats->value)
		R_PrintAliasStats ();
		
	if (r_speeds->value)
		R_PrintTimes ();

	if (r_dspeeds->value)
		R_PrintDSpeeds ();

	if (sw_reportsurfout->value && r_outofsurfaces)
		ri.Con_Printf (PRINT_ALL,"Short %d surfaces\n", r_outofsurfaces);

	if (sw_reportedgeout->value && r_outofedges)
		ri.Con_Printf (PRINT_ALL,"Short roughly %d edges\n", r_outofedges * 2 / 3);
}

/*
** R_InitGraphics
*/
void R_InitGraphics( int width, int height )
{
	vid.width  = width;
	vid.height = height;

	// free z buffer
	if ( d_pzbuffer )
	{
		free( d_pzbuffer );
		d_pzbuffer = NULL;
	}

	// free surface cache
	if ( sc_base )
	{
		D_FlushCaches ();
		free( sc_base );
		sc_base = NULL;
	}

	d_pzbuffer = malloc(vid.width*vid.height*2);

	R_InitCaches ();

	R_GammaCorrectAndSetPalette( ( const unsigned char *) d_refsoft_8to24table );
}

/*
** R_BeginFrame
*/
static void SWR_BeginFrame( float camera_separation )
{
	extern void Draw_BuildGammaTable( void );

	/*
	** rebuild the gamma correction palette if necessary
	*/
	if ( vid_gamma->modified )
	{
		Draw_BuildGammaTable();
		R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_refsoft_8to24table );

		vid_gamma->modified = false;
	}

	while ( sw_mode->modified || vid_fullscreen->modified )
	{
		rserr_t err;

		/*
		** if this returns rserr_invalid_fullscreen then it set the mode but not as a
		** fullscreen mode, e.g. 320x200 on a system that doesn't support that res
		*/
		if ( ( err = SWimp_SetMode( &vid.width, &vid.height, sw_mode->value, vid_fullscreen->value ) ) == rserr_ok )
		{
			R_InitGraphics( vid.width, vid.height );

			sw_state.prev_mode = sw_mode->value;
			vid_fullscreen->modified = false;
			sw_mode->modified = false;
		}
		else
		{
			if ( err == rserr_invalid_mode )
			{
				ri.Cvar_SetValue( "sw_mode", sw_state.prev_mode );
				ri.Con_Printf( PRINT_ALL, "ref_soft::R_BeginFrame() - could not set mode\n" );
			}
			else if ( err == rserr_invalid_fullscreen )
			{
				R_InitGraphics( vid.width, vid.height );

				ri.Cvar_SetValue( "vid_fullscreen", 0);
				ri.Con_Printf( PRINT_ALL, "ref_soft::R_BeginFrame() - fullscreen unavailable in this mode\n" );
				sw_state.prev_mode = sw_mode->value;
//				vid_fullscreen->modified = false;
//				sw_mode->modified = false;
			}
			else
			{
				ri.Sys_Error( ERR_FATAL, "ref_soft::R_BeginFrame() - catastrophic mode change failure\n" );
			}
		}
      R_InitTurb ();
	}
}

/*
** R_GammaCorrectAndSetPalette
*/
void R_GammaCorrectAndSetPalette( const unsigned char *palette )
{
	int i;

	for ( i = 0; i < 256; i++ )
	{
		sw_state.currentpalette[i*4+0] = sw_state.gammatable[palette[i*4+0]];
		sw_state.currentpalette[i*4+1] = sw_state.gammatable[palette[i*4+1]];
		sw_state.currentpalette[i*4+2] = sw_state.gammatable[palette[i*4+2]];
	}

	SWimp_SetPalette( sw_state.currentpalette );
}

/*
** SWR_CinematicSetPalette
*/
static void SWR_CinematicSetPalette( const unsigned char *palette )
{
	byte palette32[1024];
	int		i, j, w;
	int		*d;

	// clear screen to black to avoid any palette flash
	memset(vid.buffer, 0, vid.height * vid.rowbytes);

	// flush it to the screen
	SWimp_EndFrame ();

	if ( palette )
	{
		for ( i = 0; i < 256; i++ )
		{
			palette32[i*4+0] = palette[i*3+0];
			palette32[i*4+1] = palette[i*3+1];
			palette32[i*4+2] = palette[i*3+2];
			palette32[i*4+3] = 0xFF;
		}

		R_GammaCorrectAndSetPalette( palette32 );
	}
	else
	{
		R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_refsoft_8to24table );
	}
}

/*
================
Draw_BuildGammaTable
================
*/
void Draw_BuildGammaTable (void)
{
	int		i, inf;
	float	g;

	g = vid_gamma->value;

	if (g == 1.0)
	{
		for (i=0 ; i<256 ; i++)
			sw_state.gammatable[i] = i;
		return;
	}
	
	for (i=0 ; i<256 ; i++)
	{
		inf = 255 * pow ( (i+0.5)/255.5 , g ) + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		sw_state.gammatable[i] = inf;
	}
}

/*
** R_DrawBeam
*/
static void R_DrawBeam( entity_t *e )
{
#define NUM_BEAM_SEGS 6

	int	i;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, e->frame / 2, perpvec );

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		R_IMFlatShadedQuad( start_points[i],
		                    end_points[i],
							end_points[(i+1)%NUM_BEAM_SEGS],
							start_points[(i+1)%NUM_BEAM_SEGS],
							e->skinnum & 0xFF,
							e->alpha );
	}
}


//===================================================================

/*
============
R_SetSky
============
*/
// 3dstudio environment map names
extern char *suf[6];
int	r_skysideimage[6] = {5, 2, 4, 1, 0, 3};
extern	mtexinfo_t		r_skytexinfo[6];

static void SWR_SetSky (char *name, float rotate, vec3_t axis)
{
	int		i;
	char	pathname[MAX_QPATH];

	strncpy (skyname, name, sizeof(skyname)-1);
	skyrotate = rotate;
	VectorCopy (axis, skyaxis);

	for (i=0 ; i<6 ; i++)
	{
		Com_sprintf (pathname, sizeof(pathname), "env/%s%s.pcx", skyname, suf[r_skysideimage[i]]);
		r_skytexinfo[i].image = R_FindImage (pathname, it_sky);
	}
}


/*
===============
Draw_GetPalette
===============
*/
static void Draw_GetPalette (void)
{
	byte	*pal, *out;
	int		i;
	int		r, g, b;
	int		width, height;

	// get the palette and colormap
	LoadPCX ("pics/colormap.pcx", &vid.colormap, &pal, &width, &height);
	if (!vid.colormap || ((width * height) < (256 * VID_GRADES) + (256 * 256)))
		ri.Sys_Error (ERR_FATAL, "Couldn't load pics/colormap.pcx");

	vid.alphamap = vid.colormap + (256 * VID_GRADES);

	out = (byte *)d_refsoft_8to24table;
	for (i=0 ; i<256 ; i++, out+=4)
	{
		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];

        out[0] = r;
        out[1] = g;
        out[2] = b;
	}

	free (pal);
}

struct image_s *SWR_RegisterSkin (char *name);

/*
@@@@@@@@@@@@@@@@@@@@@
SWR_GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
refexport_t SWR_GetRefAPI (refimport_t rimp)
{
   refexport_t	re;

   ri = rimp;

   re.api_version = API_VERSION;

   re.BeginRegistration = SWR_BeginRegistration;
   re.RegisterModel     = SWR_RegisterModel;
   re.RegisterSkin      = SWR_RegisterSkin;
   re.RegisterPic       = SWR_Draw_FindPic;
   re.SetSky            = SWR_SetSky;
   re.EndRegistration   = SWR_EndRegistration;

   re.RenderFrame       = SWR_RenderFrame;

   re.DrawGetPicSize    = SWR_Draw_GetPicSize;
   re.DrawPic           = SWR_Draw_Pic;
   re.DrawStretchPic    = SWR_Draw_StretchPic;
   re.DrawChar          = SWR_Draw_Char;
   re.DrawTileClear     = SWR_Draw_TileClear;
   re.DrawFill          = SWR_Draw_Fill;
   re.DrawFadeScreen    = SWR_Draw_FadeScreen;

   re.DrawStretchRaw      = SWR_Draw_StretchRaw;

   re.Init                = SWR_Init;
   re.Shutdown            = SWR_Shutdown;

   re.CinematicSetPalette = SWR_CinematicSetPalette;
   re.BeginFrame          = SWR_BeginFrame;
   re.EndFrame            = SWimp_EndFrame;

   re.AppActivate         = SWimp_AppActivate;

   Swap_Init ();

   return re;
}

#ifndef REF_HARD_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	ri.Sys_Error (ERR_FATAL, "%s", text);
}

void Com_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	ri.Con_Printf (PRINT_ALL, "%s", text);
}

#endif
