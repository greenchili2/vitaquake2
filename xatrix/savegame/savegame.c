/*
 * =======================================================================
 *
 * The savegame system.
 *
 * =======================================================================
 */

/*
 * This is the Quake 2 savegame system, fixed by Yamagi
 * based on an idea by Knightmare of kmquake2. This major
 * rewrite of the original g_save.c is much more robust
 * and portable since it doesn't use any function pointers.
 *
 * Inner workings:
 * When the game is saved all function pointers are
 * translated into human readable function definition strings.
 * The same way all mmove_t pointers are translated. This
 * human readable strings are then written into the file.
 * At game load the human readable strings are retranslated
 * into the actual function pointers and struct pointers. The
 * pointers are generated at each compilation / start of the
 * client, thus the pointers are always correct.
 *
 * Limitations:
 * While savegames survive recompilations of the game source
 * and bigger changes in the source, there are some limitation
 * which a nearly impossible to fix without a object orientated
 * rewrite of the game.
 *  - If functions or mmove_t structs that a referencenced
 *    inside savegames are added or removed (e.g. the files
 *    in tables/ are altered) the load functions cannot
 *    reconnect all pointers and thus not restore the game.
 *  - If the operating system is changed internal structures
 *    may change in an unrepairable way.
 *  - If the architecture is changed pointer length and
 *    other internal datastructures change in an
 *    incompatible way.
 *  - If the edict_t struct is changed, savegames
 *    will break.
 * This is not so bad as it looks since functions and
 * struct won't be added and edict_t won't be changed
 * if no big, sweeping changes are done. The operating
 * system and architecture are in the hands of the user.
 */

#include <libretro_file.h>

#include "../header/local.h"

/*
* When ever the savegame version is changed, q2 will refuse to
* load older savegames. This should be bumped if the files
* in tables/ are changed, otherwise strange things may happen.
*/
#define SAVEGAMEVER "YQ2-3"

#define YQ2OSTYPE "libretro"
#define YQ2ARCH "unknown"

/*
 * This macros are used to prohibit loading of savegames
 * created on other systems or architectures. This will
 * crash q2 in spectacular ways
 */
#ifndef YQ2OSTYPE
#error YQ2OSTYPE should be defined by the build system
#endif

#ifndef YQ2ARCH
#error YQ2ARCH should be defined by the build system
#endif

/*
 * Older operating system and architecture detection
 * macros, implemented by savegame version YQ2-1.
 */
#if defined(__APPLE__)
#define OSTYPE_1 "MacOS X"
#elif defined(__FreeBSD__)
#define OSTYPE_1 "FreeBSD"
#elif defined(__OpenBSD__)
#define OSTYPE_1 "OpenBSD"
#elif defined(__linux__)
 #define OSTYPE_1 "Linux"
#elif defined(_WIN32)
 #define OSTYPE_1 "Windows"
#else
 #define OSTYPE_1 "Unknown"
#endif

#if defined(__i386__)
#define ARCH_1 "i386"
#elif defined(__x86_64__)
#define ARCH_1 "amd64"
#elif defined(__sparc__)
#define ARCH_1 "sparc64"
#elif defined(__ia64__)
 #define ARCH_1 "ia64"
#else
 #define ARCH_1 "unknown"
#endif

/*
 * Connects a human readable
 * function signature with
 * the corresponding pointer
 */
typedef struct
{
	char *funcStr;
	byte *funcPtr;
} functionList_t;

/*
 * Connects a human readable
 * mmove_t string with the
 * correspondig pointer
 * */
typedef struct
{
	char	*mmoveStr;
	mmove_t *mmovePtr;
} mmoveList_t;

/* ========================================================= */

/*
 * Prototypes for forward
 * declaration for all game
 * functions.
 */
#include "tables/gamefunc_decs.h"

/*
 * List with function pointer
 * to each of the functions
 * prototyped above.
 */
functionList_t functionList[] = {
	#include "tables/gamefunc_list.h"
};

/*
 * Prtotypes for forward
 * declaration for all game
 * mmove_t functions.
 */
#include "tables/gamemmove_decs.h"

/*
 * List with pointers to
 * each of the mmove_t
 * functions prototyped
 * above.
 */
mmoveList_t mmoveList[] = {
	#include "tables/gamemmove_list.h"
};

/*
 * Fields to be saved
 */
field_t fields[] = {
	#include "tables/fields.h"
};

/*
 * Level fields to
 * be saved
 */
field_t levelfields[] = {
	#include "tables/levelfields.h"
};

/*
 * Client fields to
 * be saved
 */
field_t clientfields[] = {
	#include "tables/clientfields.h"
};

/* ========================================================= */

/*
 * This will be called when the dll is first loaded,
 * which only happens when a new game is started or
 * a save game is loaded.
 */
void
InitGame(void)
{
	gi.dprintf("Game is starting up.\n");
	gi.dprintf("Game is %s built on %s.\n", GAMEVERSION, __DATE__);

	gun_x = gi.cvar ("gun_x", "0", 0);
	gun_y = gi.cvar ("gun_y", "0", 0);
	gun_z = gi.cvar ("gun_z", "0", 0);
	sv_rollspeed = gi.cvar ("sv_rollspeed", "200", 0);
	sv_rollangle = gi.cvar ("sv_rollangle", "2", 0);
	sv_maxvelocity = gi.cvar ("sv_maxvelocity", "2000", 0);
	sv_gravity = gi.cvar ("sv_gravity", "800", 0);

	/* noset vars */
	dedicated = gi.cvar ("dedicated", "0", CVAR_NOSET);

	/* latched vars */
	sv_cheats = gi.cvar ("cheats", "0", CVAR_SERVERINFO|CVAR_LATCH);
	gi.cvar ("gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_LATCH);
	gi.cvar ("gamedate", __DATE__ , CVAR_SERVERINFO | CVAR_LATCH);
	maxclients = gi.cvar ("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	maxspectators = gi.cvar ("maxspectators", "4", CVAR_SERVERINFO);
	deathmatch = gi.cvar ("deathmatch", "0", CVAR_LATCH);
	coop = gi.cvar ("coop", "0", CVAR_LATCH);
	skill = gi.cvar ("skill", "1", CVAR_LATCH);
	maxentities = gi.cvar ("maxentities", "1024", CVAR_LATCH);

	/* change anytime vars */
	dmflags = gi.cvar ("dmflags", "0", CVAR_SERVERINFO);
	fraglimit = gi.cvar ("fraglimit", "0", CVAR_SERVERINFO);
	timelimit = gi.cvar ("timelimit", "0", CVAR_SERVERINFO);
	password = gi.cvar ("password", "", CVAR_USERINFO);
	spectator_password = gi.cvar ("spectator_password", "", CVAR_USERINFO);
	needpass = gi.cvar ("needpass", "0", CVAR_SERVERINFO);
	filterban = gi.cvar ("filterban", "1", 0);
	g_select_empty = gi.cvar ("g_select_empty", "0", CVAR_ARCHIVE);
	run_pitch = gi.cvar ("run_pitch", "0.002", 0);
	run_roll = gi.cvar ("run_roll", "0.005", 0);
	bob_up  = gi.cvar ("bob_up", "0.005", 0);
	bob_pitch = gi.cvar ("bob_pitch", "0.002", 0);
	bob_roll = gi.cvar ("bob_roll", "0.002", 0);

	/* flood control */
	flood_msgs = gi.cvar ("flood_msgs", "4", 0);
	flood_persecond = gi.cvar ("flood_persecond", "4", 0);
	flood_waitdelay = gi.cvar ("flood_waitdelay", "10", 0);

	/* dm map list */
	sv_maplist = gi.cvar ("sv_maplist", "", 0);

	/* items */
	InitItems ();

	game.helpmessage1[0] = 0;
	game.helpmessage2[0] = 0;

	/* initialize all entities for this game */
	game.maxentities = maxentities->value;
	g_edicts =  gi.TagMalloc (game.maxentities * sizeof(g_edicts[0]), TAG_GAME);
	globals.edicts = g_edicts;
	globals.max_edicts = game.maxentities;

	/* initialize all clients for this game */
	game.maxclients = maxclients->value;
	game.clients = gi.TagMalloc (game.maxclients * sizeof(game.clients[0]), TAG_GAME);
	globals.num_edicts = game.maxclients+1;
}

/* ========================================================= */

/*
 * Helper function to get
 * the human readable function
 * definition by an address.
 * Called by WriteField1 and
 * WriteField2.
 */
functionList_t *
GetFunctionByAddress(byte *adr)
{
	int i;

	for (i = 0; functionList[i].funcStr; i++)
	{
		if (functionList[i].funcPtr == adr)
		{
			return &functionList[i];
		}
	}

	return NULL;
}

/*
 * Helper function to get the
 * pointer to a function by
 * it's human readable name.
 * Called by WriteField1 and
 * WriteField2.
 */
byte *
FindFunctionByName(char *name)
{
	int i;

	for (i = 0; functionList[i].funcStr; i++)
	{
		if (!strcmp(name, functionList[i].funcStr))
		{
			return functionList[i].funcPtr;
		}
	}

	return NULL;
}

/*
 * Helper function to get the
 * human readable definition of
 * a mmove_t struct by a pointer.
 */
mmoveList_t *
GetMmoveByAddress(mmove_t *adr)
{
	int i;

	for (i = 0; mmoveList[i].mmoveStr; i++)
	{
		if (mmoveList[i].mmovePtr == adr)
		{
			return &mmoveList[i];
		}
	}

	return NULL;
}

/*
 * Helper function to get the
 * pointer to a mmove_t struct
 * by a human readable definition.
 */
mmove_t *
FindMmoveByName(char *name)
{
	int i;

	for (i = 0; mmoveList[i].mmoveStr; i++)
	{
		if (!strcmp(name, mmoveList[i].mmoveStr))
		{
			return mmoveList[i].mmovePtr;
		}
	}

	return NULL;
}


/* ========================================================= */

/*
 * The following two functions are
 * doing the dirty work to write the
 * data generated by the functions
 * below this block into files.
 */
void
WriteField1(RFILE *f, field_t *field, byte *base)
{
	void *p;
	int len;
	int index;
	functionList_t *func;
	mmoveList_t *mmove;

	if (field->flags & FFL_SPAWNTEMP)
	{
		return;
	}

	p = (void *)(base + field->ofs);

	switch (field->type)
	{
		case F_INT:
		case F_FLOAT:
		case F_ANGLEHACK:
		case F_VECTOR:
		case F_IGNORE:
			break;

		case F_LSTRING:
		case F_GSTRING:

			if (*(char **)p)
			{
				len = strlen(*(char **)p) + 1;
			}
			else
			{
				len = 0;
			}

			*(int *)p = len;
			break;
		case F_EDICT:

			if (*(edict_t **)p == NULL)
			{
				index = -1;
			}
			else
			{
				index = *(edict_t **)p - g_edicts;
			}

			*(int *)p = index;
			break;
		case F_CLIENT:

			if (*(gclient_t **)p == NULL)
			{
				index = -1;
			}
			else
			{
				index = *(gclient_t **)p - game.clients;
			}

			*(int *)p = index;
			break;
		case F_ITEM:

			if (*(edict_t **)p == NULL)
			{
				index = -1;
			}
			else
			{
				index = *(gitem_t **)p - itemlist;
			}

			*(int *)p = index;
			break;
		case F_FUNCTION:

			if (*(byte **)p == NULL)
			{
				len = 0;
			}
			else
			{
				func = GetFunctionByAddress (*(byte **)p);

				if (!func)
				{
					gi.error ("WriteField1: function not in list, can't save game");
				}

				len = strlen(func->funcStr)+1;
			}

			*(int *)p = len;
			break;
		case F_MMOVE:

			if (*(byte **)p == NULL)
			{
				len = 0;
			}
			else
			{
				mmove = GetMmoveByAddress (*(mmove_t **)p);

				if (!mmove)
				{
					gi.error ("WriteField1: mmove not in list, can't save game");
				}

				len = strlen(mmove->mmoveStr)+1;
			}

			*(int *)p = len;
			break;
		default:
			gi.error("WriteEdict: unknown field type");
	}
}

void
WriteField2(RFILE *f, field_t *field, byte *base)
{
	int len;
	void *p;
	functionList_t *func;
	mmoveList_t *mmove;

	if (field->flags & FFL_SPAWNTEMP)
	{
		return;
	}

	p = (void *)(base + field->ofs);

	switch (field->type)
	{
		case F_LSTRING:

			if (*(char **)p)
			{
				len = strlen(*(char **)p) + 1;
				rfwrite(*(char **)p, len, 1, f);
			}

			break;
		case F_FUNCTION:

			if (*(byte **)p)
			{
				func = GetFunctionByAddress (*(byte **)p);

				if (!func)
				{
					gi.error ("WriteField2: function not in list, can't save game");
				}

				len = strlen(func->funcStr)+1;
				rfwrite (func->funcStr, len, 1, f);
			}

			break;
		case F_MMOVE:

			if (*(byte **)p)
			{
				mmove = GetMmoveByAddress (*(mmove_t **)p);

				if (!mmove)
				{
					gi.error ("WriteField2: mmove not in list, can't save game");
				}

				len = strlen(mmove->mmoveStr)+1;
				rfwrite (mmove->mmoveStr, len, 1, f);
			}

			break;
		default:
			break;
	}
}

/* ========================================================= */

/*
 * This function does the dirty
 * work to read the data from a
 * file. The processing of the
 * data is done in the functions
 * below
 */
void
ReadField(RFILE *f, field_t *field, byte *base)
{
	void *p;
	int len;
	int index;
	char funcStr[2048];

	if (field->flags & FFL_SPAWNTEMP)
	{
		return;
	}

	p = (void *)(base + field->ofs);

	switch (field->type)
	{
		case F_INT:
		case F_FLOAT:
		case F_ANGLEHACK:
		case F_VECTOR:
		case F_IGNORE:
			break;

		case F_LSTRING:
			len = *(int *)p;

			if (!len)
			{
				*(char **)p = NULL;
			}
			else
			{
				*(char **)p = gi.TagMalloc(32 + len, TAG_LEVEL);
				rfread(*(char **)p, len, 1, f);
			}

			break;
		case F_EDICT:
			index = *(int *)p;

			if (index == -1)
			{
				*(edict_t **)p = NULL;
			}
			else
			{
				*(edict_t **)p = &g_edicts[index];
			}

			break;
		case F_CLIENT:
			index = *(int *)p;

			if (index == -1)
			{
				*(gclient_t **)p = NULL;
			}
			else
			{
				*(gclient_t **)p = &game.clients[index];
			}

			break;
		case F_ITEM:
			index = *(int *)p;

			if (index == -1)
			{
				*(gitem_t **)p = NULL;
			}
			else
			{
				*(gitem_t **)p = &itemlist[index];
			}

			break;
		case F_FUNCTION:
			len = *(int *)p;

			if (!len)
			{
				*(byte **)p = NULL;
			}
			else
			{
				if (len > sizeof(funcStr))
				{
					gi.error ("ReadField: function name is longer than buffer (%i chars)",
							  (int)sizeof(funcStr));
				}

				rfread (funcStr, len, 1, f);

				if ( !(*(byte **)p = FindFunctionByName (funcStr)) )
				{
					gi.error ("ReadField: function %s not found in table, can't load game", funcStr);
				}

			}
			break;
		case F_MMOVE:
			len = *(int *)p;

			if (!len)
			{
				*(byte **)p = NULL;
			}
			else
			{
				if (len > sizeof(funcStr))
				{
					gi.error ("ReadField: mmove name is longer than buffer (%i chars)",
							  (int)sizeof(funcStr));
				}

				rfread (funcStr, len, 1, f);

				if ( !(*(mmove_t **)p = FindMmoveByName (funcStr)) )
				{
					gi.error ("ReadField: mmove %s not found in table, can't load game", funcStr);
				}
			}
			break;

		default:
			gi.error("ReadEdict: unknown field type");
	}
}

/* ========================================================= */

/*
 * Write the client struct into a file.
 */
void
WriteClient(RFILE *f, gclient_t *client)
{
	field_t *field;
	gclient_t temp;

	/* all of the ints, floats, and vectors stay as they are */
	temp = *client;

	/* change the pointers to indexes */
	for (field = clientfields; field->name; field++)
	{
		WriteField1(f, field, (byte *)&temp);
	}

	/* write the block */
	rfwrite(&temp, sizeof(temp), 1, f);

	/* now write any allocated data following the edict */
	for (field = clientfields; field->name; field++)
	{
		WriteField2(f, field, (byte *)client);
	}
}

/*
 * Read the client struct from a file
 */
void
ReadClient(RFILE *f, gclient_t *client, short save_ver)
{
	field_t *field;

	rfread(client, sizeof(*client), 1, f);

	for (field = clientfields; field->name; field++)
	{
		if (field->save_ver <= save_ver)
		{
			ReadField(f, field, (byte *)client);
		}
	}

	if (save_ver < 3)
	{
		InitClientResp(client);
	}
}

/* ========================================================= */

/*
 * Writes the game struct into
 * a file. This is called when
 * ever the games goes to e new
 * level or the user saves the
 * game. Saved informations are:
 * - cross level data
 * - client states
 * - help computer info
 */
void
WriteGame(const char *filename, qboolean autosave)
{
	RFILE *f;
	int i;
	char str_ver[32];
	char str_game[32];
    char str_os[32];
	char str_arch[32];

	if (!autosave)
	{
		SaveClientData();
	}

	f = rfopen(filename, "wb");

	if (!f)
	{
		gi.error("Couldn't open %s", filename);
	}

	/* Savegame identification */
	memset(str_ver, 0, sizeof(str_ver));
	memset(str_game, 0, sizeof(str_game));
	memset(str_os, 0, sizeof(str_os));
	memset(str_arch, 0, sizeof(str_arch));

	strncpy(str_ver, SAVEGAMEVER, sizeof(str_ver) - 1);
	strncpy(str_game, GAMEVERSION, sizeof(str_game) - 1);
	strncpy(str_os, YQ2OSTYPE, sizeof(str_os) - 1);
   strncpy(str_arch, YQ2ARCH, sizeof(str_arch) - 1);

	rfwrite(str_ver, sizeof(str_ver), 1, f);
	rfwrite(str_game, sizeof(str_game), 1, f);
	rfwrite(str_os, sizeof(str_os), 1, f);
	rfwrite(str_arch, sizeof(str_arch), 1, f);

	game.autosaved = autosave;
	rfwrite(&game, sizeof(game), 1, f);
	game.autosaved = false;

	for (i = 0; i < game.maxclients; i++)
	{
		WriteClient(f, &game.clients[i]);
	}

	rfclose(f);
}

/*
 * Read the game structs from
 * a file. Called when ever a
 * savegames is loaded.
 */
void
ReadGame(const char *filename)
{
	RFILE *f;
	int i;
	char str_ver[32];
	char str_game[32];
	char str_os[32];
	char str_arch[32];
	short save_ver = 0;

	gi.FreeTags(TAG_GAME);

	f = rfopen(filename, "rb");

	if (!f)
	{
		gi.error("Couldn't open %s", filename);
	}

	/* Sanity checks */
	rfread(str_ver, sizeof(str_ver), 1, f);
	rfread(str_game, sizeof(str_game), 1, f);
	rfread(str_os, sizeof(str_os), 1, f);
	rfread(str_arch, sizeof(str_arch), 1, f);

	if (!strcmp(str_ver, SAVEGAMEVER))
	{
		save_ver = 3;

		if (strcmp(str_game, GAMEVERSION))
		{
			rfclose(f);
			gi.error("Savegame from an other game.so.\n");
		}
		else if (strcmp(str_os, YQ2OSTYPE))
		{
			rfclose(f);
			gi.error("Savegame from an other os.\n");
		}
		else if (strcmp(str_arch, YQ2ARCH))
		{
			rfclose(f);
			gi.error("Savegame from an other architecure.\n");
		}
	}
	else if (!strcmp(str_ver, "YQ2-2"))
	{
		save_ver = 2;

		if (strcmp(str_game, GAMEVERSION))
		{
			rfclose(f);
			gi.error("Savegame from an other game.so.\n");
		}
		else if (strcmp(str_os, YQ2OSTYPE))
		{
			rfclose(f);
			gi.error("Savegame from an other os.\n");
		}
		else if (strcmp(str_arch, YQ2ARCH))
		{
			rfclose(f);
			gi.error("Savegame from an other architecure.\n");
		}
	}
	else if (!strcmp(str_ver, "YQ2-1"))
	{
		save_ver = 1;

		if (strcmp(str_game, GAMEVERSION))
		{
			rfclose(f);
			gi.error("Savegame from an other game.so.\n");
		}
		else if (strcmp(str_os, OSTYPE_1))
		{
			rfclose(f);
			gi.error("Savegame from an other os.\n");
		}

		if (!strcmp(str_os, "Windows"))
		{
			/* Windows was forced to i386 */
			if (strcmp(str_arch, "i386"))
			{
				rfclose(f);
				gi.error("Savegame from an other architecure.\n");
			}
		}
		else
		{
			if (strcmp(str_arch, ARCH_1))
			{
				rfclose(f);
				gi.error("Savegame from an other architecure.\n");
			}
		}
	}
	else
	{
		rfclose(f);
		gi.error("Savegame from an incompatible version.\n");
	}

	g_edicts = gi.TagMalloc(game.maxentities * sizeof(g_edicts[0]), TAG_GAME);
	globals.edicts = g_edicts;

	rfread(&game, sizeof(game), 1, f);
	game.clients = gi.TagMalloc(game.maxclients * sizeof(game.clients[0]),
			TAG_GAME);

	for (i = 0; i < game.maxclients; i++)
	{
		ReadClient(f, &game.clients[i], save_ver);
	}

	rfclose(f);
}

/* ========================================================== */

/*
 * Helper function to write the
 * edict into a file. Called by
 * WriteLevel.
 */
void
WriteEdict(RFILE *f, edict_t *ent)
{
	field_t *field;
	edict_t temp;

	/* all of the ints, floats, and vectors stay as they are */
	temp = *ent;

	/* change the pointers to lengths or indexes */
	for (field = fields; field->name; field++)
	{
		WriteField1(f, field, (byte *)&temp);
	}

	/* write the block */
	rfwrite(&temp, sizeof(temp), 1, f);

	/* now write any allocated data following the edict */
	for (field = fields; field->name; field++)
	{
		WriteField2(f, field, (byte *)ent);
	}
}

/*
 * Helper fcuntion to write the
 * level local data into a file.
 * Called by WriteLevel.
 */
void
WriteLevelLocals(RFILE *f)
{
	field_t *field;
	level_locals_t temp;

	/* all of the ints, floats, and vectors stay as they are */
	temp = level;

	/* change the pointers to lengths or indexes */
	for (field = levelfields; field->name; field++)
	{
		WriteField1(f, field, (byte *)&temp);
	}

	/* write the block */
	rfwrite(&temp, sizeof(temp), 1, f);

	/* now write any allocated data following the edict */
	for (field = levelfields; field->name; field++)
	{
		WriteField2(f, field, (byte *)&level);
	}
}

/*
 * Writes the current level
 * into a file.
 */
void
WriteLevel(const char *filename)
{
	int i;
	edict_t *ent;
	RFILE *f;

	f = rfopen(filename, "wb");

	if (!f)
	{
		gi.error("Couldn't open %s", filename);
	}

	/* write out edict size for checking */
	i = sizeof(edict_t);
	rfwrite(&i, sizeof(i), 1, f);

	/* write out level_locals_t */
	WriteLevelLocals(f);

	/* write out all the entities */
	for (i = 0; i < globals.num_edicts; i++)
	{
		ent = &g_edicts[i];

		if (!ent->inuse)
		{
			continue;
		}

		rfwrite(&i, sizeof(i), 1, f);
		WriteEdict(f, ent);
	}

	i = -1;
	rfwrite(&i, sizeof(i), 1, f);

	rfclose(f);
}

/* ========================================================== */

/*
 * A helper function to
 * read the edict back
 * into the memory. Called
 * by ReadLevel.
 */
void
ReadEdict(RFILE *f, edict_t *ent)
{
	field_t *field;

	rfread(ent, sizeof(*ent), 1, f);

	for (field = fields; field->name; field++)
	{
		ReadField(f, field, (byte *)ent);
	}
}

/*
 * A helper function to
 * read the level local
 * data from a file.
 * Called by ReadLevel.
 */
void
ReadLevelLocals(RFILE *f)
{
	field_t *field;

	rfread(&level, sizeof(level), 1, f);

	for (field = levelfields; field->name; field++)
	{
		ReadField(f, field, (byte *)&level);
	}
}

/*
 * Reads a level back into the memory.
 * SpawnEntities were allready called
 * in the same way when the level was
 * saved. All world links were cleared
 * befor this function was called. When
 * this function is called, no clients
 * are connected to the server.
 */
void
ReadLevel(const char *filename)
{
	int entnum;
	RFILE *f;
	int i;
	edict_t *ent;

	f = rfopen(filename, "rb");

	if (!f)
	{
		gi.error("Couldn't open %s", filename);
	}

	/* free any dynamic memory allocated by
	   loading the level  base state */
	gi.FreeTags(TAG_LEVEL);

	/* wipe all the entities */
	memset(g_edicts, 0, game.maxentities * sizeof(g_edicts[0]));
	globals.num_edicts = maxclients->value + 1;

	/* check edict size */
	rfread(&i, sizeof(i), 1, f);

	if (i != sizeof(edict_t))
	{
		rfclose(f);
		gi.error("ReadLevel: mismatched edict size");
	}

	/* load the level locals */
	ReadLevelLocals(f);

	/* load all the entities */
	while (1)
	{
		if (rfread(&entnum, sizeof(entnum), 1, f) != 1)
		{
			rfclose(f);
			gi.error("ReadLevel: failed to read entnum");
		}

		if (entnum == -1)
		{
			break;
		}

		if (entnum >= globals.num_edicts)
		{
			globals.num_edicts = entnum + 1;
		}

		ent = &g_edicts[entnum];
		ReadEdict(f, ent);

		/* let the server rebuild world links for this ent */
		memset(&ent->area, 0, sizeof(ent->area));
		gi.linkentity(ent);
	}

	rfclose(f);

	/* mark all clients as unconnected */
	for (i = 0; i < maxclients->value; i++)
	{
		ent = &g_edicts[i + 1];
		ent->client = game.clients + i;
		ent->client->pers.connected = false;
	}

	/* do any load time things at this point */
	for (i = 0; i < globals.num_edicts; i++)
	{
		ent = &g_edicts[i];

		if (!ent->inuse)
		{
			continue;
		}

		/* fire any cross-level triggers */
		if (ent->classname)
		{
			if (strcmp(ent->classname, "target_crosslevel_target") == 0)
			{
				ent->nextthink = level.time + ent->delay;
			}
		}
	}
}

