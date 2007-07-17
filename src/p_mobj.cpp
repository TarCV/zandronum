// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Moving object handling. Spawn functions.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

// [BB] network.h has to be included before stats.h under Linux.
// The reason should be investigated.
#include "network.h"

#include "templates.h"
#include "m_alloc.h"
#include "i_system.h"
#include "m_random.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "p_effect.h"
#include "p_terrain.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "s_sound.h"
#include "doomstat.h"
#include "v_video.h"
#include "c_cvars.h"
#include "stats.h"
#include "a_doomglobal.h"
#include "a_hexenglobal.h"
#include "a_sharedglobal.h"
#include "gi.h"
#include "sbar.h"
#include "p_acs.h"
#include "cmdlib.h"
#include "decallib.h"
#include "ravenshared.h"
#include "a_action.h"
#include "a_keys.h"
#include "p_conversation.h"
#include "thingdef.h"
#include "deathmatch.h"
#include "duel.h"
#include "g_game.h"
#include "gamemode.h"
#include "cl_main.h"
#include "cooperative.h"
#include "invasion.h"
#include "scoreboard.h"
#include "team.h"
#include "sv_commands.h"
#include "cl_demo.h"

#include "gl/gl_functions.h"

// MACROS ------------------------------------------------------------------

#define WATER_SINK_FACTOR		3
#define WATER_SINK_SMALL_FACTOR	4
#define WATER_SINK_SPEED		(FRACUNIT/2)
#define WATER_JUMP_SPEED		(FRACUNIT*7/2)

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void G_PlayerReborn (int player);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void PlayerLandedOnThing (AActor *mo, AActor *onmobj);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t attackrange;
extern int tmfloorpic;
extern sector_t *tmfloorsector;
EXTERN_CVAR (Bool, r_drawfuzz);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/*static*/ bool SpawningMapThing;
static FRandom pr_explodemissile ("ExplodeMissile");
static FRandom pr_bounce ("Bounce");
static FRandom pr_reflect ("Reflect");
static FRandom pr_nightmarerespawn ("NightmareRespawn");
static FRandom pr_botspawnmobj ("BotSpawnActor");
static FRandom pr_spawnmapthing ("SpawnMapThing");
static FRandom pr_spawnpuff ("SpawnPuff");
static FRandom pr_spawnblood ("SpawnBlood");
static FRandom pr_splatter ("BloodSplatter");
static FRandom pr_takedamage ("TakeDamage");
static FRandom pr_ripperblood ("RipperBlood");
static FRandom pr_chunk ("Chunk");
static FRandom pr_checkmissilespawn ("CheckMissileSpawn");
static FRandom pr_spawnmissile ("SpawnMissile");
static FRandom pr_missiledamage ("MissileDamage");
 FRandom pr_slam ("SkullSlam");
static FRandom pr_multiclasschoice ("MultiClassChoice");

/*static*/	NETIDNODE_t	g_NetIDList[MAX_NETID];
static	ULONG		g_ulFirstFreeNetID = 1;

static	LONG	g_lSpawnCount = 0;
static	cycle_t	g_SpawnCycles = 0;
static	LONG	g_lStaleSpawnCount = 0;
static	cycle_t	g_StaleSpawnCycles = 0;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FRandom pr_spawnmobj ("SpawnActor");

CUSTOM_CVAR (Float, sv_gravity, 800.f, CVAR_SERVERINFO|CVAR_NOSAVE)
{
	level.gravity = self;
}

CVAR (Bool, cl_missiledecals, true, CVAR_ARCHIVE)
CVAR (Bool, addrocketexplosion, false, CVAR_ARCHIVE)

fixed_t FloatBobOffsets[64] =
{
	0, 51389, 102283, 152192,
	200636, 247147, 291278, 332604,
	370727, 405280, 435929, 462380,
	484378, 501712, 514213, 521763,
	524287, 521763, 514213, 501712,
	484378, 462380, 435929, 405280,
	370727, 332604, 291278, 247147,
	200636, 152192, 102283, 51389,
	-1, -51390, -102284, -152193,
	-200637, -247148, -291279, -332605,
	-370728, -405281, -435930, -462381,
	-484380, -501713, -514215, -521764,
	-524288, -521764, -514214, -501713,
	-484379, -462381, -435930, -405280,
	-370728, -332605, -291279, -247148,
	-200637, -152193, -102284, -51389
};

fixed_t FloatBobDiffs[64] =
{
	51389, 51389, 50894, 49909, 48444,
	46511, 44131, 41326, 38123,
	34553, 30649, 26451, 21998,
	17334, 12501, 7550, 2524,
	-2524, -7550, -12501, -17334,
	-21998, -26451, -30649, -34553,
	-38123, -41326, -44131, -46511,
	-48444, -49909, -50894, -51390,
	-51389, -50894, -49909, -48444,
	-46511, -44131, -41326, -38123,
	-34553, -30649, -26451, -21999,
	-17333, -12502, -7549, -2524,
	2524, 7550, 12501, 17334,
	21998, 26451, 30650, 34552,
	38123, 41326, 44131, 46511,
	48444, 49909, 50895
};

CVAR (Int, cl_pufftype, 0, CVAR_ARCHIVE);
CVAR (Int, cl_bloodtype, 0, CVAR_ARCHIVE);

AActor *MissileActor;
AActor *PuffSpawned;

// CODE --------------------------------------------------------------------

IMPLEMENT_POINTY_CLASS (AActor)
 DECLARE_POINTER (target)
 DECLARE_POINTER (lastenemy)
 DECLARE_POINTER (tracer)
 DECLARE_POINTER (goal)
 DECLARE_POINTER (LastLook)	// This is actually a union
 DECLARE_POINTER (Inventory)
 DECLARE_POINTER (LastHeard)
 DECLARE_POINTER (master)
END_POINTERS

AActor::~AActor ()
{
	// Please avoid calling the destructor directly (or through delete)!
	// Use Destroy() instead.
}

void AActor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	if (arc.IsStoring ())
	{
		arc.WriteSprite (sprite);
	}
	else
	{
		sprite = arc.ReadSprite ();
	}

	arc << x
		<< y
		<< z
		<< angle
		<< frame
		<< scaleX
		<< scaleY
		<< RenderStyle
		<< renderflags;
	if (arc.IsStoring ())
	{
		BYTE ff;
		if (picnum == 0xFFFF)
		{
			ff = 0xFF;
			arc << ff;
		}
		else
		{
			ff = 0;
			arc << ff;
			TexMan.WriteTexture (arc, picnum);
		}
		TexMan.WriteTexture (arc, floorpic);
		TexMan.WriteTexture (arc, ceilingpic);
	}
	else
	{
		BYTE ff;
		arc << ff;
		if (ff == 0xFF)
		{
			picnum = 0xFFFF;
		}
		else
		{
			picnum = TexMan.ReadTexture (arc);
		}
		floorpic = TexMan.ReadTexture (arc);
		ceilingpic = TexMan.ReadTexture (arc);
	}
	arc << TIDtoHate;
	if (TIDtoHate == 0)
	{
		arc << LastLook.PlayerNumber;
	}
	else
	{
		arc << LastLook.Actor;
	}
	arc << effects
		<< alpha
		<< alphacolor
		<< pitch
		<< roll
		<< Sector
		<< floorz
		<< ceilingz
		<< dropoffz
		<< floorsector
		<< ceilingsector
		<< radius
		<< height
		<< momx
		<< momy
		<< momz
		<< tics
		<< state
		<< Damage
		<< flags
		<< flags2
		<< flags3
		<< flags4
		<< flags5
		<< special1
		<< special2
		<< health
		<< movedir
		<< visdir
		<< movecount
		<< target
		<< lastenemy
		<< LastHeard
		<< reactiontime
		<< threshold
		<< player
		<< SpawnPoint[0] << SpawnPoint[1] << SpawnPoint[2]
		<< SpawnAngle
		<< tracer
		<< floorclip
		<< tid
		<< special
		<< args[0] << args[1] << args[2] << args[3] << args[4]
		<< goal
		<< waterlevel
		<< MinMissileChance
		<< SpawnFlags
		<< Inventory
		<< InventoryID
		<< id
		<< FloatBobPhase
		<< Translation
		<< AR_SOUNDW(SeeSound)
		<< AR_SOUNDW(AttackSound)
		<< AR_SOUNDW(PainSound)
		<< AR_SOUNDW(DeathSound)
		<< AR_SOUNDW(ActiveSound)
		<< AR_SOUNDW(UseSound)
		<< Speed
		<< FloatSpeed
		<< Mass
		<< PainChance
		<< SpawnState
		<< SeeState
		<< PainState
		<< MeleeState
		<< MissileState
		<< CrashState
		<< DeathState
		<< XDeathState
		<< BDeathState
		<< IDeathState
		<< EDeathState
		<< RaiseState
		<< WoundState
		<< HealState
		<< YesState
		<< NoState
		<< GreetingsState
		<< CrushState
		<< MaxDropOffHeight 
		<< MaxStepHeight
		<< bouncefactor
		<< bouncecount
		<< meleerange
		<< DamageType
		<< (DWORD &)ulSTFlags
		<< (DWORD &)ulNetworkFlags
		<< (DWORD &)ulInvasionWave
		<< (DWORD &)ulInvasionSpawnerID;

	if (arc.IsStoring ())
	{
		int convnum = 0;
		unsigned int i;

		if (Conversation != NULL)
		{
			for (i = 0; i < StrifeDialogues.Size(); ++i)
			{
				if (StrifeDialogues[i] == GetDefault()->Conversation)
				{
					break;
				}
			}
			for (; i + convnum < StrifeDialogues.Size(); ++convnum)
			{
				if (StrifeDialogues[i + convnum] == Conversation)
				{
					break;
				}
			}
			if (i + convnum < StrifeDialogues.Size())
			{
				convnum++;
			}
			else
			{
				convnum = 0;
			}
		}
		arc.WriteCount (convnum);
	}
	else
	{
		int convnum;
		unsigned int i;

		convnum = arc.ReadCount();
		if (convnum == 0 || GetDefault()->Conversation == NULL)
		{
			Conversation = NULL;
		}
		else
		{
			for (i = 0; i < StrifeDialogues.Size(); ++i)
			{
				if (StrifeDialogues[i] == GetDefault()->Conversation)
				{
					break;
				}
			}
			if (i + convnum <= StrifeDialogues.Size())
			{
				Conversation = StrifeDialogues[i + convnum - 1];
			}
			else
			{
				Conversation = GetDefault()->Conversation;
			}
		}
	}

	if (arc.IsLoading ())
	{
		touching_sectorlist = NULL;
		LinkToWorld (Sector);
		AddToHash ();
		SetShade (alphacolor);
		if (player)
		{
			if (playeringame[player - players] && 
				player->cls != NULL &&
				state->sprite.index == 
				GetDefaultByType (player->cls)->SpawnState->sprite.index)
			{ // Give player back the skin
				sprite = skins[player->userinfo.skin].sprite;
				// [GZDoom]
				scaleX = scaleY = skins[player->userinfo.skin].Scale;
				//xscale = yscale = skins[player->userinfo.skin].scale;
			}
			if (Speed == 0)
			{
				Speed = GetDefault()->Speed;
			}
		}
		PrevX = x;
		PrevY = y;
		PrevZ = z;
		UpdateWaterLevel(z, false);
	}
}

void MapThing::Serialize (FArchive &arc)
{
	arc << thingid << x << y << z << angle << type << flags << special
		<< args[0] << args[1] << args[2] << args[3] << args[4];
}

AActor::AActor () throw()
{
}

AActor::AActor (const AActor &other) throw()
{
	memcpy (&x, &other.x, (BYTE *)&this[1] - (BYTE *)&x);
}

AActor &AActor::operator= (const AActor &other)
{
	memcpy (&x, &other.x, (BYTE *)&this[1] - (BYTE *)&x);
	return *this;
}

//==========================================================================
//
// AActor::InStateSequence
//
// Checks whether the current state is in a contiguous sequence that
// starts with basestate
//
//==========================================================================

bool AActor::InStateSequence(FState * newstate, FState * basestate)
{
	if (basestate == NULL) return false;

	FState * thisstate = basestate;
	do
	{
		if (newstate == thisstate) return true;
		basestate = thisstate;
		thisstate = thisstate->GetNextState();
	}
	while (thisstate == basestate+1);
	return false;
}

//==========================================================================
//
// AActor::GetTics
//
// Get the actual duration of the next state
// This is a more generalized attempt to make the Demon faster in
// nightmare mode. Actually changing the states' durations has to
// be considered highly problematic.
//
//==========================================================================

int AActor::GetTics(FState * newstate)
{
	int tics = newstate->GetTics();
	
	if (gameskill == sk_nightmare || (dmflags & DF_FAST_MONSTERS))
	{
		if (flags5 & MF5_FASTER)
		{
			if (InStateSequence(newstate, SeeState)) return tics - (tics>>1);
		}
		if (flags5 & MF5_FASTMELEE)
		{
			if (InStateSequence(newstate, MeleeState)) return tics - (tics>>1);
		}
	}
	return tics;
}

//==========================================================================
//
// AActor::SetState
//
// Returns true if the mobj is still present.
//
//==========================================================================

bool AActor::SetState (FState *newstate)
{
/*
	if (debugfile && player && (player->cheats & CF_PREDICTING))
		fprintf (debugfile, "for pl %d: SetState while predicting!\n", player-players);
*/
	do
	{
		if (newstate == NULL)
		{
			state = NULL;
			Destroy ();
			return false;
		}
		// [BB] Dead things should not have positive health.
		if ( (newstate == DeathState) && (health > 0) )
			health = 0;

		int prevsprite, newsprite;

		if (state != NULL)
		{
			prevsprite = state->sprite.index;
		}
		else
		{
			prevsprite = -1;
		}
		state = newstate;
		tics = GetTics(newstate);
		renderflags = (renderflags & ~RF_FULLBRIGHT) | newstate->GetFullbright();
		newsprite = newstate->sprite.index;
		if (newsprite != 1)
		{
			// Sprite 1 is ----, which means "do not change the sprite"
			frame = newstate->GetFrame();

			if (!(flags4 & MF4_NOSKIN) && newsprite == SpawnState->sprite.index)
			{ // [RH] If the new sprite is the same as the original sprite, and
			// this actor is attached to a player, use the player's skin's
			// sprite. If a player is not attached, do not change the sprite
			// unless it is different from the previous state's sprite; a
			// player may have been attached, died, and respawned elsewhere,
			// and we do not want to lose the skin on the body. If it wasn't
			// for Dehacked, I would move sprite changing out of the states
			// altogether, since actors rarely change their sprites after
			// spawning.
				if (player != NULL)
				{
					sprite = skins[player->userinfo.skin].sprite;
				}
				else if (newsprite != prevsprite)
				{
					sprite = newsprite;
				}
			}
			else
			{
				sprite = newsprite;
			}
		}
		if (newstate->GetAction())
		{
			// The parameterized action functions need access to the current state and
			// if a function is supposed to work with both actors and weapons
			// there is no real means to get to it reliably so I store it in a global variable here.
			// Yes, I know this is truly awful but it is the only method I can think of 
			// that does not involve changing stuff throughout the code. 
			// Of course this should be rewritten ASAP.
			CallingState = newstate;

			newstate->GetAction() (this);

			// Check whether the called action function resulted in destroying the actor
			if (ObjectFlags & OF_MassDestruction) return false;
		}
		newstate = newstate->GetNextState();
	} while (tics == 0);

	gl_SetActorLights(this);
	return true;
}

//----------------------------------------------------------------------------
//
// FUNC AActor::SetStateNF
//
// Same as SetState, but does not call the state function.
//
//----------------------------------------------------------------------------

bool AActor::SetStateNF (FState *newstate)
{
	do
	{
		if (newstate == NULL)
		{
			state = NULL;
			Destroy ();
			return false;
		}
		int prevsprite, newsprite;

		if (state != NULL)
		{
			prevsprite = state->sprite.index;
		}
		else
		{
			prevsprite = -1;
		}
		state = newstate;
		tics = GetTics(newstate);
		renderflags = (renderflags & ~RF_FULLBRIGHT) | newstate->GetFullbright();
		newsprite = newstate->sprite.index;
		if (newsprite != 1)
		{
			// Sprite 1 is ----, which means "do not change the sprite"

			frame = newstate->GetFrame();
			if (!(flags4 & MF4_NOSKIN) && newsprite == SpawnState->sprite.index)
			{
				if (player != NULL && gameinfo.gametype != GAME_Hexen)
				{
					sprite = skins[player->userinfo.skin].sprite;
				}
				else if (newsprite != prevsprite)
				{
					sprite = newsprite;
				}
			}
			else
			{
				sprite = newsprite;
			}
		}
		newstate = newstate->GetNextState();
	} while (tics == 0);

	gl_SetActorLights(this);
	return true;
}

//----------------------------------------------------------------------------
//
// AActor::InDeathState
//
//----------------------------------------------------------------------------

bool AActor::InDeathState()
{
	if( state != NULL )
	{
		FState *pDeadState = DeathState;
		FState *pState = NULL;
		while ( pDeadState != NULL )
		{
			if ( state == pDeadState )
				return true;
			pState = pDeadState;
			pDeadState = pDeadState->GetNextState( );

			// If the state loops back to the beginning of the death state, or to itself, break out.
			if (( pDeadState == DeathState ) || ( pState == pDeadState ))
				break;
		}
	}
	return false;
}

//============================================================================
//
// AActor :: AddInventory
//
//============================================================================

void AActor::AddInventory (AInventory *item)
{
	// Check if it's already attached to an actor
	if (item->Owner != NULL)
	{
		// Is it attached to us?
		if (item->Owner == this)
			return;

		// No, then remove it from the other actor first
		item->Owner->RemoveInventory (item);
	}

	item->Owner = this;
	item->Inventory = Inventory;
	Inventory = item;

	// Each item receives an unique ID when added to an actor's inventory.
	// This is used by the DEM_INVUSE command to identify the item. Simply
	// using the item's position in the list won't work, because ticcmds get
	// run sometime in the future, so by the time it runs, the inventory
	// might not be in the same state as it was when DEM_INVUSE was sent.
	Inventory->InventoryID = InventoryID++;
}

//============================================================================
//
// AActor :: RemoveInventory
//
//============================================================================

void AActor::RemoveInventory (AInventory *item)
{
	AInventory *inv, **invp;

	invp = &item->Owner->Inventory;
	for (inv = *invp; inv != NULL; invp = &inv->Inventory, inv = *invp)
	{
		if (inv == item)
		{
			*invp = item->Inventory;
			item->DetachFromOwner ();
			item->Owner = NULL;
			break;
		}
	}
}

//============================================================================
//
// AActor :: DestroyAllInventory
//
//============================================================================

void AActor::DestroyAllInventory ()
{
	while (Inventory != NULL)
	{
		AInventory *item = Inventory;
		// [BC] In certain modes, we may need to keep this item around.
		if (( item->ulSTFlags & STFL_LEVELSPAWNED ) &&
			( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_MAPRESETS ))
		{
			item->HideIndefinitely( );

			// And now, some code from AInventory::Destroy() to remove this object
			// from the owner's inventory..
			if ( item->Owner != NULL )
				item->Owner->RemoveInventory( item );

			item->Inventory = NULL;
		}
		else
			item->Destroy ();
		assert (item != Inventory);
	}
}

//============================================================================
//
// AActor :: FirstInv
//
// Returns the first item in this actor's inventory that has IF_INVBAR set.
//
//============================================================================

AInventory *AActor::FirstInv () const
{
	if (Inventory == NULL)
	{
		return NULL;
	}
	if (Inventory->ItemFlags & IF_INVBAR)
	{
		return Inventory;
	}
	return Inventory->NextInv ();
}

//============================================================================
//
// AActor :: UseInventory
//
// Attempts to use an item. If the use succeeds, one copy of the item is
// removed from the inventory. If all copies are removed, then the item is
// destroyed.
//
//============================================================================

bool AActor::UseInventory (AInventory *item)
{
	// No using items if you're dead.
	if (health <= 0)
	{
		return false;
	}
	if (!item->Use (false))
	{
		return false;
	}
	if (--item->Amount <= 0)
	{
		item->Destroy ();
	}
	return true;
}

//===========================================================================
//
// AActor :: DropInventory
//
// Removes a single copy of an item and throws it out in front of the actor.
//
//===========================================================================

AInventory *AActor::DropInventory (AInventory *item)
{
	fixed_t dropdist;
	angle_t an;
	AInventory *drop = item->CreateTossable ();

	if (drop == NULL)
	{
		return NULL;
	}
	an = angle >> ANGLETOFINESHIFT;
	/* 92682 = sqrt(2) * FRACUNIT */
	dropdist = FixedMul (92682, radius + 8*FRACUNIT + item->radius);
	drop->x = x;
	drop->y = y;
	drop->z = z + 10*FRACUNIT;
	P_TryMove (drop, x, y, true);
	drop->angle = angle;
	drop->momx = momx + 5 * finecosine[an];
	drop->momy = momy + 5 * finesine[an];
	drop->momz = momz + FRACUNIT;
	drop->flags &= ~MF_NOGRAVITY;	// Don't float
	return drop;
}

//============================================================================
//
// AActor :: FindInventory
//
//============================================================================

AInventory *AActor::FindInventory (const PClass *type) const
{
	AInventory *item;

	if (type == NULL) return NULL;

	assert (type->ActorInfo != NULL);
	for (item = Inventory; item != NULL; item = item->Inventory)
	{
		if (item->GetClass() == type)
		{
			break;
		}
	}
	return item;
}

AInventory *AActor::FindInventory (FName type) const
{
	return FindInventory(PClass::FindClass(type));
}

//============================================================================
//
// AActor :: GiveInventoryType
//
//============================================================================

AInventory *AActor::GiveInventoryType (const PClass *type)
{
	AInventory *item;

	item = static_cast<AInventory *>(Spawn (type, 0,0,0, NO_REPLACE));
	if (!item->TryPickup (this))
	{
		item->Destroy ();
		return NULL;
	}
	return item;
}

//============================================================================
//
// AActor :: GiveAmmo
//
// Returns true if the ammo was added, false if not.
//
//============================================================================

bool AActor::GiveAmmo (const PClass *type, int amount)
{
	AInventory *item = static_cast<AInventory *>(Spawn (type, 0, 0, 0, NO_REPLACE));
	item->Amount = amount;
	item->flags |= MF_DROPPED;
	if (!item->TryPickup (this))
	{
		item->Destroy ();
		return false;
	}
	return true;
}

//============================================================================
//
// AActor :: CopyFriendliness
//
// Makes this actor hate (or like) the same things another actor does.
//
//============================================================================

void AActor::CopyFriendliness (const AActor *other, bool changeTarget)
{
	level.total_monsters -= CountsAsKill();
	TIDtoHate = other->TIDtoHate;
	LastLook = other->LastLook;
	flags  = (flags & ~MF_FRIENDLY) | (other->flags & MF_FRIENDLY);
	flags3 = (flags3 & ~(MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS)) | (other->flags3 & (MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS));
	flags4 = (flags4 & ~MF4_NOHATEPLAYERS) | (other->flags4 & MF4_NOHATEPLAYERS);
	FriendPlayer = other->FriendPlayer;
	if (changeTarget)
	{
		// LastHeard must be set as well so that A_Look can react to the new target if called
		LastHeard = target = other->target;
	}
	level.total_monsters += CountsAsKill();
}

//============================================================================
//
// AActor :: ObtainInventory
//
// Removes the items from the other actor and puts them in this actor's
// inventory. The actor receiving the inventory must not have any items.
//
//============================================================================

void AActor::ObtainInventory (AActor *other)
{
	Inventory = other->Inventory;
	InventoryID = other->InventoryID;
	other->Inventory = NULL;
	other->InventoryID = 0;

	if (other->IsKindOf(RUNTIME_CLASS(APlayerPawn)) && this->IsKindOf(RUNTIME_CLASS(APlayerPawn)))
	{
		APlayerPawn *you = static_cast<APlayerPawn *>(other);
		APlayerPawn *me = static_cast<APlayerPawn *>(this);
		me->InvFirst = you->InvFirst;
		me->InvSel = you->InvSel;
		you->InvFirst = NULL;
		you->InvSel = NULL;
	}

	AInventory *item = Inventory;
	while (item != NULL)
	{
		item->Owner = this;
		item = item->Inventory;
	}
}

//============================================================================
//
// AActor :: CheckLocalView
//
// Returns true if this actor is local for the player. Here, local means the
// player is either looking out this actor's eyes, or this actor is the player
// and the player is looking out the eyes of something non-"sentient."
//
//============================================================================

bool AActor::CheckLocalView (int playernum) const
{
	if (players[playernum].camera == this)
	{
		return true;
	}
	if (players[playernum].mo != this || players[playernum].camera == NULL)
	{
		return false;
	}
	if (players[playernum].camera->player == NULL &&
		!(players[playernum].camera->flags3 & MF3_ISMONSTER))
	{
		return true;
	}
	return false;
}

//============================================================================
//
// AActor :: ConversationAnimation
//
// Plays a conversation-related animation:
//	 0 = greeting
//   1 = "yes"
//   2 = "no"
//
//============================================================================

void AActor::ConversationAnimation (int animnum)
{
	switch (animnum)
	{
	case 0:
		if (GreetingsState != NULL) SetState (GreetingsState);
		break;
	case 1:
		if (YesState != NULL) SetState (YesState);
		break;
	case 2:
		if (NoState != NULL) SetState (NoState);
		break;
	}
}

//============================================================================
//
// AActor :: Touch
//
// Something just touched this actor. Normally used only for inventory items,
// but some Strife monsters also use it.
//
//============================================================================

void AActor::Touch (AActor *toucher)
{
}

//============================================================================
//
// AActor :: Massacre
//
// Called by the massacre cheat to kill monsters. Returns true if the monster
// was killed and false if it was already dead.
//============================================================================

bool AActor::Massacre ()
{
	int prevhealth;

	if (health > 0)
	{
		flags |= MF_SHOOTABLE;
		flags2 &= ~(MF2_DORMANT|MF2_INVULNERABLE);
		do
		{
			prevhealth = health;
			P_DamageMobj (this, NULL, NULL, 1000000, MOD_MASSACRE);
		}
		while (health != prevhealth && health > 0);	//abort if the actor wasn't hurt.
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------
//
// PROC P_ExplodeMissile
//
//----------------------------------------------------------------------------

void P_ExplodeMissile (AActor *mo, line_t *line, AActor *target)
{
	if (mo->flags3 & MF3_EXPLOCOUNT)
	{
		if (++mo->special2 < mo->special1)
		{
			return;
		}
	}
	mo->momx = mo->momy = mo->momz = 0;
	mo->effects = 0;		// [RH]
	
	FState *nextstate=NULL;
	
	if (target != NULL && target->flags & MF_SHOOTABLE)
	{
		if (target->flags & MF_NOBLOOD) nextstate = mo->CrashState;
		if (nextstate == NULL) nextstate = mo->XDeathState;
	}
	if (nextstate == NULL) nextstate = mo->DeathState;
	mo->SetState (nextstate);
	
	if (mo->ObjectFlags & OF_MassDestruction)
	{
		return;
	}

	if (line != NULL && line->special == Line_Horizon)
	{
		// [RH] Don't explode missiles on horizon lines.
		mo->Destroy ();
		return;
	}

	if (line != NULL && cl_missiledecals)
	{
		int side = P_PointOnLineSide (mo->x, mo->y, line);
		if (line->sidenum[side] == NO_SIDE)
			side ^= 1;
		if (line->sidenum[side] != NO_SIDE)
		{
			FDecalBase *base = mo->DecalGenerator;
			if (base != NULL)
			{
				// Find the nearest point on the line, and stick a decal there
				fixed_t x, y, z;
				SQWORD num, den;

				den = (SQWORD)line->dx*line->dx + (SQWORD)line->dy*line->dy;
				if (den != 0)
				{
					SDWORD frac;

					num = (SQWORD)(mo->x-line->v1->x)*line->dx+(SQWORD)(mo->y-line->v1->y)*line->dy;
					if (num <= 0)
					{
						frac = 0;
					}
					else if (num >= den)
					{
						frac = 1<<30;
					}
					else
					{
						frac = (SDWORD)(num / (den>>30));
					}

					x = line->v1->x + MulScale30 (line->dx, frac);
					y = line->v1->y + MulScale30 (line->dy, frac);
					z = mo->z;

					F3DFloor * ffloor=NULL;
					if (line->sidenum[side^1]!=NO_SIDE && sides[line->sidenum[side^1]].sector->e->ffloors.Size())
					{
						// find a 3D-floor to stick to
						sector_t * backsector=sides[line->sidenum[side^1]].sector;
						for(int i=0;i<backsector->e->ffloors.Size();i++)
						{
							F3DFloor * rover=backsector->e->ffloors[i];

							if ((rover->flags&(FF_EXISTS|FF_SOLID|FF_RENDERSIDES))==(FF_EXISTS|FF_SOLID|FF_RENDERSIDES))
							{
								if (z<=rover->top.plane->ZatPoint(x, y) && z>=rover->bottom.plane->ZatPoint( x, y))
								{
									ffloor=rover;
									break;
								}
							}
						}
					}

					// [BC] Servers don't need to spawn decals.
					if ( NETWORK_GetState( ) != NETSTATE_SERVER )
					{
						DImpactDecal::StaticCreate (base->GetDecal (),
							x, y, z, sides + line->sidenum[side]);
					}
				}
			}
		}
	}

	// [BC] Tell clients that this missile blew up.
//	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( mo->IsKindOf( RUNTIME_CLASS( ARevenantTracer )) || ( line == false )))
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MissileExplode( mo, line );

	if (mo->DeathState != NULL)
	{
		// [RH] Change render style of exploding rockets
		if (mo->flags5 & MF5_DEHEXPLOSION)
		{
			if (deh.ExplosionStyle == 255)
			{
				if (addrocketexplosion)
				{
					mo->RenderStyle = STYLE_Add;
					mo->alpha = FRACUNIT;
				}
				else
				{
					mo->RenderStyle = STYLE_Translucent;
					mo->alpha = FRACUNIT*2/3;
				}
			}
			else
			{
				mo->RenderStyle = deh.ExplosionStyle;
				mo->alpha = deh.ExplosionAlpha;
			}
		}

		if (mo->flags4 & MF4_RANDOMIZE)
		{
			mo->tics -= (pr_explodemissile() & 3) * TICRATE / 35;
			if (mo->tics < 1)
				mo->tics = 1;
		}

		mo->flags &= ~MF_MISSILE;

		if (mo->DeathSound)
		{
			S_SoundID (mo, CHAN_VOICE, mo->DeathSound, 1,
				(mo->flags3 & MF3_FULLVOLDEATH) ? ATTN_NONE : ATTN_NORM);
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC P_FloorBounceMissile
//
// Returns true if the missile was destroyed
//----------------------------------------------------------------------------

bool AActor::FloorBounceMissile (secplane_t &plane)
{
	if (z <= floorz && P_HitFloor (this))
	{
		// Landed in some sort of liquid
		if (flags5 & MF5_EXPLODEONWATER)
		{
			P_ExplodeMissile(this, NULL, NULL);
			return true;
		}
		if (!(flags3 & MF3_CANBOUNCEWATER))
		{
			Destroy ();
			return true;
		}
	}

	// The amount of bounces is limited
	if (bouncecount>0 && --bouncecount==0)
	{
		P_ExplodeMissile(this, NULL, NULL);
		return true;
	}

	fixed_t dot = TMulScale16 (momx, plane.a, momy, plane.b, momz, plane.c);

	if ((flags2 & MF2_BOUNCETYPE) == MF2_HERETICBOUNCE)
	{
		momx -= MulScale15 (plane.a, dot);
		momy -= MulScale15 (plane.b, dot);
		momz -= MulScale15 (plane.c, dot);
		angle = R_PointToAngle2 (0, 0, momx, momy);
		flags |= MF_INBOUNCE;
		SetState (DeathState);
		flags &= ~MF_INBOUNCE;
		return false;
	}

	// The reflected velocity keeps only about 70% of its original speed
	long bouncescale = 0x4000 * bouncefactor;
	momx = MulScale30 (momx - MulScale15 (plane.a, dot), bouncescale);
	momy = MulScale30 (momy - MulScale15 (plane.b, dot), bouncescale);
	momz = MulScale30 (momz - MulScale15 (plane.c, dot), bouncescale);
	angle = R_PointToAngle2 (0, 0, momx, momy);

	// [BC] Actors don't yet have a bounce sound, so this will have to be hacked for now.
	if ( this->GetClass( ) == RUNTIME_CLASS( AGrenade ))
		S_Sound( this, CHAN_VOICE, "weapons/grbnce", 1, ATTN_IDLE );
	else if (SeeSound && !(flags4 & MF4_NOBOUNCESOUND))
	{
		S_SoundID (this, CHAN_VOICE, SeeSound, 1, ATTN_IDLE);
	}

	if ((flags2 & MF2_BOUNCETYPE) == MF2_DOOMBOUNCE)
	{
		if (!(flags & MF_NOGRAVITY) && (momz < 3*FRACUNIT))
		{
			flags2 &= ~MF2_BOUNCETYPE;
		}
	}
	return false;
}

//----------------------------------------------------------------------------
//
// PROC P_ThrustMobj
//
//----------------------------------------------------------------------------

void P_ThrustMobj (AActor *mo, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;
	mo->momx += FixedMul (move, finecosine[angle]);
	mo->momy += FixedMul (move, finesine[angle]);
}

//----------------------------------------------------------------------------
//
// FUNC P_FaceMobj
//
// Returns 1 if 'source' needs to turn clockwise, or 0 if 'source' needs
// to turn counter clockwise.  'delta' is set to the amount 'source'
// needs to turn.
//
//----------------------------------------------------------------------------

int P_FaceMobj (AActor *source, AActor *target, angle_t *delta)
{
	angle_t diff;
	angle_t angle1;
	angle_t angle2;

	angle1 = source->angle;
	angle2 = R_PointToAngle2 (source->x, source->y, target->x, target->y);
	if (angle2 > angle1)
	{
		diff = angle2 - angle1;
		if (diff > ANGLE_180)
		{
			*delta = ANGLE_MAX - diff;
			return 0;
		}
		else
		{
			*delta = diff;
			return 1;
		}
	}
	else
	{
		diff = angle1 - angle2;
		if (diff > ANGLE_180)
		{
			*delta = ANGLE_MAX - diff;
			return 1;
		}
		else
		{
			*delta = diff;
			return 0;
		}
	}
}

//----------------------------------------------------------------------------
//
// FUNC P_SeekerMissile
//
// The missile's tracer field must be the target.  Returns true if
// target was tracked, false if not.
//
//----------------------------------------------------------------------------

bool P_SeekerMissile (AActor *actor, angle_t thresh, angle_t turnMax)
{
	int dir;
	int dist;
	angle_t delta;
	angle_t angle;
	AActor *target;

	target = actor->tracer;
	if (target == NULL || actor->Speed == 0)
	{
		return false;
	}
	if (!(target->flags & MF_SHOOTABLE))
	{ // Target died
		actor->tracer = NULL;
		return false;
	}
	dir = P_FaceMobj (actor, target, &delta);
	if (delta > thresh)
	{
		delta >>= 1;
		if (delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if (dir)
	{ // Turn clockwise
		actor->angle += delta;
	}
	else
	{ // Turn counter clockwise
		actor->angle -= delta;
	}
	angle = actor->angle>>ANGLETOFINESHIFT;
	actor->momx = FixedMul (actor->Speed, finecosine[angle]);
	actor->momy = FixedMul (actor->Speed, finesine[angle]);
	if (actor->z + actor->height < target->z ||
		target->z + target->height < actor->z)
	{ // Need to seek vertically
		dist = P_AproxDistance (target->x - actor->x, target->y - actor->y);
		dist = dist / actor->Speed;
		if (dist < 1)
		{
			dist = 1;
		}
		actor->momz = ((target->z+target->height/2) - (actor->z+actor->height/2)) / dist;
	}
	return true;
}

//
// P_XYMovement  
//
#define STOPSPEED			0x1000
#define FRICTION			0xe800
#define CARRYSTOPSPEED		(STOPSPEED*32/3)

void P_XYMovement (AActor *mo, fixed_t scrollx, fixed_t scrolly) 
{
	bool bForceSlide = scrollx || scrolly;
	angle_t angle;
	fixed_t ptryx, ptryy;
	player_t *player;
	fixed_t xmove, ymove;
	const secplane_t * walkplane;
	static const int windTab[3] = {2048*5, 2048*10, 2048*25};
	int steps, step, totalsteps;
	fixed_t startx, starty;

	fixed_t maxmove = (mo->waterlevel < 1) || (mo->flags & MF_MISSILE) || 
					  (mo->player && mo->player->crouchoffset<-10*FRACUNIT) ? MAXMOVE : MAXMOVE/4;

	if (mo->flags2 & MF2_WINDTHRUST && mo->waterlevel < 2 && !(mo->flags & MF_NOCLIP))
	{
		int special = mo->Sector->special;
		switch (special)
		{
			case 40: case 41: case 42: // Wind_East
				P_ThrustMobj (mo, 0, windTab[special-40]);
				break;
			case 43: case 44: case 45: // Wind_North
				P_ThrustMobj (mo, ANG90, windTab[special-43]);
				break;
			case 46: case 47: case 48: // Wind_South
				P_ThrustMobj (mo, ANG270, windTab[special-46]);
				break;
			case 49: case 50: case 51: // Wind_West
				P_ThrustMobj (mo, ANG180, windTab[special-49]);
				break;
		}
	}

	// [RH] No need to clamp these now. However, wall running needs it so
	// that large thrusts can't propel an actor through a wall, because wall
	// running depends on the player's original movement continuing even after
	// it gets blocked.
	if (mo->player != NULL && (i_compatflags & COMPATF_WALLRUN) || (mo->waterlevel >= 1) ||
		(mo->player != NULL && mo->player->crouchfactor < FRACUNIT*3/4))
	{
		// preserve the direction instead of clamping x and y independently.
		xmove = clamp (mo->momx, -maxmove, maxmove);
		ymove = clamp (mo->momy, -maxmove, maxmove);

		fixed_t xfac = FixedDiv(xmove, mo->momx);
		fixed_t yfac = FixedDiv(ymove, mo->momy);
		fixed_t fac = MIN(xfac, yfac);

		xmove = mo->momx = FixedMul(mo->momx, fac);
		ymove = mo->momy = FixedMul(mo->momy, fac);
	}
	else
	{
		xmove = mo->momx;
		ymove = mo->momy;
	}
	// [RH] Carrying sectors didn't work with low speeds in BOOM. This is
	// because BOOM relied on the speed being fast enough to accumulate
	// despite friction. If the speed is too low, then its movement will get
	// cancelled, and it won't accumulate to the desired speed.
	if (abs(scrollx) > CARRYSTOPSPEED)
	{
		scrollx = FixedMul (scrollx, CARRYFACTOR);
		mo->momx += scrollx;
	}
	if (abs(scrolly) > CARRYSTOPSPEED)
	{
		scrolly = FixedMul (scrolly, CARRYFACTOR);
		mo->momy += scrolly;
	}
	xmove += scrollx;
	ymove += scrolly;

	if ((xmove | ymove) == 0)
	{
		if (mo->flags & MF_SKULLFLY)
		{
			// the skull slammed into something
			mo->flags &= ~MF_SKULLFLY;
			mo->momx = mo->momy = mo->momz = 0;

			mo->SetState (mo->SeeState != NULL ? mo->SeeState : mo->SpawnState);

			// If we are the server, tell clients about the state change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingState( mo, mo->SeeState != NULL ? STATE_SEE : STATE_SPAWN );
		}
		return;
	}

	player = mo->player;

	// [RH] Adjust player movement on sloped floors
	fixed_t startxmove = xmove;
	fixed_t startymove = ymove;
	walkplane = P_CheckSlopeWalk (mo, xmove, ymove);

	// [RH] Take smaller steps when moving faster than the object's size permits.
	// Moving as fast as the object's "diameter" is bad because it could skip
	// some lines because the actor could land such that it is just touching the
	// line. For Doom to detect that the line is there, it needs to actually cut
	// through the actor.

	{
		maxmove = mo->radius - FRACUNIT;

		if (maxmove <= 0)
		{ // gibs can have radius 0, so don't divide by zero below!
			maxmove = MAXMOVE;
		}

		const fixed_t xspeed = abs (xmove);
		const fixed_t yspeed = abs (ymove);

		steps = 1;

		if (xspeed > yspeed)
		{
			if (xspeed > maxmove)
			{
				steps = 1 + xspeed / maxmove;
			}
		}
		else
		{
			if (yspeed > maxmove)
			{
				steps = 1 + yspeed / maxmove;
			}
		}
	}

	// P_SlideMove needs to know the step size before P_CheckSlopeWalk
	// because it also calls P_CheckSlopeWalk on its clipped steps.
	fixed_t onestepx = startxmove / steps;
	fixed_t onestepy = startymove / steps;

	startx = mo->x;
	starty = mo->y;
	step = 1;
	totalsteps = steps;

	// [RH] Instead of doing ripping damage each step, do it each tic.
	// This makes it compatible with Heretic and Hexen, which only did
	// one step for their missiles with ripping damage (excluding those
	// that don't use P_XYMovement). It's also more intuitive since it
	// makes the damage done dependant on the amount of time the projectile
	// spends inside a target rather than on the projectile's size. The
	// last actor ripped through is recorded so that if the projectile
	// passes through more than one actor this tic, each one takes damage
	// and not just the first one.

	if (mo->flags2 & MF2_RIP)
	{
		DoRipping = true;
		LastRipped = NULL;
	}

	do
	{
		ptryx = startx + Scale (xmove, step, steps);
		ptryy = starty + Scale (ymove, step, steps);

//		Printf ("%d,%d/%d: %d %d %d %d %d %d %d %d\n", level.time, step, steps, startxmove, Scale(xmove,step,steps), startymove, Scale(ymove,step,steps), mo->x, mo->y, mo->z);

		// [RH] If walking on a slope, stay on the slope
		// killough 3/15/98: Allow objects to drop off
		if (!P_TryMove (mo, ptryx, ptryy, true, walkplane))
		{
			// blocked move

			if (mo->flags2 & MF2_SLIDE || bForceSlide)
			{
				// try to slide along it
				if (BlockingMobj == NULL)
				{ // slide against wall
					if (BlockingLine != NULL &&
						mo->player && mo->waterlevel && mo->waterlevel < 3 &&
						(mo->player->cmd.ucmd.forwardmove | mo->player->cmd.ucmd.sidemove) &&
						BlockingLine->sidenum[1] != NO_SIDE)
					{
						mo->momz = WATER_JUMP_SPEED;
					}
					if (player && (i_compatflags & COMPATF_WALLRUN))
					{
					// [RH] Here is the key to wall running: The move is clipped using its full speed.
					// If the move is done a second time (because it was too fast for one move), it
					// is still clipped against the wall at its full speed, so you effectively
					// execute two moves in one tic.
						P_SlideMove (mo, mo->momx, mo->momy, 1);
					}
					else
					{
						P_SlideMove (mo, onestepx, onestepy, totalsteps);
					}
					if ((mo->momx | mo->momy) == 0)
					{
						steps = 0;
					}
					else
					{
						if (!player || !(i_compatflags & COMPATF_WALLRUN))
						{
							xmove = mo->momx;
							ymove = mo->momy;
							onestepx = xmove / steps;
							onestepy = ymove / steps;
							P_CheckSlopeWalk (mo, xmove, ymove);
						}
						startx = mo->x - Scale (xmove, step, steps);
						starty = mo->y - Scale (ymove, step, steps);
					}
				}
				else
				{ // slide against another actor
					fixed_t tx, ty;
					tx = 0, ty = onestepy;
					walkplane = P_CheckSlopeWalk (mo, tx, ty);
					if (P_TryMove (mo, mo->x + tx, mo->y + ty, true, walkplane))
					{
						mo->momx = 0;
					}
					else
					{
						tx = onestepx, ty = 0;
						walkplane = P_CheckSlopeWalk (mo, tx, ty);
						if (P_TryMove (mo, mo->x + tx, mo->y + ty, true, walkplane))
						{
							mo->momy = 0;
						}
						else
						{
							mo->momx = mo->momy = 0;
						}
					}
					if (player && player->mo == mo)
					{
						if (mo->momx == 0)
							player->momx = 0;
						if (mo->momy == 0)
							player->momy = 0;
					}
					steps = 0;
				}
			}
			else if (mo->flags & MF_MISSILE)
			{
				steps = 0;
				if (BlockingMobj)
				{
					if (mo->flags2 & MF2_BOUNCE2)
					{
						if (mo->flags5&MF5_BOUNCEONACTORS ||
							(BlockingMobj->flags2 & MF2_REFLECTIVE) ||
							((!BlockingMobj->player) &&
							(!(BlockingMobj->flags3 & MF3_ISMONSTER))))
						{
							fixed_t speed;

							angle = R_PointToAngle2 (BlockingMobj->x,
								BlockingMobj->y, mo->x, mo->y)
								+ANGLE_1*((pr_bounce()%16)-8);
							speed = P_AproxDistance (mo->momx, mo->momy);
							speed = FixedMul (speed, (fixed_t)(0.75*FRACUNIT));
							mo->angle = angle;
							angle >>= ANGLETOFINESHIFT;
							mo->momx = FixedMul (speed, finecosine[angle]);
							mo->momy = FixedMul (speed, finesine[angle]);
							// [BC] Actors don't yet have a bounce sound, so this will have to be hacked for now.
							if ( mo->GetClass( ) == RUNTIME_CLASS( AGrenade ))
								S_Sound( mo, CHAN_VOICE, "weapons/grbnce", 1, ATTN_IDLE );
							else if (mo->SeeSound && !(mo->flags4&MF4_NOBOUNCESOUND))
							{
								S_SoundID (mo, CHAN_VOICE, mo->SeeSound, 1, ATTN_IDLE);
							}
							DoRipping = false;
							return;
						}
						else
						{ // Struck a player/creature
						
							// Potentially reward the player who shot this missile with an accuracy/precision medal.
							if ((( mo->ulSTFlags & STFL_EXPLODEONDEATH ) == false ) && mo->target && mo->target->player )
							{
								if ( mo->target->player->bStruckPlayer )
									PLAYER_StruckPlayer( mo->target->player );
								else
									mo->target->player->ulConsecutiveHits = 0;
							}

							P_ExplodeMissile (mo, NULL, BlockingMobj);
							DoRipping = false;
							return;
						}
					}
				}
				else
				{
					// Struck a wall
					if (P_BounceWall (mo))
					{
						// [BC] If the object was destroyed in P_BounceWall, no need to play
						// the bounce sound.
						if (( mo->ObjectFlags & OF_MassDestruction ) == false )
						{
							// [BC] Actors don't yet have a bounce sound, so this will have to be hacked for now.
							if ( mo->GetClass( ) == RUNTIME_CLASS( AGrenade ) && !(mo->flags3 & MF3_NOWALLBOUNCESND))
								S_Sound( mo, CHAN_VOICE, "weapons/grbnce", 1, ATTN_IDLE );
							else if (mo->SeeSound && !(mo->flags3 & MF3_NOWALLBOUNCESND))
							{
								S_SoundID (mo, CHAN_VOICE, mo->SeeSound, 1, ATTN_IDLE);
							}
						}
						DoRipping = false;
						return;
					}
				}
				if (BlockingMobj &&
					(BlockingMobj->flags2 & MF2_REFLECTIVE))
				{
					angle = R_PointToAngle2(BlockingMobj->x,
													BlockingMobj->y,
													mo->x, mo->y);

				// Change angle for deflection/reflection
					if (mo->AdjustReflectionAngle (BlockingMobj, angle))
					{
						goto explode;
					}

					// Reflect the missile along angle
					mo->angle = angle;
					angle >>= ANGLETOFINESHIFT;
					mo->momx = FixedMul (mo->Speed>>1, finecosine[angle]);
					mo->momy = FixedMul (mo->Speed>>1, finesine[angle]);
					mo->momz = -mo->momz/2;
					if (mo->flags2 & MF2_SEEKERMISSILE)
					{
						mo->tracer = mo->target;
					}
					mo->target = BlockingMobj;
					DoRipping = false;
					return;
				}
explode:
				// explode a missile
				if (ceilingline &&
					ceilingline->backsector &&
					ceilingline->backsector->ceilingpic == skyflatnum &&
					mo->z >= ceilingline->backsector->ceilingplane.ZatPoint (mo->x, mo->y) && //killough
					!(mo->flags3 & MF3_SKYEXPLODE))
				{
					// Hack to prevent missiles exploding against the sky.
					// Does not handle sky floors.

					// Player didn't strike another player with this missile.
					if ( mo->target && mo->target->player )
						mo->target->player->ulConsecutiveHits = 0;

					mo->Destroy ();
					DoRipping = false;
					return;
				}

				// Potentially reward the player who shot this missile with an accuracy/precision medal.
				if ((( mo->ulSTFlags & STFL_EXPLODEONDEATH ) == false ) && mo->target && mo->target->player )
				{
					if ( mo->target->player->bStruckPlayer )
						PLAYER_StruckPlayer( mo->target->player );
					else
						mo->target->player->ulConsecutiveHits = 0;
				}

				// [RH] Don't explode on horizon lines.
				if (BlockingLine != NULL && BlockingLine->special == Line_Horizon)
				{
					mo->Destroy ();
					DoRipping = false;
					return;
				}
				P_ExplodeMissile (mo, BlockingLine, BlockingMobj);
				DoRipping = false;
				return;
			}
			else
			{
				mo->momx = mo->momy = 0;
				steps = 0;
			}
		}
		else
		{
			if (mo->x != ptryx || mo->y != ptryy)
			{
				// If the new position does not match the desired position, the player
				// must have gone through a teleporter, so stop moving right now if it
				// was a regular teleporter. If it was a line-to-line or fogless teleporter,
				// the move should continue, but startx and starty need to change.
				if (mo->momx == 0 && mo->momy == 0)
				{
					step = steps;
				}
				else
				{
					startx = mo->x - Scale (xmove, step, steps);
					starty = mo->y - Scale (ymove, step, steps);
				}
			}
		}
	} while (++step <= steps);

	DoRipping = false;

	// Friction

	if (player && player->mo == mo && player->cheats & CF_NOMOMENTUM)
	{ // debug option for no sliding at all
		mo->momx = mo->momy = 0;
		player->momx = player->momy = 0;
		return;
	}

	if (mo->flags & (MF_MISSILE | MF_SKULLFLY))
	{ // no friction for missiles
		return;
	}

	if (mo->z > mo->floorz && !(mo->flags2 & MF2_ONMOBJ) &&
		(!(mo->flags2 & MF2_FLY) || !(mo->flags & MF_NOGRAVITY)) && !mo->waterlevel)
	{ // [RH] Friction when falling is available for larger aircontrols
		if (player != NULL && level.airfriction != FRACUNIT)
		{
			mo->momx = FixedMul (mo->momx, level.airfriction);
			mo->momy = FixedMul (mo->momy, level.airfriction);

			if (player->mo == mo)		//  Not voodoo dolls
			{
				player->momx = FixedMul (player->momx, level.airfriction);
				player->momy = FixedMul (player->momy, level.airfriction);
			}
		}
		return;
	}

	if (mo->flags & MF_CORPSE)
	{ // Don't stop sliding if halfway off a step with some momentum
		if (mo->momx > FRACUNIT/4 || mo->momx < -FRACUNIT/4
			|| mo->momy > FRACUNIT/4 || mo->momy < -FRACUNIT/4)
		{
			if (mo->floorz > mo->Sector->floorplane.ZatPoint (mo->x, mo->y))
			{
				unsigned i;
				for(i=0;i<mo->Sector->e->ffloors.Size();i++)
				{
					// Sliding around on 3D floors looks extremely bad so
					// if the floor comes from one in the current sector stop sliding the corpse!
					F3DFloor * rover=mo->Sector->e->ffloors[i];
					if (!(rover->flags&FF_EXISTS)) continue;
					if (rover->flags&FF_SOLID && rover->top.plane->ZatPoint(mo->x,mo->y)==mo->floorz) break;
				}
				if (i==mo->Sector->e->ffloors.Size()) return;
			}
		}
	}

	// killough 11/98:
	// Stop voodoo dolls that have come to rest, despite any
	// moving corresponding player:
	if (mo->momx > -STOPSPEED && mo->momx < STOPSPEED
		&& mo->momy > -STOPSPEED && mo->momy < STOPSPEED
		&& (!player || (player->mo != mo)
			|| !(player->cmd.ucmd.forwardmove | player->cmd.ucmd.sidemove)))
	{
		// if in a walking frame, stop moving
		// killough 10/98:
		// Don't affect main player when voodoo dolls stop:
		if (player && player->mo == mo)// && !(player->cheats & CF_PREDICTING))
		{
			if ((( NETWORK_GetState( ) == NETSTATE_CLIENT ) && (( player - players ) != consoleplayer )) == false )
				player->mo->PlayIdle ();
		}

		mo->momx = mo->momy = 0;

		// killough 10/98: kill any bobbing momentum too (except in voodoo dolls)
		if (player && player->mo == mo)
			player->momx = player->momy = 0; 
	}
	else
	{
		// phares 3/17/98
		// Friction will have been adjusted by friction thinkers for icy
		// or muddy floors. Otherwise it was never touched and
		// remained set at ORIG_FRICTION
		//
		// killough 8/28/98: removed inefficient thinker algorithm,
		// instead using touching_sectorlist in P_GetFriction() to
		// determine friction (and thus only when it is needed).
		//
		// killough 10/98: changed to work with new bobbing method.
		// Reducing player momentum is no longer needed to reduce
		// bobbing, so ice works much better now.

		fixed_t friction = P_GetFriction (mo, NULL);

		mo->momx = FixedMul (mo->momx, friction);
		mo->momy = FixedMul (mo->momy, friction);

		// killough 10/98: Always decrease player bobbing by ORIG_FRICTION.
		// This prevents problems with bobbing on ice, where it was not being
		// reduced fast enough, leading to all sorts of kludges being developed.

		if (player && player->mo == mo)		//  Not voodoo dolls
		{
			player->momx = FixedMul (player->momx, ORIG_FRICTION);
			player->momy = FixedMul (player->momy, ORIG_FRICTION);
		}
	}
}

//*****************************************************************************
// prBoom version of XY movement.
void P_OldXYMovement( AActor *mo, bool bForceSlide )
{
	player_t *player;
	fixed_t ptryx, ptryy;
	fixed_t xmove, ymove;
	fixed_t	maxmove;

	maxmove = MAXMOVE;

	xmove = clamp( mo->momx, -maxmove, maxmove );
	ymove = clamp( mo->momy, -maxmove, maxmove );

	if (!(mo->momx | mo->momy)) // Any momentum?
	{
		if (mo->flags & MF_SKULLFLY)
		{

			// the skull slammed into something
			mo->flags &= ~MF_SKULLFLY;
			mo->momz = 0;

			mo->SetState (mo->SeeState != NULL ? mo->SeeState : mo->SpawnState);
		}
		return;
	}

	player = mo->player;

	maxmove /= 2;

	do
	{
		// killough 8/9/98: fix bug in original Doom source:
		// Large negative displacements were never considered.
		// This explains the tendency for Mancubus fireballs
		// to pass through walls.
		// CPhipps - compatibility optioned
/*
		if ( 0 )
//		if (xmove > MAXMOVE/2 || ymove > MAXMOVE/2 ||
//			xmove < -MAXMOVE/2 || ymove < -MAXMOVE/2)
		{
			ptryx = mo->x + xmove/2;
			ptryy = mo->y + ymove/2;
			xmove >>= 1;
			ymove >>= 1;
		}
*/
		if (xmove > maxmove || ymove > maxmove)
		{
			ptryx = mo->x + (xmove >>= 1);
			ptryy = mo->y + (ymove >>= 1);
		}
		else
		{
			ptryx = mo->x + xmove;
			ptryy = mo->y + ymove;
			xmove = ymove = 0;
		}

		// killough 3/15/98: Allow objects to drop off

		if (!P_OldTryMove (mo, ptryx, ptryy, true))
		{
			// blocked move

			// killough 8/11/98: bouncing off walls
			// killough 10/98:
			// Add ability for objects other than players to bounce on ice
/*	  
			if (!(mo->flags & MF_MISSILE) && 
				(mo->flags & MF_BOUNCES ||
				(!player && blockline &&
				variable_friction && mo->z <= mo->floorz &&
				P_GetFriction(mo, NULL) > ORIG_FRICTION)))
			{
				if (blockline)
				{
					fixed_t r = ((blockline->dx >> FRACBITS) * mo->momx +
						(blockline->dy >> FRACBITS) * mo->momy) /
						((blockline->dx >> FRACBITS)*(blockline->dx >> FRACBITS)+
						(blockline->dy >> FRACBITS)*(blockline->dy >> FRACBITS));
				
					fixed_t x = FixedMul(r, blockline->dx);
					fixed_t y = FixedMul(r, blockline->dy);

					// reflect momentum away from wall
					mo->momx = x*2 - mo->momx;
					mo->momy = y*2 - mo->momy;

					// if under gravity, slow down in
					// direction perpendicular to wall.
					if (!(mo->flags & MF_NOGRAVITY))
					{
						mo->momx = (mo->momx + x)/2;
						mo->momy = (mo->momy + y)/2;
					}
				}
				else
					mo->momx = mo->momy = 0;
			}
*/
			if ( 0 )
			{
			}
			else
			{
				if (player)   // try to slide along it
					P_OldSlideMove (mo);

				// [BC] This function is only potentially being used for player movement.
/*
				else 
				{
					if (mo->flags & MF_MISSILE)
					{
						// explode a missile

						if (ceilingline &&
							ceilingline->backsector &&
							ceilingline->backsector->ceilingpic == skyflatnum)
							if (demo_compatibility ||  // killough
							mo->z > ceilingline->backsector->ceilingheight)
						{
							// Hack to prevent missiles exploding
							// against the sky.
							// Does not handle sky floors.

							P_RemoveMobj (mo);
							return;
						}
						P_ExplodeMissile (mo);
					}
					else // whatever else it is, it is now standing still in (x,y)
						mo->momx = mo->momy = 0;
				}
*/
			}
		}
	} while (xmove || ymove);

	// slow down

	/* no friction for missiles or skulls ever, no friction when airborne */
	if (mo->flags & (MF_MISSILE | MF_SKULLFLY) || mo->z > mo->floorz)
		return;

	if (mo->flags & MF_CORPSE)
	{ // Don't stop sliding if halfway off a step with some momentum
		if (mo->momx > FRACUNIT/4 || mo->momx < -FRACUNIT/4
			|| mo->momy > FRACUNIT/4 || mo->momy < -FRACUNIT/4)
		{
			if (mo->floorz > mo->Sector->floorplane.ZatPoint (mo->x, mo->y))
				return;
		}
	}

	// killough 11/98:
	// Stop voodoo dolls that have come to rest, despite any
	// moving corresponding player:
	if (mo->momx > -STOPSPEED && mo->momx < STOPSPEED
		&& mo->momy > -STOPSPEED && mo->momy < STOPSPEED
		&& (!player || (player->mo != mo)
			|| !(player->cmd.ucmd.forwardmove | player->cmd.ucmd.sidemove)))
	{
		// if in a walking frame, stop moving
		// killough 10/98:
		// Don't affect main player when voodoo dolls stop:
		if (player && player->mo == mo)// && !(player->cheats & CF_PREDICTING))
		{
			if ((( NETWORK_GetState( ) == NETSTATE_CLIENT ) && (( player - players ) != consoleplayer )) == false )
				player->mo->PlayIdle ();
		}

		mo->momx = mo->momy = 0;

		// killough 10/98: kill any bobbing momentum too (except in voodoo dolls)
		if (player && player->mo == mo)
			player->momx = player->momy = 0; 
	}
	else
	{
		/* phares 3/17/98
		*
		* Friction will have been adjusted by friction thinkers for
		* icy or muddy floors. Otherwise it was never touched and
		* remained set at ORIG_FRICTION
		*
		* killough 8/28/98: removed inefficient thinker algorithm,
		* instead using touching_sectorlist in P_GetFriction() to
		* determine friction (and thus only when it is needed).
		*
		* killough 10/98: changed to work with new bobbing method.
		* Reducing player momentum is no longer needed to reduce
		* bobbing, so ice works much better now.
		*
		* cph - DEMOSYNC - need old code for Boom demos?
		*/

		fixed_t friction = P_GetFriction(mo, NULL);

		mo->momx = FixedMul(mo->momx, friction);
		mo->momy = FixedMul(mo->momy, friction);

		/* killough 10/98: Always decrease player bobbing by ORIG_FRICTION.
		* This prevents problems with bobbing on ice, where it was not being
		* reduced fast enough, leading to all sorts of kludges being developed.
		*/

		if (player && player->mo == mo)     /* Not voodoo dolls */
		{
			player->momx = FixedMul(player->momx, ORIG_FRICTION);
			player->momy = FixedMul(player->momy, ORIG_FRICTION);
		}

    }
}

// Move this to p_inter ***
void P_MonsterFallingDamage (AActor *mo)
{
	int damage;
	int mom;

	if (!(level.flags&LEVEL_MONSTERFALLINGDAMAGE))
		return;

	mom = abs(mo->momz);
	if (mom > 35*FRACUNIT)
	{ // automatic death
		damage = 1000000;
	}
	else
	{
		damage = ((mom - (23*FRACUNIT))*6)>>FRACBITS;
	}
	damage = 1000000;	// always kill 'em
	P_DamageMobj (mo, NULL, NULL, damage, MOD_FALLING);
}

//
// P_ZMovement
//
void P_ZMovement (AActor *mo)
{
	fixed_t dist;
	fixed_t delta;
	fixed_t oldz = mo->z;

	// Intercept the stupid 'fall through 3dfloors' bug SSNTails 06-13-2002

	// [GrafZahl] This is a really ugly workaround... :(
	// But unless the collision code is completely rewritten it is the 
	// only way to avoid problems caused by incorrect positioning info...
	for(int i=0;i<mo->Sector->e->ffloors.Size();i++)
    {
		F3DFloor*  rover=mo->Sector->e->ffloors[i];

		if (!(rover->flags&FF_EXISTS)) continue;
		if(!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

		fixed_t ff_bottom=rover->bottom.plane->ZatPoint(mo->x, mo->y);
		fixed_t ff_top=rover->top.plane->ZatPoint(mo->x, mo->y);
		
		fixed_t delta1 = mo->z - (ff_bottom + ((ff_top-ff_bottom)/2));
		fixed_t delta2 = mo->z + mo->height - (ff_bottom + ((ff_top-ff_bottom)/2));

		if(ff_top > mo->floorz && abs(delta1) < abs(delta2)) mo->floorz = ff_top;
		if(ff_bottom < mo->ceilingz && abs(delta1) >= abs(delta2)) mo->ceilingz = ff_bottom;
    }

//	
// check for smooth step up
//
	if (mo->player && mo->player->mo == mo && mo->z < mo->floorz)
	{
		mo->player->viewheight -= mo->floorz - mo->z;
		mo->player->deltaviewheight = mo->player->GetDeltaViewHeight();
	}

	if (!(mo->flags2&MF2_FLOATBOB)) mo->z += mo->momz;

//
// apply gravity
//
	if (mo->z > mo->floorz && !(mo->flags & MF_NOGRAVITY))
	{
		fixed_t startmomz = mo->momz;

		if (!mo->waterlevel || mo->flags & MF_CORPSE || (mo->player &&
			!(mo->player->cmd.ucmd.forwardmove | mo->player->cmd.ucmd.sidemove)))
		{
			mo->momz -= (fixed_t)(level.gravity * mo->Sector->gravity *
				(( mo->ulSTFlags & STFL_QUARTERGRAVITY ) ? 20.48 : (mo->flags2 & MF2_LOGRAV ? 10.24 : 81.92)));
		}
		if (mo->waterlevel > 1)
		{
			fixed_t sinkspeed = mo->flags & MF_CORPSE ? -WATER_SINK_SPEED/3 : -WATER_SINK_SPEED;

			if (mo->momz < sinkspeed)
			{
				mo->momz = (startmomz < sinkspeed) ? startmomz : sinkspeed;
			}
			else
			{
				mo->momz = startmomz + ((mo->momz - startmomz) >>
					(mo->waterlevel == 1 ? WATER_SINK_SMALL_FACTOR : WATER_SINK_FACTOR));
			}
		}
	}

	if (mo->flags2&MF2_FLOATBOB) mo->z += mo->momz;

//
// adjust height
//
	if ((mo->flags & MF_FLOAT) && !(mo->flags2 & MF2_DORMANT) && mo->target)
	{	// float down towards target if too close
		if (!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
		{
			dist = P_AproxDistance (mo->x - mo->target->x, mo->y - mo->target->y);
			delta = (mo->target->z + (mo->height>>1)) - mo->z;
			if (delta < 0 && dist < -(delta*3))
				mo->z -= mo->FloatSpeed, mo->momz = 0;
			else if (delta > 0 && dist < (delta*3))
				mo->z += mo->FloatSpeed, mo->momz = 0;
		}
	}
	if (mo->player && (mo->flags & MF_NOGRAVITY) && (mo->z > mo->floorz))
	{
		mo->z += finesine[(FINEANGLES/80*level.maptime)&FINEMASK]/8;
		mo->momz = FixedMul (mo->momz, FRICTION_FLY);
	}
	if (mo->waterlevel && !(mo->flags & MF_NOGRAVITY))
	{
		mo->momz = FixedMul (mo->momz, mo->Sector->friction);
	}

//
// clip movement
//
	if (mo->z <= mo->floorz)
	{	// Hit the floor
		// [BC] Why "!mo->player"? This makes jump pads and stuff not work. They only work
		// if you walk onto the pad while on the ground (ex. you can't jump onto it), but
		// with that check, why would they even work at all? Odd.
/*
		if ((!mo->player || !(mo->player->cheats & CF_PREDICTING)) &&
			mo->Sector->SecActTarget != NULL &&
*/
		if  (mo->Sector->SecActTarget != NULL &&
			mo->Sector->floorplane.ZatPoint (mo->x, mo->y) == mo->floorz)
		{ // [RH] Let the sector do something to the actor
			mo->Sector->SecActTarget->TriggerAction (mo, SECSPAC_HitFloor);
		}
		P_CheckFor3DFloorHit(mo);
		// [RH] Need to recheck this because the sector action might have
		// teleported the actor so it is no longer below the floor.
		if (mo->z <= mo->floorz)
		{
			// [BC] We need to do the sky check first, otherwise bouncy things
			// can potentially bounce off the sky (such as grenades).
			if (( mo->flags & MF_MISSILE ) && ( mo->Sector->floorpic == skyflatnum ) && (( mo->flags3 & MF3_SKYEXPLODE ) == false ))
			{
				// Player didn't strike another player with this missile.
				if ( mo->target && mo->target->player )
					mo->target->player->ulConsecutiveHits = 0;

				mo->Destroy( );
				return;
			}

			if ((mo->flags & MF_MISSILE) &&
				(gameinfo.gametype != GAME_Doom || !(mo->flags & MF_NOCLIP)))
			{
				mo->z = mo->floorz;
				if (mo->flags2 & MF2_BOUNCETYPE)
				{
					mo->FloorBounceMissile (mo->floorsector->floorplane);
					return;
				}
				else if (mo->flags3 & MF3_NOEXPLODEFLOOR)
				{
					mo->momz = 0;
					P_HitFloor (mo);
					return;
				}
				else if (mo->flags3 & MF3_FLOORHUGGER)
				{ // Floor huggers can go up steps
					return;
				}
				else
				{
					if (mo->floorpic == skyflatnum && !(mo->flags3 & MF3_SKYEXPLODE))
					{
						// [RH] Just remove the missile without exploding it
						//		if this is a sky floor.
						mo->Destroy ();
						return;
					}
					P_HitFloor (mo);

					// [BC] Potentially reward the player who shot this missile with an accuracy/precision medal.
					if ((( mo->ulSTFlags & STFL_EXPLODEONDEATH ) == false ) && mo->target && mo->target->player )
					{
						if ( mo->target->player->bStruckPlayer )
							PLAYER_StruckPlayer( mo->target->player );
						else
							mo->target->player->ulConsecutiveHits = 0;
					}

					P_ExplodeMissile (mo, NULL, NULL);
					return;
				}
			}

			// [BC] Apply spring pads.
			if ( mo->floorsector->FloorFlags & SECF_SPRINGPAD )
			{
				mo->momz = -mo->momz;
				mo->z = mo->floorz;
				return;
			}

			if (mo->flags3 & MF3_ISMONSTER)		// Blasted mobj falling
			{
				if (mo->momz < -(23*FRACUNIT))
				{
					P_MonsterFallingDamage (mo);
				}
			}
			mo->z = mo->floorz;

			if (mo->momz < 0)
			{
				// [RH] avoid integer roundoff by doing comparisons with floats
				// I can't think of any good reason why this varied with gravity
				float minmom = 800.f /*level.gravity * mo->Sector->gravity*/ * -655.36f;
				float mom = (float)mo->momz;

				// Spawn splashes, etc.
				P_HitFloor (mo);
				if (mo->DamageType == MOD_ICE && mom < minmom)
				{
					mo->tics = 1;
					mo->momx = 0;
					mo->momy = 0;
					mo->momz = 0;
					return;
				}
				// Let the actor do something special for hitting the floor
				mo->HitFloor ();
				if (mo->player)
				{
					mo->player->jumpTics = 7;	// delay any jumping for a short while
					if (mom < minmom && !(mo->flags & MF_NOGRAVITY))
					{
						// Squat down.
						// Decrease viewheight for a moment after hitting the ground (hard),
						// and utter appropriate sound.
						PlayerLandedOnThing (mo, NULL);
					}
				}
				mo->momz = 0;
			}
			if (mo->flags & MF_SKULLFLY)
			{ // The skull slammed into something
				mo->momz = -mo->momz;
			}
			if (mo->CrashState &&
				(mo->flags & MF_CORPSE) &&
				!(mo->flags3 & MF3_CRASHED) &&
				mo->DamageType != MOD_ICE)
			{
				mo->flags3 |= MF3_CRASHED;
				mo->SetState (mo->CrashState);
			}
		}
	}

	if (mo->flags2 & MF2_FLOORCLIP)
	{
		mo->AdjustFloorClip ();
	}

	if (mo->z + mo->height > mo->ceilingz)
	{ // hit the ceiling
		if ((!mo->player /*|| !(mo->player->cheats & CF_PREDICTING)*/) &&
			mo->Sector->SecActTarget != NULL &&
			mo->Sector->ceilingplane.ZatPoint (mo->x, mo->y) == mo->ceilingz)
		{ // [RH] Let the sector do something to the actor
			mo->Sector->SecActTarget->TriggerAction (mo, SECSPAC_HitCeiling);
		}
		P_CheckFor3DCeilingHit(mo);
		// [RH] Need to recheck this because the sector action might have
		// teleported the actor so it is no longer above the ceiling.
		if (mo->z + mo->height > mo->ceilingz)
		{
			// [BC] We need to do the sky check first, otherwise bouncy things
			// can potentially bounce off the sky (such as grenades).
			if (( mo->flags & MF_MISSILE ) && ( mo->ceilingpic == skyflatnum ) && (( mo->flags3 & MF3_SKYEXPLODE ) == false ))
			{
				// Player didn't strike another player with this missile.
				if ( mo->target && mo->target->player )
					mo->target->player->ulConsecutiveHits = 0;

				mo->Destroy( );
				return;
			}

			mo->z = mo->ceilingz - mo->height;
			if (mo->flags2 & MF2_BOUNCETYPE)
			{	// ceiling bounce
				mo->FloorBounceMissile (mo->ceilingsector->ceilingplane);
				return;
			}
			if (mo->momz > 0)
				mo->momz = 0;
			if (mo->flags & MF_SKULLFLY)
			{	// the skull slammed into something
				mo->momz = -mo->momz;
			}
			if (mo->flags & MF_MISSILE &&
				(gameinfo.gametype != GAME_Doom || !(mo->flags & MF_NOCLIP)))
			{
				if (mo->flags3 & MF3_CEILINGHUGGER)
				{
					return;
				}
				if (mo->ceilingpic == skyflatnum &&  !(mo->flags3 & MF3_SKYEXPLODE))
				{
					mo->Destroy ();
					return;
				}

				// [BC] Potentially reward the player who shot this missile with an accuracy/precision medal.
				if ((( mo->ulSTFlags & STFL_EXPLODEONDEATH ) == false ) && mo->target && mo->target->player )
				{
					if ( mo->target->player->bStruckPlayer )
						PLAYER_StruckPlayer( mo->target->player );
					else
						mo->target->player->ulConsecutiveHits = 0;
				}

				P_ExplodeMissile (mo, NULL, NULL);
				return;
			}
		}
	}
	P_CheckFakeFloorTriggers (mo, oldz);
}

void P_CheckFakeFloorTriggers (AActor *mo, fixed_t oldz)
{
/*
	if (mo->player && (mo->player->cheats & CF_PREDICTING))
	{
		return;
	}
*/
	sector_t *sec = mo->Sector;
	assert (sec != NULL);
	if (sec == NULL)
	{
		return;
	}
	if (sec->heightsec != NULL && sec->SecActTarget != NULL)
	{
		sector_t *hs = sec->heightsec;
		fixed_t waterz = hs->floorplane.ZatPoint (mo->x, mo->y);
		fixed_t newz;
		fixed_t viewheight;

		if (mo->player != NULL)
		{
			viewheight = mo->player->viewheight;
		}
		else
		{
			viewheight = mo->height / 2;
		}

		if (oldz > waterz && mo->z <= waterz)
		{ // Feet hit fake floor
			sec->SecActTarget->TriggerAction (mo, SECSPAC_HitFakeFloor);
		}

		newz = mo->z + viewheight;
		oldz += viewheight;

		if (oldz <= waterz && newz > waterz)
		{ // View went above fake floor
			sec->SecActTarget->TriggerAction (mo, SECSPAC_EyesSurface);
		}
		else if (oldz > waterz && newz <= waterz)
		{ // View went below fake floor
			sec->SecActTarget->TriggerAction (mo, SECSPAC_EyesDive);
		}

		if (!(hs->MoreFlags & SECF_FAKEFLOORONLY))
		{
			waterz = hs->ceilingplane.ZatPoint (mo->x, mo->y);
			if (oldz <= waterz && newz > waterz)
			{ // View went above fake ceiling
				sec->SecActTarget->TriggerAction (mo, SECSPAC_EyesAboveC);
			}
			else if (oldz > waterz && newz <= waterz)
			{ // View went below fake ceiling
				sec->SecActTarget->TriggerAction (mo, SECSPAC_EyesBelowC);
			}
		}
	}
}

//===========================================================================
//
// PlayerLandedOnThing
//
//===========================================================================

static void PlayerLandedOnThing (AActor *mo, AActor *onmobj)
{
	bool grunted;

	if (!mo->player)
		return;

	if (mo->player->mo == mo)
	{
		mo->player->deltaviewheight = mo->momz>>3;
	}

	if ( CLIENT_PREDICT_IsPredicting( ))
		return;
/*
	if (mo->player->cheats & CF_PREDICTING)
		return;
*/
	P_FallingDamage (mo);

	// [RH] only make noise if alive
	if (!mo->player->morphTics && mo->health > 0)
	{
		grunted = false;
		// Why should this number vary by gravity?
		if (mo->momz < (fixed_t)(800.f /*level.gravity * mo->Sector->gravity*/ * -983.04f) && mo->health > 0)
		{
			S_Sound (mo, CHAN_VOICE, "*grunt", 1, ATTN_NORM);
			grunted = true;

			// [BC] Tell players that this player struck the ground (hard!)
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( mo, CHAN_VOICE, "*grunt", 127, ATTN_NORM, ULONG( mo->player - players ), SVCF_SKIPTHISCLIENT );
		}
		if (onmobj != NULL || !Terrains[P_GetThingFloorType (mo)].IsLiquid)
		{
			if (!grunted || !S_AreSoundsEquivalent (mo, "*grunt", "*land"))
			{
				S_Sound (mo, CHAN_AUTO, "*land", 1, ATTN_NORM);

				// [BC] Tell players that this player struck the ground (hard!)
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SoundActor( mo, CHAN_AUTO, "*land", 1, ATTN_NORM, ULONG( mo->player - players ), SVCF_SKIPTHISCLIENT );
			}
		}
	}
//	mo->player->centering = true;
}



//
// P_NightmareRespawn
//
void P_NightmareRespawn (AActor *mobj)
{
	fixed_t x, y, z;
	AActor *mo;
	AActor *info = mobj->GetDefault();

	// spawn the new monster (assume the spawn will be good)
	if (info->flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else if (info->flags2 & MF2_SPAWNFLOAT)
		z = FLOATRANDZ;
	else if (info->flags2 & MF2_FLOATBOB)
		z = mobj->SpawnPoint[2] << FRACBITS;
	else
		z = ONFLOORZ;

	// spawn it
	x = mobj->SpawnPoint[0] << FRACBITS;
	y = mobj->SpawnPoint[1] << FRACBITS;
	mo = Spawn (RUNTIME_TYPE(mobj), x, y, z, NO_REPLACE);

	if (z == ONFLOORZ)
		mo->z += mo->SpawnPoint[2] << FRACBITS;
	else if (z == ONCEILINGZ)
		mo->z -= mo->SpawnPoint[2] << FRACBITS;

	// something is occupying its position?
	if (!P_TestMobjLocation (mo))
	{
		//[GrafZahl] MF_COUNTKILL still needs to be checked here.
		if (mo->CountsAsKill()) level.total_monsters--;

		mo->Destroy ();
		return;		// no respawn
	}

	z = mo->z;

	// inherit attributes from deceased one
	mo->SpawnPoint[0] = mobj->SpawnPoint[0];
	mo->SpawnPoint[1] = mobj->SpawnPoint[1];
	mo->SpawnPoint[2] = mobj->SpawnPoint[2];
	mo->SpawnAngle = mobj->SpawnAngle;
	mo->SpawnFlags = mobj->SpawnFlags;
	mo->angle = ANG45 * (mobj->SpawnAngle/45);

	mo->HandleSpawnFlags ();
	mo->reactiontime = 18;
	mo->CopyFriendliness (mobj, false);
	mo->Translation = mobj->Translation;

	// [BC] If we're the server, tell clients to spawn the thing.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( mo );

	// spawn a teleport fog at old spot because of removal of the body?
	mo = Spawn ("TeleportFog", mobj->x, mobj->y, mobj->z, ALLOW_REPLACE);
	if (mo != NULL)
	{
		mo->z += TELEFOGHEIGHT;

		// [BC] If we're the server, tell clients to spawn the thing.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( mo );
	}

	// spawn a teleport fog at the new spot
	mo = Spawn ("TeleportFog", x, y, z, ALLOW_REPLACE);
	if (mo != NULL)
	{
		mo->z += TELEFOGHEIGHT;

		// [BC] If we're the server, tell clients to spawn the thing.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( mo );
	}

	// [BC] Tell clients to destroy the old monster.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DestroyThing( mobj );

	// remove the old monster
	mobj->Destroy ();
}


AActor *AActor::TIDHash[128];

//
// P_ClearTidHashes
//
// Clears the tid hashtable.
//

void AActor::ClearTIDHashes ()
{
	int i;

	for (i = 0; i < 128; i++)
		TIDHash[i] = NULL;
}

//
// P_AddMobjToHash
//
// Inserts an mobj into the correct chain based on its tid.
// If its tid is 0, this function does nothing.
//
void AActor::AddToHash ()
{
	if (tid == 0)
	{
		iprev = NULL;
		inext = NULL;
		return;
	}
	else
	{
		int hash = TIDHASH (tid);

		inext = TIDHash[hash];
		iprev = &TIDHash[hash];
		TIDHash[hash] = this;
		if (inext)
		{
			inext->iprev = &inext;
		}
	}
}

//
// P_RemoveMobjFromHash
//
// Removes an mobj from its hash chain.
//
void AActor::RemoveFromHash ()
{
	if (tid != 0 && iprev)
	{
		*iprev = inext;
		if (inext)
		{
			inext->iprev = iprev;
		}
		iprev = NULL;
		inext = NULL;
	}
	tid = 0;
}

angle_t AActor::AngleIncrements ()
{
	return ANGLE_45;
}

void AActor::PreExplode ()
{
}

void AActor::GetExplodeParms (int &damage, int &dist, bool &hurtSource)
{
}

//==========================================================================
//
// AActor :: GetMissileDamage
//
// If the actor's damage amount is an expression, evaluate it and return
// the result. Otherwise, return ((random() & mask) + add) * damage.
//
//==========================================================================

int AActor::GetMissileDamage (int mask, int add)
{
	if ((Damage & 0xC0000000) == 0x40000000)
	{
		return EvalExpressionI (Damage & 0x3FFFFFFF, this);
	}
	if (Damage == 0)
	{
		return 0;
	}
	else if (mask == 0)
	{
		return add * Damage;
	}
	else
	{
		return ((pr_missiledamage() & mask) + add) * Damage;
	}
}

void AActor::Howl ()
{
}

void AActor::NoBlockingSet ()
{
}

fixed_t AActor::GetSinkSpeed ()
{
	return FRACUNIT;
}

fixed_t AActor::GetRaiseSpeed ()
{
	return 2*FRACUNIT;
}

void AActor::HitFloor ()
{
}

bool AActor::Slam (AActor *thing)
{
	int dam = GetMissileDamage (7, 1);
	P_DamageMobj (thing, this, this, dam, MOD_HIT);
	P_TraceBleed (dam, thing, this);
	flags &= ~MF_SKULLFLY;
	momx = momy = momz = 0;
	SetState (SeeState != NULL ? SeeState : SpawnState);

	// If we are the server, tell clients about the state change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetThingState( this, SeeState != NULL ? STATE_SEE : STATE_SPAWN );

	return false;			// stop moving
}

bool AActor::SpecialBlastHandling (AActor *source, fixed_t strength)
{
	return true;
}

int AActor::SpecialMissileHit (AActor *victim)
{
	return -1;
}

bool AActor::AdjustReflectionAngle (AActor *thing, angle_t &angle)
{
	// Change angle for reflection
	if (thing->flags4&MF4_SHIELDREFLECT)
	{
		// Shield reflection (from the Centaur
		if (abs (angle - thing->angle)>>24 > 45)
			return true;	// Let missile explode

		if (thing->IsKindOf (RUNTIME_CLASS(AHolySpirit)))	// shouldn't this be handled by another flag???
			return true;

		if (pr_reflect () < 128)
			angle += ANGLE_45;
		else
			angle -= ANGLE_45;

	}
	else if (thing->flags4&MF4_DEFLECT)
	{
		// deflect (like the Heresiarch)
		if(pr_reflect() < 128) 
			angle += ANG45;
		else 
			angle -= ANG45;
	}
	else 
		angle += ANGLE_1 * ((pr_reflect()%16)-8);
	return false;
}

void AActor::PlayActiveSound ()
{
	if (ActiveSound && !S_IsActorPlayingSomething (this, CHAN_VOICE, -1))
	{
		S_SoundID (this, CHAN_VOICE, ActiveSound, 1,
			(flags3 & MF3_FULLVOLACTIVE) ? ATTN_NONE : ATTN_IDLE);
	}
}

bool AActor::IsOkayToAttack (AActor *link)
{
	if (player)				// Minotaur looking around player
	{
		if ((link->flags3 & MF3_ISMONSTER) ||
			(link->player && (link != this)))
		{
			if (!(link->flags&MF_SHOOTABLE))
			{
				return false;
			}
			if (link->flags2&MF2_DORMANT)
			{
				return false;
			}
			if ((link->IsKindOf (RUNTIME_CLASS(AMinotaur))) &&
				(link->tracer == this))
			{
				return false;
			}
			if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) && !deathmatch && link->player)
			{
				return false;
			}
			if (P_CheckSight (this, link))
			{
				return true;
			}
		}
	}
	return false;
}

void AActor::ChangeSpecial (int special, int data1, int data2,
	int data3, int data4, int data5)
{
	this->special = special;
	args[0] = data1;
	args[1] = data2;
	args[2] = data3;
	args[3] = data4;
	args[4] = data5;
}

void AActor::SetShade (DWORD rgb)
{
	PalEntry *entry = (PalEntry *)&rgb;
	alphacolor = rgb | (ColorMatcher.Pick (entry->r, entry->g, entry->b) << 24);
}

void AActor::SetShade (int r, int g, int b)
{
	alphacolor = MAKEARGB(ColorMatcher.Pick (r, g, b), r, g, b);
}

//
// P_MobjThinker
//
void AActor::Tick ()
{
	// [RH] Data for Heretic/Hexen scrolling sectors
	static const BYTE HexenScrollDirs[8] = { 64, 0, 192, 128, 96, 32, 224, 160 };
	static const BYTE HexenSpeedMuls[3] = { 5, 10, 25 };
	static const SBYTE HexenScrollies[24][2] =
	{
		{  0,  1 }, {  0,  2 }, {  0,  4 },
		{ -1,  0 }, { -2,  0 }, { -4,  0 },
		{  0, -1 }, {  0, -2 }, {  0, -4 },
		{  1,  0 }, {  2,  0 }, {  4,  0 },
		{  1,  1 }, {  2,  2 }, {  4,  4 },
		{ -1,  1 }, { -2,  2 }, { -4,  4 },
		{ -1, -1 }, { -2, -2 }, { -4, -4 },
		{  1, -1 }, {  2, -2 }, {  4, -4 }
	};

	static const BYTE HereticScrollDirs[4] = { 6, 9, 1, 4 };
	static const BYTE HereticSpeedMuls[5] = { 5, 10, 25, 30, 35 };

	AActor *onmo;

	assert (state != NULL);
	if (state == NULL)
	{
		Destroy();
		return;
	}

	// [BC] There are times when we don't want to tick this actor if it's a player.
	if ( player )
	{
		// In client mode, only allow the prediction module to tick the console player.
		if ((( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( ))) &&
			( player - players == consoleplayer ) &&
			( CLIENT_PREDICT_IsPredicting( ) == false ))
		{
			return;
		}

		// In server mode, only allow the ticking of a player if he's a client currently
		// having his movement commands executed.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
			( SERVER_GetCurrentClient( ) != ( player - players )))
		{
			return;
		}
	}

	PrevX = x;
	PrevY = y;
	PrevZ = z;

	if (flags & MF_UNMORPHED)
	{
		return;
	}

	// Apply freeze mode.
	if (( GAME_GetFreezeMode( ) == true ) &&
		(( player && (( player->cheats & CF_FREEZE ) || ( player->Powers & PW_TIMEFREEZE ))) == false ))
	{
		return;
	}

	fixed_t oldz = z;

	// [RH] Give the pain elemental vertical friction
	// This used to be in APainElemental::Tick but in order to use
	// A_PainAttack with other monsters it has to be here!
	if (flags4 & MF4_VFRICTION)
	{
		if (health >0)
		{
			if (abs (momz) < FRACUNIT/4)
			{
				momz = 0;
				flags4 &= ~MF4_VFRICTION;
			}
			else
			{
				momz = FixedMul (momz, 0xe800);
			}
		}
	}

	// [BC] Flicker this objects visibility... ala starman in SMB.
	if ( effects & FX_VISIBILITYFLICKER )
	{
		switch ( M_Random( ) % 3 )
		{
		case 0:		alpha = TRANSLUC25;	break;
		case 1:		alpha = TRANSLUC50;	break;
		case 2:		alpha = TRANSLUC75;	break;
		}
	}
	// [RH] Pulse in and out of visibility
	else if (effects & FX_VISIBILITYPULSE)
	{
		if (visdir > 0)
		{
			alpha += 0x800;
			if (alpha >= OPAQUE)
			{
				alpha = OPAQUE;
				visdir = -1;
			}
		}
		else
		{
			alpha -= 0x800;
			if (alpha <= TRANSLUC25)
			{
				alpha = TRANSLUC25;
				visdir = 1;
			}
		}
	}
	else if (flags & MF_STEALTH)
	{
		if ( i_compatflags & COMPATF_DISABLESTEALTHMONSTERS )
		{
			alpha = OPAQUE;
			visdir = 0;
		}
		else
		{
			// [RH] Fade a stealth monster in and out of visibility
			if (visdir > 0)
			{
				alpha += 2*FRACUNIT/TICRATE;
				if (alpha > OPAQUE)
				{
					alpha = OPAQUE;
					visdir = 0;
				}
			}
			else if (visdir < 0)
			{
				alpha -= 3*FRACUNIT/TICRATE/2;
				if (alpha < 0)
				{
					alpha = 0;
					visdir = 0;
				}
			}
		}
	}

	// [RH] Consider carrying sectors here
	fixed_t cummx = 0, cummy = 0;
	if ((level.Scrolls != NULL || player != NULL) && !(flags & MF_NOCLIP) && !(flags & MF_NOSECTOR))
	{
		fixed_t height, waterheight;	// killough 4/4/98: add waterheight
		const msecnode_t *node;
		int countx, county;

		// killough 3/7/98: Carry things on floor
		// killough 3/20/98: use new sector list which reflects true members
		// killough 3/27/98: fix carrier bug
		// killough 4/4/98: Underwater, carry things even w/o gravity

		// Move objects only if on floor or underwater,
		// non-floating, and clipped.

		countx = county = 0;

		for (node = touching_sectorlist; node; node = node->m_tnext)
		{
			const sector_t *sec = node->m_sector;
			fixed_t scrollx, scrolly;

			if (level.Scrolls != NULL)
			{
				const FSectorScrollValues *scroll = &level.Scrolls[sec - sectors];
				scrollx = scroll->ScrollX;
				scrolly = scroll->ScrollY;
			}
			else
			{
				scrollx = scrolly = 0;
			}

			if (player != NULL)
			{
				int scrolltype = sec->special & 0xff;

				if (scrolltype >= Scroll_North_Slow &&
					scrolltype <= Scroll_SouthWest_Fast)
				{ // Hexen scroll special
					scrolltype -= Scroll_North_Slow;
					if (i_compatflags&COMPATF_RAVENSCROLL)
					{
						angle_t fineangle = HexenScrollDirs[scrolltype / 3] * 32;
						fixed_t carryspeed = DivScale32 (HexenSpeedMuls[scrolltype % 3], 32*CARRYFACTOR);
						scrollx += FixedMul (carryspeed, finecosine[fineangle]);
						scrolly += FixedMul (carryspeed, finesine[fineangle]);
					}
					else
					{
						// Use speeds that actually match the scrolling textures!
						scrollx -= HexenScrollies[scrolltype][0] << (FRACBITS-1);
						scrolly += HexenScrollies[scrolltype][1] << (FRACBITS-1);
					}
				}
				else if (scrolltype >= Carry_East5 &&
						 scrolltype <= Carry_West35)
				{ // Heretic scroll special
					scrolltype -= Carry_East5;
					BYTE dir = HereticScrollDirs[scrolltype / 5];
					fixed_t carryspeed = DivScale32 (HereticSpeedMuls[scrolltype % 5], 32*CARRYFACTOR);
					if (scrolltype<=Carry_East35 && !(i_compatflags&COMPATF_RAVENSCROLL)) 
					{
						// Use speeds that actually match the scrolling textures!
						carryspeed = (1 << ((scrolltype%5) + FRACBITS-1));
					}
					scrollx += carryspeed * ((dir & 3) - 1);
					scrolly += carryspeed * (((dir & 12) >> 2) - 1);
				}
				else if (scrolltype == dScroll_EastLavaDamage)
				{ // Special Heretic scroll special
					if (i_compatflags&COMPATF_RAVENSCROLL)
					{
						scrollx += DivScale32 (28, 32*CARRYFACTOR);
					}
					else
					{
						// Use a speed that actually matches the scrolling texture!
						scrollx += DivScale32 (12, 32*CARRYFACTOR);
					}
				}
				else if (scrolltype == Scroll_StrifeCurrent)
				{ // Strife scroll special
					int anglespeed = sec->tag - 100;
					fixed_t carryspeed = DivScale32 (anglespeed % 10, 16*CARRYFACTOR);
					angle_t fineangle = (anglespeed / 10) << (32-3);
					fineangle >>= ANGLETOFINESHIFT;
					scrollx += FixedMul (carryspeed, finecosine[fineangle]);
					scrolly += FixedMul (carryspeed, finesine[fineangle]);
				}
			}

			if ((scrollx | scrolly) == 0)
			{
				continue;
			}
			if (flags & MF_NOGRAVITY &&
				(sec->heightsec == NULL || (sec->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC)))
			{
				continue;
			}
			height = sec->floorplane.ZatPoint (x, y);
			if (z > height)
			{
				if (sec->heightsec == NULL || (sec->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
				{
					continue;
				}

				waterheight = sec->heightsec->floorplane.ZatPoint (x, y);
				if (waterheight > height && z >= waterheight)
				{
					continue;
				}
			}

			cummx += scrollx;
			cummy += scrolly;
			if (scrollx) countx++;
			if (scrolly) county++;
		}

		// Some levels designed with Boom in mind actually want things to accelerate
		// at neighboring scrolling sector boundaries. But it is only important for
		// non-player objects.
		if (player != NULL || !(i_compatflags & COMPATF_BOOMSCROLL))
		{
			if (countx > 1)
			{
				cummx /= countx;
			}
			if (county > 1)
			{
				cummy /= county;
			}
		}
	}

	// [RH] If standing on a steep slope, fall down it
	if ((flags & MF_SOLID) && !(flags & (MF_NOCLIP|MF_NOGRAVITY)) &&
		!(flags & MF_NOBLOCKMAP) &&
		momz <= 0 &&
		floorz == z)
	{
		const secplane_t * floorplane = &floorsector->floorplane;
		static secplane_t copyplane;

		// Check 3D floors as well
		for(unsigned int i=0;i<floorsector->e->ffloors.Size();i++)
		{
			F3DFloor * rover= floorsector->e->ffloors[i];
			if(!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

			if (rover->top.plane->ZatPoint(x, y) == floorz)
			{
				copyplane = *rover->top.plane;
				if (copyplane.c<0) copyplane.FlipVert();
				floorplane = &copyplane;
				break;
			}
		}

		if (floorplane->c < STEEPSLOPE &&
			floorplane->ZatPoint (x, y) <= floorz)
		{
			const msecnode_t *node;
			bool dopush = true;

			if (floorplane->c > STEEPSLOPE*2/3)
			{
				for (node = touching_sectorlist; node; node = node->m_tnext)
				{
					const sector_t *sec = node->m_sector;
					if (sec->floorplane.c >= STEEPSLOPE)
					{
						if (floorplane->ZatPoint (x, y) >= z - MaxStepHeight)
						{
							dopush = false;
							break;
						}
					}
				}
			}
			if (dopush)
			{
				momx += floorplane->a;
				momy += floorplane->b;
			}
		}
	}

	// [RH] Missiles moving perfectly vertical need some X/Y movement, or they
	// won't hurt anything. Don't do this if damage is 0! That way, you can
	// still have missiles that go straight up and down through actors without
	// damaging anything.
	if ((flags & MF_MISSILE) && (momx|momy) == 0 && Damage != 0)
	{
		momx = 1;
	}

	// Handle X and Y momemtums
	BlockingMobj = NULL;
//	if ( player && ( player->bSpectating == false ) && ( i_compatflags & COMPATF_PLASMA_BUMP_BUG ))
//		P_OldXYMovement( this, bForceSlide );
//	else
		P_XYMovement (this, cummx, cummy);
	if (ObjectFlags & OF_MassDestruction)
	{ // actor was destroyed
		return;
	}
	if ((momx | momy) == 0 && (flags2 & MF2_BLASTED))
	{ // Reset to not blasted when momentums are gone
		flags2 &= ~MF2_BLASTED;
		if (!(flags & MF_ICECORPSE))
		{
			flags2 &= ~MF2_SLIDE;
		}
	}

	if (flags2 & MF2_FLOATBOB)
	{ // Floating item bobbing motion
		z += FloatBobDiffs[(FloatBobPhase + level.maptime) & 63];
	}
	if (momz || BlockingMobj ||
		(z != floorz && (!(flags2 & MF2_FLOATBOB) ||
		(z - FloatBobOffsets[(FloatBobPhase + level.maptime) & 63] != floorz)
		)))
	{	// Handle Z momentum and gravity
		if (((flags2 & MF2_PASSMOBJ) || (flags & MF_SPECIAL)) && !(i_compatflags & COMPATF_NO_PASSMOBJ))
		{
			if (!(onmo = P_CheckOnmobj (this)))
			{
				P_ZMovement (this);
				flags2 &= ~MF2_ONMOBJ;
			}
			else
			{
				if (player)
				{
					if (momz < (fixed_t)(level.gravity * Sector->gravity * -655.36f)
						&& !(flags&MF_NOGRAVITY))
					{
						PlayerLandedOnThing (this, onmo);
					}
				}
				if (onmo->z + onmo->height - z <= MaxStepHeight)
				{
					if (player && player->mo == this)
					{
						// Don't alter viewheight if we're just predicting.
						if ( CLIENT_PREDICT_IsPredicting( ) == false )
						{
							player->viewheight -= onmo->z + onmo->height - z;
							fixed_t deltaview = player->GetDeltaViewHeight();
							if (deltaview > player->deltaviewheight)
							{
								player->deltaviewheight = deltaview;
							}
						}
					}
					z = onmo->z + onmo->height;
				}
				flags2 |= MF2_ONMOBJ;
				momz = 0;
				if (CrashState &&
					(flags & MF_CORPSE) &&
					!(flags3 & MF3_CRASHED) &&
					DamageType != MOD_ICE)
				{
					flags3 |= MF3_CRASHED;
					SetState (CrashState);
				}
			}
		}
		else
		{
			P_ZMovement (this);
		}

		if (ObjectFlags & OF_MassDestruction)
			return;		// actor was destroyed
	}
	else if (z <= floorz)
	{
		if (CrashState &&
			(flags & MF_CORPSE) &&
			!(flags3 & MF3_CRASHED) &&
			DamageType != MOD_ICE)
		{
			flags3 |= MF3_CRASHED;
			SetState (CrashState);
		}
	}

	if (UpdateWaterLevel (oldz))
	{
		P_HitWater (this, Sector);
	}

/*
	// [RH] Don't advance if predicting a player
	if (player && (player->cheats & CF_PREDICTING))
	{
		return;
	}
*/
	// [BC] Don't tick states while predicting.
	if ( CLIENT_PREDICT_IsPredicting( ))
		return;

	// cycle through states, calling action functions at transitions
	if (tics != -1)
	{
		tics--;
				
		// you can cycle through multiple states in a tic
		// [RH] Use <= 0 instead of == 0 so that spawnstates
		// of 0 tics work as expected.
		if (tics <= 0)
		{
			assert (state != NULL);
			if (state == NULL)
			{
				Destroy();
				return;
			}
			if (!SetState (state->GetNextState()))
				return; 		// freed itself
		}
	}
	else
	{
		// The rest is server-side.
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			return;

		// check for nightmare respawn
		if (!respawnmonsters || !(flags3 & MF3_ISMONSTER) || (flags2 & MF2_DORMANT))
			return;

		movecount++;

		if (movecount < respawnmonsters)
			return;

		if (level.time & 31)
			return;

		if (pr_nightmarerespawn() > 4)
			return;

		P_NightmareRespawn (this);
	}
}

//==========================================================================
//
// AActor::UpdateWaterLevel
//
// Returns true if actor should splash
//
//==========================================================================

bool AActor::UpdateWaterLevel (fixed_t oldz, bool dosplash)
{
	BYTE lastwaterlevel = waterlevel;
	fixed_t fh=FIXED_MIN;
	bool reset=false;

	// Server will tell us what our waterlevel is.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && (( player == NULL ) || ( player - players != consoleplayer ) || ( player->bSpectating )))
		return ( false );

	waterlevel = 0;

	if (Sector == NULL)
	{
		return false;
	}

	if (Sector->MoreFlags & SECF_UNDERWATERMASK)
	{
		waterlevel = 3;
	}
	else
	{
		const sector_t *hsec = Sector->heightsec;
		if (hsec != NULL && !(hsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
		{
			fh = hsec->floorplane.ZatPoint (x, y);
			//if (hsec->MoreFlags & SECF_UNDERWATERMASK)	// also check Boom-style non-swimmable sectors!
			{
				if (z < fh)
				{
					waterlevel = 1;
					if (z + height/2 < fh)
					{
						waterlevel = 2;
						if ((player && z + player->viewheight <= fh) ||
							(z + height <= fh))
						{
							waterlevel = 3;
						}
					}
				}
				else if (z + height > hsec->ceilingplane.ZatPoint (x, y))
				{
					waterlevel = 3;
				}
				else
				{
					waterlevel=0;
				}
			}
			// even non-swimmable deep water must be checked here to do the splashes correctly
			// But the water level must be reset when this function returns!
			if (!(hsec->MoreFlags&SECF_UNDERWATER)) reset=true;

			}
			else
			{
			// Check 3D floors as well!
			for(int i=0;i<Sector->e->ffloors.Size();i++)
			{
				F3DFloor*  rover=Sector->e->ffloors[i];

				if (!(rover->flags & FF_EXISTS)) continue;
				if(!(rover->flags & FF_SWIMMABLE) || rover->flags & FF_SOLID) continue;

				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(x, y);
				fixed_t ff_top=rover->top.plane->ZatPoint(x, y);

				if(ff_top <= z || ff_bottom > (z + (height >> 1))) continue;
				
				fh=ff_top;
				if (z < fh)
				{
					waterlevel = 1;
					if (z + height/2 < fh)
					{
						waterlevel = 2;
						if ((player && z + player->viewheight <= fh) ||
							(z + height <= fh))
						{
							waterlevel = 3;
						}
					}
				}

				break;
			}
		}
	}
		
	// some additional checks to make deep sectors � la Boom splash without setting
	// the water flags. 
	if (boomwaterlevel == 0 && waterlevel != 0 && dosplash) P_HitWater(this, Sector, fh);
	boomwaterlevel=waterlevel;
	if (reset) waterlevel=lastwaterlevel;
	return false;	// we did the splash ourselves! ;)
}

//----------------------------------------------------------------------------
//
// PROC A_FreeTargMobj
//
//----------------------------------------------------------------------------

void A_FreeTargMobj (AActor *mo)
{
	mo->momx = mo->momy = mo->momz = 0;
	mo->z = mo->ceilingz + 4*FRACUNIT;
	mo->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY|MF_SOLID);
	mo->flags |= MF_CORPSE|MF_DROPOFF|MF_NOGRAVITY;
	mo->flags2 &= ~(MF2_PASSMOBJ|MF2_LOGRAV);
	mo->player = NULL;
}

//==========================================================================
//
// A_GenericFreezeDeath
//
//==========================================================================

void A_GenericFreezeDeath (AActor *actor)
{
	actor->Translation = TRANSLATION(TRANSLATION_Standard, 7);
	A_FreezeDeath (actor);
}

//==========================================================================
//
// AActor stuff
//
//==========================================================================

FState AActor::States[] =
{
	S_NORMAL (TNT1, 'A', -1, NULL, NULL),
	S_NORMAL (TNT1, 'E', 1050, A_FreeTargMobj, NULL),
	S_NORMAL (TNT1, 'A', 1, NULL, NULL),	// S_NULL

	// Generic freeze death frames. Woo!
	S_NORMAL (----, 'A', 5, A_GenericFreezeDeath, &States[4]),
	S_NORMAL (----, 'A', 1, A_FreezeDeathChunks, &States[4])
};

BEGIN_DEFAULTS (AActor, Any, -1, 0)
	PROP_XScale (FRACUNIT)
	PROP_YScale (FRACUNIT)
	PROP_SpawnState (2)
	PROP_SpawnHealth (1000)
	PROP_ReactionTime (8)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Mass (100)
	PROP_RenderStyle (STYLE_Normal)
	PROP_Alpha (FRACUNIT)
	PROP_MinMissileChance (200)
	PROP_MeleeRange(44)		// MELEERANGE(64) - 20
	PROP_MaxDropOffHeight(24)
	PROP_MaxStepHeight(24)
	PROP_BounceFactor(FRACUNIT*7/10)
	PROP_BounceCount(-1)
	PROP_FloatSpeed(4)
END_DEFAULTS

//*****************************************************************************
//
void ACTOR_ClearNetIDList( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_NETID; ulIdx++ )
	{
		g_NetIDList[ulIdx].bFree = true;
		g_NetIDList[ulIdx].pActor = NULL;
	}

	g_ulFirstFreeNetID = 1;
}

//*****************************************************************************
//
ULONG ACTOR_GetNewNetID( void )
{
	ULONG	ulID;

	// Actor's network ID is the first availible net ID.
	ulID = g_ulFirstFreeNetID;

	do
	{
		g_ulFirstFreeNetID++;
		if ( g_ulFirstFreeNetID >= MAX_NETID )
			g_ulFirstFreeNetID = 1;

		if ( g_ulFirstFreeNetID == ulID )
		{
			Printf( "ACTOR_GetNewNetID: Network ID limit reached (>=%d actors)\n", MAX_NETID );
			return ( 0 );
		}
	} while ( g_NetIDList[g_ulFirstFreeNetID].bFree == false );

	return ( ulID );
}

//==========================================================================
//
// P_SpawnMobj
//
//==========================================================================

AActor *AActor::StaticSpawn (const PClass *type, fixed_t ix, fixed_t iy, fixed_t iz, replace_t allowreplacement)
{
	g_lSpawnCount++;
	clock( g_SpawnCycles );

	if (type == NULL)
	{
		I_Error ("Tried to spawn a class-less actor\n");
	}

	if (type->ActorInfo == NULL)
	{
		I_Error ("%s is not an actor\n", type->TypeName.GetChars());
	}

	if (allowreplacement)
		type = type->ActorInfo->GetReplacement()->Class;


	AActor *actor;

	actor = static_cast<AActor *>(const_cast<PClass *>(type)->CreateNew ());

	actor->x = actor->PrevX = ix;
	actor->y = actor->PrevY = iy;
	actor->z = actor->PrevZ = iz;
	actor->picnum = 0xffff;

	FRandom &rng = pr_spawnmobj;

	if (gameskill == sk_nightmare && actor->flags3 & MF3_ISMONSTER)
		actor->reactiontime = 0;

	if (actor->flags3 & MF3_ISMONSTER)
	{
		actor->LastLook.PlayerNumber = rng() % MAXPLAYERS;
		actor->TIDtoHate = 0;
	}

	// Set the state, but do not use SetState, because action
	// routines can't be called yet.  If the spawnstate has an action
	// routine, it will not be called.
	FState *st = actor->SpawnState;
	actor->state = st;
	actor->tics = st->GetTics();
	actor->sprite = st->sprite.index;
	actor->frame = st->GetFrame();
	actor->renderflags = (actor->renderflags & ~RF_FULLBRIGHT) | st->GetFullbright();
	actor->touching_sectorlist = NULL;	// NULL head of sector list // phares 3/13/98
	actor->Speed = actor->GetClass()->Meta.GetMetaFixed(AMETA_FastSpeed, actor->Speed);

	// set subsector and/or block links
	actor->LinkToWorld (SpawningMapThing);
	if (SpawningMapThing || !type->IsDescendantOf (RUNTIME_CLASS(APlayerPawn)))
	{
		actor->dropoffz =			// killough 11/98: for tracking dropoffs
		actor->floorz = actor->Sector->floorplane.ZatPoint (ix, iy);
		actor->ceilingz = actor->Sector->ceilingplane.ZatPoint (ix, iy);
		actor->floorsector = actor->Sector;
		actor->floorpic = actor->floorsector->floorpic;
		actor->ceilingsector = actor->Sector;
		actor->ceilingpic = actor->ceilingsector->ceilingpic;
	}
	else
	{
		P_FindFloorCeiling (actor);
		actor->floorz = tmffloorz;
		actor->dropoffz = tmfdropoffz;
		actor->ceilingz = tmfceilingz;
		actor->floorpic = tmffloorpic;
		actor->floorsector = tmffloorsector;
		actor->ceilingpic = tmfceilingpic;
		actor->ceilingsector = tmfceilingsector;
	}

	actor->SpawnPoint[0] = ix >> FRACBITS;
	actor->SpawnPoint[1] = iy >> FRACBITS;

	if (iz == ONFLOORZ)
	{
		actor->z = actor->floorz;
	}
	else if (iz == ONCEILINGZ)
	{
		actor->z = actor->ceilingz - actor->height;
	}
	else if (iz == FLOATRANDZ)
	{
		fixed_t space = actor->ceilingz - actor->height - actor->floorz;
		if (space > 48*FRACUNIT)
		{
			space -= 40*FRACUNIT;
			actor->z = MulScale8 (space, rng()) + actor->floorz + 40*FRACUNIT;
		}
		else
		{
			actor->z = actor->floorz;
		}
	}
	else
	{
		actor->SpawnPoint[2] = (actor->z - actor->floorz) >> FRACBITS;
	}

	if (actor->flags2 & MF2_FLOATBOB)
	{ // Prime the bobber
		actor->FloatBobPhase = rng();
		actor->z += FloatBobOffsets[(actor->FloatBobPhase + level.maptime - 1) & 63];
	}
	if (actor->flags2 & MF2_FLOORCLIP)
	{
		actor->AdjustFloorClip ();
	}
	else
	{
		actor->floorclip = 0;
	}
	actor->UpdateWaterLevel (actor->z);
	if (!SpawningMapThing)
	{
		actor->BeginPlay ();
		if (actor->ObjectFlags & OF_MassDestruction)
		{
			unclock( g_SpawnCycles );
			return NULL;
		}
	}
	// [RH] Count monsters whenever they are spawned.
	if (actor->CountsAsKill())
	{
		level.total_monsters++;

		// [BC] Do some invasion mode stuff.
		if (( invasion ) &&
			( INVASION_IncreaseNumMonstersOnSpawn( )) &&
			( NETWORK_GetState( ) != NETSTATE_CLIENT ))
		{
			INVASION_SetNumMonstersLeft( INVASION_GetNumMonstersLeft( ) + 1 );

			if ( actor->GetClass( ) == RUNTIME_CLASS( AArchvile ))
				INVASION_SetNumArchVilesLeft( INVASION_GetNumArchVilesLeft( ) + 1 );

			// [BC] If we're the server, tell the client how many monsters are left.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetInvasionNumMonstersLeft( );
		}
	}
	// [RH] Same, for items
	if (actor->flags & MF_COUNTITEM)
	{
		level.total_items++;
	}

	if ((( actor->ulNetworkFlags & NETFL_NONETID ) == false ) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		actor->lNetID = ACTOR_GetNewNetID( );
		g_NetIDList[actor->lNetID].pActor = actor;
		g_NetIDList[actor->lNetID].bFree = false;
	}
	else
		actor->lNetID = -1;

	// Check if the flag or skull has spawned in an instant return zone.
	if (( actor->Sector->MoreFlags & SECF_RETURNZONE ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ))
	{
		if ( actor->GetClass( ) == TEAM_GetFlagItem( TEAM_BLUE ))
			TEAM_ExecuteReturnRoutine( TEAM_BLUE, NULL );

		if ( actor->GetClass( ) == TEAM_GetFlagItem( TEAM_RED ))
			TEAM_ExecuteReturnRoutine( TEAM_RED, NULL );

		if (( oneflagctf ) && ( actor->GetClass( ) == PClass::FindClass( "WhiteFlag" )))
			TEAM_ExecuteReturnRoutine( NUM_TEAMS, NULL );
	}

	gl_SetActorLights(actor);

	unclock( g_SpawnCycles );
	return actor;
}

AActor *Spawn (const char *type, fixed_t x, fixed_t y, fixed_t z, replace_t allowreplacement)
{
	const PClass *cls = PClass::FindClass(type);
	if (cls == NULL) 
	{
		I_Error("Attempt to spawn actor of unknown type '%s'\n", type);
	}
	return AActor::StaticSpawn (cls, x, y, z, allowreplacement);
}

void AActor::LevelSpawned ()
{
	if (tics > 0 && !(flags4 & MF4_SYNCHRONIZED))
		tics = 1 + (pr_spawnmapthing() % tics);
	angle_t incs = AngleIncrements ();
	angle -= angle % incs;
	flags &= ~MF_DROPPED;		// [RH] clear MF_DROPPED flag
	HandleSpawnFlags ();

	// [BC] Mark this item as having been spawned on the map.
	ulSTFlags |= STFL_LEVELSPAWNED;
}

void AActor::HandleSpawnFlags ()
{
	if (SpawnFlags & MTF_AMBUSH)
	{
		flags |= MF_AMBUSH;
	}
	if (SpawnFlags & MTF_DORMANT)
	{
		Deactivate (NULL);
	}
	if (SpawnFlags & MTF_STANDSTILL)
	{
		flags4 |= MF4_STANDSTILL;
	}
	if (SpawnFlags & MTF_FRIENDLY)
	{
		flags |= MF_FRIENDLY;
		// Friendlies don't count as kills!
		if (flags & MF_COUNTKILL)
		{
			flags &= ~MF_COUNTKILL;
			level.total_monsters--;
		}
	}
	if (SpawnFlags & MTF_SHADOW)
	{
		flags |= MF_SHADOW;
		RenderStyle = STYLE_Translucent;
		alpha = TRANSLUC25;
	}
	else if (SpawnFlags & MTF_ALTSHADOW)
	{
		RenderStyle = STYLE_None;
	}
}

void AActor::BeginPlay ()
{
}

void AActor::Activate (AActor *activator)
{
	if ((flags3 & MF3_ISMONSTER) && (health > 0 || (flags & MF_ICECORPSE)))
	{
		if (flags2 & MF2_DORMANT)
		{
			flags2 &= ~MF2_DORMANT;
			tics = 1;
		}
	}

	// If we're the server, tell clients to activate this actor.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_ThingActivate( this, activator );
}

void AActor::Deactivate (AActor *activator)
{
	if ((flags3 & MF3_ISMONSTER) && (health > 0 || (flags & MF_ICECORPSE)))
	{
		if (!(flags2 & MF2_DORMANT))
		{
			flags2 |= MF2_DORMANT;
			tics = -1;
		}
	}

	// [BC] If we're the server, tell clients to deactivate this actor.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_ThingDeactivate( this, activator );
}

//
// P_RemoveMobj
//

void AActor::Destroy ()
{
	ULONG	ulIdx;

	// [BC] Free it's network ID.
//	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
//	{
		if ( lNetID && ( lNetID < 65536 ))
		{
			g_NetIDList[lNetID].bFree = true;
			g_NetIDList[lNetID].pActor = NULL;
		}
//	}

	lNetID = 0;

	// [BC] If this is a bot's goal object, tell the bot it's been removed.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( players[ulIdx].pSkullBot == NULL ) ||
			( BOTS_IsBotInitialized( ulIdx ) == false ))
		{
			continue;
		}

		if ( this == players[ulIdx].pSkullBot->m_pGoalActor )
		{
			players[ulIdx].pSkullBot->m_pGoalActor = NULL;
			players[ulIdx].pSkullBot->PostEvent( BOTEVENT_GOAL_REMOVED );
			players[ulIdx].pSkullBot->m_ulPathType = BOTPATHTYPE_NONE;
//			ASTAR_ClearPath( ulIdx );
		}
	}

	// [RH] Destroy any inventory this actor is carrying
	DestroyAllInventory ();

	// [RH] Unlink from tid chain
	RemoveFromHash ();

	// unlink from sector and block lists
	UnlinkFromWorld ();
	flags |= MF_NOSECTOR|MF_NOBLOCKMAP;

	// Delete all nodes on the current sector_list			phares 3/16/98
	P_DelSector_List();

	// Transform any playing sound into positioned, non-actor sounds.
	S_RelinkSound (this, NULL);

	Super::Destroy ();
}

//===========================================================================
//
// AdjustFloorClip
//
//===========================================================================

void AActor::AdjustFloorClip ()
{
	if (flags3 & MF3_SPECIALFLOORCLIP)
	{
		return;
	}

	fixed_t oldclip = floorclip;
	fixed_t shallowestclip = FIXED_MAX;
	const msecnode_t *m;

	// possibly standing on a 3D-floor!
	if (Sector->e->ffloors.Size() && z>Sector->floorplane.ZatPoint(x,y)) floorclip=0;

	// [RH] clip based on shallowest floor player is standing on
	// If the sector has a deep water effect, then let that effect
	// do the floorclipping instead of the terrain type.
	for (m = touching_sectorlist; m; m = m->m_tnext)
	{
		if ((m->m_sector->heightsec == NULL ||
			 m->m_sector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
			m->m_sector->floorplane.ZatPoint (x, y) == z)
		{
			fixed_t clip = Terrains[TerrainTypes[m->m_sector->floorpic]].FootClip;
			if (clip < shallowestclip)
			{
				shallowestclip = clip;
			}
		}
	}
	if (shallowestclip == FIXED_MAX)
	{
		floorclip = 0;
	}
	else
	{
		floorclip = shallowestclip;
	}
	if (player && player->mo == this && oldclip != floorclip)
	{
		// Don't adjust viewheight if we're just predicting.
		if ( CLIENT_PREDICT_IsPredicting( ) == false )
		{
			player->viewheight -= oldclip - floorclip;
			player->deltaviewheight = player->GetDeltaViewHeight();
		}
	}
}

//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged between levels.
//
EXTERN_CVAR (Bool, chasedemo)
extern bool demonew;

void P_SpawnPlayer (mapthing2_t *mthing, bool bClientUpdate, player_t *p, bool tempplayer)
{
	int		  playernum;
	APlayerPawn *mobj, *oldactor;
	BYTE	  state;
	fixed_t		SpawnX;
	fixed_t		SpawnY;
	angle_t		SpawnAngle;
	// [BC]
	LONG		lSkin;
	AInventory	*pInventory;

	// [BC] This is not client-side.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	if ( p == NULL )
	{
		// [RH] Things 4001-? are also multiplayer starts. Just like 1-4.
		//		To make things simpler, figure out which player is being
		//		spawned here.
		if (mthing->type <= 4 || gameinfo.gametype == GAME_Strife)		// don't forget Strife's starts 5-8 here!
		{
			playernum = mthing->type - 1;
		}
		else if (gameinfo.gametype != GAME_Hexen)
		{
			playernum = mthing->type - 4001 + 4;
		}
		else
		{
			playernum = mthing->type - 9100 + 4;
		}

		// not playing?
		if (playernum >= MAXPLAYERS || !playeringame[playernum])
			return;

		p = &players[playernum];
	}
	else
		playernum = p - players;

	if (p->cls == NULL)
	{
		// [GRB] Pick a class from player class list
		if (PlayerClasses.Size () > 1)
		{
			int type;

			// [BC] Cooperative is !deathmatch && !teamgame.
			// [BB] The server host always picks the multiplayer class choice.
			if ( ((!deathmatch && !teamgame) || ( NETWORK_GetState( ) == NETSTATE_SINGLE )) && !( NETWORK_GetState( ) == NETSTATE_SERVER ) )
			{
				type = SinglePlayerClass[playernum];
			}
			else
			{
				type = p->userinfo.PlayerClass;
				if (type < 0)
				{
					type = pr_multiclasschoice() % PlayerClasses.Size ();
				}
			}
			p->CurrentPlayerClass = type;
		}
		else
		{
			p->CurrentPlayerClass = 0;
		}
		p->cls = PlayerClasses[p->CurrentPlayerClass].Type;
	}

	if (( dmflags2 & DF2_SAME_SPAWN_SPOT ) &&
		(( p->playerstate == PST_REBORN ) || ( p->playerstate == PST_REBORNNOINVENTORY )) && 
		( deathmatch == false ) &&
		( teamgame == false ) &&
		( gameaction != ga_worlddone ) &&
		( p->bSpawnOkay ))
	{
		SpawnX = p->SpawnX;
		SpawnY = p->SpawnY;
		SpawnAngle = p->SpawnAngle;
	}
	else
	{
		SpawnX = mthing->x << FRACBITS;
		SpawnY = mthing->y << FRACBITS;
		SpawnAngle = ANG45 * (mthing->angle/45);
	}

	mobj = static_cast<APlayerPawn *>
		(Spawn (p->cls, SpawnX, SpawnY, ONFLOORZ, NO_REPLACE));

	mobj->FriendPlayer = playernum + 1;	// [RH] players are their own friends
	oldactor = p->mo;
	p->mo = mobj;
	mobj->player = p;
	state = p->playerstate;
	// [BC] Added PST_REBORNNOINVENTORY, PST_ENTERNOINVENTORY.
	if (state == PST_REBORN || state == PST_ENTER || state == PST_REBORNNOINVENTORY || state == PST_ENTERNOINVENTORY)
	{
		G_PlayerReborn (playernum);
	}
	else if (oldactor != NULL && oldactor->player == p && !tempplayer)
	{
		// Move the voodoo doll's inventory to the new player.
		mobj->ObtainInventory (oldactor);
		FBehavior::StaticStopMyScripts (oldactor);	// cancel all ENTER/RESPAWN scripts for the voodoo doll
	}

	// [GRB] Reset skin
	p->userinfo.skin = R_FindSkin (skins[p->userinfo.skin].name, p->CurrentPlayerClass);
	// [BC] The server does not have a status bar.
	if ( StatusBar )
		StatusBar->SetFace (&skins[p->userinfo.skin]);

	// [RH] Be sure the player has the right translation
	R_BuildPlayerTranslation (playernum);

	// [RH] set color translations for player sprites
	mobj->Translation = TRANSLATION(TRANSLATION_Players,playernum);

	mobj->angle = SpawnAngle;
	mobj->pitch = mobj->roll = 0;
	mobj->health = p->health;
	mobj->lFixedColormap = 0;

	//Added by MC: Identification (number in the players[MAXPLAYERS] array)
    mobj->id = playernum;

	// [RH] Set player sprite based on skin
	// [BC] Handle cl_skins here.
	if ( cl_skins <= 0 )
	{
		lSkin = R_FindSkin( "base", p->CurrentPlayerClass );
		mobj->flags4 |= MF4_NOSKIN;
	}
	else if ( cl_skins >= 2 )
	{
		if ( skins[p->userinfo.skin].bCheat )
		{
			lSkin = R_FindSkin( "base", p->CurrentPlayerClass );
			mobj->flags4 |= MF4_NOSKIN;
		}
		else
			lSkin = p->userinfo.skin;
	}
	else
		lSkin = p->userinfo.skin;

	if (( lSkin < 0 ) || ( lSkin >= numskins ))
		lSkin = R_FindSkin( "base", p->CurrentPlayerClass );

	mobj->sprite = skins[lSkin].sprite;
	// [GZDoom]
	mobj->scaleX = mobj->scaleY = skins[lSkin].Scale;
	//mobj->xscale = mobj->yscale = skins[lSkin].scale;

	p->DesiredFOV = p->FOV = 90.f;
	p->camera = p->mo;
	p->playerstate = PST_LIVE;
	p->refire = 0;
	p->damagecount = 0;
	p->bonuscount = 0;
	p->morphTics = 0;
	p->extralight = 0;
	p->fixedcolormap = 0;
	p->viewheight = mobj->ViewHeight;
	p->attacker = NULL;
	p->BlendR = p->BlendG = p->BlendB = p->BlendA = 0.f;
	p->air_finished = level.time + level.airsupply;
	p->Uncrouch();

	p->momx = p->momy = 0;		// killough 10/98: initialize bobbing to 0.

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		if (players[consoleplayer].camera == oldactor)
		{
			players[consoleplayer].camera = mobj;
		}
	}

	// [RH] Allow chasecam for demo watching
	if ((demoplayback || demonew) && chasedemo)
		p->cheats = CF_CHASECAM;

	// setup gun psprite
	if (!tempplayer)
	{
		// This can also start a script so don't do it for
		// the dummy player.
		P_SetupPsprites (p);
	}

	if (deathmatch)
	{ // Give all cards in death match mode.
		p->mo->GiveDeathmatchInventory ();
	}
	// [BC] Don't filter coop inventory in teamgame mode.
	else if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) && state == PST_REBORN && oldactor != NULL && ( teamgame == false ))
	{ // Special inventory handling for respawning in coop
		p->mo->FilterCoopRespawnInventory (oldactor);
	}
	if (oldactor != NULL)
	{ // Remove any inventory left from the old actor. Coop handles
	  // it above, but the other modes don't.
		oldactor->DestroyAllInventory();
	}

	if (StatusBar != NULL && (playernum == consoleplayer || StatusBar->GetPlayer() == playernum))
	{
		StatusBar->AttachToPlayer (p);
	}

	if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
	{
		unsigned an = ( ANG45 * (mthing->angle/45) ) >> ANGLETOFINESHIFT;
		Spawn ("TeleportFog", mobj->x+20*finecosine[an], mobj->y+20*finesine[an], mobj->z + TELEFOGHEIGHT, ALLOW_REPLACE);
	}

	if ( p->bSpectating )
	{
		if ( p->mo )
		{
			// Don't allow "dead" spectators!
			players[playernum].playerstate = PST_LIVE;
			players[playernum].health = players[playernum].mo->health = deh.StartHealth;

			// Make the layer unshootable, etc.
			p->mo->flags2 |= (MF2_CANNOTPUSH);
			p->mo->flags &= ~(MF_SOLID|MF_SHOOTABLE|MF_PICKUP);
			p->mo->flags2 &= ~(MF2_PASSMOBJ);
			p->mo->RenderStyle = STYLE_None;
			
			// Make me flat!
			p->mo->height = 0;

			// Make monsters unable to "see" this player.
			p->cheats |= CF_NOTARGET;
		}

		if ( p->pSkullBot )
			p->pSkullBot->PostEvent( BOTEVENT_SPECTATING );
	}

	// "Fix" for one of the starts on exec.wad MAP01: If you start inside the ceiling,
	// drop down below it, even if that means sinking into the floor.
	if (mobj->z + mobj->height > mobj->ceilingz)
	{
		mobj->z = mobj->ceilingz - mobj->height;
	}

	// [RH] If someone is in the way, kill them
	if ( p->bSpectating == false )
		P_PlayerStartStomp (mobj);

	// Tell clients about the respawning player.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( bClientUpdate )
			SERVERCOMMANDS_SpawnPlayer( playernum, state );

		pInventory = mobj->FindInventory( RUNTIME_CLASS( APowerInvulnerable ));
		if (( pInventory ) && ( p->bSpectating == false ))
		{
			SERVERCOMMANDS_GivePowerup( playernum, static_cast<APowerup *>( pInventory ));
			SERVERCOMMANDS_PlayerRespawnInvulnerability( playernum );
		}
	}

	// [BC] Do script stuff
	if (!tempplayer)
	{
		if (state == PST_ENTER || (state == PST_LIVE && !savegamerestore))
		{
			// [BC] If we're the server, just mark this client as needing to have his enter
			// scripts run when he actually authenticates the level.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVER_GetClient( p - players )->bRunEnterScripts = true;
			else
				FBehavior::StaticStartTypedScripts (SCRIPT_Enter, p->mo, true);
		}
		// [BC] Added PST_REBORNNOINVENTORY.
		else if (state == PST_REBORN || state == PST_REBORNNOINVENTORY)
		{
			// [BC] In Skulltag, it's okay if the player is being reborn without an oldactor,
			// since there's times where we delete a player's body, and respawn him without him
			// having been killed (ex. when a player changes teams).
			if ( oldactor != NULL )
				DObject::PointerSubstitution (oldactor, p->mo);
/* [BB] Check if this neeeds to be done
			// PointerSubstitution() will also affect the bodyque, so undo that now.
			for (int ii=0; ii < BODYQUESIZE; ++ii)
				if (bodyque[ii] == p->mo)
					bodyque[ii] = oldactor;
*/
			FBehavior::StaticStartTypedScripts (SCRIPT_Respawn, p->mo, true);
		}
	}

	// Incoming players have various stats reset.
	if ( state == PST_ENTER )
	{
		p->ulTime = 0;
		p->ulDeathCount = 0;
	}
	// If this is a bot, tell him he's been respawned.
	else if (( state == PST_REBORN ) || ( state == PST_REBORNNOINVENTORY ))
	{
		if ( p->pSkullBot )
			p->pSkullBot->PostEvent( BOTEVENT_RESPAWNED );
	}

	// If this is a bot, clear out their path. They won't have a path if they don't have a
	// script, however.
	if (( p->pSkullBot ) && ( p->pSkullBot->m_bHasScript ))
	{
		p->pSkullBot->m_ulPathType = BOTPATHTYPE_NONE;
		ASTAR_ClearPath( p - players );
	}

	SCOREBOARD_RefreshHUD( );
}


//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host BYTE order.
//
// [RH] position is used to weed out unwanted start spots
void P_SpawnMapThing (mapthing2_t *mthing, int position)
{
	const PClass *i;
	int mask;
	AActor *mobj;
	fixed_t x, y, z;
	static unsigned int classFlags[] =
	{
		MTF_FIGHTER,
		MTF_CLERIC,
		MTF_MAGE,
	};

	if (mthing->type == 0 || mthing->type == -1)
		return;

	// count deathmatch start positions
	if (mthing->type == 11)
	{
		deathmatchstarts.Push (*mthing);
		return;
	}

	// [BC] Count temporary team starts.
	if ( mthing->type == 5082 )
	{
		TemporaryTeamStarts.Push( *mthing );
		return;
	}

	// [BC] Count blue team starts.
	if ( mthing->type == 5080 )
	{
		BlueTeamStarts.Push( *mthing );
		return;
	}

	// [BC] Count red team starts.
	if ( mthing->type == 5081 )
	{
		RedTeamStarts.Push( *mthing );
		return;
	}

	// [RC] Catalog possession starts
	if ( mthing->type == 6000 )
	{
		PossessionStarts.Push( *mthing );
		return;
	}

	// [RC] Catalog terminator starts
	if ( mthing->type == 6001 )
	{
		TerminatorStarts.Push( *mthing );
		return;
	}

	// [BC] Count invasion starts.
	if (( mthing->type >= 5200 ) && ( mthing->type <= 5277 ))
	{
		GenericInvasionStarts.Push( *mthing );
//		return;
	}

	// Convert Strife starts to Hexen-style starts
	if (gameinfo.gametype == GAME_Strife && mthing->type >= 118 && mthing->type <= 127)
	{
		mthing->args[0] = mthing->type - 117;
		mthing->type = 1;
	}

	// [RH] Record polyobject-related things
	if (gameinfo.gametype == GAME_Hexen)
	{
		switch (mthing->type)
		{
		case PO_HEX_ANCHOR_TYPE:
			mthing->type = PO_ANCHOR_TYPE;
			break;
		case PO_HEX_SPAWN_TYPE:
			mthing->type = PO_SPAWN_TYPE;
			break;
		case PO_HEX_SPAWNCRUSH_TYPE:
			mthing->type = PO_SPAWNCRUSH_TYPE;
			break;
		}
	}

	if (mthing->type == PO_ANCHOR_TYPE ||
		mthing->type == PO_SPAWN_TYPE ||
		mthing->type == PO_SPAWNCRUSH_TYPE ||
		mthing->type == PO_SPAWNHURT_TYPE)
	{
		polyspawns_t *polyspawn = new polyspawns_t;
		polyspawn->next = polyspawns;
		polyspawn->x = mthing->x << FRACBITS;
		polyspawn->y = mthing->y << FRACBITS;
		polyspawn->angle = mthing->angle;
		polyspawn->type = mthing->type;
		polyspawns = polyspawn;
		if (mthing->type != PO_ANCHOR_TYPE)
			po_NumPolyobjs++;
		return;
	}

	// check for players specially
	int pnum = -1;

	if (mthing->type <= 4 && mthing->type > 0)
	{
		pnum = mthing->type - 1;
	}
	else
	{
		const int base = (gameinfo.gametype == GAME_Strife) ? 5 :
						 (gameinfo.gametype == GAME_Hexen) ? 9100 : 4001;

		if (mthing->type >= base && mthing->type < base + MAXPLAYERS - 4)
		{
			pnum = mthing->type - base + 4;
		}
	}

	if (pnum == -1 || (level.flags & LEVEL_FILTERSTARTS))
	{
		// These checks are now done later.
/*
		// check for appropriate game type
		if (deathmatch) 
		{
			mask = MTF_DEATHMATCH;
		}
		else if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
		{
			mask = MTF_COOPERATIVE;
		}
		else
		{
			mask = MTF_SINGLE;
		}
		if (!(mthing->flags & mask))
		{
			return;
		}
*/
		// check for apropriate skill level
		if (gameskill == sk_baby)
		{
			mask = MTF_EASY;
		}
		else if (gameskill == sk_nightmare)
		{
			mask = MTF_HARD;
		}
		else
		{
			mask = 1 << (gameskill - 1);
		}
		if (!(mthing->flags & mask))
		{
			return;
		}

		// Check class spawn masks. Now with player classes available
		// this is enabled for all games.
		if ( NETWORK_GetState( ) == NETSTATE_SINGLE )
		{ // Single player
			int spawnmask = players[consoleplayer].GetSpawnClass();
			if (spawnmask != 0 && (mthing->flags & spawnmask) == 0)
			{ // Not for current class
				return;
			}
		}
		// [BC] Cooperative = !deathmatch && !teamgame, and also always spawn the item
		// in multiplayer.
		else if ((!deathmatch && !teamgame) && ( NETWORK_GetState( ) == NETSTATE_SINGLE ))
		{ // Cooperative
			mask = 0;
			for (int i = 0; i < MAXPLAYERS; i++)
			{
				if (playeringame[i])
				{
					int spawnmask = players[i].GetSpawnClass();
					if (spawnmask != 0)
						mask |= spawnmask;
					else 
						mask = -1;
				}
			}
			if (mask != -1 && (mthing->flags & mask) == 0)
			{
				return;
			}
		}
	}

	if (pnum != -1)
	{
		// [RH] Only spawn spots that match position.
		if (mthing->args[0] != position)
			return;

		// save spots for respawning in network games
		playerstarts[pnum] = *mthing;

		// Need to maintain this method so that we get voodoo dolls, which some doomers are used to.
		if ( NETWORK_GetState( ) == NETSTATE_SINGLE && ( deathmatch == false ) && ( teamgame == false ))
			P_SpawnPlayer (mthing, false, NULL);

		return;
	}

	// [RH] sound sequence overriders
	if (mthing->type >= 1400 && mthing->type < 1410)
	{
		R_PointInSubsector (mthing->x<<FRACBITS,
			mthing->y<<FRACBITS)->sector->seqType = mthing->type - 1400;
		return;
	}
	else if (mthing->type == 1411)
	{
		int type;

		if (mthing->args[0] == 255)
			type = -1;
		else
			type = mthing->args[0];

		if (type > 63)
		{
			Printf ("Sound sequence %d out of range\n", type);
		}
		else
		{
			R_PointInSubsector (mthing->x << FRACBITS,
				mthing->y << FRACBITS)->sector->seqType = type;
		}
		return;
	}

	// [RH] Determine if it is an old ambient thing, and if so,
	//		map it to MT_AMBIENT with the proper parameter.
	if (mthing->type >= 14001 && mthing->type <= 14064)
	{
		mthing->args[0] = mthing->type - 14000;
		mthing->type = 14065;
		i = RUNTIME_CLASS(AAmbientSound);
	}
	else
	{
		// find which type to spawn
		i = DoomEdMap.FindType (mthing->type);
	}

	if (i == NULL)
	{
		// [RH] Don't die if the map tries to spawn an unknown thing
		Printf ("Unknown type %i at (%i, %i)\n",
				 mthing->type,
				 mthing->x, mthing->y);
		i = RUNTIME_CLASS(AUnknown);
	}
	// [RH] If the thing's corresponding sprite has no frames, also map
	//		it to the unknown thing.
	else
	{
		// Handle decorate replacements explicitly here
		// to check for missing frames in the replacement object.
		i = i->ActorInfo->GetReplacement()->Class;

		const AActor *defaults = GetDefaultByType (i);
		if (defaults->SpawnState == NULL ||
			sprites[defaults->SpawnState->sprite.index].numframes == 0)
		{
			Printf ("%s at (%i, %i) has no frames\n",
					i->TypeName.GetChars(), mthing->x, mthing->y);
			i = RUNTIME_CLASS(AUnknown);
		}
	}

	const AActor *info = GetDefaultByType (i);
/*
	// don't spawn keycards and players in deathmatch
	if (deathmatch && info->flags & MF_NOTDMATCH)
		return;
*/
/*
	// [RH] don't spawn extra weapons in coop if so desired
	if (multiplayer && !deathmatch && (dmflags & DF_NO_COOP_WEAPON_SPAWN))
	{
		if (i->IsDescendantOf (RUNTIME_CLASS(AWeapon)))
		{
			if ((mthing->flags & (MTF_DEATHMATCH|MTF_SINGLE)) == MTF_DEATHMATCH)
				return;
		}
	}
*/
/*
	// don't spawn any monsters if -nomonsters
	if (dmflags & DF_NO_MONSTERS && info->flags3 & MF3_ISMONSTER )
	{
		return;
	}
	
	// [RH] Other things that shouldn't be spawned depending on dmflags
	if (deathmatch || alwaysapplydmflags)
	{
		if (dmflags & DF_NO_HEALTH)
		{
			if (i->IsDescendantOf (RUNTIME_CLASS(AHealth)))
				return;
			if (i->TypeName == NAME_Berserk)
				return;
			if (i->TypeName == NAME_Soulsphere)
				return;
			if (i->TypeName == NAME_Megasphere)
				return;
		}
		if (dmflags & DF_NO_ITEMS)
		{
//			if (i->IsDescendantOf (RUNTIME_CLASS(AArtifact)))
//				return;
		}
		if (dmflags & DF_NO_ARMOR)
		{
			if (i->IsDescendantOf (RUNTIME_CLASS(AArmor)))
				return;
			if (i->TypeName == NAME_Megasphere)
				return;
		}
	}
*/
	// [BC] If we're a client, there's no need to spawn map things (unless specified).
	if ((( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( ))) && 
		(( info->ulNetworkFlags & NETFL_ALLOWCLIENTSPAWN ) == false ))
	{
		return;
	}

	// spawn it
	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;

	if (info->flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else if (info->flags2 & MF2_SPAWNFLOAT)
		z = FLOATRANDZ;
	else
		z = ONFLOORZ;

	SpawningMapThing = true;
	mobj = Spawn (i, x, y, z, NO_REPLACE);
	SpawningMapThing = false;

	if (z == ONFLOORZ)
		mobj->z += mthing->z << FRACBITS;
	else if (z == ONCEILINGZ)
		mobj->z -= mthing->z << FRACBITS;

	mobj->SpawnPoint[0] = mthing->x;
	mobj->SpawnPoint[1] = mthing->y;
	mobj->SpawnPoint[2] = mthing->z;
	mobj->SpawnAngle = mthing->angle;
	mobj->SpawnFlags = mthing->flags;

	// [RH] Set the thing's special
	mobj->special = mthing->special;
	for(int j=0;j<5;j++) mobj->args[j]=mthing->args[j];

	// [RH] Add ThingID to mobj and link it in with the others
	mobj->tid = mthing->thingid;
	mobj->AddToHash ();

	mobj->angle = (DWORD)((mthing->angle * UCONST64(0x100000000)) / 360);
	mobj->BeginPlay ();
	if (mobj->ObjectFlags & OF_MassDestruction)
	{
		return;
	}
	mobj->LevelSpawned ();
}



//
// GAME SPAWN FUNCTIONS
//


//
// P_SpawnPuff
//

AActor *P_SpawnPuff (const PClass *pufftype, fixed_t x, fixed_t y, fixed_t z, angle_t dir, int updown, bool hitthing)
{
	AActor *puff;

	z += pr_spawnpuff.Random2 () << 10;

	puff = Spawn (pufftype, x, y, z, ALLOW_REPLACE);

	// [BC] If we're the server, tell clients to spawn the thing.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( puff );

	// If a puff has a crash state and an actor was not hit,
	// it will enter the crash state. This is used by the StrifeSpark
	// and BlasterPuff.
	if (hitthing == false && puff->CrashState != NULL)
	{
		puff->SetState (puff->CrashState);
	}
	else if (attackrange == MELEERANGE && puff->MeleeState != NULL)
	{
		// handle the hard coded state jump of Doom's bullet puff
		// in a more flexible manner.
		puff->SetState (puff->MeleeState);
	}

	if (cl_pufftype && updown != 3 && (puff->flags4 & MF4_ALLOWPARTICLES))
	{
		P_DrawSplash2 (32, x, y, z, dir, updown, 1);
		puff->renderflags |= RF_INVISIBLE;
	}

	if (hitthing && puff->SeeSound)
	{ // Hit thing sound
		S_SoundID (puff, CHAN_BODY, puff->SeeSound, 1, ATTN_NORM);

		// [BC] If we're the server, play this sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundIDActor( puff, CHAN_BODY, puff->SeeSound, 127, ATTN_NORM );
	}
	else if (puff->AttackSound)
	{
		S_SoundID (puff, CHAN_BODY, puff->AttackSound, 1, ATTN_NORM);

		// [BC] If we're the server, play this sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundIDActor( puff, CHAN_BODY, puff->AttackSound, 127, ATTN_NORM );
	}

	PuffSpawned = puff;
	return puff;
}



//---------------------------------------------------------------------------
//
// P_SpawnBlood
// 
//---------------------------------------------------------------------------
void P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage, AActor *originator)
{
	ABlood *th;
	PalEntry bloodcolor = (PalEntry)originator->GetClass()->Meta.GetMetaInt(AMETA_BloodColor);

	// [BC] Always spawn blood if we're the server. That way we can tell clients to spawn
	// the blood actor, and then they can elect to spawn either the actor or particles.
	if (( cl_bloodtype <= 1 ) || ( NETWORK_GetState( ) == NETSTATE_SERVER ))
	{
		z += pr_spawnblood.Random2 () << 10;
		th = Spawn<ABlood> (x, y, z, ALLOW_REPLACE);
		th->momz = FRACUNIT*2;
		th->angle = dir;
		if (gameinfo.gametype == GAME_Doom)
		{
			th->tics -= pr_spawnblood() & 3;

			if (th->tics < 1)
				th->tics = 1;
		}
		// colorize the blood!
		if (bloodcolor != 0)
		{
			th->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
		}
		th->SetDamage (damage);

		// [BC] If we're the server, tell clients to spawn the blood.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThingNoNetID( th );
	}

	if (cl_bloodtype >= 1)
		P_DrawSplash2 (40, x, y, z, dir, 2, bloodcolor);
}

//---------------------------------------------------------------------------
//
// PROC P_BloodSplatter
//
//---------------------------------------------------------------------------

void P_BloodSplatter (fixed_t x, fixed_t y, fixed_t z, AActor *originator)
{
	PalEntry bloodcolor = (PalEntry)originator->GetClass()->Meta.GetMetaInt(AMETA_BloodColor);

	if (cl_bloodtype <= 1)
	{
		AActor *mo;

		mo = Spawn("BloodSplatter", x, y, z, ALLOW_REPLACE);
		mo->target = originator;
		mo->momx = pr_splatter.Random2 () << 10;
		mo->momy = pr_splatter.Random2 () << 10;
		mo->momz = 3*FRACUNIT;

		// colorize the blood!
		if (bloodcolor!=0) mo->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
	}
	if (cl_bloodtype >= 1)
	{
		P_DrawSplash2 (40, x, y, z, R_PointToAngle2 (x, y, originator->x, originator->y), 2, bloodcolor);
	}
}

//---------------------------------------------------------------------------
//
// PROC P_RipperBlood
//
//---------------------------------------------------------------------------

void P_RipperBlood (AActor *mo, AActor *bleeder)
{
	fixed_t x, y, z;
	PalEntry bloodcolor = (PalEntry)bleeder->GetClass()->Meta.GetMetaInt(AMETA_BloodColor);

	x = mo->x + (pr_ripperblood.Random2 () << 12);
	y = mo->y + (pr_ripperblood.Random2 () << 12);
	z = mo->z + (pr_ripperblood.Random2 () << 12);
	if (cl_bloodtype <= 1)
	{
		AActor *th;
		th = Spawn<ABlood> (x, y, z, ALLOW_REPLACE);
		if (gameinfo.gametype == GAME_Heretic)
			th->flags |= MF_NOGRAVITY;
		th->momx = mo->momx >> 1;
		th->momy = mo->momy >> 1;
		th->tics += pr_ripperblood () & 3;

		// colorize the blood!
		if (bloodcolor!=0) th->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
	}
	if (cl_bloodtype >= 1)
	{
		P_DrawSplash2 (28, x, y, z, 0, 0, bloodcolor);
	}
}

//---------------------------------------------------------------------------
//
// FUNC P_GetThingFloorType
//
//---------------------------------------------------------------------------

int P_GetThingFloorType (AActor *thing)
{
	if (thing->floorpic)
	{		
		return TerrainTypes[thing->floorpic];
	}
	else
	{
		return TerrainTypes[thing->Sector->floorpic];
	}
}

//---------------------------------------------------------------------------
//
// FUNC P_HitWater
//
// Returns true if hit liquid and splashed, false if not.
//---------------------------------------------------------------------------

bool P_HitWater (AActor * thing, sector_t * sec, fixed_t z)
{
	if (thing->flags2 & MF2_FLOATBOB || thing->flags3 & MF3_DONTSPLASH)
		return false;

	if ( CLIENT_PREDICT_IsPredicting( ))
		return ( false );
/*
	if (thing->player && (thing->player->cheats & CF_PREDICTING))
		return false;
*/
	AActor *mo = NULL;
	FSplashDef *splash;
	int terrainnum;
	
	if (z==FIXED_MIN) z=thing->z;
	// don't splash above the object!
	else if (z>thing->z+(thing->height>>1)) return false;

	for(int i=0;i<sec->e->ffloors.Size();i++)
	{		
		F3DFloor * rover = sec->e->ffloors[i];
		if (!(rover->flags & FF_EXISTS)) continue;
		if (rover->top.plane->ZatPoint(thing->x, thing->y) == z)
		{
			if (rover->flags & (FF_SOLID|FF_SWIMMABLE) )
			{
				terrainnum = TerrainTypes[*rover->top.texture];
				goto foundone;
			}
		}
	}
	if (sec->heightsec == NULL ||
		//!sec->heightsec->waterzone ||
		(sec->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) ||
		!(sec->heightsec->MoreFlags & SECF_CLIPFAKEPLANES))
	{
		terrainnum = TerrainTypes[sec->floorpic];
	}
	else
	{
		terrainnum = TerrainTypes[sec->heightsec->floorpic];
	}
foundone:
	
	int splashnum = Terrains[terrainnum].Splash;
	bool smallsplash = false;
	const secplane_t *plane;


	if (splashnum == -1)
		return Terrains[terrainnum].IsLiquid;

	// don't splash when touching an underwater floor
	if (thing->waterlevel>=1 && z<=thing->floorz) return Terrains[terrainnum].IsLiquid;

	plane = (sec->heightsec != NULL &&
		//sec->heightsec->waterzone &&
		!(sec->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	  ? &sec->heightsec->floorplane : &sec->floorplane;
	//z = plane->ZatPoint (thing->x, thing->y);

	// Don't splash for living things with small vertical velocities.
	// There are levels where the constant splashing from the monsters gets extremely annoying
	if ((thing->flags3&MF3_ISMONSTER || thing->player) && thing->momz>=-5*FRACUNIT) return Terrains[terrainnum].IsLiquid;

	splash = &Splashes[splashnum];

	// Small splash for small masses
	if (thing->Mass < 10)
		smallsplash = true;

	if (smallsplash && splash->SmallSplash)
	{
		mo = Spawn (splash->SmallSplash, thing->x, thing->y, z, ALLOW_REPLACE);
		if (mo) mo->floorclip += splash->SmallSplashClip;
	}
	else
	{
		if (splash->SplashChunk)
		{
			mo = Spawn (splash->SplashChunk, thing->x, thing->y, z, ALLOW_REPLACE);
			mo->target = thing;
			if (splash->ChunkXVelShift != 255)
			{
				mo->momx = pr_chunk.Random2() << splash->ChunkXVelShift;
			}
			if (splash->ChunkYVelShift != 255)
			{
				mo->momy = pr_chunk.Random2() << splash->ChunkYVelShift;
			}
			mo->momz = splash->ChunkBaseZVel + (pr_chunk() << splash->ChunkZVelShift);
		}
		if (splash->SplashBase)
		{
			mo = Spawn (splash->SplashBase, thing->x, thing->y, z, ALLOW_REPLACE);
		}
		if (thing->player && !splash->NoAlert)
		{
			P_NoiseAlert (thing, thing, true);
		}
	}
	if (mo)
	{
		S_SoundID (mo, CHAN_ITEM, smallsplash ?
			splash->SmallSplashSound : splash->NormalSplashSound,
			1, ATTN_IDLE);
	}
	else
	{
		S_SoundID (thing->x, thing->y, z, CHAN_ITEM, smallsplash ?
			splash->SmallSplashSound : splash->NormalSplashSound,
			1, ATTN_IDLE);
	}

	// Don't let deep water eat missiles
	return plane == &sec->floorplane ? Terrains[terrainnum].IsLiquid : false;
}

//---------------------------------------------------------------------------
//
// FUNC P_HitFloor
//
// Returns true if hit liquid and splashed, false if not.
//---------------------------------------------------------------------------

bool P_HitFloor (AActor *thing)
{
	const msecnode_t *m;

	if (thing->flags2 & MF2_FLOATBOB || thing->flags3 & MF3_DONTSPLASH)
		return false;

	// don't splash if landing on the edge above water/lava/etc....
	for (m = thing->touching_sectorlist; m; m = m->m_tnext)
	{
		if (thing->z == m->m_sector->floorplane.ZatPoint (thing->x, thing->y))
		{
			break;
		}

		// Check 3D floors
		for(int i=0;i<m->m_sector->e->ffloors.Size();i++)
		{		
			F3DFloor * rover = m->m_sector->e->ffloors[i];
			if (!(rover->flags & FF_EXISTS)) continue;
			if (rover->flags & (FF_SOLID|FF_SWIMMABLE))
			{
				if (rover->top.plane->ZatPoint(thing->x, thing->y) == thing->z)
				{
					return P_HitWater (thing, m->m_sector);
				}
			}
		}
	}
	if (m == NULL ||
		(m->m_sector->heightsec != NULL &&
		!(m->m_sector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC)))
	{ 
		return false;
	}

	return P_HitWater (thing, m->m_sector);
}

//---------------------------------------------------------------------------
//
// FUNC P_CheckMissileSpawn
//
// Returns true if the missile is at a valid spawn point, otherwise
// explodes it and returns false.
//
//---------------------------------------------------------------------------

bool P_CheckMissileSpawn (AActor* th)
{
	// [RH] Don't decrement tics if they are already less than 1
	if ((th->flags4 & MF4_RANDOMIZE) && th->tics > 0)
	{
		th->tics -= pr_checkmissilespawn() & 3;
		if (th->tics < 1)
			th->tics = 1;
	}

	// move a little forward so an angle can be computed if it immediately explodes
	if (th->Speed >= 100*FRACUNIT)
	{ // Ultra-fast ripper spawning missile
		th->x += th->momx>>3;
		th->y += th->momy>>3;
		th->z += th->momz>>3;
	}
	else
	{ // Normal missile
		th->x += th->momx>>1;
		th->y += th->momy>>1;
		th->z += th->momz>>1;
	}

	// killough 3/15/98: no dropoff (really = don't care for missiles)

	if (!P_TryMove (th, th->x, th->y, false))
	{
		// [RH] Don't explode ripping missiles that spawn inside something
		if (BlockingMobj == NULL || !(th->flags2 & MF2_RIP))
		{
			// [RH] Don't explode missiles that spawn on top of horizon lines
			if (BlockingLine != NULL && BlockingLine->special == Line_Horizon)
			{
				th->Destroy ();
			}
			else
			{
				// Potentially reward the player who shot this missile with an accuracy/precision medal.
				if ((( th->ulSTFlags & STFL_EXPLODEONDEATH ) == false ) && th->target && th->target->player )
				{
					if ( th->target->player->bStruckPlayer )
						PLAYER_StruckPlayer( th->target->player );
					else
						th->target->player->ulConsecutiveHits = 0;
				}

				P_ExplodeMissile (th, NULL, NULL);
			}
			return false;
		}
	}
	return true;
}


//---------------------------------------------------------------------------
//
// FUNC P_SpawnMissile
//
// Returns NULL if the missile exploded immediately, otherwise returns
// a mobj_t pointer to the missile.
//
//---------------------------------------------------------------------------

AActor *P_SpawnMissile (AActor *source, AActor *dest, const PClass *type)
{
	return P_SpawnMissileXYZ (source->x, source->y, source->z + 32*FRACUNIT,
		source, dest, type);
}

AActor *P_SpawnMissileZ (AActor *source, fixed_t z, AActor *dest, const PClass *type)
{
	return P_SpawnMissileXYZ (source->x, source->y, z, source, dest, type);
}

AActor *P_SpawnMissileXYZ (fixed_t x, fixed_t y, fixed_t z,
	AActor *source, AActor *dest, const PClass *type)
{
	int defflags3 = GetDefaultByType (type)->flags3;

	if (defflags3 & MF3_FLOORHUGGER)
	{
		z = ONFLOORZ;
	}
	else if (defflags3 & MF3_CEILINGHUGGER)
	{
		z = ONCEILINGZ;
	}
	else if (z != ONFLOORZ)
	{
		z -= source->floorclip;
	}

	AActor *th = Spawn (type, x, y, z, ALLOW_REPLACE);
	
	if (th->SeeSound)
		S_SoundID (th, CHAN_VOICE, th->SeeSound, 1, ATTN_NORM);

	th->target = source;		// record missile's originator

	float speed = (float)(th->Speed);

	// [RH]
	// Hexen calculates the missile velocity based on the source's location.
	// Would it be more useful to base it on the actual position of the
	// missile? I'll leave it like this for now.
	// Answer. No, because this way, you can set up sets of parallel missiles.

	FVector3 velocity(dest->x - source->x, dest->y - source->y, dest->z - source->z);
	// Floor and ceiling huggers should never have a vertical component to their velocity
	if (defflags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER))
	{
		velocity.Z = 0;
	}
	// [RH] Adjust the trajectory if the missile will go over the player's head.
	else if (z - source->z >= dest->height)
	{
		velocity.Z += dest->height - z + source->z;
	}
	velocity.Resize (speed);
	th->momx = (fixed_t)(velocity.X);
	th->momy = (fixed_t)(velocity.Y);
	th->momz = (fixed_t)(velocity.Z);

	// invisible target: rotate velocity vector in 2D
	if (dest->flags & MF_SHADOW)
	{
		angle_t an = pr_spawnmissile.Random2 () << 20;
		an >>= ANGLETOFINESHIFT;
		
		fixed_t newx = DMulScale16 (th->momx, finecosine[an], -th->momy, finesine[an]);
		fixed_t newy = DMulScale16 (th->momx, finesine[an], th->momy, finecosine[an]);
		th->momx = newx;
		th->momy = newy;
	}

	th->angle = R_PointToAngle2 (0, 0, th->momx, th->momy);

	AActor *spawnedMissle = (P_CheckMissileSpawn (th) ? th : NULL);

	// [BB] If we're the server, tell clients to spawn the missile.
	// This should fix the problem that projectiles shot by Hexen/Heretic monsters
	// are not spawned on the client in an online game. If this turns out to work nicely,
	// the Doom monsters should use this, too.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( spawnedMissle ) && ( (gameinfo.gametype == GAME_Hexen) || (gameinfo.gametype == GAME_Heretic) ) )
		SERVERCOMMANDS_SpawnMissile( spawnedMissle );

	return spawnedMissle;
}

//---------------------------------------------------------------------------
//
// FUNC P_SpawnMissileAngle
//
// Returns NULL if the missile exploded immediately, otherwise returns
// a mobj_t pointer to the missile.
//
//---------------------------------------------------------------------------

AActor *P_SpawnMissileAngle (AActor *source, const PClass *type,
	angle_t angle, fixed_t momz)
{
	return P_SpawnMissileAngleZSpeed (source, source->z + 32*FRACUNIT,
		type, angle, momz, GetDefaultByType (type)->Speed);
}

AActor *P_SpawnMissileAngleZ (AActor *source, fixed_t z,
	const PClass *type, angle_t angle, fixed_t momz)
{
	return P_SpawnMissileAngleZSpeed (source, z, type, angle, momz,
		GetDefaultByType (type)->Speed);
}

AActor *P_SpawnMissileZAimed (AActor *source, fixed_t z, AActor *dest, const PClass *type)
{
	angle_t an;
	fixed_t dist;
	fixed_t speed;
	fixed_t momz;

	an = source->angle;

	if (dest->flags & MF_SHADOW)
	{
		an += pr_spawnmissile.Random2() << 20;
	}
	dist = P_AproxDistance (dest->x - source->x, dest->y - source->y);
	speed = GetDefaultByType (type)->Speed;
	dist /= speed;
	momz = dist != 0 ? (dest->z - source->z)/dist : speed;
	return P_SpawnMissileAngleZSpeed (source, z, type, an, momz, speed);
}

//---------------------------------------------------------------------------
//
// FUNC P_SpawnMissileAngleSpeed
//
// Returns NULL if the missile exploded immediately, otherwise returns
// a mobj_t pointer to the missile.
//
//---------------------------------------------------------------------------

AActor *P_SpawnMissileAngleSpeed (AActor *source, const PClass *type,
	angle_t angle, fixed_t momz, fixed_t speed)
{
	return P_SpawnMissileAngleZSpeed (source, source->z + 32*FRACUNIT,
		type, angle, momz, speed);
}

AActor *P_SpawnMissileAngleZSpeed (AActor *source, fixed_t z,
	const PClass *type, angle_t angle, fixed_t momz, fixed_t speed, AActor *owner)
{
	AActor *mo;
	int defflags3 = GetDefaultByType (type)->flags3;

	if (defflags3 & MF3_FLOORHUGGER)
	{
		z = ONFLOORZ;
	}
	else if (defflags3 & MF3_CEILINGHUGGER)
	{
		z = ONCEILINGZ;
	}
	if (z != ONFLOORZ)
	{
		z -= source->floorclip;
	}
	mo = Spawn (type, source->x, source->y, z, ALLOW_REPLACE);
	if (mo->SeeSound)
	{
		S_SoundID (mo, CHAN_VOICE, mo->SeeSound, 1, ATTN_NORM);
	}
	mo->target = owner != NULL ? owner : source; // Originator
	mo->angle = angle;
	angle >>= ANGLETOFINESHIFT;
	mo->momx = FixedMul (speed, finecosine[angle]);
	mo->momy = FixedMul (speed, finesine[angle]);
	mo->momz = momz;
	return P_CheckMissileSpawn(mo) ? mo : NULL;
}

/*
================
=
= P_SpawnPlayerMissile
=
= Tries to aim at a nearby monster
================
*/

AActor *P_SpawnPlayerMissile (AActor *source, const PClass *type)
{
	return P_SpawnPlayerMissile (source, source->x, source->y, source->z, type, source->angle);
}

AActor *P_SpawnPlayerMissile (AActor *source, const PClass *type, angle_t angle, bool bSpawnSound )
{
	return P_SpawnPlayerMissile (source, source->x, source->y, source->z, type, angle, bSpawnSound );
}

AActor *P_SpawnPlayerMissile (AActor *source, fixed_t x, fixed_t y, fixed_t z,
							  const PClass *type, angle_t angle, bool bSpawnSound )
{
	static const int angdiff[3] = { -1<<26, 1<<26, 0 };
	int i;
	angle_t an;
	angle_t pitch;

	// see which target is to be aimed at
	i = 2;
	do
	{
		an = angle + angdiff[i];
		pitch = P_AimLineAttack (source, an, 16*64*FRACUNIT);

		if (source->player != NULL &&
			!(dmflags & DF_NO_FREELOOK) &&
			source->player->userinfo.aimdist <= ANGLE_1/2)
		{
			break;
		}
	} while (linetarget == NULL && --i >= 0);

	if (linetarget == NULL)
	{
		an = angle;
	}

	i = GetDefaultByType (type)->flags3;

	if (i & MF3_FLOORHUGGER)
	{
		z = ONFLOORZ;
	}
	else if (i & MF3_CEILINGHUGGER)
	{
		z = ONCEILINGZ;
	}
	if (z != ONFLOORZ && z != ONCEILINGZ)
	{
		z += 4*8*FRACUNIT - source->floorclip + (source->player? source->player->crouchoffset : 0);
	}
	MissileActor = Spawn (type, x, y, z, ALLOW_REPLACE);

	if (MissileActor->SeeSound && bSpawnSound )
	{
		S_SoundID (MissileActor, CHAN_VOICE, MissileActor->SeeSound, 1, ATTN_NORM);
	}
	MissileActor->target = source;
	MissileActor->angle = an;

	fixed_t vx, vy, vz, speed;

	vx = FixedMul (finecosine[pitch>>ANGLETOFINESHIFT], finecosine[an>>ANGLETOFINESHIFT]);
	vy = FixedMul (finecosine[pitch>>ANGLETOFINESHIFT], finesine[an>>ANGLETOFINESHIFT]);
	vz = -finesine[pitch>>ANGLETOFINESHIFT];
	speed = MissileActor->Speed;

	MissileActor->momx = FixedMul (vx, speed);
	MissileActor->momy = FixedMul (vy, speed);
	MissileActor->momz = FixedMul (vz, speed);

	// [BC] Tell clients to spawn this missile.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnMissileExact( MissileActor );
	
	if (P_CheckMissileSpawn (MissileActor))
	{
		return MissileActor;
	}
	return NULL;
}

bool AActor::IsTeammate (AActor *other)
{
	if (!player || !other || !other->player)
		return false;

	// Can't be an enemy of ourselves!
	if ( this == other )
		return ( true );

	// Teamplay deathmatch, CTF, Skulltag, etc.
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
	{
		// If either player isn't on a team...
		if ( player->bOnTeam == false || other->player->bOnTeam == false )
			return ( false );

		// If they're not on our team...
		if ( player->ulTeam != other->player->ulTeam )
			return ( false );

		// Passed checks. Player is on our team.
		return ( true );
	}

	// Allow co-op players to be teammates.
	if (( deathmatch == false ) && ( teamgame == false ))
		return ( true );

	return ( false );
}

//==========================================================================
//
// AActor :: GetSpecies
//
// Species is defined as the lowest base class that is a monster
// with no non-monster class in between. This is virtualized, so special
// monsters can change this behavior if they like.
//
//==========================================================================

const PClass *AActor::GetSpecies()
{
	const PClass *thistype = GetClass();

	if (GetDefaultByType(thistype)->flags3 & MF3_ISMONSTER)
	{
		while (thistype->ParentClass)
		{
			if (GetDefaultByType(thistype->ParentClass)->flags3 & MF3_ISMONSTER)
				thistype = thistype->ParentClass;
			else 
				break;
		}
	}
	return thistype;
}

//==========================================================================
//
// AActor :: IsFriend
//
// Checks if two monsters have to be considered friendly.
//
//==========================================================================

bool AActor::IsFriend (AActor *other)
{
	if (flags & other->flags & MF_FRIENDLY)
	{
		return !deathmatch ||
			FriendPlayer == other->FriendPlayer ||
			FriendPlayer == 0 ||
			other->FriendPlayer == 0;
	}
	return false;
}

//==========================================================================
//
// AActor :: IsHostile
//
// Checks if two monsters have to be considered hostile under any circumstances
//
//==========================================================================

bool AActor::IsHostile (AActor *other)
{
	// Both monsters are non-friendlies so hostilities depend on infighting settings
	if (!((flags | other->flags) & MF_FRIENDLY)) return false;

	// Both monsters are friendly and belong to the same player if applicable.
	if (flags & other->flags & MF_FRIENDLY)
	{
		return deathmatch &&
			FriendPlayer != other->FriendPlayer &&
			FriendPlayer !=0 &&
			other->FriendPlayer != 0;
	}
	return true;
}

int AActor::DoSpecialDamage (AActor *target, int damage)
{
	if (target->player && target->player->mo == target && damage < 1000 &&
		(target->player->cheats & CF_GODMODE))
	{
		return -1;
	}
	else
	{
		if (target->player)
		{
			int poisondamage = GetClass()->Meta.GetMetaInt(AMETA_PoisonDamage);
			if (poisondamage > 0)
			{
				P_PoisonPlayer (target->player, this, this->target, poisondamage);
				damage >>= 1;
			}
		}
	
		return damage;
	}
}

int AActor::TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, int damagetype)
{
	// If the actor does not have a corresponding death state, then it does not take damage.
	// Note that DeathState matches every kind of damagetype, so if an actor has that, it can
	// be hurt with any type of damage. Exception: Massacre damage always succeeds, because
	// it needs to work.
	FState *death;

	if (flags5 & MF5_NODAMAGE)
	{
		target = source;
		if (PainState != NULL && pr_takedamage() < PainChance)
		{
			SetState (PainState);
		}
		return -1;
	}

	if (DeathState != NULL)
	{
		return damage;
	}
	if (EDeathState==NULL && BDeathState==NULL && IDeathState==NULL)
	{
		// If there is no death state at all, kill it always.
		return damage;
	}

	switch (damagetype)
	{
	case MOD_MASSACRE:
		return damage;

	case MOD_DISINTEGRATE:
		death = EDeathState;
		break;

	case MOD_FIRE:
		death = BDeathState;
		break;

	case MOD_ICE:
		death = IDeathState;
		if (death == NULL && !deh.NoAutofreeze && !(flags4 & MF4_NOICEDEATH) &&
			(player || (flags3 & MF3_ISMONSTER)))
		{
			death = &AActor::States[S_GENERICFREEZEDEATH];
		}
		break;

	default:
		death = NULL;
		break;
	}
	return (death == NULL) ? -1 : damage;
}

FArchive &operator<< (FArchive &arc, FSoundIndex &snd)
{
	if (arc.IsStoring ())
	{
		arc.WriteName (snd.Index ? S_sfx[snd.Index].name : NULL);
	}
	else
	{
		const char *name = arc.ReadName ();;
		snd.Index = name != NULL ? S_FindSound (name) : 0;
	}
	return arc;
}

FArchive &operator<< (FArchive &arc, FSoundIndexWord &snd)
{
	FSoundIndex snd2 = { snd.Index };
	arc << snd2;
	snd.Index = snd2.Index;
	return arc;
}

// [BC] meh
void P_ResetSpawnCounters( void )
{
	if ( g_lSpawnCount )
	{
		g_lStaleSpawnCount = g_lSpawnCount;
		g_StaleSpawnCycles = g_SpawnCycles;

		g_lSpawnCount = 0;
		g_SpawnCycles = 0;
	}
}

// [BC]
ADD_STAT( spawns )
{
	FString	Out;

	Out.Format( "Actors spawned: %d in %04.1f ms",
		g_lStaleSpawnCount, SecondsPerCycle * (double)g_StaleSpawnCycles * 1000 );

	return ( Out );
}

#ifdef _DEBUG
// [BC]
#include "c_dispatch.h"
CCMD( dumpactors )
{
	AActor						*pActor;
	TThinkerIterator<AActor>	Iterator;

	while ( pActor = Iterator.Next( ))
		Printf( "%s (%d, %d)\n", pActor->GetClass( )->TypeName.GetChars( ), pActor->x >> FRACBITS, pActor->y >> FRACBITS );
}

CCMD( respawnactors )
{
	AActor						*pActor;
	AActor						*pNewActor;
	AActor						*pActorInfo;
	fixed_t						X;
	fixed_t						Y;
	fixed_t						Z;
	TThinkerIterator<AActor>	Iterator;

	while ( pActor = Iterator.Next( ))
	{
		if (( pActor->state == &AInventory::States[0] ) ||
			( pActor->state == &AInventory::States[3] ) ||
			( pActor->state == &AInventory::States[17] ))
		{
//			CLIENT_RestoreSpecialPosition( pActor );
//			CLIENT_RestoreSpecialDoomThing( pActor, true );

			// Get the default information for this actor, so we can determine how to
			// respawn it.
			pActorInfo = pActor->GetDefault( );

			// This item appears to be untouched; no need to respawn it.
			if (( pActor->x == pActor->SpawnPoint[0] << FRACBITS ) &&
				( pActor->y == pActor->SpawnPoint[1] << FRACBITS ) &&
				( pActor->state == pActor->SpawnState ) &&
				( pActor->health == pActorInfo->health ))
			{
				continue;
			}

			// Determine the Z position to spawn this monster in.
			if ( pActorInfo->flags & MF_SPAWNCEILING )
				Z = ONCEILINGZ;
			else if ( pActorInfo->flags2 & MF2_SPAWNFLOAT )
				Z = FLOATRANDZ;
			else if ( pActorInfo->flags2 & MF2_FLOATBOB )
				Z = pActor->SpawnPoint[2] << FRACBITS;
			else
				Z = ONFLOORZ;

			// Spawn the new monster.
			X = pActor->SpawnPoint[0] << FRACBITS;
			Y = pActor->SpawnPoint[1] << FRACBITS;
			pNewActor = Spawn( RUNTIME_TYPE( pActor ), X, Y, Z, NO_REPLACE );

			if ( Z == ONFLOORZ )
				pNewActor->z += pNewActor->SpawnPoint[2] << FRACBITS;
			else if ( Z == ONCEILINGZ )
				pNewActor->z -= pNewActor->SpawnPoint[2] << FRACBITS;

			// Inherit attributes from the old actor.
			pNewActor->SpawnPoint[0] = pActor->SpawnPoint[0];
			pNewActor->SpawnPoint[1] = pActor->SpawnPoint[1];
			pNewActor->SpawnPoint[2] = pActor->SpawnPoint[2];
			pNewActor->SpawnAngle = pActor->SpawnAngle;
			pNewActor->SpawnFlags = pActor->SpawnFlags;
			pNewActor->angle = ANG45 * ( pActor->SpawnAngle / 45 );

			// Just do this stuff for monsters.
			if ( pActor->flags & MF_COUNTKILL )
			{
				if ( pActor->SpawnFlags & MTF_AMBUSH )
					pNewActor->flags |= MF_AMBUSH;

				pNewActor->reactiontime = 18;

				pNewActor->TIDtoHate = pActor->TIDtoHate;
				pNewActor->LastLook = pActor->LastLook;
				pNewActor->flags3 |= pActor->flags3 & MF3_HUNTPLAYERS;
				pNewActor->flags4 |= pActor->flags4 & MF4_NOHATEPLAYERS;
			}

			pNewActor->flags &= ~MF_DROPPED;
			pNewActor->ulSTFlags |= STFL_LEVELSPAWNED;

			// Remove the old actor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( pActor );

			pActor->Destroy( );

			// Tell clients to spawn the new actor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pNewActor );
		}
	}
}
#endif
