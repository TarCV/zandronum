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
// DESCRIPTION:
//
//
//-----------------------------------------------------------------------------


#ifndef __D_PLAYER_H__
#define __D_PLAYER_H__

// Finally, for odd reasons, the player input
// is buffered within the player data struct,
// as commands per game tick.
#include "d_ticcmd.h"

#include "a_artifacts.h"

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "actor.h"

#include "d_netinf.h"
#include "r_defs.h"

#include "a_sharedglobal.h"
#include "bots.h"
#include "medal.h"

enum
{
	APMETA_BASE = 0x95000,

	APMETA_DisplayName,		// display name (used in menus etc.)
	APMETA_SoundClass,		// sound class
	APMETA_ColorRange,		// skin color range
	APMETA_InvulMode,
	APMETA_HealingRadius,
	APMETA_Hexenarmor0,
	APMETA_Hexenarmor1,
	APMETA_Hexenarmor2,
	APMETA_Hexenarmor3,
	APMETA_Hexenarmor4,
};

class player_s;
class	CSkullBot;
class	AFloatyIcon;

class APlayerPawn : public AActor
{
	DECLARE_STATELESS_ACTOR (APlayerPawn, AActor)
	HAS_OBJECT_POINTERS
public:
	virtual void Serialize (FArchive &arc);

	virtual void Tick();
	virtual void AddInventory (AInventory *item);
	virtual void RemoveInventory (AInventory *item);
	virtual bool UseInventory (AInventory *item);

	virtual void PlayIdle ();
	virtual void PlayRunning ();
	virtual void ThrowPoisonBag ();

	// This is called when a player is leaving the game, going to spectate, etc., but
	// has special items of interest (terminator, flags, etc.). Those need to be dropped or else
	// the game will become disrupted.
	virtual void DropImportantItems( bool bLeavingGame );

	virtual void TweakSpeeds (int &forwardmove, int &sidemove);
	virtual void MorphPlayerThink ();
	virtual void ActivateMorphWeapon ();
	virtual AWeapon *PickNewWeapon (const PClass *ammotype);
	virtual AWeapon *BestWeapon (const PClass *ammotype);
	virtual void GiveDeathmatchInventory ();
	virtual void FilterCoopRespawnInventory (APlayerPawn *oldplayer);
	// [BC]
	virtual void Destroy( );

	void GiveDefaultInventory ();
	void PlayAttacking ();
	void PlayAttacking2 ();
	const char *GetSoundClass ();

	enum EInvulState
	{
		INVUL_Start,
		INVUL_Active,
		INVUL_Stop,
		INVUL_GetAlpha
	};

	void BeginPlay ();
	void Die (AActor *source, AActor *inflictor);

	int			crouchsprite;
	int			MaxHealth;
	int			RunHealth;
	TObjPtr<AInventory> InvFirst;		// first inventory item displayed on inventory bar
	TObjPtr<AInventory> InvSel;			// selected inventory item

	// [GRB] Player class properties
	fixed_t		JumpZ;
	fixed_t		ViewHeight;
	fixed_t		ForwardMove1, ForwardMove2;
	fixed_t		SideMove1, SideMove2;
	int			ScoreIcon;
	int			SpawnMask;
	FNameNoInit	MorphWeapon;
	fixed_t		AttackZOffset;			// attack height, relative to player center

	// [CW] Fades for when you are being damaged.
	bool		HasDamageFade;
	float		RedDamageFade;
	float		GreenDamageFade;
	float		BlueDamageFade;

	bool UpdateWaterLevel (fixed_t oldz, bool splash);
	bool ResetAirSupply ();

	int GetMaxHealth() const;
};

class APlayerChunk : public APlayerPawn
{
	DECLARE_STATELESS_ACTOR (APlayerChunk, APlayerPawn)
};

//
// Player states.
//
typedef enum
{
	PST_LIVE,	// Playing or camping.
	PST_DEAD,	// Dead on the ground, view follows killer.
	PST_REBORN,	// Ready to restart/respawn???
	PST_ENTER,	// [BC] Entered the game
	PST_REBORNNOINVENTORY,	// [BC] Player should respawn, without triggering enter scripts, and without keeping his/her inventory.
	PST_ENTERNOINVENTORY,	// [BC] Player should respawn and trigger enter scripts, without keeping his/her inventory.
} playerstate_t;


//*****************************************************************************
//	Lead states.
typedef enum
{
	LEADSTATE_NOTINTHELEAD,
	LEADSTATE_TIEDFORTHELEAD,
	LEADSTATE_INTHELEAD,

} LEADSTATE_e;

//
// Player internal flags, for cheats and debug.
//
typedef enum
{
	CF_NOCLIP			= 1 << 0,		// No clipping, walk through barriers.
	CF_GODMODE			= 1 << 1,		// No damage, no health loss.
	CF_NOMOMENTUM		= 1 << 2,		// Not really a cheat, just a debug aid.
	CF_NOTARGET			= 1 << 3,		// [RH] Monsters don't target
	CF_FLY				= 1 << 4,		// [RH] Flying player
	CF_CHASECAM			= 1 << 5,		// [RH] Put camera behind player
	CF_FROZEN			= 1 << 6,		// [RH] Don't let the player move
	CF_REVERTPLEASE		= 1 << 7,		// [RH] Stick camera in player's head if (s)he moves
	CF_STEPLEFT			= 1 << 9,		// [RH] Play left footstep sound next time
	CF_FRIGHTENING		= 1 << 10,		// [RH] Scare monsters away
	CF_INSTANTWEAPSWITCH= 1 << 11,		// [RH] Switch weapons instantly
	CF_TOTALLYFROZEN	= 1 << 12,		// [RH] All players can do is press +use
	// [BC] We don't use CF_PREDICTING in ST.
	//CF_PREDICTING		= 1 << 13,		// [RH] Player movement is being predicted
	CF_WEAPONREADY		= 1 << 14,		// [RH] Weapon is in the ready state, so bob it when walking
	CF_TIMEFREEZE		= 1 << 15,		// Player has an active time freezer
	CF_DRAIN			= 1 << 16,		// Player owns a drain powerup
	CF_REGENERATION		= 1 << 17,		// Player owns a regeneration artifact
	CF_HIGHJUMP			= 1 << 18,		// more Skulltag flags. Implemetation not guaranteed though. ;)
	CF_REFLECTION		= 1 << 19,
	CF_PROSPERITY		= 1 << 20,
	CF_DOUBLEFIRINGSPEED= 1 << 21,
	CF_EXTREMELYDEAD	= 1 << 22,		// [RH] Reliably let the status bar know about extreme deaths.

	// [BC] Player can move freely while the game is in freeze mode.
	CF_FREEZE			= 0x800000,

	// [BC] Powerups added by Skulltag.
	CF_POSSESSIONARTIFACT	= 0x01000000,
	CF_TERMINATORARTIFACT	= 0x02000000,

	// [BC] Rune effects.
	CF_SPREAD				= 0x08000000,
	CF_SPEED25				= 0x20000000,
	
	CF_FIRERESISTANT		= 0x40000000,

} cheat_t;

#define WPIECE1		1
#define WPIECE2		2
#define WPIECE3		4

#define WP_NOCHANGE ((AWeapon*)~0)

//
// Extended player object info: player_t
//
class player_s
{
public:
	player_s();

	void Serialize (FArchive &arc);
	size_t FixPointers (const DObject *obj, DObject *replacement);
	size_t PropagateMark();

	void SetLogNumber (int num);
	void SetLogText (const char *text);

	APlayerPawn	*mo;
	BYTE		playerstate;
	ticcmd_t	cmd;

	userinfo_t	userinfo;				// [RH] who is this?
	
	const PClass *cls;				// class of associated PlayerPawn

	float		DesiredFOV;				// desired field of vision
	float		FOV;					// current field of vision
	fixed_t		viewz;					// focal origin above r.z
	fixed_t		viewheight;				// base height above floor for viewz
	fixed_t		deltaviewheight;		// squat speed.
	fixed_t		bob;					// bounded/scaled total momentum

	// killough 10/98: used for realistic bobbing (i.e. not simply overall speed)
	// mo->momx and mo->momy represent true momenta experienced by player.
	// This only represents the thrust that the player applies himself.
	// This avoids anomolies with such things as Boom ice and conveyors.
	fixed_t		momx, momy;				// killough 10/98

	bool		centering;
	BYTE		turnticks;
	short		oldbuttons;
	bool		attackdown;
	int			health;					// only used between levels, mo->health
										// is used during levels

	int			inventorytics;
	BYTE		CurrentPlayerClass;		// class # for this player instance
	int			pieces;					// Fourth Weapon pieces
	bool		backpack;
	
	int			fragcount;				// [RH] Cumulative frags for this player

	AWeapon	   *ReadyWeapon;
	AWeapon	   *PendingWeapon;			// WP_NOCHANGE if not changing

	int			cheats;					// bit flags
	short		refire;					// refired shots are less accurate
	int			killcount, itemcount, secretcount;		// for intermission
	int			damagecount, bonuscount;// for screen flashing
	int			hazardcount;			// for delayed Strife damage
	int			poisoncount;			// screen flash for poison damage
	AActor		*poisoner;				// NULL for non-player actors
	AActor		*attacker;				// who did damage (NULL for floors)
	int			extralight;				// so gun flashes light up areas
	int			fixedcolormap;			// can be set to REDCOLORMAP, etc.
	pspdef_t	psprites[NUMPSPRITES];	// view sprites (gun, etc)
	int			morphTics;				// player is a chicken/pig if > 0
	AWeapon		*PremorphWeapon;		// ready weapon before morphing
	int			chickenPeck;			// chicken peck countdown
	int			jumpTics;				// delay the next jump for a moment

	int			respawn_time;			// [RH] delay respawning until this tic
	AActor		*camera;				// [RH] Whose eyes this player sees through

	int			air_finished;			// [RH] Time when you start drowning

	WORD		accuracy, stamina;		// [RH] Strife stats

	FName		LastDamageType;			// [RH] For damage-specific pain and death sounds

	bool		settings_controller;	// Player can control game settings.

	float		BlendR;		// [RH] Final blending values
	float		BlendG;
	float		BlendB;
	float		BlendA;

	FString		LogText;	// [RH] Log for Strife

	SBYTE	crouching;
	SBYTE	crouchdir;
	fixed_t crouchfactor;
	fixed_t crouchoffset;
	fixed_t crouchviewdelta;

	// [BC] Start of a lot of new stuff.
	// This player is on a team for ST/CTF.
	bool		bOnTeam;

	// Team this player is on for ST/CTF.
	ULONG		ulTeam;

	// Amount of points this player has scored so far.
	LONG		lPointCount;

	// How many times has this player died?
	ULONG		ulDeathCount;

	// The last tick this player got a frag.
	ULONG		ulLastFragTick;

	// The last tick this player got an "Excellent!" medal.
	ULONG		ulLastExcellentTick;

	// The last tick this player killed someone with the BFG9000.
	ULONG		ulLastBFGFragTick;

	// Number of consecutive hits the player has made with his weapon without missing.
	ULONG		ulConsecutiveHits;

	// Number of consecutive hits the player has made with his railgun without missing.
	ULONG		ulConsecutiveRailgunHits;

	// Amount of frags this player has gotten without dying.
	ULONG		ulFragsWithoutDeath;

	// Amount of deaths this player has gotten without getting a frag.
	ULONG		ulDeathsWithoutFrag;

	// This player is chatting.
	bool		bChatting;

	// This player is currently spectating.
	bool		bSpectating;

	// This player is currently spectating after dying in LMS or survival co-op.
	bool		bDeadSpectator;

	// This player hit another player with his attack.
	bool		bStruckPlayer;

	// Number of times the railgun has been fired. Every 4 times, a reload is in order.
	ULONG		ulRailgunShots;

	// Number of medals the player currently has of each type.
	ULONG		ulMedalCount[NUM_MEDALS];

	// Icon currently above this player's head.
	AFloatyIcon	*pIcon;

	// Bonus to the maximum amount of health the player can have.
	LONG		lMaxHealthBonus;

	// Consecutive wins in duel mode.
	ULONG		ulWins;

	// Pointer to the bot information for this player.
	CSkullBot	*pSkullBot;

	// Is this player a bot?
	bool		bIsBot;

	// *** THE FOLLOWING ARE NETWORK VARIABLES ***
	// True XYZ position as told to us by the server.
	fixed_t		ServerXYZ[3];

	// True XYZ momentum as told to us by the server.
	fixed_t		ServerXYZMom[3];

	// Ping of the player to the server he's playing on.
	ULONG		ulPing;

	// Last tick this player received a packet.
//	ULONG		ulLastTick;

	// Is this player ready for the next map? (intermission)
	bool		bReadyToGoOn;

	// Is it alright to respawn in the same spot we died? (same spawn spot dmflag)
	bool		bSpawnOkay;

	// Position/angle we died at. This is for the same spawn spot dmflag.
	fixed_t		SpawnX;
	fixed_t		SpawnY;
	angle_t		SpawnAngle;

	// Save the old pending weapon. If the current one differs, update some clients.
	AWeapon		*OldPendingWeapon;

	// Is this player lagging to the server?
	bool		bLagging;

	// If this player was telefragged at the beginning of a round, allow him to respawn normally
	// in LMS games.
	bool		bSpawnTelefragged;

	// Amount of time this player has been on the server.
	ULONG		ulTime;

	// [BC] End of ST additions.

	fixed_t GetDeltaViewHeight() const
	{
		return (mo->ViewHeight + crouchviewdelta - viewheight) >> 3;
	}

	void Uncrouch()
	{
		crouchfactor = FRACUNIT;
		crouchoffset = 0;
		crouchdir = 0;
		crouching = 0;
		crouchviewdelta = 0;
	}

	int GetSpawnClass();
};

typedef player_s player_t;

// Bookkeeping on players - state.
extern player_s players[MAXPLAYERS];
extern DWORD playerswiping;

inline FArchive &operator<< (FArchive &arc, player_s *&p)
{
	return arc.SerializePointer (players, (BYTE **)&p, sizeof(*players));
}

//*****************************************************************************
//	PROTOTYPES

void	PLAYER_SetFragcount( player_s *pPlayer, LONG lFragCount, bool bAnnounce, bool bUpdateTeamFrags );
void	PLAYER_ResetAllPlayersFragcount( void );
void	PLAYER_GivePossessionPoint( player_s *pPlayer );
void	PLAYER_SetTeam( player_s *pPlayer, ULONG ulTeam, bool bNoBroadcast );
void	PLAYER_SetSpectator( player_s *pPlayer, bool bBroadcast, bool bDeadSpectator );
void	PLAYER_SetWins( player_s *pPlayer, ULONG ulWins );
void	PLAYER_GetName( player_s *pPlayer, char *pszOutBuf );
bool	PLAYER_IsTrueSpectator( player_s *pPlayer );
void	PLAYER_StruckPlayer( player_s *pPlayer );
bool	PLAYER_ShouldSpawnAsSpectator( player_s *pPlayer );
bool	PLAYER_Taunt( player_s *pPlayer );
LONG	PLAYER_GetRailgunColor( player_s *pPlayer );

EXTERN_CVAR( Bool, iwanttousecrouchingeventhoughitsretardedandunnecessaryanditsimplementationishorribleimeanverticallyshrinkingskinscomeonthatsinsanebutwhatevergoaheadandhaveyourcrouching );

void P_CheckPlayerSprites();


#define CROUCHSPEED (FRACUNIT/12)

EXTERN_CVAR(Float, maxviewpitch)

extern int currentrenderer;
#define MAX_DN_ANGLE	((int)((netgame || currentrenderer==0)? 56 : maxviewpitch))		// Max looking down angle
#define MAX_UP_ANGLE	((int)((netgame || currentrenderer==0)? 32 : maxviewpitch))		// Max looking up angle

// [GRB] Custom player classes
enum
{
	PCF_NOMENU			= 1,	// Hide in new game menu
};

class FPlayerClass
{
public:
	FPlayerClass ();
	FPlayerClass (const FPlayerClass &other);
	~FPlayerClass ();

	bool CheckSkin (int skin);

	const PClass *Type;
	DWORD Flags;
	TArray<int> Skins;
};

extern TArray<FPlayerClass> PlayerClasses;

#endif // __D_PLAYER_H__
