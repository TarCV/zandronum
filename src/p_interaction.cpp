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
//		Handling interactions (i.e., collisions).
//
//-----------------------------------------------------------------------------




// Data.
#include "doomdef.h"
#include "gstrings.h"

#include "doomstat.h"

#include "m_random.h"
#include "i_system.h"
#include "announcer.h"

#include "am_map.h"

#include "c_console.h"
#include "c_dispatch.h"

#include "p_local.h"

#include "p_lnspec.h"
#include "p_effect.h"
#include "p_acs.h"

#include "a_doomglobal.h"
#include "a_hereticglobal.h"
#include "ravenshared.h"
#include "a_hexenglobal.h"
#include "a_sharedglobal.h"
#include "a_pickups.h"
#include "gi.h"
#include "templates.h"
#include "sbar.h"
#include "deathmatch.h"
#include "duel.h"
#include "medal.h"
#include "network.h"
#include "sv_commands.h"
#include "g_game.h"
#include "gamemode.h"
#include "cl_main.h"
#include "joinqueue.h"
#include "lastmanstanding.h"
#include "scoreboard.h"
#include "cooperative.h"
#include "invasion.h"
#include "survival.h"
#include "team.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "win32/g15/g15.h"
#include "r_translate.h"

// [BC] Ugh.
void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags );
void SERVERCONSOLE_UpdateScoreboard( void );

static FRandom pr_obituary ("Obituary");
static FRandom pr_botrespawn ("BotRespawn");
static FRandom pr_killmobj ("ActorDie");
static FRandom pr_damagemobj ("ActorTakeDamage");
static FRandom pr_lightning ("LightningDamage");
static FRandom pr_poison ("PoisonDamage");
static FRandom pr_switcher ("SwitchTarget");

EXTERN_CVAR (Bool, show_obituaries)


FName MeansOfDeath;
bool FriendlyFire;

//
// GET STUFF
//

//
// P_TouchSpecialThing
//
void P_TouchSpecialThing (AActor *special, AActor *toucher)
{
	fixed_t delta = special->z - toucher->z;

	if (delta > toucher->height || delta < -32*FRACUNIT)
	{ // out of reach
		return;
	}

	// Dead thing touching.
	// Can happen with a sliding player corpse.
	if (toucher->health <= 0)
		return;

	// [BC] Don't allow non-players to touch special things.
	if ( toucher->player == NULL )
		return;

	// [BC] Don't allow spectating players to pickup items.
	if ( toucher->player->bSpectating )
		return;

	special->Touch (toucher);
}


// [RH]
// SexMessage: Replace parts of strings with gender-specific pronouns
//
// The following expansions are performed:
//		%g -> he/she/it
//		%h -> him/her/it
//		%p -> his/her/its
//		%o -> other (victim)
//		%k -> killer
//
void SexMessage (const char *from, char *to, int gender, const char *victim, const char *killer)
{
	static const char *genderstuff[3][3] =
	{
		{ "he",  "him", "his" },
		{ "she", "her", "her" },
		{ "it",  "it",  "its" }
	};
	static const int gendershift[3][3] =
	{
		{ 2, 3, 3 },
		{ 3, 3, 3 },
		{ 2, 2, 3 }
	};
	const char *subst = NULL;

	do
	{
		if (*from != '%')
		{
			*to++ = *from;
		}
		else
		{
			int gendermsg = -1;
			
			switch (from[1])
			{
			case 'g':	gendermsg = 0;	break;
			case 'h':	gendermsg = 1;	break;
			case 'p':	gendermsg = 2;	break;
			case 'o':	subst = victim;	break;
			case 'k':	subst = killer;	break;
			}
			if (subst != NULL)
			{
				size_t len = strlen (subst);
				memcpy (to, subst, len);
				to += len;
				from++;
				subst = NULL;
			}
			else if (gendermsg < 0)
			{
				*to++ = '%';
			}
			else
			{
				strcpy (to, genderstuff[gender][gendermsg]);
				to += gendershift[gender][gendermsg];
				from++;
			}
		}
	} while (*from++);
}

// [RH]
// ClientObituary: Show a message when a player dies
//
// [BC] Allow passing in of the MOD so clients can use this function too.
void ClientObituary (AActor *self, AActor *inflictor, AActor *attacker, FName MeansOfDeath)
{
	FName	mod;
	const char *message;
	const char *messagename;
	char gendermessage[1024];
	bool friendly;
	int  gender;
	// We enough characters for the player's name, the terminating zero, and 4 characters
	// to strip the color codes (I actually believe it's 3, but let's play it safe).
	char	szAttacker[MAXPLAYERNAME+1+4];
	char	szVictim[MAXPLAYERNAME+1+4];

	// No obituaries for non-players, voodoo dolls or when not wanted
	if (self->player == NULL || self->player->mo != self || !show_obituaries)
		return;

	gender = self->player->userinfo.gender;

	// Treat voodoo dolls as unknown deaths
	if (inflictor && inflictor->player == self->player)
		MeansOfDeath = NAME_None;

	// Must be in cooperative mode.
	if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) && ( deathmatch == false ) && ( teamgame == false ))
		FriendlyFire = true;

	friendly = FriendlyFire;
	mod = MeansOfDeath;
	message = NULL;
	messagename = NULL;

	switch (mod)
	{
	case NAME_Suicide:		messagename = "OB_SUICIDE";		break;
	case NAME_Falling:		messagename = "OB_FALLING";		break;
	case NAME_Crush:		messagename = "OB_CRUSH";		break;
	case NAME_Exit:			messagename = "OB_EXIT";		break;
	case NAME_Drowning:		messagename = "OB_WATER";		break;
	case NAME_Slime:		messagename = "OB_SLIME";		break;
	case NAME_Fire:			if (attacker == NULL) messagename = "OB_LAVA";		break;
	}

	if (messagename != NULL)
		message = GStrings(messagename);

	if (attacker != NULL && message == NULL)
	{
		if (attacker == self)
		{
			// [BB] Added switch here.
			switch (mod)
			{
			// [BC] Obituaries for killing yourself with Skulltag weapons.
			case NAME_Grenade:	messagename = "OB_GRENADE_SELF";	break;
			case NAME_BFG10k:	messagename = "OB_BFG10K_SELF";		break;
			}
			if (messagename != NULL)
				message = GStrings(messagename);
			else
				message = GStrings("OB_KILLEDSELF");
		}
		else if (attacker->player == NULL)
		{
			if ((mod == NAME_Telefrag) || (mod == NAME_SpawnTelefrag))
			{
				message = GStrings("OB_MONTELEFRAG");
			}
			else if (mod == NAME_Melee)
			{
				message = attacker->GetClass()->Meta.GetMetaString (AMETA_HitObituary);
				if (message == NULL)
				{
					message = attacker->GetClass()->Meta.GetMetaString (AMETA_Obituary);
				}
			}
			else
			{
				message = attacker->GetClass()->Meta.GetMetaString (AMETA_Obituary);
			}
		}
	}

	if (message == NULL && attacker != NULL && attacker->player != NULL)
	{
		if (friendly)
		{
			// [BC] We'll do this elsewhere.
//			attacker->player->fragcount -= 2;
			self = attacker;
			gender = self->player->userinfo.gender;
			sprintf (gendermessage, "OB_FRIENDLY%c", '1' + (pr_obituary() & 3));
			message = GStrings(gendermessage);
		}
		else
		{
			// [BC] NAME_SpawnTelefrag, too.
			if ((mod == NAME_Telefrag) || (mod == NAME_SpawnTelefrag)) message = GStrings("OB_MPTELEFRAG");
			// [BC] Handle Skulltag's reflection rune.
			// [RC] Moved here to fix the "[victim] was killed via [victim]'s reflection rune" bug.
			else if ( mod == NAME_Reflection )
			{
				messagename = "OB_REFLECTION";
				message = GStrings(messagename);
			}

			if (message == NULL)
			{
				if (inflictor != NULL)
				{
					message = inflictor->GetClass()->Meta.GetMetaString (AMETA_Obituary);
				}
				if (message == NULL && attacker->player->ReadyWeapon != NULL)
				{
					message = attacker->player->ReadyWeapon->GetClass()->Meta.GetMetaString (AMETA_Obituary);
				}
				if (message == NULL)
				{
					switch (mod)
					{
					case NAME_BFGSplash:	messagename = "OB_MPBFG_SPLASH";	break;
					case NAME_Railgun:		messagename = "OB_RAILGUN";			break;
					}
					if (messagename != NULL)
						message = GStrings(messagename);
				}
			}
		}
	}
	else attacker = self;	// for the message creation

	if (message != NULL && message[0] == '$') 
	{
		message=GStrings[message+1];
	}

	if (message == NULL)
	{
		message = GStrings("OB_DEFAULT");
	}

	// [BC] Stop the color codes after we display a name in the obituary string.
	if ( attacker && attacker->player )
		sprintf( szAttacker, "%s\\c-", attacker->player->userinfo.netname );
	else
		szAttacker[0] = 0;

	if ( self && self->player )
		sprintf( szVictim, "%s\\c-", self->player->userinfo.netname );
	else
		szVictim[0] = 0;

	SexMessage (message, gendermessage, gender,
		szVictim[0] ? szVictim : self->player->userinfo.netname,
		szAttacker[0] ? szAttacker : attacker->player->userinfo.netname);
	Printf (PRINT_MEDIUM, "%s\n", gendermessage);
}


//
// KillMobj
//
EXTERN_CVAR (Int, fraglimit)

static int GibHealth(AActor *actor)
{
	return -abs(
		actor->GetClass()->Meta.GetMetaInt (
			AMETA_GibHealth,
			gameinfo.gametype == GAME_Doom ?
				-actor->GetDefault()->health :
				-actor->GetDefault()->health/2));
}

void AActor::Die (AActor *source, AActor *inflictor)
{
	// [BB] Potentially get rid of some corpses. This isn't necessarily client-only.
	//CLIENT_RemoveMonsterCorpses();

	// [BC]
	bool	bPossessedTerminatorArtifact;

	// Handle possible unmorph on death
	bool wasgibbed = (health < GibHealth(this));
	AActor *realthis = NULL;
	int realstyle = 0;
	int realhealth = 0;
	if (P_MorphedDeath(this, &realthis, &realstyle, &realhealth))
	{
		if (!(realstyle & MORPH_UNDOBYDEATHSAVES))
		{
			if (wasgibbed)
			{
				int realgibhealth = GibHealth(realthis);
				if (realthis->health >= realgibhealth)
				{
					realthis->health = realgibhealth -1; // if morphed was gibbed, so must original be (where allowed)
				}
			}
			realthis->Die(source, inflictor);
		}
		return;
	}

	// [SO] 9/2/02 -- It's rather funny to see an exploded player body with the invuln sparkle active :) 
	effects &= ~FX_RESPAWNINVUL;

	// Also kill the alpha flicker cause by the visibility flicker.
	if ( effects & FX_VISIBILITYFLICKER )
	{
		RenderStyle = STYLE_Normal;
		effects &= ~FX_VISIBILITYFLICKER;
	}

	//flags &= ~MF_INVINCIBLE;
/*
	if (debugfile && this->player)
	{
		static int dieticks[MAXPLAYERS];
		int pnum = this->player-players;
		if (dieticks[pnum] == gametic)
			gametic=gametic;
		dieticks[pnum] = gametic;
		fprintf (debugfile, "died (%d) on tic %d (%s)\n", pnum, gametic,
		this->player->cheats&CF_PREDICTING?"predicting":"real");
	}
*/
	// [BC] Since the player loses his terminator status after we tell his inventory
	// that we died, check for it before doing so.
	bPossessedTerminatorArtifact = !!(( player ) && ( player->cheats & CF_TERMINATORARTIFACT ));

	// [BC] Check to see if any medals need to be awarded.
	if (( player ) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		if (( source ) &&
			( source->player ))
		{
			MEDAL_PlayerDied( ULONG( player - players ), ULONG( source->player - players ));
		}
		else
			MEDAL_PlayerDied( ULONG( player - players ), MAXPLAYERS );
	}

	// [RH] Notify this actor's items.
	for (AInventory *item = Inventory; item != NULL; )
	{
		AInventory *next = item->Inventory;
		item->OwnerDied();
		item = next;
	}

	if (flags & MF_MISSILE)
	{ // [RH] When missiles die, they just explode
		P_ExplodeMissile (this, NULL, NULL);
		return;
	}
	// [RH] Set the target to the thing that killed it. Strife apparently does this.
	if (source != NULL)
	{
		target = source;
	}

	flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);
	if (!(flags4 & MF4_DONTFALL)) flags&=~MF_NOGRAVITY;
	flags |= MF_DROPOFF;
	if ((flags3 & MF3_ISMONSTER) || FindState(NAME_Raise) != NULL || IsKindOf(RUNTIME_CLASS(APlayerPawn)))
	{	// [RH] Only monsters get to be corpses.
		// Objects with a raise state should get the flag as well so they can
		// be revived by an Arch-Vile. Batman Doom needs this.
		flags |= MF_CORPSE;
		// [BB] Potentially get rid of one corpse in invasion from the previous wave.
		if ( invasion )
			INVASION_RemoveMonsterCorpse();
	}
	// [RH] Allow the death height to be overridden using metadata.
	fixed_t metaheight = 0;
	if (DamageType == NAME_Fire)
	{
		metaheight = GetClass()->Meta.GetMetaFixed (AMETA_BurnHeight);
	}
	if (metaheight == 0)
	{
		metaheight = GetClass()->Meta.GetMetaFixed (AMETA_DeathHeight);
	}
	if (metaheight != 0)
	{
		height = MAX<fixed_t> (metaheight, 0);
	}
	else
	{
		height >>= 2;
	}

	// [RH] If the thing has a special, execute and remove it
	//		Note that the thing that killed it is considered
	//		the activator of the script.
	// New: In Hexen, the thing that died is the activator,
	//		so now a level flag selects who the activator gets to be.
	if (special && (!(flags & MF_SPECIAL) || (flags3 & MF3_ISMONSTER)))
	{
		LineSpecials[special] (NULL, level.flags & LEVEL_ACTOWNSPECIAL
			? this : source, false, args[0], args[1], args[2], args[3], args[4]);
		special = 0;
	}

	if (CountsAsKill())
	{
		level.killed_monsters++;
		
		// [BB] The number of remaining monsters decreased, update the invasion monster count accordingly.
		INVASION_UpdateMonsterCount ( this, true );
	}

	if (source && source->player)
	{
		// [BC] Don't do this in client mode.
		if ((CountsAsKill()) &&
			( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
			( CLIENTDEMO_IsPlaying( ) == false ))
		{ // count for intermission
			source->player->killcount++;
			
			// Update the clients with this player's updated killcount.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetPlayerKillCount( ULONG( source->player - players ) );

				// Also, update the scoreboard.
				SERVERCONSOLE_UpdatePlayerInfo( ULONG( source->player - players ), UDF_FRAGS );
				SERVERCONSOLE_UpdateScoreboard( );
			}
		}

		// Don't count any frags at level start, because they're just telefrags
		// resulting from insufficient deathmatch starts, and it wouldn't be
		// fair to count them toward a player's score.
		if (player && ( MeansOfDeath != NAME_SpawnTelefrag ))
		{
			if ( source->player->pSkullBot )
			{
				source->player->pSkullBot->m_ulPlayerKilled = player - players;
				if (( player - players ) == source->player->pSkullBot->m_ulPlayerEnemy )
					source->player->pSkullBot->PostEvent( BOTEVENT_ENEMY_KILLED );
			}

			// [BC] Potentially get rid of some corpses. This isn't necessarily client-only.
			CLIENT_RemoveCorpses( );

			if (player == source->player)	// [RH] Cumulative frag count
			{
				// [BC] Frags are server side.
				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
					PLAYER_SetFragcount( player, player->fragcount - (( bPossessedTerminatorArtifact ) ? 10 : 1 ), true, true );
			}
			else
			{
				// [BC] Frags are server side.
				// [BC] Player receives 10 frags for killing the terminator!
				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
				{
					if ((dmflags2 & DF2_YES_LOSEFRAG) && deathmatch)
						PLAYER_SetFragcount( player, player->fragcount - 1, true, true );

					if ( source->IsTeammate( this ))
						PLAYER_SetFragcount( source->player, source->player->fragcount - (( bPossessedTerminatorArtifact ) ? 10 : 1 ), true, true );
					else
						PLAYER_SetFragcount( source->player, source->player->fragcount + (( bPossessedTerminatorArtifact ) ? 10 : 1 ), true, true );
				}

				// [BC] Add this frag to the server's statistic module.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_STATISTIC_AddToTotalFrags( );

				if (source->player->morphTics)
				{ // Make a super chicken
					source->GiveInventoryType (RUNTIME_CLASS(APowerWeaponLevel2));
				}
			}

			// Play announcer sounds for amount of frags remaining.
			if (( lastmanstanding == false ) && ( teamlms == false ) && ( possession == false ) && ( teampossession == false ) && ( domination == false ) && deathmatch && fraglimit )
			{
				// [RH] Implement fraglimit
				// [BC] Betterized!
				// [BB] Clients may not do this.
				if ( fraglimit <= D_GetFragCount( source->player )
					 && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ) )
				{
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						SERVER_Printf( PRINT_HIGH, "%s\n", GStrings( "TXT_FRAGLIMIT" ));
						if ( teamplay && ( source->player->bOnTeam ))
							SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", TEAM_GetName( source->player->ulTeam ));
						else
							SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", source->player->userinfo.netname );
					}
					else
					{
						Printf( "%s\n", GStrings( "TXT_FRAGLIMIT" ));
						if ( teamplay && ( source->player->bOnTeam ))
							Printf( "%s \\c-wins!\n", TEAM_GetName( source->player->ulTeam ));
						else
							Printf( "%s \\c-wins!\n", source->player->userinfo.netname );

						if (( duel == false ) && ( source->player - players == consoleplayer ))
							ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
					}

					// Pause for five seconds for the win sequence.
					if ( duel )
					{
						// Also, do the win sequence for the player.
						DUEL_SetLoser( player - players );
						DUEL_DoWinSequence( source->player - players );

						// Give the winner a win.
						PLAYER_SetWins( source->player, source->player->ulWins + 1 );

						GAME_SetEndLevelDelay( 5 * TICRATE );
					}
					// End the level after five seconds.
					else
					{
						char				szString[64];
						DHUDMessageFadeOut	*pMsg;

						// Just print "YOU WIN!" in single player.
						if (( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ) && ( players[consoleplayer].mo->CheckLocalView( source->player - players )))
							sprintf( szString, "\\cGYOU WIN!" );
						else if (( teamplay ) && ( source->player->bOnTeam ))
						{
							if ( source->player->ulTeam == TEAM_BLUE )
								sprintf( szString, "\\cH%s wins!\n", TEAM_GetName( source->player->ulTeam ));
							else
								sprintf( szString, "\\cG%s wins!\n", TEAM_GetName( source->player->ulTeam ));
						}
						else
							sprintf( szString, "%s \\cGWINS!", players[source->player - players].userinfo.netname );
						V_ColorizeString( szString );

						if ( NETWORK_GetState( ) != NETSTATE_SERVER )
						{
							screen->SetFont( BigFont );

							// Display "%s WINS!" HUD message.
							pMsg = new DHUDMessageFadeOut( szString,
								160.4f,
								75.0f,
								320,
								200,
								CR_WHITE,
								3.0f,
								2.0f );

							StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
							screen->SetFont( SmallFont );

							szString[0] = 0;
							pMsg = new DHUDMessageFadeOut( szString,
								0.0f,
								0.0f,
								0,
								0,
								CR_RED,
								3.0f,
								2.0f );

							StatusBar->AttachMessage( pMsg, MAKE_ID('F','R','A','G') );

							pMsg = new DHUDMessageFadeOut( szString,
								0.0f,
								0.0f,
								0,
								0,
								CR_RED,
								3.0f,
								2.0f );

							StatusBar->AttachMessage( pMsg, MAKE_ID('P','L','A','C') );
						}
						else
						{
							SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 160.4f, 75.0f, 320, 200, CR_WHITE, 3.0f, 2.0f, "BigFont", false, MAKE_ID('C','N','T','R') );
							szString[0] = 0;
							SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 0.0f, 0.0f, 0, 0, CR_WHITE, 3.0f, 2.0f, "BigFont", false, MAKE_ID('F','R','A','G') );
							SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 0.0f, 0.0f, 0, 0, CR_WHITE, 3.0f, 2.0f, "BigFont", false, MAKE_ID('P','L','A','C') );
						}

						GAME_SetEndLevelDelay( 5 * TICRATE );
					}
				}
			}
		}

		// If the player got telefragged by a player trying to spawn, allow him to respawn.
		if (( player ) && ( lastmanstanding || teamlms || survival ) && ( MeansOfDeath == NAME_SpawnTelefrag ))
			player->bSpawnTelefragged = true;
	}
	else if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ) && (CountsAsKill()))
	{
		// count all monster deaths,
		// even those caused by other monsters

		// Why give player 0 credit? :P
		// Meh, just do it in single player.
		if ( NETWORK_GetState( ) == NETSTATE_SINGLE )
			players[0].killcount++;

		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
//			SERVER_PlayerKilledMonster( MAXPLAYERS );

			// Also, update the scoreboard.
			SERVERCONSOLE_UpdateScoreboard( );
		}
	}
	
	// [BC] Don't do this block in client mode.
	if (player &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		// [BC] If this is a bot, tell it it died.
		if ( player->pSkullBot )
		{
			if ( source && source->player )
				player->pSkullBot->m_ulPlayerKilledBy = source->player - players;
			else
				player->pSkullBot->m_ulPlayerKilledBy = MAXPLAYERS;

			if ( source && source->player )
			{
				if ( source->player == player )
					player->pSkullBot->PostEvent( BOTEVENT_KILLED_BYSELF );
				else if (( source->player - players ) == player->pSkullBot->m_ulPlayerEnemy )
					player->pSkullBot->PostEvent( BOTEVENT_KILLED_BYENEMY );
				else
					player->pSkullBot->PostEvent( BOTEVENT_KILLED_BYPLAYER );
			}
			else
				player->pSkullBot->PostEvent( BOTEVENT_KILLED_BYENVIORNMENT );
		}

		// [BC] Keep track of where we died for the "same spot respawn" dmflags.
		player->SpawnX = x;
		player->SpawnY = y;
		player->SpawnAngle = angle;
		player->bSpawnOkay = true;

		// Death script execution, care of Skull Tag
		FBehavior::StaticStartTypedScripts (SCRIPT_Death, this, true);

		// [RH] Force a delay between death and respawn
		if ((( i_compatflags & COMPATF_INSTANTRESPAWN ) == false ) ||
			( player->bSpawnTelefragged ))
		{
			player->respawn_time = level.time + TICRATE;

			// [BC] Don't respawn quite so fast on forced respawn. It sounds weird when your
			// scream isn't completed.
			if ( dmflags & DF_FORCE_RESPAWN )
				player->respawn_time += TICRATE/2;
		}

		// count environment kills against you
		if (!source)
		{
			PLAYER_SetFragcount( player, player->fragcount - (( bPossessedTerminatorArtifact ) ? 10 : 1 ), true, true );	// [RH] Cumulative frag count

			// Spawning in nukage or getting crushed is NOT
			// somewhere where you would want to be at when you respawn again
			player->bSpawnOkay = false;
		}
						
		// [BC] Increment team deathcount.
		if (( teamplay || teampossession ) && ( player->bOnTeam ))
			TEAM_SetDeathCount( player->ulTeam, TEAM_GetDeathCount( player->ulTeam ) + 1 );
	}

	// [BC] We can do this stuff in client mode.
	if (player)
	{
		flags &= ~MF_SOLID;
		player->playerstate = PST_DEAD;

		P_DropWeapon (player);

		if (this == players[consoleplayer].camera && automapactive)
		{
			// don't die in auto map, switch view prior to dying
			AM_Stop ();
		}

		// [GRB] Clear extralight. When you killed yourself with weapon that
		// called A_Light1/2 before it called A_Light0, extraligh remained.
		player->extralight = 0;
	}

	// [RH] If this is the unmorphed version of another monster, destroy this
	// actor, because the morphed version is the one that will stick around in
	// the level.
	if (flags & MF_UNMORPHED)
	{
		Destroy ();
		return;
	}

	// [BC] Tell clients that this thing died.
	// [BC] We need to move this block a little higher, otherwise things can be destroyed
	// before we tell the client to kill them.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// If there isn't a player attached to this object, treat it like a normal thing.
		if ( player == NULL )
			SERVERCOMMANDS_KillThing( this, source, inflictor );
		else
			SERVERCOMMANDS_KillPlayer( ULONG( player - players ), source, inflictor, MeansOfDeath );
	}

	FState *diestate = NULL;

	if (DamageType != NAME_None)
	{
		diestate = FindState (NAME_Death, DamageType, true);
		if (diestate == NULL)
		{
			if (DamageType == NAME_Ice)
			{ // If an actor doesn't have an ice death, we can still give them a generic one.

				if (!deh.NoAutofreeze && !(flags4 & MF4_NOICEDEATH) && (player || (flags3 & MF3_ISMONSTER)))
				{
					diestate = &AActor::States[S_GENERICFREEZEDEATH];
				}
			}
		}
	}
	if (diestate == NULL)
	{
		int flags4 = inflictor == NULL ? 0 : inflictor->flags4;

		int gibhealth = GibHealth(this);
		
		// Don't pass on a damage type this actor cannot handle.
		// (most importantly, prevent barrels from passing on ice damage.)
		// Massacre must be preserved though.
		if (DamageType != NAME_Massacre)
		{
			DamageType = NAME_None;	
		}

		if ((health < gibhealth || flags4 & MF4_EXTREMEDEATH) && !(flags4 & MF4_NOEXTREMEDEATH))
		{ // Extreme death
			diestate = FindState (NAME_Death, NAME_Extreme, true);
			// If a non-player, mark as extremely dead for the crash state.
			if (diestate != NULL && player == NULL && health >= gibhealth)
			{
				health = gibhealth - 1;
			}
			// For players, mark the appropriate flag.
			else if (player != NULL)
			{
				player->cheats |= CF_EXTREMELYDEAD;
			}
		}
		if (diestate == NULL)
		{ // Normal death
			diestate = FindState (NAME_Death);
		}
	}

	if (diestate != NULL)
	{
		SetState (diestate);

		tics -= pr_killmobj() & 3;
		if (tics < 1)
			tics = 1;
	}
	else
	{
		Destroy();
	}

	if (( deathmatch || teamgame ) &&
		( NETWORK_GetState( ) != NETSTATE_SERVER ) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ) &&
		((( duel ) && (( DUEL_GetState( ) == DS_WINSEQUENCE ) || ( DUEL_GetState( ) == DS_COUNTDOWN ))) == false ) &&
		((( lastmanstanding || teamlms ) && (( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE ) || ( LASTMANSTANDING_GetState( ) == LMSS_COUNTDOWN ))) == false ))
	{
		if (( player ) && ( source ) && ( source->player ) && ( player != source->player ) && ( MeansOfDeath != NAME_SpawnTelefrag ))
		{
			if ((( deathmatch == false ) || (( fraglimit == 0 ) || ( source->player->fragcount < fraglimit ))) &&
				(( lastmanstanding == false ) || (( winlimit == 0 ) || ( source->player->ulWins < static_cast<unsigned> (winlimit) ))) &&
				(( teamlms == false ) || (( winlimit == 0 ) || ( TEAM_GetWinCount( source->player->ulTeam ) < winlimit ))))
			{
				// Display a large "You were fragged by <name>." message in the middle of the screen.
				if (( player - players ) == consoleplayer )
				{
					if ( cl_showlargefragmessages )
						SCOREBOARD_DisplayFraggedMessage( source->player );
					
					if ( G15_IsReady() ) // [RC] Also show the message on the Logitech G15 (if enabled).
						G15_ShowLargeFragMessage( source->player->userinfo.netname, false );
				}

				// Display a large "You fragged <name>!" message in the middle of the screen.
				else if (( source->player - players ) == consoleplayer )
				{
					if ( cl_showlargefragmessages )
						SCOREBOARD_DisplayFragMessage( player );
					
					if ( G15_IsReady() ) // [RC] Also show the message on the Logitech G15 (if enabled).
						G15_ShowLargeFragMessage( player->userinfo.netname, true );
				}
			}
		}
	}

	// [RH] Death messages
	if (( player ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		ClientObituary (this, inflictor, source, MeansOfDeath);

}




//---------------------------------------------------------------------------
//
// PROC P_AutoUseHealth
//
//---------------------------------------------------------------------------

void P_AutoUseHealth(player_t *player, int saveHealth)
{
	int i;
	int count;
	const PClass *normalType = PClass::FindClass (NAME_ArtiHealth);
	const PClass *superType = PClass::FindClass (NAME_ArtiSuperHealth);
	AInventory *normalItem = player->mo->FindInventory (normalType);
	AInventory *superItem = player->mo->FindInventory (superType);
	int normalAmount, superAmount;

	normalAmount = normalItem != NULL ? normalItem->Amount : 0;
	superAmount = superItem != NULL ? superItem->Amount : 0;

	bool skilluse = !!G_SkillProperty(SKILLP_AutoUseHealth);

	if (skilluse && (normalAmount*25 >= saveHealth))
	{ // Use quartz flasks
		count = (saveHealth+24)/25;
		for(i = 0; i < count; i++)
		{
			player->health += 25;
			if (--normalItem->Amount == 0)
			{
				normalItem->Destroy ();
				break;
			}
		}
	}
	else if (superAmount*100 >= saveHealth)
	{ // Use mystic urns
		count = (saveHealth+99)/100;
		for(i = 0; i < count; i++)
		{
			player->health += 100;
			if (--superItem->Amount == 0)
			{
				superItem->Destroy ();
				break;
			}
		}
	}
	else if (skilluse
		&& (superAmount*100+normalAmount*25 >= saveHealth))
	{ // Use mystic urns and quartz flasks
		count = (saveHealth+24)/25;
		saveHealth -= count*25;
		for(i = 0; i < count; i++)
		{
			player->health += 25;
			if (--normalItem->Amount == 0)
			{
				normalItem->Destroy ();
				break;
			}
		}
		count = (saveHealth+99)/100;
		for(i = 0; i < count; i++)
		{
			player->health += 100;
			if (--superItem->Amount == 0)
			{
				superItem->Destroy ();
				break;
			}
		}
	}
	player->mo->health = player->health;
}

//============================================================================
//
// P_AutoUseStrifeHealth
//
//============================================================================

void P_AutoUseStrifeHealth (player_t *player)
{
	static const ENamedName healthnames[2] = { NAME_MedicalKit, NAME_MedPatch };

	for (int i = 0; i < 2; ++i)
	{
		const PClass *type = PClass::FindClass (healthnames[i]);

		while (player->health < 50)
		{
			AInventory *item = player->mo->FindInventory (type);
			if (item == NULL)
				break;
			if (!player->mo->UseInventory (item))
				break;
		}
	}
}

/*
=================
=
= P_DamageMobj
=
= Damages both enemies and players
= inflictor is the thing that caused the damage
= 		creature or missile, can be NULL (slime, etc)
= source is the thing to target after taking damage
=		creature or NULL
= Source and inflictor are the same for melee attacks
= source can be null for barrel explosions and other environmental stuff
==================
*/


void P_DamageMobj (AActor *target, AActor *inflictor, AActor *source, int damage, FName mod, int flags)
{
	unsigned ang;
	player_t *player;
	fixed_t thrust;
	int temp;
	// [BC]
	LONG	lOldTargetHealth;

	// [BC] Game is currently in a suspended state; don't hurt anyone.
	if ( GAME_GetEndLevelDelay( ))
		return;

	if (target == NULL || !(target->flags & MF_SHOOTABLE))
	{ // Shouldn't happen
		return;
	}

	// Spectral targets only take damage from spectral projectiles.
	if (target->flags4 & MF4_SPECTRAL && damage < 1000000)
	{
		if (inflictor == NULL || !(inflictor->flags4 & MF4_SPECTRAL))
		{
			/*
			if (target->MissileState != NULL)
			{
				target->SetState (target->MissileState);
			}
			*/
			return;
		}
	}
	if (target->health <= 0)
	{
		if (inflictor && mod == NAME_Ice)
		{
			return;
		}
		else if (target->flags & MF_ICECORPSE) // frozen
		{
			target->tics = 1;
			target->momx = target->momy = target->momz = 0;

			// [BC] If we're the server, tell clients to update this thing's tics and
			// momentum.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetThingTics( target );
				SERVERCOMMANDS_MoveThing( target, CM_MOMX|CM_MOMY|CM_MOMZ );
			}
		}
		return;
	}
	if ((target->flags2 & MF2_INVULNERABLE) && damage < 1000000)
	{ // actor is invulnerable
		if (!target->player)
		{
			if (!inflictor || !(inflictor->flags3 & MF3_FOILINVUL))
			{
				return;
			}
		}
		else
		{
			// Only in Hexen invulnerable players are excluded from getting
			// thrust by damage.
			if (gameinfo.gametype == GAME_Hexen) return;
		}
		
	}
	if (inflictor != NULL)
	{
		if (inflictor->flags5 & MF5_PIERCEARMOR) flags |= DMG_NO_ARMOR;
	}
	
	MeansOfDeath = mod;
	FriendlyFire = false;
	// [RH] Andy Baker's Stealth monsters
	if (target->flags & MF_STEALTH)
	{
		target->alpha = OPAQUE;
		target->visdir = -1;
	}
	// [BB] The clients may not do this.
	if ( (target->flags & MF_SKULLFLY)
	     && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ) )
	{
		target->momx = target->momy = target->momz = 0;

		// [BC] If we're the server, tell clients to update this thing's momentum
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThing( target, CM_MOMX|CM_MOMY|CM_MOMZ );
	}
	if (target->flags2 & MF2_DORMANT)
	{
		// Invulnerable, and won't wake up
		return;
	}
	player = target->player;
	if (player && damage > 1)
	{
		// Take half damage in trainer mode
		damage = FixedMul(damage, G_SkillProperty(SKILLP_DamageFactor));
	}
	// Special damage types
	if (inflictor)
	{
		if (inflictor->flags4 & MF4_SPECTRAL)
		{
			if (player != NULL)
			{
				if (inflictor->health == -1)
					return;
			}
			else if (target->flags4 & MF4_SPECTRAL)
			{
				if (inflictor->health == -2)
					return;
			}
		}

		damage = inflictor->DoSpecialDamage (target, damage);
		if (damage == -1)
		{
			return;
		}

	}
	// Handle active damage modifiers (e.g. PowerDamage)
	if (source != NULL && source->Inventory != NULL)
	{
		int olddam = damage;
		source->Inventory->ModifyDamage(olddam, mod, damage, false);
		if (olddam != damage && damage <= 0) return;
	}
	// Handle passive damage modifiers (e.g. PowerProtection)
	if (target->Inventory != NULL)
	{
 		int olddam = damage;
		target->Inventory->ModifyDamage(olddam, mod, damage, true);
		if (olddam != damage && damage <= 0) return;
	}

	// to be removed and replaced by an actual damage factor 
	// once the actors using it are converted to DECORATE.
	if (mod == NAME_Fire && target->flags4 & MF4_FIRERESIST)
	{
		damage /= 2;
	}
	else
	{
		DmgFactors * df = target->GetClass()->ActorInfo->DamageFactors;
		if (df != NULL)
		{
			fixed_t * pdf = df->CheckKey(mod);
			if (pdf != NULL)
			{
				damage = FixedMul(damage, *pdf);
				if (damage <= 0) return;
			}
		}
	}

	damage = target->TakeSpecialDamage (inflictor, source, damage, mod);

	if (damage == -1)
	{
		return;
	}

	// [BC] If the target player has the reflection rune, damage the source with 50% of the
	// this player is being damaged with.
	if (( target->player ) &&
		( target->player->cheats & CF_REFLECTION ) &&
		( source ) &&
		( mod != NAME_Reflection ) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		if ( target != source )
		{
			P_DamageMobj( source, NULL, target, (( damage * 3 ) / 4 ), NAME_Reflection );

			// Reset means of death flag.
			MeansOfDeath = mod;
		}
	}

	// Push the target unless the source's weapon's kickback is 0.
	// (i.e. Guantlets/Chainsaw)
	// [BB] The server handles this.
	if (inflictor && inflictor != target	// [RH] Not if hurting own self
		&& !(target->flags & MF_NOCLIP)
		&& !(inflictor->flags2 & MF2_NODMGTHRUST)
		&& !(flags & DMG_THRUSTLESS)
		&& ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		&& ( CLIENTDEMO_IsPlaying( ) == false ) )
	{
		int kickback;

		if (!source || !source->player || !source->player->ReadyWeapon)
			kickback = gameinfo.defKickback;
		else
			kickback = source->player->ReadyWeapon->Kickback;

		if (kickback)
		{
			AActor *origin = (source && (flags & DMG_INFLICTOR_IS_PUFF))? source : inflictor;
			
			ang = R_PointToAngle2 (origin->x, origin->y,
				target->x, target->y);
			thrust = damage*(FRACUNIT>>3)*kickback / target->Mass;
			// [RH] If thrust overflows, use a more reasonable amount
			if (thrust < 0 || thrust > 10*FRACUNIT)
			{
				thrust = 10*FRACUNIT;
			}
			// make fall forwards sometimes
			if ((damage < 40) && (damage > target->health)
				 && (target->z - origin->z > 64*FRACUNIT)
				 && (pr_damagemobj()&1)
				 // [RH] But only if not too fast and not flying
				 && thrust < 10*FRACUNIT
				 && !(target->flags & MF_NOGRAVITY))
			{
				ang += ANG180;
				thrust *= 4;
			}
			ang >>= ANGLETOFINESHIFT;
			if (source && source->player && (source == inflictor)
				&& source->player->ReadyWeapon != NULL &&
				(source->player->ReadyWeapon->WeaponFlags & WIF_STAFF2_KICKBACK))
			{
				// Staff power level 2
				target->momx += FixedMul (10*FRACUNIT, finecosine[ang]);
				target->momy += FixedMul (10*FRACUNIT, finesine[ang]);
				if (!(target->flags & MF_NOGRAVITY))
				{
					target->momz += 5*FRACUNIT;
				}
			}
			else
			{
				target->momx += FixedMul (thrust, finecosine[ang]);
				target->momy += FixedMul (thrust, finesine[ang]);
			}

			// [BC] Set the thing's momentum.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_MoveThingExact( target, CM_MOMX|CM_MOMY|CM_MOMZ );
			}
		}
	}

	//
	// player specific
	//
	// [BC]
	lOldTargetHealth = target->health;
	if (player)
	{
		if ((target->flags2 & MF2_INVULNERABLE) && damage < 1000000)
		{ // player is invulnerable, so don't hurt him
			return;
		}

		// end of game hell hack
		if ((target->Sector->special & 255) == dDamage_End
			&& damage >= target->health
			// [BB] A player who tries to exit a map in a competitive game mode when DF_NO_EXIT is set,
			// should not be saved by the hack, but killed.
			&& MeansOfDeath != NAME_Exit)
		{
			damage = target->health - 1;
		}

		if (damage < 1000 && ((target->player->cheats & CF_GODMODE)
			|| (target->player->mo->flags2 & MF2_INVULNERABLE)))
		{
			return;
		}

		// [RH] Avoid friendly fire if enabled
		if (source != NULL && player != source->player && target->IsTeammate (source))
		{
			if ((( teamlms || survival ) && ( MeansOfDeath == NAME_SpawnTelefrag )) == false )
				FriendlyFire = true;
			if (damage < 1000000)
			{ // Still allow telefragging :-(
				damage = (int)((float)damage * level.teamdamage);
				if (damage <= 0)
					return;
			}
		}
		if (!(flags & DMG_NO_ARMOR) && player->mo->Inventory != NULL)
		{
			int newdam = damage;
			player->mo->Inventory->AbsorbDamage (damage, mod, newdam);
			damage = newdam;
			if (damage <= 0)
			{
				// [BB] The player didn't lose health but armor. The server needs
				// to tell the client about this.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetPlayerArmor( player - players );
				return;
			}
		}

		// [BC] For the red armor's special fire resistance, potentially reduce the amount
		// of damage taken AFTER the player's armor has been depleted.
		if (( player->cheats & CF_FIRERESISTANT ) &&
			(( mod == NAME_Fire ) ||
			( mod == NAME_Grenade ) ||
			( mod == NAME_Rocket )))
		{
			damage /= 8;
		}

		if (damage >= player->health
			&& (G_SkillProperty(SKILLP_AutoUseHealth) || deathmatch)
			&& !player->morphTics)
		{ // Try to use some inventory health
			P_AutoUseHealth (player, damage - player->health + 1);
		}
		player->health -= damage;		// mirror mobj health here for Dave
		// [RH] Make voodoo dolls and real players record the same health
		target->health = player->mo->health -= damage;
		if (player->health < 50 && !deathmatch)
		{
			P_AutoUseStrifeHealth (player);
			player->mo->health = player->health;
		}
		if (player->health < 0)
		{
			player->health = 0;
		}
		player->LastDamageType = mod;

		if ( player->pSkullBot )
		{
			// Tell the bot he's been damaged by a player.
			if ( source && source->player && ( source->player != player ))
			{
				player->pSkullBot->m_ulLastPlayerDamagedBy = source->player - players;
				player->pSkullBot->PostEvent( BOTEVENT_DAMAGEDBY_PLAYER );
			}
		}

		player->attacker = source;
		player->damagecount += damage;	// add damage after armor / invuln
		if (player->damagecount > 100)
		{
			player->damagecount = 100;	// teleport stomp does 10k points...
		}
		if ( player->damagecount < 0 )
			player->damagecount = 0;
		temp = damage < 100 ? damage : 100;
		if (player == &players[consoleplayer])
		{
			I_Tactile (40,10,40+temp*2);
		}
	}
	else
	{
		target->health -= damage;	
	}

	//
	// the damage has been dealt; now deal with the consequences
	//

	// If the damaging player has the power of drain, give the player 50% of the damage
	// done in health.
	if ( source && source->player && source->player->cheats & CF_DRAIN &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		if (( target->player == false ) || ( target->player != source->player ))
		{
			if ( P_GiveBody( source, MIN( (int)lOldTargetHealth, damage ) / 2 ))
			{
				// [BC] If we're the server, send out the health change, and play the
				// health sound.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SetPlayerHealth( source->player - players );
					SERVERCOMMANDS_SoundActor( source, CHAN_ITEM, "misc/i_pkup", 1, ATTN_NORM );
				}

				S_Sound( source, CHAN_ITEM, "misc/i_pkup", 1, ATTN_NORM );
			}
		}
	}

	// [BB] Save the damage the player has dealt to monsters here, it's only converted to points
	// though if DF2_AWARD_DAMAGE_INSTEAD_KILLS is set.
	if ( source && source->player
	     && ( target->player == false )
	     && ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	     && ( CLIENTDEMO_IsPlaying( ) == false ))
	{
		source->player->ulUnrewardedDamageDealt += MIN( (int)lOldTargetHealth, damage );
	}

	// [BC] Tell clients that this thing was damaged.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( player )
			SERVERCOMMANDS_DamagePlayer( ULONG( player - players ));
		else
			// Is this even necessary?
			SERVERCOMMANDS_DamageThing( target );
	}

	if (target->health <= 0)
	{ // Death
		target->special1 = damage;
		// check for special fire damage or ice damage deaths
		if (mod == NAME_Fire)
		{
			if (player && !player->morphTics)
			{ // Check for flame death
				if (!inflictor ||
					((target->health > -50) && (damage > 25)) ||
					!inflictor->IsKindOf (RUNTIME_CLASS(APhoenixFX1)))
				{
					target->DamageType = NAME_Fire;
				}
			}
			else
			{
				target->DamageType = NAME_Fire;
			}
		}
		else
		{
			target->DamageType = mod;
		}
		if (source && source->tracer && source->IsKindOf (RUNTIME_CLASS (AMinotaur)))
		{ // Minotaur's kills go to his master
			// Make sure still alive and not a pointer to fighter head
			if (source->tracer->player && (source->tracer->player->mo == source->tracer))
			{
				source = source->tracer;
			}
		}

		// Deaths are server side.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
			( CLIENTDEMO_IsPlaying( ) == false ))
		{
			target->Die (source, inflictor);
		}
		return;
	}

	FState * woundstate = target->FindState(NAME_Wound, mod);
	if (woundstate != NULL)
	{
		int woundhealth = RUNTIME_TYPE(target)->Meta.GetMetaInt (AMETA_WoundHealth, 6);

		if (target->health <= woundhealth)
		{
			target->SetState (woundstate);
			return;
		}
	}
	
	PainChanceList * pc = target->GetClass()->ActorInfo->PainChances;
	int painchance = target->PainChance;
	if (pc != NULL)
	{
		BYTE * ppc = pc->CheckKey(mod);
		if (ppc != NULL)
		{
			painchance = *ppc;
		}
	}
	
	if (!(target->flags5 & MF5_NOPAIN) && (pr_damagemobj() < painchance) && !(target->flags & MF_SKULLFLY) &&
		 ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
	{
		if (inflictor && inflictor->IsKindOf (RUNTIME_CLASS(ALightning)))
		{
			if (pr_lightning() < 96)
			{
				target->flags |= MF_JUSTHIT; // fight back!
				FState * painstate = target->FindState(NAME_Pain, mod);
				if (painstate != NULL)
				{
					// If we are the server, tell clients about the state change.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SetThingState( target, STATE_PAIN );

					target->SetState (painstate);
				}
			}
			else
			{ // "electrocute" the target
				target->renderflags |= RF_FULLBRIGHT;
				if ((target->flags3 & MF3_ISMONSTER) && pr_lightning() < 128)
				{
					target->Howl ();
				}
			}
		}
		else
		{
			// If we are the server, tell clients about the state change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingState( target, STATE_PAIN );

			target->flags |= MF_JUSTHIT; // fight back!
			FState * painstate = target->FindState(NAME_Pain, mod);
			if (painstate != NULL) target->SetState (painstate);

			if (inflictor && inflictor->IsKindOf (RUNTIME_CLASS(APoisonCloud)))
			{
				if ((target->flags3 & MF3_ISMONSTER) && pr_poison() < 128)
				{
					target->Howl ();
				}
			}
		}
	}

	// Nothing more to do!
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return;

	target->reactiontime = 0;			// we're awake now...	
	if (source)
	{
		if (source == target->target)
		{
			target->threshold = BASETHRESHOLD;
			if (target->state == target->SpawnState && target->SeeState != NULL)
			{
				// [BB] If we are the server, tell clients about the state change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetThingState( target, STATE_SEE );

				target->SetState (target->SeeState);
			}
		}
		else if (source != target->target && target->OkayToSwitchTarget (source))
		{
			// Target actor is not intent on another actor,
			// so make him chase after source

			// killough 2/15/98: remember last enemy, to prevent
			// sleeping early; 2/21/98: Place priority on players

			if (target->lastenemy == NULL ||
				(target->lastenemy->player == NULL && target->TIDtoHate == 0) ||
				target->lastenemy->health <= 0)
			{
				target->lastenemy = target->target; // remember last enemy - killough
			}
			target->target = source;
			target->threshold = BASETHRESHOLD;
			if (target->state == target->SpawnState && target->SeeState != NULL)
			{
				// If we are the server, tell clients about the state change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetThingState( target, STATE_SEE );

				target->SetState (target->SeeState);
			}
		}
	}
}

bool AActor::OkayToSwitchTarget (AActor *other)
{
	if (other == this)
		return false;		// [RH] Don't hate self (can happen when shooting barrels)

	if (!(other->flags & MF_SHOOTABLE))
		return false;		// Don't attack things that can't be hurt

	if ((flags4 & MF4_NOTARGETSWITCH) && target != NULL)
		return false;		// Don't switch target if not allowed

	if ((master != NULL && other->IsA(master->GetClass())) ||		// don't attack your master (or others of its type)
		(other->master != NULL && IsA(other->master->GetClass())))	// don't attack your minion (or those of others of your type)
	{
		if (!IsHostile (other) &&								// allow target switch if other is considered hostile
			(other->tid != TIDtoHate || TIDtoHate == 0) &&		// or has the tid we hate
			other->TIDtoHate == TIDtoHate)						// or has different hate information
		{
			return false;
		}
	}

	if ((other->flags3 & MF3_NOTARGET) &&
		(other->tid != TIDtoHate || TIDtoHate == 0) &&
		!IsHostile (other))
		return false;
	if (threshold != 0 && !(flags4 & MF4_QUICKTORETALIATE))
		return false;
	if (IsFriend (other))
	{ // [RH] Friendlies don't target other friendlies
		return false;
	}
	
	int infight;
	if (flags5 & MF5_NOINFIGHTING) infight=-1;	
	else if (level.flags & LEVEL_TOTALINFIGHTING) infight=1;
	else if (level.flags & LEVEL_NOINFIGHTING) infight=-1;	
	else infight = infighting;
	
	// [BC] No infighting during invasion mode.
	if ((infight < 0 || invasion )&&	other->player == NULL && !IsHostile (other))
	{
		return false;	// infighting off: Non-friendlies don't target other non-friendlies
	}
	if (TIDtoHate != 0 && TIDtoHate == other->TIDtoHate)
		return false;		// [RH] Don't target "teammates"
	if (other->player != NULL && (flags4 & MF4_NOHATEPLAYERS))
		return false;		// [RH] Don't target players
	if (target != NULL && target->health > 0 &&
		TIDtoHate != 0 && target->tid == TIDtoHate && pr_switcher() < 128 &&
		P_CheckSight (this, target))
		return false;		// [RH] Don't be too quick to give up things we hate

	return true;
}

//==========================================================================
//
// P_PoisonPlayer - Sets up all data concerning poisoning
//
//==========================================================================

void P_PoisonPlayer (player_t *player, AActor *poisoner, AActor *source, int poison)
{
	// [BC] This is handled server side.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return;

	if((player->cheats&CF_GODMODE) || (player->mo->flags2 & MF2_INVULNERABLE))
	{
		return;
	}
	if (source != NULL && source->player != player && player->mo->IsTeammate (source))
	{
		poison = (int)((float)poison * level.teamdamage);
	}
	if (poison > 0)
	{
		player->poisoncount += poison;
		player->poisoner = poisoner;
		if(player->poisoncount > 100)
		{
			player->poisoncount = 100;
		}

		// [BC] Update the player's poisoncount.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerPoisonCount( ULONG( player - players ));
	}
}

//==========================================================================
//
// P_PoisonDamage - Similar to P_DamageMobj
//
//==========================================================================

void P_PoisonDamage (player_t *player, AActor *source, int damage,
	bool playPainSound)
{
	AActor *target;
	AActor *inflictor;

	target = player->mo;
	inflictor = source;
	if (target->health <= 0)
	{
		return;
	}
	if (target->flags2&MF2_INVULNERABLE && damage < 1000000)
	{ // target is invulnerable
		return;
	}
	if (player)
	{
		// Take half damage in trainer mode
		damage = FixedMul(damage, G_SkillProperty(SKILLP_DamageFactor));
	}
	if(damage < 1000 && ((player->cheats&CF_GODMODE)
		|| (player->mo->flags2 & MF2_INVULNERABLE)))
	{
		return;
	}
	if (damage >= player->health
		&& (G_SkillProperty(SKILLP_AutoUseHealth) || deathmatch)
		&& !player->morphTics)
	{ // Try to use some inventory health
		P_AutoUseHealth (player, damage - player->health+1);
	}
	player->health -= damage; // mirror mobj health here for Dave
	if (player->health < 50 && !deathmatch)
	{
		P_AutoUseStrifeHealth (player);
	}
	if (player->health < 0)
	{
		player->health = 0;
	}
	player->attacker = source;

	//
	// do the damage
	//
	target->health -= damage;
	if (target->health <= 0)
	{ // Death
		target->special1 = damage;
		if (player && inflictor && !player->morphTics)
		{ // Check for flame death
			if ((inflictor->DamageType == NAME_Fire)
				&& (target->health > -50) && (damage > 25))
			{
				target->DamageType = NAME_Fire;
			}
			else target->DamageType = inflictor->DamageType;
		}
		target->Die (source, source);
		return;
	}
	if (!(level.time&63) && playPainSound)
	{
		// If we are the server, tell clients about the state change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( target, STATE_PAIN );

		FState * painstate = target->FindState(NAME_Pain, target->DamageType);
		if (painstate != NULL) target->SetState (painstate);
	}
/*
	if((P_Random() < target->info->painchance)
		&& !(target->flags&MF_SKULLFLY))
	{
		target->flags |= MF_JUSTHIT; // fight back!
		P_SetMobjState(target, target->info->painstate);
	}
*/
}

bool CheckCheatmode ();

//*****************************************************************************
//
void PLAYER_SetFragcount( player_t *pPlayer, LONG lFragCount, bool bAnnounce, bool bUpdateTeamFrags )
{
	// Don't bother with fragcount during warm-ups.
	if ((( duel ) && ( DUEL_GetState( ) == DS_COUNTDOWN )) ||
		(( lastmanstanding || teamlms ) && ( LASTMANSTANDING_GetState( ) == LMSS_COUNTDOWN )))
	{
		return;
	}

	// Don't announce events related to frag changes during teamplay, LMS,
	// or possession games.
	if (( bAnnounce ) &&
		( deathmatch ) &&
		( teamplay == false ) &&
		( lastmanstanding == false ) &&
		( teamlms == false ) &&
		( possession == false ) &&
		( teampossession == false ))
	{
		ANNOUNCER_PlayFragSounds( pPlayer - players, pPlayer->fragcount, lFragCount );
	}

	// If this is a teamplay deathmatch, update the team frags.
	if ( bUpdateTeamFrags )
	{
		if (( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) && ( pPlayer->bOnTeam ))
			TEAM_SetFragCount( pPlayer->ulTeam, TEAM_GetFragCount( pPlayer->ulTeam ) + ( lFragCount - pPlayer->fragcount ), bAnnounce );
	}

	// Set the player's fragcount.
	pPlayer->fragcount = lFragCount;

	// If we're the server, notify the clients of the fragcount change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetPlayerFrags( pPlayer - players );

		// Also, update the scoreboard.
		SERVERCONSOLE_UpdatePlayerInfo( pPlayer - players, UDF_FRAGS );
		SERVERCONSOLE_UpdateScoreboard( );
	}

	// Refresh the HUD since a score has changed.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void PLAYER_ResetAllPlayersFragcount( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] == false )
			continue;

		players[ulIdx].fragcount = 0;

		// If we're the server, 
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCONSOLE_UpdatePlayerInfo( ulIdx, UDF_FRAGS );
			SERVERCONSOLE_UpdateScoreboard( );
		}
	}

	// Refresh the HUD since a score has changed.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void PLAYER_ResetAllPlayersSpecialCounters ( )
{
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		PLAYER_ResetSpecialCounters ( &players[ulIdx] );
}

//*****************************************************************************
//
void PLAYER_ResetSpecialCounters ( player_t *pPlayer )
{
	if ( pPlayer == NULL )
		return;

	pPlayer->ulLastExcellentTick = 0;
	pPlayer->ulLastFragTick = 0;
	pPlayer->ulLastBFGFragTick = 0;
	pPlayer->ulConsecutiveHits = 0;
	pPlayer->ulConsecutiveRailgunHits = 0;
	pPlayer->ulDeathsWithoutFrag = 0;
	pPlayer->ulFragsWithoutDeath = 0;
	pPlayer->ulRailgunShots = 0;
	pPlayer->ulUnrewardedDamageDealt = 0;
}

//*****************************************************************************
//
void PLAYER_GivePossessionPoint( player_t *pPlayer )
{
	char				szString[64];
	DHUDMessageFadeOut	*pMsg;

//	if ( possession )
//		ANNOUNCER_PlayScoreSounds( pPlayer - players, pPlayer->lPointCount, pPlayer->lPointCount + 1 );

	// If we're in possession mode, give the player a point. Also, determine if the player's lead
	// state changed. If it did, announce it.
	if ( possession )
	{
		pPlayer->lPointCount++;

		// If we're the server, notify the clients of the pointcount change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerPoints( pPlayer - players );
	}
	// If this is team possession, also give the player's team a point.
	else if ( teampossession && pPlayer->bOnTeam )
	{
		TEAM_SetScore( pPlayer->ulTeam, TEAM_GetScore( pPlayer->ulTeam ) + 1, true );

		pPlayer->lPointCount++;

		// If we're the server, notify the clients of the pointcount change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerPoints( pPlayer - players );
	}
	else
		return;

	SCOREBOARD_RefreshHUD( );

	// If a pointlimit has been set, determine if the game has been won.
	if ( pointlimit )
	{
		if ( possession && ( pPlayer->lPointCount >= pointlimit ))
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVER_Printf( PRINT_HIGH, "Pointlimit hit.\n" );
				SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", pPlayer->userinfo.netname );
			}
			else
			{
				Printf( "Pointlimit hit.\n" );
				Printf( "%s \\c-wins!\n", pPlayer->userinfo.netname );

				if ( pPlayer->mo->CheckLocalView( consoleplayer ))
					ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
			}

			// Just print "YOU WIN!" in single player.
			if (( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ) && ( pPlayer->mo->CheckLocalView( consoleplayer )))
				sprintf( szString, "YOU WIN!" );
			else
				sprintf( szString, "%s \\c-WINS!", pPlayer->userinfo.netname );
			V_ColorizeString( szString );

			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( BigFont );

				// Display "%s WINS!" HUD message.
				pMsg = new DHUDMessageFadeOut( szString,
					160.4f,
					75.0f,
					320,
					200,
					CR_RED,
					3.0f,
					2.0f );

				StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
				screen->SetFont( SmallFont );
			}
			else
			{
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 160.4f, 75.0f, 320, 200, CR_RED, 3.0f, 2.0f, "BigFont", false, MAKE_ID('C','N','T','R') );
			}

			// End the level after five seconds.
			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
		else if ( teampossession && ( TEAM_GetScore( pPlayer->ulTeam ) >= pointlimit ))
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVER_Printf( PRINT_HIGH, "Pointlimit hit.\n" );
				SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", TEAM_GetName( pPlayer->ulTeam ));
			}
			else
			{
				Printf( "Pointlimit hit.\n" );
				Printf( "%s \\c-wins!\n", TEAM_GetName( pPlayer->ulTeam ));

				if ( pPlayer->mo->IsTeammate( players[consoleplayer].camera ))
					ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
			}

			if ( pPlayer->ulTeam == TEAM_BLUE )
				sprintf( szString, "\\chBLUE WINS!" );
			else
				sprintf( szString, "\\cgRED WINS!" );
			V_ColorizeString( szString );

			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				screen->SetFont( BigFont );

				// Display "%s WINS!" HUD message.
				pMsg = new DHUDMessageFadeOut( szString,
					1.5f,
					0.375f,
					320,
					200,
					CR_RED,
					3.0f,
					2.0f );

				StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
				screen->SetFont( SmallFont );
			}
			else
			{
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, 0.375f, 320, 200, CR_RED, 3.0f, 2.0f, "BigFont", false, MAKE_ID('C','N','T','R') );
			}

			// End the level after five seconds.
			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
	}
}

//*****************************************************************************
//
void PLAYER_SetTeam( player_t *pPlayer, ULONG ulTeam, bool bNoBroadcast )
{
	bool	bBroadcastChange = false;

	if (
		// Player was removed from a team.
		( pPlayer->bOnTeam && ulTeam == NUM_TEAMS ) || 

		// Player was put on a team after not being on one.
		( pPlayer->bOnTeam == false && ulTeam < NUM_TEAMS ) ||
		
		// Player is on a team, but is being put on a different team.
		( pPlayer->bOnTeam && ( ulTeam < NUM_TEAMS ) && ( ulTeam != pPlayer->ulTeam ))
		)
	{
		bBroadcastChange = true;
	}

	// We don't want to broadcast a print message.
	if ( bNoBroadcast )
		bBroadcastChange = false;

	// Set whether or not this player is on a team.
	if ( ulTeam == NUM_TEAMS )
		pPlayer->bOnTeam = false;
	else
		pPlayer->bOnTeam = true;

	// Set the team.
	pPlayer->ulTeam = ulTeam;

	// [BB] Make sure that the player only uses a class available to his team.
	TEAM_EnsurePlayerHasValidClass ( pPlayer );

	// If we're the server, tell clients about this team change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetPlayerTeam( pPlayer - players );

		// Player has changed his team! Tell clients.
		if ( bBroadcastChange )
		{
			SERVER_Printf( PRINT_HIGH, "%s \\c-joined the \\c%c%s \\c-team.\n", pPlayer->userinfo.netname, V_GetColorChar( TEAM_GetTextColor( ulTeam ) ), TEAM_GetName( ulTeam )); 
		}		
	}

	// Finally, update the player's color.
	R_BuildPlayerTranslation( pPlayer - players );
	if ( StatusBar && ( pPlayer->mo->CheckLocalView( consoleplayer )))
		StatusBar->AttachToPlayer( pPlayer );

	// Update this player's info on the scoreboard.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCONSOLE_UpdatePlayerInfo( pPlayer - players, UDF_FRAGS );
}

//*****************************************************************************
//
// [BC] *grumble*
void	G_DoReborn (int playernum, bool freshbot);
void PLAYER_SetSpectator( player_t *pPlayer, bool bBroadcast, bool bDeadSpectator )
{
	AActor	*pOldBody;

	// Already a spectator. Check if their spectating state is changing.
	if ( pPlayer->bSpectating == true )
	{
		// Player is trying to become a true spectator (if not one already).
		if ( bDeadSpectator == false )
		{
			// If they're becoming a true spectator after being a dead spectator, do all the
			// special spectator stuff we didn't do before.
			if ( pPlayer->bDeadSpectator )
			{
				pPlayer->bDeadSpectator = false;

				// Run the disconnect scripts now that the player is leaving.
				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
					( CLIENTDEMO_IsPlaying( ) == false ))
				{
					FBehavior::StaticStartTypedScripts( SCRIPT_Disconnect, NULL, true, pPlayer - players );
				}

				if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
					( CLIENTDEMO_IsPlaying( ) == false ))
				{
					// Tell the join queue module that a player is leaving the game.
					JOINQUEUE_PlayerLeftGame( true );
				}

				pPlayer->health = pPlayer->mo->health = deh.StartHealth;

				if ( bBroadcast )
				{
					// Send out a message saying this player joined the spectators.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVER_Printf( PRINT_HIGH, "%s \\c-joined the spectators.\n", pPlayer->userinfo.netname );
					else
						Printf( "%s \\c-joined the spectators.\n", pPlayer->userinfo.netname );
				}

				// This player no longer has a team affiliation.
				pPlayer->bOnTeam = false;
			}
		}

		if (( pPlayer->fragcount > 0 ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
			PLAYER_SetFragcount( pPlayer, 0, false, false );

		return;
	}

	// Flag this player as being a spectator.
	pPlayer->bSpectating = true;
	pPlayer->bDeadSpectator = bDeadSpectator;

	// Run the disconnect scripts if the player is leaving the game.
	if (( bDeadSpectator == false ) &&
		( NETWORK_GetState( ) != NETSTATE_CLIENT ) &&
		( CLIENTDEMO_IsPlaying( ) == false ))
	{
		FBehavior::StaticStartTypedScripts( SCRIPT_Disconnect, NULL, true, pPlayer - players );
	}

	// If this player was eligible to get an assist, cancel that.
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
	{
		if ( TEAM_GetAssistPlayer( TEAM_BLUE ) == static_cast<unsigned> (pPlayer - players) )
			TEAM_SetAssistPlayer( TEAM_BLUE, MAXPLAYERS );
		if ( TEAM_GetAssistPlayer( TEAM_RED ) == static_cast<unsigned> (pPlayer - players) )
			TEAM_SetAssistPlayer( TEAM_RED, MAXPLAYERS );
	}

	if ( pPlayer->mo )
	{
		// Before we start fucking with the player's body, drop important items
		// like flags, etc.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
			pPlayer->mo->DropImportantItems( false );

		// Is this player tagged as a dead spectator, give him life.
		pPlayer->playerstate = PST_LIVE;
		if ( bDeadSpectator == false )
			pPlayer->health = pPlayer->mo->health = deh.StartHealth;

		// If this player is being forced into spectatorship, don't destroy his or her
		// old body.
		if ( bDeadSpectator )
		{
			// Save the player's old body, and respawn him or her.
			pOldBody = pPlayer->mo;
			G_DoReborn( pPlayer - players, false );

			// Set the player's new body to the position of his or her old body.
			if (( pPlayer->mo ) &&
				( pOldBody ))
			{
				pPlayer->mo->SetOrigin( pOldBody->x, pOldBody->y, pOldBody->z );

				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_MoveLocalPlayer( ULONG( pPlayer - players ));
			}
		}

		// [BB] Set a bunch of stuff, e.g. make the player unshootable, etc.
		PLAYER_SetDefaultSpectatorValues ( pPlayer );

		// Take away all of the player's inventory.
		pPlayer->mo->DestroyAllInventory( );

		// [BB] We also need to stop all sounds associated to the player pawn, spectators
		// aren't supposed to make any sounds. This is especially crucial if a looping sound
		// is played by the player pawn.
		S_StopAllSoundsFromActor ( pPlayer->mo );

		if (( bDeadSpectator == false ) && bBroadcast )
		{
			// Send out a message saying this player joined the spectators.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVER_Printf( PRINT_HIGH, "%s \\c-joined the spectators.\n", pPlayer->userinfo.netname );
			else
				Printf( "%s \\c-joined the spectators.\n", pPlayer->userinfo.netname );
		}
	}

	// This player no longer has a team affiliation.
	if ( bDeadSpectator == false )
		pPlayer->bOnTeam = false;

	// Player's lose all their frags when they become a spectator.
	if ( bDeadSpectator == false )
	{
		if (( pPlayer->fragcount > 0 ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
			PLAYER_SetFragcount( pPlayer, 0, false, false );

		// Also, tell the joinqueue module that a player has left the game.
		if (( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		{
			// Tell the join queue module that a player is leaving the game.
			JOINQUEUE_PlayerLeftGame( true );
		}

		if ( pPlayer->pSkullBot )
			pPlayer->pSkullBot->PostEvent( BOTEVENT_SPECTATING );
	}

	// Player's also lose all of their wins in duel mode.
	if (( duel || ( lastmanstanding && ( bDeadSpectator == false ))) && ( pPlayer->ulWins > 0 ) && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && ( CLIENTDEMO_IsPlaying( ) == false ))
		PLAYER_SetWins( pPlayer, 0 );

	// Update this player's info on the scoreboard.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCONSOLE_UpdatePlayerInfo( pPlayer - players, UDF_FRAGS );
}

//*****************************************************************************
//
void PLAYER_SetDefaultSpectatorValues( player_t *pPlayer )
{
	if ( ( pPlayer == NULL ) || ( pPlayer->mo == NULL ) )
		return;

	// Make the player unshootable, etc.
	pPlayer->mo->flags2 |= (MF2_CANNOTPUSH);
	pPlayer->mo->flags &= ~(MF_SOLID|MF_SHOOTABLE|MF_PICKUP);
	pPlayer->mo->flags2 &= ~(MF2_PASSMOBJ|MF2_FLOATBOB);
	pPlayer->mo->flags3 = MF3_NOBLOCKMONST;
	pPlayer->mo->flags4 = 0;
	pPlayer->mo->flags5 = 0;
	pPlayer->mo->RenderStyle = STYLE_None;

	// [BB] Speed and viewheight of spectators should be independent of the player class.
	pPlayer->mo->ForwardMove1 = pPlayer->mo->ForwardMove2 = FRACUNIT;
	pPlayer->mo->SideMove1 = pPlayer->mo->SideMove2 = FRACUNIT;
	pPlayer->mo->ViewHeight = 41*FRACUNIT;

	// Make the player flat, so he can travel under doors and such.
	pPlayer->mo->height = 0;

	// Make monsters unable to "see" this player.
	pPlayer->cheats |= CF_NOTARGET;

	// Reset a bunch of other stuff.
	pPlayer->extralight = 0;
	pPlayer->fixedcolormap = 0;
	pPlayer->damagecount = 0;
	pPlayer->bonuscount = 0;
	pPlayer->poisoncount = 0;
	pPlayer->inventorytics = 0;
}

//*****************************************************************************
//
void PLAYER_SetWins( player_t *pPlayer, ULONG ulWins )
{
	// Set the player's fragcount.
	pPlayer->ulWins = ulWins;

	// Refresh the HUD since a score has changed.
	SCOREBOARD_RefreshHUD( );

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// If we're the server, notify the clients of the win count change.
		SERVERCOMMANDS_SetPlayerWins( pPlayer - players );

		// Also, update the scoreboard.
		SERVERCONSOLE_UpdatePlayerInfo( pPlayer - players, UDF_FRAGS );
		SERVERCONSOLE_UpdateScoreboard( );
	}
}

//*****************************************************************************
//
void PLAYER_GetName( player_t *pPlayer, char *pszOutBuf )
{
	// Build the buffer, which has a "remove color code" tag at the end of it.
	sprintf( pszOutBuf, "%s\\c-", pPlayer->userinfo.netname );
}

//*****************************************************************************
//
bool PLAYER_IsTrueSpectator( player_t *pPlayer )
{
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_DEADSPECTATORS )
		return (( pPlayer->bSpectating ) && ( pPlayer->bDeadSpectator == false ));

	return ( pPlayer->bSpectating );
}

//*****************************************************************************
//
void PLAYER_StruckPlayer( player_t *pPlayer )
{
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( CLIENTDEMO_IsPlaying( )))
		return;

	pPlayer->ulConsecutiveHits++;

	// If the player has made 5 straight consecutive hits with a weapon, award a medal.
	if (( pPlayer->ulConsecutiveHits % 5 ) == 0 )
	{
		// If this player has made 10+ consecuvite hits, award a "Precision" medal.
		if ( pPlayer->ulConsecutiveHits >= 10 )
		{
			MEDAL_GiveMedal( pPlayer - players, MEDAL_PRECISION );

			// Tell clients about the medal that been given.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_GivePlayerMedal( pPlayer - players, MEDAL_PRECISION );
		}
		// Otherwise, award an "Accuracy" medal.
		else
		{
			MEDAL_GiveMedal( pPlayer - players, MEDAL_ACCURACY );

			// Tell clients about the medal that been given.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_GivePlayerMedal( pPlayer - players, MEDAL_ACCURACY );
		}
	}

	// Reset the struck player flag.
	pPlayer->bStruckPlayer = false;
}

//*****************************************************************************
//
bool PLAYER_ShouldSpawnAsSpectator( player_t *pPlayer )
{
	UCVarValue	Val;

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// If there's a join password, the player should start as a spectator.
		Val = sv_joinpassword.GetGenericRep( CVAR_String );
		if (( sv_forcejoinpassword ) && ( strlen( Val.String )))
			return ( true );

		// If the number of players in the game exceeds sv_maxplayers, spawn as a spectator.
		Val = sv_maxplayers.GetGenericRep( CVAR_Int );
		if ( SERVER_CalcNumNonSpectatingPlayers( pPlayer - players ) >= static_cast<unsigned> (Val.Int) )
			return ( true );
	}

	// If an LMS game is in progress, the player should start as a spectator.
	if (( lastmanstanding || teamlms ) &&
		(( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) || ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE )) &&
		( gameaction != ga_worlddone ))
	{
		return ( true );
	}

	// If the duel isn't in the "waiting for players" state, the player should start as a spectator.
	if ( duel )
	{
		if (( DUEL_GetState( ) != DS_WAITINGFORPLAYERS ) ||
			( SERVER_CalcNumNonSpectatingPlayers( pPlayer - players ) >= 2 ))
		{
			return ( true );
		}
	}

	// If a survival game is in progress, the player should start as a spectator.
	if (( survival ) &&
		(( SURVIVAL_GetState( ) == SURVS_INPROGRESS ) || ( SURVIVAL_GetState( ) == SURVS_MISSIONFAILED )) &&
		( gameaction != ga_worlddone ))
	{
		return ( true );
	}

	// [RC] If an invasion game is in progress with sv_maxlives, the player should start as a spectator.
	if ( INVASION_PreventPlayersFromJoining() )
		return ( true );

	// Players entering a teamplay game must choose a team first before joining the fray.
	if (( pPlayer->bOnTeam == false ) || ( playeringame[pPlayer - players] == false ))
	{
		if (( teamplay && (( dmflags2 & DF2_NO_TEAM_SELECT ) == false )) ||
			( teamlms && (( dmflags2 & DF2_NO_TEAM_SELECT ) == false )) ||
			( teampossession && (( dmflags2 & DF2_NO_TEAM_SELECT ) == false )) ||
			( domination && (dmflags2 & DF2_NO_TEAM_SELECT ) == false ) ||
			( teamgame && (( dmflags2 & DF2_NO_TEAM_SELECT ) == false ) && ( TemporaryTeamStarts.Size( ) == 0 )))
		{
			return ( true );
		}
	}

	// Passed all checks!
	return ( false );
}

//*****************************************************************************
//
bool PLAYER_Taunt( player_t *pPlayer )
{
	// Don't taunt if we're not in a level!
	if ( gamestate != GS_LEVEL )
		return ( false );

	// Spectators or dead people can't taunt!
	if (( pPlayer->bSpectating ) ||
		( pPlayer->health <= 0 ) ||
		( pPlayer->mo == NULL ) ||
		( i_compatflags & COMPATF_DISABLETAUNTS ))
	{
		return ( false );
	}

	if ( cl_taunts )
		S_Sound( pPlayer->mo, CHAN_VOICE, "*taunt", 1, ATTN_NORM );

	return ( true );
}

//*****************************************************************************
//
LONG PLAYER_GetRailgunColor( player_t *pPlayer )
{
	// Determine the railgun trail's color.
	switch ( pPlayer->userinfo.lRailgunTrailColor )
	{
	case RAILCOLOR_BLUE:
	default:

		return ( V_GetColorFromString( NULL, "00 00 ff" ));
	case RAILCOLOR_RED:

		return ( V_GetColorFromString( NULL, "ff 00 00" ));
	case RAILCOLOR_YELLOW:

		return ( V_GetColorFromString( NULL, "ff ff 00" ));
	case RAILCOLOR_BLACK:

		return ( V_GetColorFromString( NULL, "0f 0f 0f" ));
	case RAILCOLOR_SILVER:

		return ( V_GetColorFromString( NULL, "9f 9f 9f" ));
	case RAILCOLOR_GOLD:

		return ( V_GetColorFromString( NULL, "bf 8f 2f" ));
	case RAILCOLOR_GREEN:

		return ( V_GetColorFromString( NULL, "00 ff 00" ));
	case RAILCOLOR_WHITE:

		return ( V_GetColorFromString( NULL, "ff ff ff" ));
	case RAILCOLOR_PURPLE:

		return ( V_GetColorFromString( NULL, "ff 00 ff" ));
	case RAILCOLOR_ORANGE:

		return ( V_GetColorFromString( NULL, "ff 8f 00" ));
	case RAILCOLOR_RAINBOW:

		return ( -2 );
	}
}

//*****************************************************************************
//
void PLAYER_AwardDamagePointsForAllPlayers( void )
{
	if ( (dmflags2 & DF2_AWARD_DAMAGE_INSTEAD_KILLS)
		&& (GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSEARNKILLS) )
	{
		for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] == false )
				continue;

			player_t *p = &players[ulIdx];

			int points = p->ulUnrewardedDamageDealt / 100;
			if ( points > 0 )
			{
				p->lPointCount += points;
				p->ulUnrewardedDamageDealt -= 100 * points;

				// Update the clients with this player's updated points.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SetPlayerPoints( static_cast<ULONG>( p - players ) );

					// Also, update the scoreboard.
					SERVERCONSOLE_UpdatePlayerInfo( static_cast<ULONG>( p - players ), UDF_FRAGS );
					SERVERCONSOLE_UpdateScoreboard( );
				}
			}
		}
	}
}

CCMD (kill)
{
	// Only allow it in a level.
	if ( gamestate != GS_LEVEL )
		return;

	// [BC] Don't let spectators kill themselves.
	if ( players[consoleplayer].bSpectating == true )
		return;

	// [BC] Don't allow suiciding during a duel.
	if ( duel && ( DUEL_GetState( ) == DS_INDUEL ))
	{
		Printf( "You cannot suicide during a duel.\n" );
		return;
	}

	if (argv.argc() > 1)
	{
		if (CheckCheatmode ())
			return;

		if (!stricmp (argv[1], "monsters"))
		{
			// Kill all the monsters
			if (CheckCheatmode ())
				return;

			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_MASSACRE);
		}
		else
		{
			Net_WriteByte (DEM_KILLCLASSCHEAT);
			Net_WriteString (argv[1]);
		}
	}
	else
	{
		// [BC] Tell the server we wish to suicide.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( players[consoleplayer].bSpectating == false ))
			CLIENTCOMMANDS_Suicide( );

		// Kill the player
		Net_WriteByte (DEM_SUICIDE);
	}
	C_HideConsole ();
}

//*****************************************************************************
//
CCMD( taunt )
{
	if ( PLAYER_Taunt( &players[consoleplayer] ))
	{
		// Tell the server we taunted!
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			CLIENTCOMMANDS_Taunt( );

		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteLocalCommand( CLD_TAUNT, NULL );
	}
}
