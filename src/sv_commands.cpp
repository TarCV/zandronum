//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: sv_commands.cpp
//
// Description: Contains a set of functions that correspond to each message a server
// can send out. Each functions handles the send out of each message.
//
//-----------------------------------------------------------------------------

#include "chat.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "doomtype.h"
#include "duel.h"
#include "g_game.h"
#include "gamemode.h"
#include "gi.h"
#include "invasion.h"
#include "joinqueue.h"
#include "lastmanstanding.h"
#include "network.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_state.h"
#include "sbar.h"
#include "sv_commands.h"
#include "sv_main.h"
#include "team.h"
#include "survival.h"
#include "vectors.h"
#include "v_palette.h"
#include "r_data/r_translate.h"
#include "domination.h"
#include "p_acs.h"
#include "templates.h"
#include "a_movingcamera.h"
#include "po_man.h"
#include "i_system.h"
#include "r_data/colormaps.h"
#include "network_enums.h"
#include "decallib.h"
#include "network/netcommand.h"
#include "network/servercommands.h"

CVAR (Bool, sv_showwarnings, false, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)

EXTERN_CVAR( Float, sv_aircontrol )

//*****************************************************************************
//	FUNCTIONS

// [BB] Check if the actor has a valid net ID. Returns true if it does, returns false and prints a warning if not.
bool EnsureActorHasNetID( const AActor *pActor )
{
	if ( pActor == NULL )
		return false;

	if ( pActor->lNetID == -1 )
	{
		if ( sv_showwarnings && !( pActor->ulNetworkFlags & NETFL_SERVERSIDEONLY ) )
			Printf ( "Warning: Actor %s doesn't have a netID and therefore can't be manipulated online!\n", pActor->GetClass()->TypeName.GetChars() );
		return false;
	}
	else
		return true;
}

//*****************************************************************************
//
// [BB] Check if the actor already was updated during this tic, and alters ulBits accorindly.
void RemoveUnnecessaryPositionUpdateFlags( AActor *pActor, ULONG &ulBits )
{
	if ( (pActor->lastNetXUpdateTic == gametic) && (pActor->lastX == pActor->x) )
		ulBits  &= ~CM_X;
	if ( (pActor->lastNetYUpdateTic == gametic) && (pActor->lastY == pActor->y) )
		ulBits  &= ~CM_Y;
	if ( (pActor->lastNetZUpdateTic == gametic) && (pActor->lastZ == pActor->z) )
		ulBits  &= ~CM_Z;
	if ( (pActor->lastNetVelXUpdateTic == gametic) && (pActor->lastVelx == pActor->velx) )
		ulBits  &= ~CM_VELX;
	if ( (pActor->lastNetVelYUpdateTic == gametic) && (pActor->lastVely == pActor->vely) )
		ulBits  &= ~CM_VELY;
	if ( (pActor->lastNetVelZUpdateTic == gametic) && (pActor->lastVelz == pActor->velz) )
		ulBits  &= ~CM_VELZ;
	if ( (pActor->lastNetMovedirUpdateTic == gametic) && (pActor->lastMovedir == pActor->movedir) )
		ulBits  &= ~CM_MOVEDIR;
}


//*****************************************************************************
//
// [BB] Try to find the state label and the correspoding offset belonging to the target state.
// [Dusk] Pretty much rewrote this based on dumpstates code, this now recurses through state
// children now to check them as well. This is needed to find stuff like custom pain states.

bool FindStateAddressRecursor( AActor *pActor, FState *pState, FString prefix, FStateLabels *list, FString &label, LONG &lOffset )
{
	for ( int i = 0; i < list->NumLabels; ++i ) {
		if ( list->Labels[i].State != NULL ) {
			unsigned int offset;
			if ( pActor->InState( list->Labels[i].State, &offset, pState ) && ( offset <= 255 ) ) {
				// Found it!
				label = prefix + list->Labels[i].Label.GetChars();
				lOffset = static_cast<LONG>( offset );
				return true;
			}
		}

		// Not in there, check the children
		if ( list->Labels[i].Children != NULL ) {
			FString newprefix = prefix + list->Labels[i].Label.GetChars() + '.';
			bool found = FindStateAddressRecursor( pActor, pState, newprefix, list->Labels[i].Children, label, lOffset );
			if ( found == true )
				return true;
		}
	}

	return false;
}

void FindStateLabelAndOffset( AActor *pActor, FState *pState, FString &stateLabel, LONG &lOffset, const bool bSuppressWarning = false )
{
	stateLabel = "";
	lOffset = 0;
	FStateLabels *pStateList = pActor->GetClass()->ActorInfo->StateList;

	bool found = FindStateAddressRecursor( pActor, pState, "", pStateList, stateLabel, lOffset );
	if ( found == false && sv_showwarnings && ( bSuppressWarning == false ) )
		Printf ("FindStateLabelAndOffset: Couldn't find state!\n");
}

//*****************************************************************************
//
// [BB]
bool ClassOwnsState( const PClass *pClass, const FState *pState )
{
	if ( ( pState == NULL ) || ( pClass == NULL ) )
		return false;

	const PClass *pStateOwnerClass = FState::StaticFindStateOwner ( pState );

	if ( pStateOwnerClass == NULL )
		return false;

	if ( pStateOwnerClass == pClass )
		return true;

	return false;
}

// [BB]
bool ActorOwnsState( const AActor *pActor, const FState *pState )
{
	if ( pActor == NULL )
		return false;

	return ClassOwnsState( pActor->GetClass(), pState );
}

//*****************************************************************************
//
// [BB] Helper function for SERVERCOMMANDS_SetThingFrame.
bool OffsetAndStateOwnershipValidityCheck ( const LONG lOffset, const PClass *pClass, const FState *pState )
{
	// [BB] The offset is out of range.
	if (( lOffset < 0 ) || ( lOffset > 255 ) )
		return false;

	// [BB] If the offset is zero, it doesn't matter whether the actor owns the state.
	if ( lOffset == 0 )
		return true;

	if ( ClassOwnsState( pClass, pState ) )
		return true;

	// [BB] A non-zero but otherwise valid offset can only be used of the actor owns the state.
	return false;
}

//*****************************************************************************
//
// [BB] Helper function for SERVERCOMMANDS_SetThingFrame.
bool OffsetAndStateOwnershipValidityCheck ( const LONG lOffset, const AActor *pActor, const FState *pState )
{
	if ( pActor == NULL )
		return false;

	return OffsetAndStateOwnershipValidityCheck ( lOffset, pActor->GetClass(), pState );
}
//*****************************************************************************
//
// [BB] Mark the actor as updated according to ulBits.
void ActorNetPositionUpdated( AActor *pActor, ULONG &ulBits )
{
	if ( ulBits & CM_X )
	{
		pActor->lastNetXUpdateTic = gametic;
		pActor->lastX = pActor->x;
	}
	if ( ulBits & CM_Y )
	{
		pActor->lastNetYUpdateTic = gametic;
		pActor->lastY = pActor->y;
	}
	if ( ulBits & CM_Z )
	{
		pActor->lastNetZUpdateTic = gametic;
		pActor->lastZ = pActor->z;
	}
	if ( ulBits & CM_VELX )
	{
		pActor->lastNetVelXUpdateTic = gametic;
		pActor->lastVelx = pActor->velx;
	}
	if ( ulBits & CM_VELY )
	{
		pActor->lastNetVelYUpdateTic = gametic;
		pActor->lastVely = pActor->vely;
	}
	if ( ulBits & CM_VELZ )
	{
		pActor->lastNetVelZUpdateTic = gametic;
		pActor->lastVelz = pActor->velz;
	}
	if ( ulBits & CM_MOVEDIR )
	{
		pActor->lastNetMovedirUpdateTic = gametic;
		pActor->lastMovedir = pActor->movedir;
	}
}
//*****************************************************************************
//
// [BB/WS] Tell clients to re-use their last updated position. This saves a lot of bandwidth.
void CheckPositionReuse( AActor *pActor, ULONG &ulBits )
{
	if ( ulBits & CM_X && pActor->lastX == pActor->x )
	{
		ulBits &= ~CM_X;
		ulBits |= CM_REUSE_X;
	}
	if ( ulBits & CM_Y && pActor->lastY == pActor->y )
	{
		ulBits &= ~CM_Y;
		ulBits |= CM_REUSE_Y;
	}
	if ( ulBits & CM_Z && pActor->lastZ == pActor->z )
	{
		ulBits &= ~CM_Z;
		ulBits |= CM_REUSE_Z;
	}
}
//*****************************************************************************
//
void SERVERCOMMANDS_Ping( ULONG ulTime )
{
	NetCommand command( SVC_PING );
	command.addLong( ulTime );
	command.setUnreliable( true );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_Nothing( ULONG ulPlayer, bool bReliable )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	NetCommand command( SVC_NOTHING );
	command.setUnreliable( bReliable == false );
	command.sendCommandToOneClient( ulPlayer );
}

//*****************************************************************************
//
void SERVERCOMMANDS_BeginSnapshot( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	NetCommand command( SVC_BEGINSNAPSHOT );
	command.sendCommandToOneClient( ulPlayer );
}

//*****************************************************************************
//
void SERVERCOMMANDS_EndSnapshot( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	NetCommand command( SVC_ENDSNAPSHOT );
	command.sendCommandToOneClient( ulPlayer );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SpawnPlayer( ULONG ulPlayer, LONG lPlayerState, ULONG ulPlayerExtra, ServerCommandFlags flags, bool bMorph )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SpawnPlayer command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetPriorState( lPlayerState );
	command.SetIsBot( players[ulPlayer].bIsBot );
	// Do we really need to send this? Shouldn't it always be PST_LIVE?
	command.SetPlayerState( players[ulPlayer].playerstate );
	command.SetIsSpectating( players[ulPlayer].bSpectating );
	command.SetIsDeadSpectator( players[ulPlayer].bDeadSpectator );
	command.SetIsMorphed( bMorph );
	command.SetNetid( players[ulPlayer].mo->lNetID );
	command.SetAngle( players[ulPlayer].mo->angle );
	command.SetX( players[ulPlayer].mo->x );
	command.SetY( players[ulPlayer].mo->y );
	command.SetZ( players[ulPlayer].mo->z );
	command.SetPlayerClass( players[ulPlayer].CurrentPlayerClass );
	// command.addByte( players[ulPlayer].userinfo.GetPlayerClassNum() );
	command.SetMorphedClass( players[ulPlayer].mo ? players[ulPlayer].mo->GetClass() : NULL );
	command.SetMorphStyle( players[ulPlayer].MorphStyle );
	command.sendCommandToClients( ulPlayerExtra, flags );

	// [BB]: If the player still has any cheats activated from the last level, tell
	// him about it. Not doing this leads for example to jerky movement on client side
	// in case of NOCLIP.
	// We don't have to tell about CF_NOTARGET, since it's only used on the server.
	if( players[ulPlayer].cheats && players[ulPlayer].cheats != CF_NOTARGET )
		SERVERCOMMANDS_SetPlayerCheats( ulPlayer, ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MovePlayer( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ULONG ulPlayerAttackFlags = 0;

	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	// [BB] Check if ulPlayer is pressing any attack buttons.
	if ( players[ulPlayer].cmd.ucmd.buttons & BT_ATTACK )
		ulPlayerAttackFlags |= PLAYER_ATTACK;
	if ( players[ulPlayer].cmd.ucmd.buttons & BT_ALTATTACK )
		ulPlayerAttackFlags |= PLAYER_ALTATTACK;

	ServerCommands::MovePlayer fullCommand;
	fullCommand.SetPlayer ( &players[ulPlayer] );
	fullCommand.SetFlags( ulPlayerAttackFlags | PLAYER_VISIBLE );
	fullCommand.SetX( players[ulPlayer].mo->x );
	fullCommand.SetY( players[ulPlayer].mo->y );
	fullCommand.SetZ( players[ulPlayer].mo->z );
	fullCommand.SetAngle( players[ulPlayer].mo->angle );
	fullCommand.SetVelx( players[ulPlayer].mo->velx );
	fullCommand.SetVely( players[ulPlayer].mo->vely );
	fullCommand.SetVelz( players[ulPlayer].mo->velz );
	fullCommand.SetIsCrouching(( players[ulPlayer].crouchdir >= 0 ) ? true : false );

	ServerCommands::MovePlayer stubCommand = fullCommand;
	stubCommand.SetFlags( ulPlayerAttackFlags );

	for ( ClientIterator it ( ulPlayerExtra, flags ); it.notAtEnd(); ++it )
	{
		if ( SERVER_IsPlayerVisible( *it, ulPlayer ))
			fullCommand.sendCommandToClients( *it, SVCF_ONLYTHISCLIENT );
		else
			stubCommand.sendCommandToClients( *it, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_DamagePlayer( ULONG ulPlayer )
{
	ULONG		ulArmorPoints;
	AInventory	*pArmor;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Determine how much armor the damaged player now has, so we can send that information.
	if ( players[ulPlayer].mo == NULL )
		ulArmorPoints = 0;
	else
	{
		// Search the player's inventory for armor.
		pArmor = players[ulPlayer].mo->FindInventory< ABasicArmor >( );

		// If the player doesn't possess any armor, then his armor points are 0.
		ulArmorPoints = ( pArmor != NULL ) ? pArmor->Amount : 0;
	}

	ServerCommands::DamagePlayer fullCommand;
	fullCommand.SetPlayer( &players[ulPlayer] );
	fullCommand.SetHealth( players[ulPlayer].health );
	fullCommand.SetArmor( ulArmorPoints );
	fullCommand.SetAttacker( players[ulPlayer].attacker );

	for ( ClientIterator it; it.notAtEnd(); ++it )
	{
		// [EP] Send the updated health and armor of the player who's being damaged to this client
		// only if this client is allowed to know (still, don't forget the pain state!).
		if ( SERVER_IsPlayerAllowedToKnowHealth( *it, ulPlayer ))
			fullCommand.sendCommandToClients( *it, SVCF_ONLYTHISCLIENT );
		else
			SERVERCOMMANDS_SetThingState( players[ulPlayer].mo, STATE_PAIN, *it, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_KillPlayer( ULONG ulPlayer, AActor *pSource, AActor *pInflictor, FName MOD )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	const PClass *weaponType = NULL;

	// [TP] FIXME: Wouldn't it be miles easier to find find the killing player with pSource->player?
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( players[ulIdx].mo == NULL ))
		{
			continue;
		}

		if ( players[ulIdx].mo == pSource )
		{
			if ( players[ulIdx].ReadyWeapon != NULL )
				weaponType = players[ulIdx].ReadyWeapon->GetClass();
			break;
		}
	}

	ServerCommands::KillPlayer command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetSource( pSource );
	command.SetInflictor( pInflictor );
	// [BB] We only send the health as short, so make sure that it doesn't exceed the corresponding range.
	command.SetHealth( clamp ( players[ulPlayer].mo->health, SHRT_MIN, SHRT_MAX ));
	command.SetMOD( MOD.GetChars() );
	command.SetDamageType( players[ulPlayer].mo->DamageType.GetChars() );
	command.SetWeaponType( weaponType );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerHealth( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerHealth command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetHealth( players[ulPlayer].health );

	for ( ClientIterator it ( ulPlayerExtra, flags ); it.notAtEnd(); ++it )
	{
		if ( SERVER_IsPlayerAllowedToKnowHealth( *it, ulPlayer ))
			command.sendCommandToClients( *it, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerArmor( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	AInventory *pArmor = players[ulPlayer].mo->FindInventory< ABasicArmor >( );

	if ( pArmor == NULL )
		return;

	ServerCommands::SetPlayerArmor command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetArmorAmount( pArmor->Amount );
	command.SetArmorIcon( pArmor->Icon.isValid() ? TexMan( pArmor->Icon )->Name : "" );

	for ( ClientIterator it ( ulPlayerExtra, flags ); it.notAtEnd(); ++it )
	{
		if ( SERVER_IsPlayerAllowedToKnowHealth( *it, ulPlayer ))
			command.sendCommandToClients( *it, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerHealthAndMaxHealthBonus( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	// [BB] Workaround to set max health bonus for the player on the client(s).
	if ( players[ulPlayer].lMaxHealthBonus > 0 ) {
		AInventory *pInventory = Spawn<AMaxHealth>(0,0,0, NO_REPLACE);
		if ( pInventory )
		{
			pInventory->Amount = players[ulPlayer].lMaxHealthBonus;
			SERVERCOMMANDS_GiveInventory( ulPlayer, pInventory, ulPlayerExtra, flags );
			pInventory->Destroy ();
			pInventory = NULL;
		}
	}
	SERVERCOMMANDS_SetPlayerHealth( ulPlayer, ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerArmorAndMaxArmorBonus( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	// [BB] Workaround to set max armor bonus for the player on the client(s).
	ABasicArmor *pArmor = players[ulPlayer].mo->FindInventory<ABasicArmor>( );

	if ( pArmor && ( pArmor->BonusCount > 0 ) )
	{
		AInventory *pInventory = static_cast<AInventory*> ( Spawn( PClass::FindClass( "MaxArmorBonus" ), 0,0,0, NO_REPLACE) );
		if ( pInventory )
		{
			pInventory->Amount = pArmor->BonusCount;
			SERVERCOMMANDS_GiveInventory( ulPlayer, pInventory, ulPlayerExtra, flags );
			pInventory->Destroy ();
			pInventory = NULL;
		}
	}
	SERVERCOMMANDS_SetPlayerArmor( ulPlayer, ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerState( ULONG ulPlayer, PLAYERSTATE_e ulState, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerState command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetState( ulState );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerUserInfo( ULONG ulPlayer, ULONG ulUserInfoFlags, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerUserInfo command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetFlags( ulUserInfoFlags );
	command.SetName( players[ulPlayer].userinfo.GetName() );
	command.SetGender( players[ulPlayer].userinfo.GetGender() );
	command.SetColor( players[ulPlayer].userinfo.GetColor() );
	command.SetRailgunTrailColor( players[ulPlayer].userinfo.GetRailColor() );
	command.SetSkinName( SERVER_GetClient( ulPlayer )->szSkin );
	command.SetHandicap( players[ulPlayer].userinfo.GetHandicap() );
	command.SetTicsPerUpdate( players[ulPlayer].userinfo.GetTicsPerUpdate() );
	command.SetConnectionType( players[ulPlayer].userinfo.GetConnectionType() );
	command.SetClientFlags( players[ulPlayer].userinfo.GetClientFlags() );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerFrags( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerFrags command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetFragCount( players[ulPlayer].fragcount );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPoints( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerPoints command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetPointCount( players[ulPlayer].lPointCount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerWins( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerWins command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetWins( players[ulPlayer].ulWins );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerKillCount( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerKillCount command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetKillCount( players[ulPlayer].killcount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerChatStatus( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetPlayerChatStatus command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetChatting( players[ulPlayer].bChatting );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerLaggingStatus( ULONG ulPlayer )
{
	ServerCommands::SetPlayerLaggingStatus command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetLagging( players[ulPlayer].bLagging );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerConsoleStatus( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetPlayerConsoleStatus command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetInConsole( players[ulPlayer].bInConsole );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerReadyToGoOnStatus( ULONG ulPlayer )
{
	ServerCommands::SetPlayerReadyToGoOnStatus command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetReadyToGoOn( players[ulPlayer].bReadyToGoOn );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerTeam( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerTeam command;
	command.SetPlayer( &players[ulPlayer] );

	if ( players[ulPlayer].bOnTeam == false )
		command.SetTeam( teams.Size( ));
	else
		command.SetTeam( players[ulPlayer].ulTeam );

	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerCamera( ULONG ulPlayer, AActor *camera, bool bRevertPlease )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerCamera command;
	command.SetCamera( camera );
	command.SetRevertPlease( bRevertPlease );
	command.sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPoisonCount( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_SETPLAYERPOISONCOUNT );
	command.addByte( ulPlayer );
	command.addShort( players[ulPlayer].poisoncount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerAmmoCapacity( ULONG ulPlayer, AInventory *pAmmo, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) ||
		( pAmmo == NULL ) ||
		( pAmmo->GetClass()->IsDescendantOf (RUNTIME_CLASS(AAmmo)) == false ))
	{
		return;
	}

	NetCommand command( SVC_SETPLAYERAMMOCAPACITY );
	command.addByte( ulPlayer );
	command.addShort( pAmmo->GetClass( )->getActorNetworkIndex() );
	command.addLong( pAmmo->MaxAmount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerCheats( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_SETPLAYERCHEATS );
	command.addByte( ulPlayer );
	command.addLong( players[ulPlayer].cheats );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPendingWeapon( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if (( players[ulPlayer].PendingWeapon == WP_NOCHANGE ) ||
		( players[ulPlayer].PendingWeapon == NULL ))
	{
		return;
	}

	// Only send this info to spectators.
	// [BB] Or if this is a COOP game.
	// [BB] Everybody needs to know this. Otherwise the Railgun sound is broken and spying in demos doesn't work properly.
	ServerCommands::SetPlayerPendingWeapon command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetWeaponType( players[ulPlayer].PendingWeapon->GetClass() );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
/* [BB] Does not work with the latest ZDoom changes. Check if it's still necessary.
void SERVERCOMMANDS_SetPlayerPieces( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_SETPLAYERPIECES );
	command.addByte( ulPlayer );
	command.addByte( players[ulPlayer].pieces );
	command.sendCommandToClients( ulPlayerExtra, flags );
}
*/

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPSprite( ULONG ulPlayer, FState *pState, LONG lPosition, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	FString			stateLabel;
	LONG			lOffset;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( players[ulPlayer].ReadyWeapon == NULL )
		return;

	// [BB] Try to find the state label and the correspoding offset belonging to the target state.
	FindStateLabelAndOffset( players[ulPlayer].ReadyWeapon, pState, stateLabel, lOffset, true );

	// Couldn't find the state, so just try to go based off the spawn state.
	if ( stateLabel.IsEmpty() )
	{
		lOffset = LONG( pState - players[ulPlayer].ReadyWeapon->GetReadyState( ));
		// [BB] Spawn state didn't work either, try flash state.
		if ( OffsetAndStateOwnershipValidityCheck ( lOffset, players[ulPlayer].ReadyWeapon, players[ulPlayer].ReadyWeapon->GetReadyState( ) ) == false )
		{
			const FState *pFlashState = players[ulPlayer].ReadyWeapon->FindState(NAME_Flash);
			lOffset = LONG( pState - pFlashState );
			if ( OffsetAndStateOwnershipValidityCheck ( lOffset, players[ulPlayer].ReadyWeapon, pFlashState ) == false )
			{
				// [BB] If we still couldn't find the state, show a warning.
				if ( sv_showwarnings )
					Printf ( "SERVERCOMMANDS_SetPlayerPSprite: Couldn't find state!\n" );

				return;
			}
			else
				stateLabel = ":F";
		}
		else
			stateLabel = ":R";
	}

	ServerCommands::SetPlayerPSprite command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetState( stateLabel );
	command.SetOffset( lOffset );
	command.SetPosition( lPosition );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerBlend( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerBlend command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetBlendR( players[ulPlayer].BlendR );
	command.SetBlendG( players[ulPlayer].BlendG );
	command.SetBlendB( players[ulPlayer].BlendB );
	command.SetBlendA( players[ulPlayer].BlendA );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerMaxHealth( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false || players[ulPlayer].mo == NULL )
		return;

	ServerCommands::SetPlayerMaxHealth command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetMaxHealth( players[ulPlayer].mo->MaxHealth );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerLivesLeft( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerLivesLeft command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetLivesLeft( players[ulPlayer].ulLivesLeft );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerViewHeight( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	NetCommand command( SVC2_SETPLAYERVIEWHEIGHT );
	command.addByte( ulPlayer );
	command.addLong( players[ulPlayer].mo->ViewHeight );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SyncPlayerAmmoAmount( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	AInventory *pInventory = NULL;

	for ( pInventory = players[ulPlayer].mo->Inventory; pInventory != NULL; pInventory = pInventory->Inventory )
	{
		if ( pInventory->IsKindOf( RUNTIME_CLASS( AAmmo )) == false )
			continue;

		SERVERCOMMANDS_GiveInventory( ulPlayer, pInventory, ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerPing( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::UpdatePlayerPing command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetPing( players[ulPlayer].ulPing );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerExtraData( ULONG ulPlayer, ULONG ulDisplayPlayer )
{
	if (( SERVER_IsValidClient( ulPlayer ) == false ) || ( PLAYER_IsValidPlayer( ulDisplayPlayer ) == false ))
		return;

	ServerCommands::UpdatePlayerExtraData command;
	command.SetPlayer( &players[ulDisplayPlayer] );
	command.SetPitch( players[ulDisplayPlayer].mo->pitch );
	command.SetWaterLevel( players[ulDisplayPlayer].mo->waterlevel );
	command.SetButtons( players[ulDisplayPlayer].cmd.ucmd.buttons );
	command.SetViewZ( players[ulDisplayPlayer].viewz );
	command.SetBob( players[ulDisplayPlayer].bob );
	command.sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerTime( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::UpdatePlayerTime command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetTime(( players[ulPlayer].ulTime / ( TICRATE * 60 )));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveLocalPlayer( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false || players[ulPlayer].mo == NULL )
		return;

	ServerCommands::MoveLocalPlayer command;
	command.SetClientTicOnServerEnd( SERVER_GetClient( ulPlayer )->ulClientGameTic );
	command.SetLatestServerGametic( gametic );
	command.SetX( players[ulPlayer].mo->x );
	command.SetY( players[ulPlayer].mo->y );
	command.SetZ( players[ulPlayer].mo->z );
	command.SetVelx( players[ulPlayer].mo->velx );
	command.SetVely( players[ulPlayer].mo->vely );
	command.SetVelz( players[ulPlayer].mo->velz );
	command.sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DisconnectPlayer( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::DisconnectPlayer command;
	command.SetPlayer( &players[ulPlayer] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetConsolePlayer( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	ServerCommands::SetConsolePlayer command;
	command.SetPlayerNumber( ulPlayer );
	command.sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ConsolePlayerKicked( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	ServerCommands::ConsolePlayerKicked command;
	command.sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_GivePlayerMedal( ULONG ulPlayer, ULONG ulMedal, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_GIVEPLAYERMEDAL );
	command.addByte( ulPlayer );
	command.addByte( ulMedal );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ResetAllPlayersFragcount( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_RESETALLPLAYERSFRAGCOUNT );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerIsSpectator( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_PLAYERISSPECTATOR );
	command.addByte( ulPlayer );
	command.addByte( players[ulPlayer].bDeadSpectator );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerSay( ULONG ulPlayer, const char *pszString, ULONG ulMode, bool bForbidChatToPlayers, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulPlayer != MAXPLAYERS && PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_PLAYERSAY );
	command.addByte( ulPlayer );
	command.addByte( ulMode );
	command.addString( pszString );

	for ( ClientIterator it ( ulPlayerExtra, flags ); it.notAtEnd(); ++it )
	{
		// Don't allow the chat message to be broadcasted to this player.
		if (( bForbidChatToPlayers ) && ( players[*it].bSpectating == false ))
			continue;

		// The player is sending a message to his teammates.
		if ( ulMode == CHATMODE_TEAM )
		{
			if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			{
				// If either player is not on a team, don't send the message.
				if ( (( players[*it].bOnTeam == false ) || ( players[ulPlayer].bOnTeam == false ))
					&& (( PLAYER_IsTrueSpectator ( &players[*it] ) == false ) || ( PLAYER_IsTrueSpectator ( &players[ulPlayer] ) == false )) )
					continue;

				// If the players are not on the same team, don't send the message.
				if ( ( players[*it].ulTeam != players[ulPlayer].ulTeam ) && ( ( PLAYER_IsTrueSpectator ( &players[*it] ) != PLAYER_IsTrueSpectator ( &players[ulPlayer] ) ) || ( PLAYER_IsTrueSpectator ( &players[*it] ) == false ) ) )
					continue;
			}
			// Not in a team mode.
			else
				continue;
		}

		command.sendCommandToOneClient( *it );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerTaunt( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_PLAYERTAUNT );
	command.addByte( ulPlayer );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerRespawnInvulnerability( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_PLAYERRESPAWNINVULNERABILITY );
	command.addByte( ulPlayer );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerUseInventory( ULONG ulPlayer, AInventory *pItem, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	USHORT		usActorNetworkIndex = 0;

	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) ||
		( pItem == NULL ))
	{
		return;
	}

	usActorNetworkIndex = pItem->GetClass( )->getActorNetworkIndex();
	if ( NETWORK_GetClassNameFromIdentification ( usActorNetworkIndex ) == NULL )
		return;

	NetCommand command( SVC_PLAYERUSEINVENTORY );
	command.addByte( ulPlayer );
	command.addShort( usActorNetworkIndex );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerDropInventory( ULONG ulPlayer, AInventory *pItem, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	USHORT		usActorNetworkIndex = 0;

	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) ||
		( pItem == NULL ))
	{
		return;
	}

	usActorNetworkIndex = pItem->GetClass( )->getActorNetworkIndex();
	if ( NETWORK_GetClassNameFromIdentification ( usActorNetworkIndex ) == NULL )
		return;

	NetCommand command( SVC_PLAYERDROPINVENTORY );
	command.addByte( ulPlayer );
	command.addShort( usActorNetworkIndex );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PotentiallyIgnorePlayer( ULONG ulPlayer )
{
	for ( ClientIterator it; it.notAtEnd(); ++it )
	{
		// Check whether this player is ignoring the newcomer's address.
		LONG lTicks = SERVER_GetPlayerIgnoreTic( *it, SERVER_GetClient( ulPlayer )->Address );

		if ( lTicks == 0 )
			continue;

		NetCommand command( SVC_IGNOREPLAYER );
		command.addByte( ulPlayer );
		command.addLong( lTicks );
		command.sendCommandToOneClient( *it );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThing( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	// If the actor doesn't have a network ID, it's better to send it ID-less.
	if ( pActor->lNetID == -1 )
	{
		SERVERCOMMANDS_SpawnThingNoNetID( pActor, ulPlayerExtra, flags );
		return;
	}

	ServerCommands::SpawnThing command;
	command.SetType( pActor->GetClass() );
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.SetId( pActor->lNetID );
	command.sendCommandToClients( ulPlayerExtra, flags );

	if ( pActor->ulSTFlags & STFL_LEVELSPAWNED )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS, ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingNoNetID( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	if ( pActor->ulNetworkFlags & NETFL_SERVERSIDEONLY )
		return;

	ServerCommands::SpawnThingNoNetID command;
	command.SetType( pActor->GetClass() );
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingExact( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	// If the actor doesn't have a network ID, it's better to send it ID-less.
	if ( pActor->lNetID == -1 )
	{
		SERVERCOMMANDS_SpawnThingExactNoNetID( pActor, ulPlayerExtra, flags );
		return;
	}

	ServerCommands::SpawnThingExact command;
	command.SetType( pActor->GetClass() );
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.SetId( pActor->lNetID );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingExactNoNetID( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	ServerCommands::SpawnThingExactNoNetID command;
	command.SetType( pActor->GetClass() );
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveThing( AActor *actor, ULONG bits, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (actor) )
		return;

	// [BB] Only skip updates, if sent to all players.
	if ( flags == 0 )
		RemoveUnnecessaryPositionUpdateFlags ( actor, bits );
	else // [WS] This will inform clients not to set their lastX/Y/Z with the new position.
		bits |= CM_NOLAST;

	// [WS] Check to see if the position can be re-used by the client.
	CheckPositionReuse( actor, bits );

	// Nothing to update.
	if ( bits == 0 )
		return;

	ServerCommands::MoveThing command;
	command.SetActor( actor );
	command.SetBits( bits );
	command.SetNewX( actor->x );
	command.SetNewY( actor->y );
	command.SetNewZ( actor->z );
	command.SetLastX( actor->lastX );
	command.SetLastY( actor->lastY );
	command.SetLastZ( actor->lastZ );
	command.SetAngle( actor->angle );
	command.SetVelX( actor->velx );
	command.SetVelY( actor->vely );
	command.SetVelZ( actor->velz );
	command.SetPitch( actor->pitch );
	command.SetMovedir( actor->movedir );
	command.sendCommandToClients( ulPlayerExtra, flags );

	// [BB] Only mark something as updated, if it the update was sent to all players.
	if ( flags == 0 )
		ActorNetPositionUpdated ( actor, bits );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveThingExact( AActor *actor, ULONG bits, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (actor) )
		return;

	// [BB] Only skip updates, if sent to all players.
	if ( flags == 0 )
		RemoveUnnecessaryPositionUpdateFlags ( actor, bits );
	else // [WS] This will inform clients not to set their lastX/Y/Z with the new position.
		bits |= CM_NOLAST;

	// [WS] Check to see if the position can be re-used by the client.
	CheckPositionReuse( actor, bits );

	// Nothing to update.
	if ( bits == 0 )
		return;

	ServerCommands::MoveThingExact command;
	command.SetActor( actor );
	command.SetBits( bits );
	command.SetNewX( actor->x );
	command.SetNewY( actor->y );
	command.SetNewZ( actor->z );
	command.SetLastX( actor->lastX );
	command.SetLastY( actor->lastY );
	command.SetLastZ( actor->lastZ );
	command.SetAngle( actor->angle );
	command.SetVelX( actor->velx );
	command.SetVelY( actor->vely );
	command.SetVelZ( actor->velz );
	command.SetPitch( actor->pitch );
	command.SetMovedir( actor->movedir );
	command.sendCommandToClients( ulPlayerExtra, flags );

	// [BB] Only mark something as updated, if it the update was sent to all players.
	if ( flags == 0 )
		ActorNetPositionUpdated ( actor, bits );
}

//*****************************************************************************
//
void SERVERCOMMANDS_KillThing( AActor *pActor, AActor *pSource, AActor *pInflictor )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::KillThing command;
	command.SetVictim( pActor );
	command.SetSource( pSource );
	command.SetInflictor( pInflictor );
	command.SetDamageType( pActor->DamageType.GetChars() );
	command.SetHealth( pActor->health );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingState( AActor *pActor, NetworkActorState state, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingState command;
	command.SetActor( pActor );
	command.SetState( state );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTarget( AActor *pActor )
{
	if ( !EnsureActorHasNetID (pActor) || !EnsureActorHasNetID(pActor->target) )
		return;

	ServerCommands::SetThingTarget command;
	command.SetActor( pActor );
	command.SetTarget( pActor->target );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyThing( AActor *pActor )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::DestroyThing command;
	command.SetActor( pActor );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingAngle( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingAngle command;
	command.SetActor( pActor );
	command.SetAngle( pActor->angle );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingAngleExact( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingAngleExact command;
	command.SetActor( pActor );
	command.SetAngle( pActor->angle );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingWaterLevel( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingWaterLevel command;
	command.SetActor( pActor );
	command.SetWaterlevel( pActor->waterlevel );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingFlags( AActor *pActor, FlagSet flagset, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	int actorFlags;

	switch ( flagset )
	{
		case FLAGSET_FLAGS:		actorFlags = pActor->flags; break;
		case FLAGSET_FLAGS2:	actorFlags = pActor->flags2; break;
		case FLAGSET_FLAGS3:	actorFlags = pActor->flags3; break;
		case FLAGSET_FLAGS4:	actorFlags = pActor->flags4; break;
		case FLAGSET_FLAGS5:	actorFlags = pActor->flags5; break;
		case FLAGSET_FLAGS6:	actorFlags = pActor->flags6; break;
		case FLAGSET_FLAGS7:	actorFlags = pActor->flags7; break;
		case FLAGSET_FLAGSST:	actorFlags = pActor->ulSTFlags; break;
		default: return;
	}

	ServerCommands::SetThingFlags command;
	command.SetActor( pActor );
	command.SetFlagset( flagset );
	command.SetFlags( actorFlags );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor->flags != pActor->GetDefault( )->flags )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS, ulPlayerExtra, flags );
	}
	if ( pActor->flags2 != pActor->GetDefault( )->flags2 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS2, ulPlayerExtra, flags );
	}
	if ( pActor->flags3 != pActor->GetDefault( )->flags3 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS3, ulPlayerExtra, flags );
	}
	if ( pActor->flags4 != pActor->GetDefault( )->flags4 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS4, ulPlayerExtra, flags );
	}
	if ( pActor->flags5 != pActor->GetDefault( )->flags5 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS5, ulPlayerExtra, flags );
	}
	if ( pActor->flags6 != pActor->GetDefault( )->flags6 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS6, ulPlayerExtra, flags );
	}
	if ( pActor->flags7 != pActor->GetDefault( )->flags7 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS7, ulPlayerExtra, flags );
	}
	// [BB] ulSTFlags is intentionally left out here.
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingArguments( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingArguments command;
	command.SetActor( pActor );
	command.SetArg0( pActor->args[0] );
	command.SetArg1( pActor->args[1] );
	command.SetArg2( pActor->args[2] );
	command.SetArg3( pActor->args[3] );
	command.SetArg4( pActor->args[4] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTranslation( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingTranslation command;
	command.SetActor( pActor );
	command.SetTranslation( pActor->Translation );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingProperty( AActor *pActor, ULONG ulProperty, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	int value = 0;

	// Set one of the actor's properties, depending on what was read in.
	switch ( ulProperty )
	{
	case APROP_Speed:
		value = pActor->Speed;
		break;

	case APROP_Alpha:
		value = pActor->alpha;
		break;

	case APROP_RenderStyle:
		value = pActor->RenderStyle.AsDWORD;
		break;

	case APROP_JumpZ:
		if ( pActor->IsKindOf( RUNTIME_CLASS( APlayerPawn )))
			value = static_cast<APlayerPawn *>( pActor )->JumpZ;
		break;

	default:
		return;
	}

	ServerCommands::SetThingProperty command;
	command.SetActor( pActor );
	command.SetProperty( ulProperty );
	command.SetValue( value );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSound( AActor *pActor, ULONG ulSound, const char *pszSound, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingSound command;
	command.SetActor( pActor );
	command.SetSoundType( ulSound );
	command.SetSound( pszSound );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSpawnPoint( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingSpawnPoint command;
	command.SetActor( pActor );
	command.SetSpawnPointX( pActor->SpawnPoint[0] );
	command.SetSpawnPointY( pActor->SpawnPoint[1] );
	command.SetSpawnPointZ( pActor->SpawnPoint[2] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSpecial( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	NetCommand command( SVC2_SETTHINGSPECIAL );
	command.addShort( pActor->lNetID );
	command.addShort( pActor->special );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSpecial1( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingSpecial1 command;
	command.SetActor( pActor );
	command.SetSpecial1( pActor->special1 );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSpecial2( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingSpecial2 command;
	command.SetActor( pActor );
	command.SetSpecial2( pActor->special2 );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTics( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingTics command;
	command.SetActor( pActor );
	command.SetTics( pActor->tics );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingReactionTime( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingReactionTime command;
	command.SetActor( pActor );
	command.SetReactiontime( pActor->reactiontime );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTID( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingTID command;
	command.SetActor( pActor );
	command.SetTid( pActor->tid );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingGravity( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingGravity command;
	command.SetActor( pActor );
	command.SetGravity( pActor->gravity );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingFrame( AActor *pActor, FState *pState, ULONG ulPlayerExtra, ServerCommandFlags flags, bool bCallStateFunction )
{
	FString stateLabel;
	LONG		lOffset = 0;

	if ( !EnsureActorHasNetID (pActor) || (pState == NULL) )
		return;

	// [BB] Special handling of certain states. This perhaps is not necessary
	// but at least saves a little net traffic.
	if ( bCallStateFunction
		 && (ulPlayerExtra == MAXPLAYERS)
		 && (flags == 0)
		)
	{
		if ( pState == pActor->MeleeState )
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_MELEE );
			return;
		}
		else if ( pState == pActor->MissileState )
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_MISSILE );
			return;
		}
		else if ( pState == pActor->FindState( NAME_Wound ))
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_WOUND );
			return;
		}
		else if ( pState == pActor->FindState( NAME_Pain ))
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_PAIN );
			return;
		}
	}

	// [BB] Try to find the state label and the correspoding offset belonging to the target state.
	FindStateLabelAndOffset( pActor, pState, stateLabel, lOffset, true );

	// Couldn't find the state, so just try to go based off one of the standard states.
	// [BB] This is a workaround. Therefore let the name of the state string begin
	// with ':' so that the client can handle this differently.
	// [BB] Because of inheritance it's not sufficient to only try the SpawnState.
	if ( stateLabel.IsEmpty() )
	{
		lOffset = LONG( pState - pActor->SpawnState );
		if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pActor, pActor->SpawnState ) == false )
		{
			// [BB] SpawnState doesn't work. Try MissileState.
			lOffset = LONG( pState - pActor->MissileState );
			if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pActor, pActor->MissileState ) == false )
			{
				// [BB] Try SeeState.
				lOffset = LONG( pState - pActor->SeeState );
				if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pActor, pActor->SeeState ) == false )
				{
					// [BB] Try MeleeState.
					lOffset = LONG( pState - pActor->MeleeState );
					if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pActor, pActor->MeleeState ) == false )
					{
						// [BB] Apparently pActor doesn't own the state. Find out who does.
						const PClass *pStateOwnerClass = FState::StaticFindStateOwner ( pState );
						const AActor *pStateOwner = ( pStateOwnerClass != NULL ) ? GetDefaultByType ( pStateOwnerClass ) : NULL;
						if ( pStateOwner )
						{
							lOffset = LONG( pState - pStateOwner->SpawnState );
							if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pStateOwnerClass, pStateOwner->SpawnState ) == false )
							{
								lOffset = LONG( pState - pStateOwnerClass->ActorInfo->FindState(NAME_Death) );
								if ( OffsetAndStateOwnershipValidityCheck ( lOffset, pStateOwnerClass, pStateOwnerClass->ActorInfo->FindState(NAME_Death) ) == false )
								{
									if ( sv_showwarnings )
										Printf ( "Warning: SERVERCOMMANDS_SetThingFrame failed to set the frame for actor %s.\n", pActor->GetClass()->TypeName.GetChars() );
									return;
								}
								{
									stateLabel = "+";
									stateLabel += pStateOwnerClass->TypeName;
								}
							}
							else
							{
								stateLabel = ";";
								stateLabel += pStateOwnerClass->TypeName;
							}

						}
						else
						{
							if ( sv_showwarnings )
								Printf ( "Warning: SERVERCOMMANDS_SetThingFrame failed to set the frame for actor %s (can't find state owner).\n", pActor->GetClass()->TypeName.GetChars() );
							return;
						}
					}
					else
						stateLabel = ":N";
				}
				else
					stateLabel = ":T";
			}
			else
				stateLabel = ":M";
		}
		else
			stateLabel = ":S";
	}

	if ( bCallStateFunction )
	{
		ServerCommands::SetThingFrame command;
		command.SetActor( pActor );
		command.SetStatename( stateLabel );
		command.SetOffset( lOffset );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}
	else
	{
		ServerCommands::SetThingFrameNF command;
		command.SetActor( pActor );
		command.SetStatename( stateLabel );
		command.SetOffset( lOffset );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetWeaponAmmoGive( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	if ( pActor->IsKindOf( RUNTIME_CLASS( AWeapon )) == false )
		return;

	ServerCommands::SetWeaponAmmoGive command;
	command.SetWeapon( static_cast<AWeapon *>( pActor ) );
	command.SetAmmoGive1( static_cast<AWeapon *>( pActor )->AmmoGive1 );
	command.SetAmmoGive2( static_cast<AWeapon *>( pActor )->AmmoGive2 );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingIsCorpse( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::ThingIsCorpse command;
	command.SetActor( pActor );
	command.SetIsMonster( pActor->CountsAsKill() );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_HideThing( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// [BB] The client will call HideIndefinitely on the actor, which only is possible on AInventory and descendants.
	if ( !EnsureActorHasNetID (pActor) || !(pActor->IsKindOf( RUNTIME_CLASS( AInventory ))) )
		return;

	ServerCommands::HideThing command;
	command.SetItem( static_cast<AInventory*>( pActor ) );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_TeleportThing( AActor *pActor, bool bSourceFog, bool bDestFog, bool bTeleZoom, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::TeleportThing command;
	command.SetActor( pActor );
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.SetMomx( pActor->velx );
	command.SetMomy( pActor->vely );
	command.SetMomz( pActor->velz );
	command.SetReactiontime( pActor->reactiontime );
	command.SetAngle( pActor->angle );
	command.SetSourcefog( bSourceFog );
	command.SetDestfog( bDestFog );
	command.SetTelezoom( bTeleZoom );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingActivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::ThingActivate command;
	command.SetActor( pActor );
	command.SetActivator( pActivator );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingDeactivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::ThingDeactivate command;
	command.SetActor( pActor );
	command.SetActivator( pActivator );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_RespawnDoomThing( AActor *pActor, bool bFog, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::RespawnDoomThing command;
	command.SetActor( pActor );
	command.SetFog( bFog );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_RespawnRavenThing( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::RespawnRavenThing command;
	command.SetActor( pActor );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SpawnBlood( fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage, AActor *originator, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( originator == NULL )
		return;

	ServerCommands::SpawnBlood command;
	command.SetX( x );
	command.SetY( y );
	command.SetZ( z );
	command.SetDir( dir );
	command.SetDamage( damage );
	command.SetOriginator( originator );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnBloodSplatter( fixed_t x, fixed_t y, fixed_t z, AActor *originator, bool isBloodSplatter2, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( originator == NULL )
		return;

	if ( isBloodSplatter2 == false )
	{
		ServerCommands::SpawnBloodSplatter command;
		command.SetX( x );
		command.SetY( y );
		command.SetZ( z );
		command.SetOriginator( originator );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}
	else
	{
		ServerCommands::SpawnBloodSplatter2 command;
		command.SetX( x );
		command.SetY( y );
		command.SetZ( z );
		command.SetOriginator( originator );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnPuff( AActor *pActor, ULONG ulState, bool bSendTranslation, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	ServerCommands::SpawnPuff command;
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.SetPufftype( pActor->GetClass() );
	command.SetStateid( ulState );
	command.SetReceiveTranslation( !!bSendTranslation );
	command.SetTranslation( pActor->Translation );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Print( const char *pszString, ULONG ulPrintLevel, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::Print command;
	command.SetPrintlevel( ulPrintLevel );
	command.SetMessage( pszString );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintMid( const char *pszString, bool bBold, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintMid command;
	command.SetMessage( pszString );
	command.SetBold( bBold );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintMOTD( const char *pszString, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintMOTD command;
	command.SetMotd( pszString );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessage( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintHUDMessage command;
	command.SetMessage( pszString );
	command.SetX( fX );
	command.SetY( fY );
	command.SetHudWidth( lHUDWidth );
	command.SetHudHeight( lHUDHeight );
	command.SetColor( lColor );
	command.SetHoldTime( fHoldTime );
	command.SetFontName( pszFont );
	command.SetLog( bLog );
	command.SetId( lID );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageFadeOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintHUDMessageFadeOut command;
	command.SetMessage( pszString );
	command.SetX( fX );
	command.SetY( fY );
	command.SetHudWidth( lHUDWidth );
	command.SetHudHeight( lHUDHeight );
	command.SetColor( lColor );
	command.SetHoldTime( fHoldTime );
	command.SetFadeOutTime( fFadeOutTime );
	command.SetFontName( pszFont );
	command.SetLog( bLog );
	command.SetId( lID );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageFadeInOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeInTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintHUDMessageFadeInOut command;
	command.SetMessage( pszString );
	command.SetX( fX );
	command.SetY( fY );
	command.SetHudWidth( lHUDWidth );
	command.SetHudHeight( lHUDHeight );
	command.SetColor( lColor );
	command.SetHoldTime( fHoldTime );
	command.SetFadeInTime( fFadeInTime );
	command.SetFadeOutTime( fFadeOutTime );
	command.SetFontName( pszFont );
	command.SetLog( bLog );
	command.SetId( lID );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageTypeOnFadeOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fTypeTime, float fHoldTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintHUDMessageTypeOnFadeOut command;
	command.SetMessage( pszString );
	command.SetX( fX );
	command.SetY( fY );
	command.SetHudWidth( lHUDWidth );
	command.SetHudHeight( lHUDHeight );
	command.SetColor( lColor );
	command.SetTypeOnTime( fTypeTime );
	command.SetHoldTime( fHoldTime );
	command.SetFadeOutTime( fFadeOutTime );
	command.SetFontName( pszFont );
	command.SetLog( bLog );
	command.SetId( lID );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetGameMode( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_SETGAMEMODE );
	command.addByte( GAMEMODE_GetCurrentMode( ));
	command.addByte( instagib );
	command.addByte( buckshot );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameSkill( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_SETGAMESKILL );
	command.addByte( gameskill );
	command.addByte( botskill );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameDMFlags( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETGAMEDMFLAGS );
	command.addLong ( dmflags );
	command.addLong ( dmflags2 );
	command.addLong ( compatflags );
	command.addLong ( compatflags2 );
	command.addLong ( zacompatflags );
	command.addLong ( zadmflags );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameModeLimits( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETGAMEMODELIMITS );
	command.addShort( fraglimit );
	command.addFloat( timelimit );
	command.addShort( pointlimit );
	command.addByte( duellimit );
	command.addByte( winlimit );
	command.addByte( wavelimit );
	command.addByte( sv_cheats );
	command.addByte( sv_fastweapons );
	command.addByte( sv_maxlives );
	command.addByte( sv_maxteams );
	// [BB] The value of sv_gravity is irrelevant when sv_gravity hasn't been changed since the map start
	// and the map has a custom gravity value in MAPINFO. level.gravity always contains the used gravity value,
	// so we just send this one instead of sv_gravity.
	command.addFloat( level.gravity );
	// [BB] sv_aircontrol needs to be handled similarly to sv_gravity.
	command.addFloat( static_cast<float>(level.aircontrol) / 65536.f );
	// [WS] Send in sv_coop_damagefactor.
	command.addFloat( sv_coop_damagefactor );
	// [WS] Send in alwaysapplydmflags.
	command.addByte( alwaysapplydmflags );
	// [AM] Send lobby map.
	command.addString( lobby );
	// [TP] Send sv_limitcommands
	command.addByte( sv_limitcommands );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameEndLevelDelay( ULONG ulEndLevelDelay, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_SETGAMEENDLEVELDELAY );
	command.addShort( ulEndLevelDelay );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameModeState( ULONG ulState, ULONG ulCountdownTicks, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_SETGAMEMODESTATE );
	command.addByte( ulState );
	command.addShort( ulCountdownTicks );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDuelNumDuels( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_SETDUELNUMDUELS );
	command.addByte( DUEL_GetNumDuels( ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLMSSpectatorSettings( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_SETLMSSPECTATORSETTINGS );
	command.addLong( lmsspectatorsettings );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLMSAllowedWeapons( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_SETLMSALLOWEDWEAPONS );
	command.addLong( lmsallowedweapons );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetInvasionNumMonstersLeft( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_SETINVASIONNUMMONSTERSLEFT );
	command.addShort( INVASION_GetNumMonstersLeft( ));
	command.addShort( INVASION_GetNumArchVilesLeft( ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetInvasionWave( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_SETINVASIONWAVE );
	command.addByte( INVASION_GetCurrentWave( ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSimpleCTFSTMode( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_SETSIMPLECTFSTMODE );
	command.addByte( !!TEAM_GetSimpleCTFSTMode( ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoPossessionArtifactPickedUp( ULONG ulPlayer, ULONG ulTicks, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_DOPOSSESSIONARTIFACTPICKEDUP );
	command.addByte( ulPlayer );
	command.addShort( ulTicks );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoPossessionArtifactDropped( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_DOPOSSESSIONARTIFACTDROPPED );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoGameModeFight( ULONG ulCurrentWave, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_DOGAMEMODEFIGHT );
	command.addByte( ulCurrentWave );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoGameModeCountdown( ULONG ulTicks, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_DOGAMEMODECOUNTDOWN );
	command.addShort( ulTicks );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoGameModeWinSequence( ULONG ulWinner, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_DOGAMEMODEWINSEQUENCE );
	command.addByte( ulWinner );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDominationState( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	unsigned int NumPoints = DOMINATION_NumPoints();
	unsigned int *PointOwners = DOMINATION_PointOwners();
	NetCommand command( SVC_SETDOMINATIONSTATE );
	command.addLong( NumPoints );

	for( unsigned int i = 0u; i < NumPoints; i++ )
	{
		//one byte should be enough to hold the value of the team.
		command.addByte( PointOwners[i] );
	}

	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDominationPointOwnership( ULONG ulPoint, ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_SETDOMINATIONPOINTOWNER );
	command.addByte( ulPoint );
	command.addByte( ulPlayer );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamFrags( ULONG ulTeam, LONG lFrags, bool bAnnounce, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( TEAM_CheckIfValid( ulTeam ) == false )
		return;

	NetCommand command( SVC_SETTEAMFRAGS );
	command.addByte( ulTeam );
	command.addShort( lFrags );
	command.addByte( bAnnounce );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamScore( ULONG ulTeam, LONG lScore, bool bAnnounce, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( TEAM_CheckIfValid( ulTeam ) == false )
		return;

	NetCommand command( SVC_SETTEAMSCORE );
	command.addByte( ulTeam );
	command.addShort( lScore );
	command.addByte( bAnnounce );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamWins( ULONG ulTeam, LONG lWins, bool bAnnounce, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( TEAM_CheckIfValid( ulTeam ) == false )
		return;

	NetCommand command( SVC_SETTEAMWINS );
	command.addByte( ulTeam );
	command.addShort( lWins );
	command.addByte( bAnnounce );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamReturnTicks( ULONG ulTeam, ULONG ulReturnTicks, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// [BB] Allow teams.Size( ) here, this handles the white flag.
	if (( TEAM_CheckIfValid ( ulTeam ) == false ) && ( ulTeam != teams.Size() ))
		return;

	NetCommand command( SVC_SETTEAMRETURNTICKS );
	command.addByte( ulTeam );
	command.addShort( ulReturnTicks );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_TeamFlagReturned( ULONG ulTeam, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// [BB] Allow teams.Size( ) here, this handles the white flag.
	if (( TEAM_CheckIfValid ( ulTeam ) == false ) && ( ulTeam != teams.Size() ))
		return;

	NetCommand command( SVC_TEAMFLAGRETURNED );
	command.addByte( ulTeam );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_TeamFlagDropped( ULONG ulPlayer, ULONG ulTeam, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_TEAMFLAGDROPPED );
	command.addByte( ulPlayer );
	command.addByte( ulTeam );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnMissile( AActor *pMissile, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pMissile == NULL )
		return;

	NetCommand command( SVC_SPAWNMISSILE );
	command.addShort( pMissile->x >> FRACBITS );
	command.addShort( pMissile->y >> FRACBITS );
	command.addShort( pMissile->z >> FRACBITS );
	command.addLong( pMissile->velx );
	command.addLong( pMissile->vely );
	command.addLong( pMissile->velz );
	command.addShort( pMissile->GetClass()->getActorNetworkIndex() );
	command.addShort( pMissile->lNetID );

	if ( pMissile->target )
		command.addShort( pMissile->target->lNetID );
	else
		command.addShort( -1 );

	command.sendCommandToClients( ulPlayerExtra, flags );

	// [BB] It's possible that the angle can't be derived from the velocity
	// of the missle. In this case the correct angle has to be told to the clients.
 	if( pMissile->angle != R_PointToAngle2( 0, 0, pMissile->velx, pMissile->vely ))
		SERVERCOMMANDS_SetThingAngle( pMissile, ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnMissileExact( AActor *pMissile, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pMissile == NULL )
		return;

	NetCommand command( SVC_SPAWNMISSILEEXACT );
	command.addLong( pMissile->x );
	command.addLong( pMissile->y );
	command.addLong( pMissile->z );
	command.addLong( pMissile->velx );
	command.addLong( pMissile->vely );
	command.addLong( pMissile->velz );
	command.addShort( pMissile->GetClass()->getActorNetworkIndex() );
	command.addShort( pMissile->lNetID );

	if ( pMissile->target )
		command.addShort( pMissile->target->lNetID );
	else
		command.addShort( -1 );

	command.sendCommandToClients( ulPlayerExtra, flags );

	// [BB] It's possible that the angle can't be derived from the velocity
	// of the missle. In this case the correct angle has to be told to the clients.
 	if( pMissile->angle != R_PointToAngle2( 0, 0, pMissile->velx, pMissile->vely ) )
		SERVERCOMMANDS_SetThingAngleExact( pMissile, ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MissileExplode( AActor *pMissile, line_t *pLine, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pMissile) )
		return;

	NetCommand command( SVC_MISSILEEXPLODE );
	command.addShort( pMissile->lNetID );

	if ( pLine != NULL )
		command.addShort( pLine - lines );
	else
		command.addShort( -1 );

	command.addShort( pMissile->x >> FRACBITS );
	command.addShort( pMissile->y >> FRACBITS );
	command.addShort( pMissile->z >> FRACBITS );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_WeaponSound( ULONG ulPlayer, const char *pszSound, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command( SVC_WEAPONSOUND );
	command.addByte( ulPlayer );
	command.addString( pszSound );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_WeaponChange( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false || players[ulPlayer].ReadyWeapon == NULL )
		return;

	NetCommand command( SVC_WEAPONCHANGE );
	command.addByte( ulPlayer );
	command.addShort( players[ulPlayer].ReadyWeapon->GetClass()->getActorNetworkIndex() );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_WeaponRailgun( AActor *source, const FVector3 &start, const FVector3 &end, LONG color1, LONG color2, float maxdiff, int railflags, angle_t angleoffset, const PClass* spawnclass, int duration, float sparsity, float drift, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// Evidently, to draw a railgun trail, there must be a source actor.
	if ( !EnsureActorHasNetID (source) )
		return;

	ServerCommands::WeaponRailgun command;
	command.SetSource( source );
	command.SetStart( start );
	command.SetEnd( end );
	command.SetColor1( color1 );
	command.SetColor2( color2 );
	command.SetMaxdiff( maxdiff );
	command.SetAngleoffset( angleoffset );
	command.SetSpawnclass( spawnclass );
	command.SetDuration( duration );
	command.SetSparsity( sparsity );
	command.SetDrift( drift );
	command.SetFlags( railflags );

	// [TP] Recent ZDoom versions have added more railgun parameters. Add these parameters to the command
	// only if they're not at defaults.
	command.SetExtended( angleoffset != 0
		|| spawnclass != NULL
		|| duration != 0
		|| fabs( sparsity - 1.0f ) > 1e-8
		|| fabs( drift - 1.0f ) > 1e-8 );

	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFloorPlane( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorFloorPlane command;
	command.SetSector( &sectors[ulSector] );
	command.SetHeight( sectors[ulSector].floorplane.d );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorCeilingPlane( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorCeilingPlane command;
	command.SetSector( &sectors[ulSector] );
	command.SetHeight( sectors[ulSector].ceilingplane.d );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFloorPlaneSlope( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorFloorPlaneSlope command;
	command.SetSector( &sectors[ulSector] );
	command.SetA( sectors[ulSector].floorplane.a );
	command.SetB( sectors[ulSector].floorplane.b );
	command.SetC( sectors[ulSector].floorplane.c );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorCeilingPlaneSlope( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorCeilingPlaneSlope command;
	command.SetSector( &sectors[ulSector] );
	command.SetA( sectors[ulSector].ceilingplane.a );
	command.SetB( sectors[ulSector].ceilingplane.b );
	command.SetC( sectors[ulSector].ceilingplane.c );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorLightLevel( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorLightLevel command;
	command.SetSector( &sectors[ulSector] );
	command.SetLightLevel( sectors[ulSector].lightlevel );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorColor( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorColor command;
	command.SetSector( &sectors[ulSector] );
	command.SetRed( sectors[ulSector].ColorMap->Color.r );
	command.SetGreen( sectors[ulSector].ColorMap->Color.g );
	command.SetBlue( sectors[ulSector].ColorMap->Color.b );
	command.SetDesaturate( sectors[ulSector].ColorMap->Desaturate );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorColorByTag( ULONG ulTag, ULONG ulRed, ULONG ulGreen, ULONG ulBlue, ULONG ulDesaturate, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetSectorColorByTag command;
	command.SetTag( ulTag );
	command.SetRed( ulRed );
	command.SetGreen( ulGreen );
	command.SetBlue( ulBlue );
	command.SetDesaturate( ulDesaturate );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFade( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorFade command;
	command.SetSector( &sectors[ulSector] );
	command.SetRed( sectors[ulSector].ColorMap->Fade.r );
	command.SetGreen( sectors[ulSector].ColorMap->Fade.g );
	command.SetBlue( sectors[ulSector].ColorMap->Fade.b );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFadeByTag( ULONG ulTag, ULONG ulRed, ULONG ulGreen, ULONG ulBlue, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetSectorFadeByTag command;
	command.SetTag( ulTag );
	command.SetRed( ulRed );
	command.SetGreen( ulGreen );
	command.SetBlue( ulBlue );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFlat( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorFlat command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingFlatName( TexMan( sectors[ulSector].GetTexture( sector_t::ceiling ))->Name );
	command.SetFloorFlatName( TexMan( sectors[ulSector].GetTexture( sector_t::floor ))->Name );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorPanning( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorPanning command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingXOffset( sectors[ulSector].GetXOffset( sector_t::ceiling ));
	command.SetCeilingYOffset( sectors[ulSector].GetYOffset( sector_t::ceiling, false ));
	command.SetFloorXOffset( sectors[ulSector].GetXOffset( sector_t::floor ));
	command.SetFloorYOffset( sectors[ulSector].GetYOffset( sector_t::floor, false ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorRotation( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorRotation command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingRotation( sectors[ulSector].GetAngle( sector_t::ceiling, false ) / ANGLE_1 );
	command.SetFloorRotation( sectors[ulSector].GetAngle( sector_t::floor, false ) / ANGLE_1 );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorRotationByTag( ULONG ulTag, LONG lFloorRot, LONG lCeilingRot, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetSectorRotationByTag command;
	command.SetTag( ulTag );
	command.SetCeilingRotation( lCeilingRot );
	command.SetFloorRotation( lFloorRot );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorScale( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorScale command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingXScale( sectors[ulSector].GetXScale( sector_t::ceiling ) );
	command.SetCeilingYScale( sectors[ulSector].GetYScale( sector_t::ceiling ) );
	command.SetFloorXScale( sectors[ulSector].GetXScale( sector_t::floor ) );
	command.SetFloorYScale( sectors[ulSector].GetYScale( sector_t::floor ) );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorSpecial( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorSpecial command;
	command.SetSector( &sectors[ulSector] );
	command.SetSpecial( sectors[ulSector].special );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFriction( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorFriction command;
	command.SetSector( &sectors[ulSector] );
	command.SetFriction( sectors[ulSector].friction );
	command.SetMoveFactor( sectors[ulSector].movefactor );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorAngleYOffset( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorAngleYOffset command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingBaseAngle( sectors[ulSector].planes[sector_t::ceiling].xform.base_angle );
	command.SetCeilingBaseYOffset( sectors[ulSector].planes[sector_t::ceiling].xform.base_yoffs );
	command.SetFloorBaseAngle( sectors[ulSector].planes[sector_t::floor].xform.base_angle );
	command.SetFloorBaseYOffset( sectors[ulSector].planes[sector_t::floor].xform.base_yoffs );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorGravity( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorGravity command;
	command.SetSector( &sectors[ulSector] );
	command.SetGravity( sectors[ulSector].gravity );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorReflection( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorReflection command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingReflection( sectors[ulSector].reflect[sector_t::ceiling] );
	command.SetFloorReflection( sectors[ulSector].reflect[sector_t::floor] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_StopSectorLightEffect( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::StopSectorLightEffect command;
	command.SetSector( &sectors[ulSector] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyAllSectorMovers( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DestroyAllSectorMovers().sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightFireFlicker( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	NetCommand command( SVC_DOSECTORLIGHTFIREFLICKER );
	command.addShort( ulSector );
	command.addShort( lMaxLight );
	command.addShort( lMinLight );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightFlicker( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	NetCommand command( SVC_DOSECTORLIGHTFLICKER );
	command.addShort( ulSector );
	command.addShort( lMaxLight );
	command.addShort( lMinLight );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightLightFlash( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	NetCommand command( SVC_DOSECTORLIGHTLIGHTFLASH );
	command.addShort( ulSector );
	command.addShort( lMaxLight );
	command.addShort( lMinLight );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightStrobe( ULONG ulSector, LONG lDarkTime, LONG lBrightTime, LONG lMaxLight, LONG lMinLight, LONG lCount, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	NetCommand command( SVC_DOSECTORLIGHTSTROBE );
	command.addShort( ulSector );
	command.addShort( lDarkTime );
	command.addShort( lBrightTime );
	command.addShort( lMaxLight );
	command.addShort( lMinLight );
	command.addByte( lCount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightGlow( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	NetCommand command( SVC_DOSECTORLIGHTGLOW );
	command.addShort( ulSector );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightGlow2( ULONG ulSector, LONG lStart, LONG lEnd, LONG lTics, LONG lMaxTics, bool bOneShot, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	NetCommand command( SVC_DOSECTORLIGHTGLOW2 );
	command.addShort( ulSector );
	command.addShort( lStart );
	command.addShort( lEnd );
	command.addShort( lTics );
	command.addShort( lMaxTics );
	command.addByte( bOneShot );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightPhased( ULONG ulSector, LONG lBaseLevel, LONG lPhase, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	NetCommand command ( SVC_DOSECTORLIGHTPHASED );
	command.addShort ( ulSector );
	command.addShort ( lBaseLevel );
	command.addByte ( lPhase );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetLineAlpha( ULONG ulLine, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulLine >= (ULONG)numlines )
		return;

	NetCommand command ( SVC_SETLINEALPHA );
	command.addShort ( ulLine );
	command.addLong ( lines[ulLine].Alpha );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLineTexture( ULONG ulLine, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulLine >= (ULONG)numlines )
		return;

	// No changed textures to update.
	if ( lines[ulLine].ulTexChangeFlags == 0 )
		return;

	if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_FRONTTOP )
	{
		NetCommand command ( SVC_SETLINETEXTURE );
		command.addShort( ulLine );
		command.addString( lines[ulLine].sidedef[0]->GetTexture(side_t::top).isValid() ? TexMan[lines[ulLine].sidedef[0]->GetTexture(side_t::top)]->Name : "-" );
		command.addByte( 0 );
		command.addByte( 0 );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}

	if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_FRONTMEDIUM )
	{
		NetCommand command ( SVC_SETLINETEXTURE );
		command.addShort( ulLine );
		command.addString( lines[ulLine].sidedef[0]->GetTexture(side_t::mid).isValid() ? TexMan[lines[ulLine].sidedef[0]->GetTexture(side_t::mid)]->Name : "-" );
		command.addByte( 0 );
		command.addByte( 1 );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}

	if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_FRONTBOTTOM )
	{
		NetCommand command ( SVC_SETLINETEXTURE );
		command.addShort( ulLine );
		command.addString( lines[ulLine].sidedef[0]->GetTexture(side_t::bottom).isValid() ? TexMan[lines[ulLine].sidedef[0]->GetTexture(side_t::bottom)]->Name : "-" );
		command.addByte( 0 );
		command.addByte( 2 );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}

	if ( lines[ulLine].sidedef[1] != NULL )
	{
		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKTOP )
		{
			NetCommand command ( SVC_SETLINETEXTURE );
			command.addShort( ulLine );
			command.addString( lines[ulLine].sidedef[1]->GetTexture(side_t::top).isValid() ? TexMan[lines[ulLine].sidedef[1]->GetTexture(side_t::top)]->Name : "-" );
			command.addByte( 1 );
			command.addByte( 0 );
			command.sendCommandToClients( ulPlayerExtra, flags );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKMEDIUM )
		{
			NetCommand command ( SVC_SETLINETEXTURE );
			command.addShort( ulLine );
			command.addString( lines[ulLine].sidedef[1]->GetTexture(side_t::mid).isValid() ? TexMan[lines[ulLine].sidedef[1]->GetTexture(side_t::mid)]->Name : "-" );
			command.addByte( 1 );
			command.addByte( 1 );
			command.sendCommandToClients( ulPlayerExtra, flags );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKBOTTOM )
		{
			NetCommand command ( SVC_SETLINETEXTURE );
			command.addShort( ulLine );
			command.addString( lines[ulLine].sidedef[1]->GetTexture(side_t::bottom).isValid() ? TexMan[lines[ulLine].sidedef[1]->GetTexture(side_t::bottom)]->Name : "-" );
			command.addByte( 1 );
			command.addByte( 2 );
			command.sendCommandToClients( ulPlayerExtra, flags );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLineTextureByID( ULONG ulLineID, ULONG ulSide, ULONG ulPosition, const char *pszTexName, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETLINETEXTUREBYID );
	command.addShort ( ulLineID );
	command.addString ( pszTexName );
	command.addByte ( ulSide );
	command.addByte ( ulPosition );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSomeLineFlags( ULONG ulLine, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulLine >= (ULONG)numlines )
		return;

	NetCommand command ( SVC_SETSOMELINEFLAGS );
	command.addShort ( ulLine );
	command.addLong ( lines[ulLine].flags & ( ML_BLOCKING | ML_BLOCKEVERYTHING | ML_RAILING | ML_BLOCK_PLAYERS | ML_ADDTRANS ));
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_ACSScriptExecute( int ScriptNum, AActor *pActivator, LONG lLineIdx, int levelnum, bool bBackSide, int iArg0, int iArg1, int iArg2, bool bAlways, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ACS_ExistsScript( ScriptNum ) == false )
		return;

	int netid = NETWORK_ACSScriptToNetID( ScriptNum );

	if ( netid == NO_SCRIPT_NETID )
	{
		// [TP] This shouldn't happen
		if ( sv_showwarnings )
		{
			Printf( "SERVERCOMMANDS_ACSScriptExecute: Failed to find a netid for script %s!\n",
				FName( ENamedName( -ScriptNum )).GetChars() );
		}
		return;
	}

	ServerCommands::ACSScriptExecute command;
	command.SetNetid( netid );
	command.SetActivator( pActivator );
	command.SetLineid( lLineIdx );
	command.SetLevelnum( levelnum );
	command.SetArg0( iArg0 );
	command.SetArg1( iArg1 );
	command.SetArg2( iArg2 );
	command.SetAlways( bAlways );
	command.SetBackSide( bBackSide );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetSideFlags( ULONG ulSide, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSide >= (ULONG)numsides )
		return;

	NetCommand command ( SVC_SETSIDEFLAGS );
	command.addLong ( ulSide );
	command.addByte ( sides[ulSide].Flags );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Sound( LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SOUND );
	command.addByte ( lChannel );
	command.addString ( pszSound );
	command.addByte ( LONG ( clamp( fVolume, 0.0f, 2.0f ) * 127 ) );
	command.addByte ( NETWORK_AttenuationFloatToInt ( fAttenuation ) );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundActor( AActor *pActor, LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra, ServerCommandFlags flags, bool bRespectActorPlayingSomething )
{
	if ( pActor == NULL )
		return;

	// [BB] If the actor doesn't have a NetID, we have to instruct the clients differently how to play the sound.
	if ( pActor->lNetID == -1 )
	{
		SERVERCOMMANDS_SoundPoint( pActor->x, pActor->y, pActor->z, lChannel, pszSound, fVolume, fAttenuation, ulPlayerExtra, flags );
		return;
	}

	NetCommand command ( bRespectActorPlayingSomething ? SVC_SOUNDACTORIFNOTPLAYING : SVC_SOUNDACTOR );
	command.addShort( pActor->lNetID );
	command.addShort( lChannel );
	command.addString( pszSound );
	command.addByte( LONG ( clamp( fVolume, 0.0f, 2.0f ) * 127 ) );
	command.addByte( NETWORK_AttenuationFloatToInt ( fAttenuation ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundSector( sector_t *sector, int channel, const char *sound, float volume, float attenuation, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( sector == NULL )
		return;

	int sectorID = sector - sectors;
	if (( sectorID < 0 ) || ( sectorID >= numsectors ))
		return;

	NetCommand command( SVC2_SOUNDSECTOR );
	command.addShort( sectorID );
	command.addShort( channel );
	command.addString( sound );
	command.addByte( LONG ( clamp( volume, 0.0f, 2.0f ) * 127 ) );
	command.addByte( NETWORK_AttenuationFloatToInt ( attenuation ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundPoint( LONG lX, LONG lY, LONG lZ, LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SOUNDPOINT );
	command.addShort ( lX>>FRACBITS );
	command.addShort ( lY>>FRACBITS );
	command.addShort ( lZ>>FRACBITS );
	command.addByte ( lChannel );
	command.addString ( pszSound );
	command.addByte ( LONG ( clamp( fVolume, 0.0f, 2.0f ) * 127 ) );
	command.addByte ( NETWORK_AttenuationFloatToInt ( fAttenuation ) );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_AnnouncerSound( const char *pszSound, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_ANNOUNCERSOUND );
	command.addString ( pszSound );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_StartSectorSequence( sector_t *pSector, const int Channel, const char *pszSequence, const int Modenum, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	const LONG lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	NetCommand command ( SVC_STARTSECTORSEQUENCE );
	command.addShort ( lSectorID );
	command.addByte ( Channel );
	command.addString ( pszSequence );
	command.addByte ( Modenum );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_StopSectorSequence( sector_t *pSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	NetCommand command ( SVC_STOPSECTORSEQUENCE );
	command.addShort ( lSectorID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_CallVote( ULONG ulPlayer, FString Command, FString Parameters, FString Reason, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command ( SVC_CALLVOTE );
	command.addByte ( ulPlayer );
	command.addString ( Command.GetChars() );
	command.addString ( Parameters.GetChars() );
	command.addString ( Reason.GetChars() );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerVote( ULONG ulPlayer, bool bVoteYes, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command ( SVC_PLAYERVOTE );
	command.addByte ( ulPlayer );
	command.addByte ( bVoteYes );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_VoteEnded( bool bVotePassed, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_VOTEENDED );
	command.addByte ( bVotePassed );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_MapLoad( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_MAPLOAD );
	command.addString ( level.mapname );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapNew( const char *pszMapName, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_MAPNEW );
	command.addString ( pszMapName );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapExit( LONG lPosition, const char *pszNextMap, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pszNextMap == NULL )
		return;

	NetCommand command ( SVC_MAPEXIT );
	command.addByte ( lPosition );
	command.addString ( pszNextMap );
	command.sendCommandToClients ( ulPlayerExtra, flags );

	// [BB] The clients who are authenticated, but still didn't finish loading
	// the map are not covered by the code above and need special treatment.
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_GetClient( ulIdx )->State == CLS_AUTHENTICATED )
			SERVER_GetClient( ulIdx )->State = CLS_AUTHENTICATED_BUT_OUTDATED_MAP;
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapAuthenticate( const char *pszMapName, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_MAPAUTHENTICATE );
	command.addString ( pszMapName );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapTime( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETMAPTIME );
	command.addLong ( level.time );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumKilledMonsters( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETMAPNUMKILLEDMONSTERS );
	command.addShort ( level.killed_monsters );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumFoundItems( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETMAPNUMFOUNDITEMS );
	command.addShort ( level.found_items );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumFoundSecrets( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETMAPNUMFOUNDSECRETS );
	command.addShort ( level.found_secrets );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumTotalMonsters( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETMAPNUMTOTALMONSTERS );
	command.addShort ( level.total_monsters );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumTotalItems( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETMAPNUMTOTALITEMS );
	command.addShort ( level.total_items );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumTotalSecrets( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC2_SETMAPNUMTOTALSECRETS );
	command.addShort ( level.total_secrets );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapMusic( const char *pszMusic, int track, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETMAPMUSIC );
	command.addString ( pszMusic );
	command.addByte ( track );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapSky( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETMAPSKY );
	command.addString ( level.skypic1 );
	command.addString ( level.skypic2 );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_GiveInventory( ULONG ulPlayer, AInventory *pInventory, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pInventory == NULL )
		return;

	if ( pInventory->ulNetworkFlags & NETFL_SERVERSIDEONLY )
		return;

	NetCommand command ( SVC_GIVEINVENTORY );
	command.addByte ( ulPlayer );
	command.addShort (  pInventory->GetClass()->getActorNetworkIndex() );
	command.addLong ( pInventory->Amount );
	command.sendCommandToClients ( ulPlayerExtra, flags );

	// [BB] Clients don't know that a BackpackItem may be depleted. In this case we have to resync the ammo count.
	if ( pInventory->IsKindOf (RUNTIME_CLASS(ABackpackItem)) && static_cast<ABackpackItem*> ( pInventory )->bDepleted )
		SERVERCOMMANDS_SyncPlayerAmmoAmount( ulPlayer, ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_GiveInventoryNotOverwritingAmount( AActor *pReceiver, AInventory *pItem )
{
	if( (pItem == NULL) || (pReceiver == NULL) || (pReceiver->player == NULL) )
		return;
	
	// [BB] Since SERVERCOMMANDS_GiveInventory overwrites the item amount
	// of the client with pItem->Amount, we have have to set this to the
	// correct amount the player has.
	AInventory *pInventory = NULL;
	if( pReceiver->player && pReceiver->player->mo )
		pInventory = pReceiver->player->mo->FindInventory( pItem->GetClass() );
	if ( pInventory != NULL )
		pItem->Amount = pInventory->Amount;

	SERVERCOMMANDS_GiveInventory( ULONG( pReceiver->player - players ), pItem );
	// [BB] The armor display amount has to be updated separately.
	if( pItem->GetClass()->IsDescendantOf (RUNTIME_CLASS(AArmor)))
		SERVERCOMMANDS_SetPlayerArmor( ULONG( pReceiver->player - players ));
}
//*****************************************************************************
//
void SERVERCOMMANDS_TakeInventory( ULONG ulPlayer, const PClass *inventoryClass, ULONG ulAmount, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( inventoryClass == NULL || ( GetDefaultByType ( inventoryClass )->ulNetworkFlags & NETFL_SERVERSIDEONLY ) )
		return;

	NetCommand command ( SVC_TAKEINVENTORY );
	command.addByte ( ulPlayer );
	command.addShort ( inventoryClass->getActorNetworkIndex() );
	command.addLong ( ulAmount );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_GivePowerup( ULONG ulPlayer, APowerup *pPowerup, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pPowerup == NULL )
		return;

	NetCommand command ( SVC_GIVEPOWERUP );
	command.addByte( ulPlayer );
	command.addShort( pPowerup->GetClass( )->getActorNetworkIndex() );
	// Can we have multiple amounts of a powerup? Probably not, but I'll be safe for now.
	command.addShort( pPowerup->Amount );
	command.addByte( pPowerup->IsActiveRune() );

	if ( pPowerup->IsActiveRune() == false )
		command.addShort( pPowerup->EffectTics );

	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
// [WS]
void SERVERCOMMANDS_SetPowerupBlendColor( ULONG ulPlayer, APowerup *pPowerup, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pPowerup == NULL )
		return;

	NetCommand command( SVC2_SETPOWERUPBLENDCOLOR );
	command.addByte( ulPlayer );
	command.addShort( pPowerup->GetClass( )->getActorNetworkIndex() );
	command.addLong( pPowerup->BlendColor );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_GiveWeaponHolder( ULONG ulPlayer, AWeaponHolder *pHolder, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pHolder == NULL )
		return;

	if ( pHolder->ulNetworkFlags & NETFL_SERVERSIDEONLY )
		return;

	NetCommand command( SVC2_GIVEWEAPONHOLDER );
	command.addByte( ulPlayer );
	command.addShort( pHolder->PieceMask );
	command.addShort( pHolder->PieceWeapon->getActorNetworkIndex() );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoInventoryPickup( ULONG ulPlayer, const char *pszClassName, const char *pszPickupMessage, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lLength;

	lLength = 0;
	if ( pszClassName )
		lLength += (LONG)strlen( pszClassName );
	if ( pszPickupMessage )
		lLength += (LONG)strlen( pszPickupMessage );

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command ( SVC_DOINVENTORYPICKUP );
	command.addByte ( ulPlayer );
	command.addString ( pszClassName );
	command.addString ( pszPickupMessage );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyAllInventory( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DESTROYALLINVENTORY );
	command.addByte ( ulPlayer );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetInventoryIcon( ULONG ulPlayer, AInventory *pInventory, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ( pInventory == NULL ) || ( pInventory->Icon.isValid() == false ) )
		return;

	FString iconTexName = TexMan( pInventory->Icon )->Name;

	NetCommand command ( SVC2_SETINVENTORYICON );
	command.addByte ( ulPlayer );
	command.addShort ( pInventory->GetClass()->getActorNetworkIndex() );
	command.addString ( iconTexName.GetChars() );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_SetHexenArmorSlots( ULONG ulPlayer, AHexenArmor *aHXArmor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( aHXArmor == NULL )
		return;

	NetCommand command( SVC2_SETHEXENARMORSLOTS );
	command.addByte( ulPlayer );
	for (int i = 0; i <= 4; i++)
		command.addLong( aHXArmor->Slots[i] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
void SERVERCOMMANDS_SyncHexenArmorSlots( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	AHexenArmor *aHXArmor = static_cast<AHexenArmor *>( players[ulPlayer].mo->FindInventory( RUNTIME_CLASS( AHexenArmor )));
	SERVERCOMMANDS_SetHexenArmorSlots( ulPlayer, aHXArmor, ulPlayerExtra, flags );
}

//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_SetFastChaseStrafeCount( AActor *mobj, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (mobj) )
		return;

	NetCommand command( SVC2_SETFASTCHASESTRAFECOUNT );
	command.addShort( mobj->lNetID );
	command.addByte( mobj->FastChaseStrafeCount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
// [Dusk] This function is called to set an actor's health directly on the
// client. I don't expect many things to call it (it was created for the sake
// of syncing hellstaff rain health fields) so it's an extended command for now
// instead of a regular one, despite its genericness.
//
void SERVERCOMMANDS_SetThingHealth( AActor* mobj, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (mobj) )
		return;

	NetCommand command( SVC2_SETTHINGHEALTH );
	command.addShort( mobj->lNetID );
	command.addByte( mobj->health );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingScale( AActor* mobj, unsigned int scaleFlags, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (mobj) )
		return;

	if ( scaleFlags == 0 )
		return;

	NetCommand command( SVC2_SETTHINGSCALE );
	command.addShort( mobj->lNetID );
	command.addByte( scaleFlags );
	if ( scaleFlags & ACTORSCALE_X )
		command.addLong( mobj->scaleX );
	if ( scaleFlags & ACTORSCALE_Y )
		command.addLong( mobj->scaleY );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdateThingScaleNotAtDefault( AActor* pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// [BB] Sanity check.
	if ( pActor == NULL )
		return;

	unsigned int scaleFlags = 0;
	if ( pActor->scaleX != pActor->GetDefault()->scaleX )
		scaleFlags |= ACTORSCALE_X;
	if ( pActor->scaleY != pActor->GetDefault()->scaleY )
		scaleFlags |= ACTORSCALE_Y;

	if ( scaleFlags != 0 )
		SERVERCOMMANDS_SetThingScale( pActor, scaleFlags, ulPlayerExtra, flags  );
}

//*****************************************************************************
//
void SERVERCOMMANDS_FlashStealthMonster( AActor* pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( EnsureActorHasNetID( pActor ) == false )
		return;

	NetCommand command ( SVC2_FLASHSTEALTHMONSTER );
	command.addShort( pActor->lNetID );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_FullUpdateCompleted( ULONG ulClient )
{
	NetCommand command ( SVC2_FULLUPDATECOMPLETED );
	command.sendCommandToOneClient( ulClient );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ResetMap( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC2_RESETMAP );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetIgnoreWeaponSelect( ULONG ulClient, const bool bIgnoreWeaponSelect )
{
	NetCommand command ( SVC2_SETIGNOREWEAPONSELECT );
	command.addByte ( bIgnoreWeaponSelect );
	command.sendCommandToOneClient( ulClient );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ClearConsoleplayerWeapon( ULONG ulClient )
{
	NetCommand command ( SVC2_CLEARCONSOLEPLAYERWEAPON );
	command.sendCommandToOneClient( ulClient );
}

//*****************************************************************************
//
void SERVERCOMMANDS_Lightning( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC2_LIGHTNING );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_CancelFade( const ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC2_CANCELFADE );
	command.addByte ( ulPlayer );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayBounceSound( const AActor *pActor, const bool bOnfloor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	NetCommand command ( SVC2_PLAYBOUNCESOUND );
	command.addShort ( pActor->lNetID );
	command.addByte ( bOnfloor );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoDoor( sector_t *pSector, DDoor::EVlDoor type, LONG lSpeed, LONG lDirection, LONG lLightTag, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustDoorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	NetCommand command ( SVC_DODOOR );
	command.addShort ( lSectorID );
	command.addByte ( (BYTE)type );
	command.addLong ( lSpeed );
	command.addByte ( lDirection );
	command.addShort ( lLightTag );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyDoor( LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DESTROYDOOR );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeDoorDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustDoorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	NetCommand command ( SVC_CHANGEDOORDIRECTION );
	command.addShort ( lID );
	command.addByte ( lDirection );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoFloor( DFloor::EFloor Type, sector_t *pSector, LONG lDirection, LONG lSpeed, LONG lFloorDestDist, LONG Crush, bool Hexencrush, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustFloorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	NetCommand command ( SVC_DOFLOOR );
	command.addByte ( (ULONG)Type );
	command.addShort ( lSectorID );
	command.addByte ( lDirection );
	command.addLong ( lSpeed );
	command.addLong ( lFloorDestDist );
	command.addByte ( clamp<LONG>(Crush,-128,127) );
	command.addByte ( Hexencrush );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyFloor( LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DESTROYFLOOR );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeFloorDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustFloorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	NetCommand command ( SVC_CHANGEFLOORDIRECTION );
	command.addShort ( lID );
	command.addByte ( lDirection );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeFloorType( LONG lID, DFloor::EFloor Type, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_CHANGEFLOORTYPE );
	command.addShort ( lID );
	command.addByte ( (ULONG)Type );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeFloorDestDist( LONG lID, LONG lFloorDestDist, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_CHANGEFLOORDESTDIST );
	command.addShort ( lID );
	command.addLong ( lFloorDestDist );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_StartFloorSound( LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_STARTFLOORSOUND );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_BuildStair( DFloor::EFloor Type, sector_t *pSector, int Direction, fixed_t Speed, fixed_t FloorDestDist, int Crush, bool Hexencrush, int ResetCount, int Delay, int PauseTime, int StepTime, int PerStepTime, int ID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	int		SectorID;

	SectorID = int( pSector - sectors );
	if (( SectorID < 0 ) || ( SectorID >= numsectors ))
		return;

	NetCommand command ( SVC2_BUILDSTAIR );
	command.addByte ( (int)Type );
	command.addShort ( SectorID );
	command.addByte ( clamp(Direction,-128,127) );
	command.addLong ( Speed );
	command.addLong ( FloorDestDist );
	command.addByte ( clamp(Crush,-128,127) );
	command.addByte ( Hexencrush );
	command.addLong ( ResetCount );
	command.addLong ( Delay );
	command.addLong ( PauseTime );
	command.addLong ( StepTime );
	command.addLong ( PerStepTime );
	command.addShort ( ID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoCeiling( DCeiling::ECeiling Type, sector_t *pSector, LONG lDirection, LONG lBottomHeight, LONG lTopHeight, LONG lSpeed, LONG lCrush, bool Hexencrush, LONG lSilent, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustCeilingDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	NetCommand command ( SVC_DOCEILING );
	command.addByte ( (ULONG)Type );
	command.addShort ( lSectorID );
	command.addByte ( lDirection );
	command.addLong ( lBottomHeight );
	command.addLong ( lTopHeight );
	command.addLong ( lSpeed );
	command.addByte ( clamp<LONG>(lCrush,-128,127) );
	command.addByte ( Hexencrush );
	command.addShort ( lSilent );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyCeiling( LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DESTROYCEILING );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeCeilingDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustCeilingDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	NetCommand command ( SVC_CHANGECEILINGDIRECTION );
	command.addShort ( lID );
	command.addByte ( lDirection );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangeCeilingSpeed( LONG lID, LONG lSpeed, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_CHANGECEILINGSPEED );
	command.addShort ( lID );
	command.addLong ( lSpeed );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayCeilingSound( LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_PLAYCEILINGSOUND );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoPlat( DPlat::EPlatType Type, sector_t *pSector, DPlat::EPlatState Status, LONG lHigh, LONG lLow, LONG lSpeed, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	NetCommand command ( SVC_DOPLAT );
	command.addByte ( (ULONG)Type );
	command.addShort ( lSectorID );
	command.addByte ( (ULONG)Status );
	command.addLong ( lHigh );
	command.addLong ( lLow );
	command.addLong ( lSpeed );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyPlat( LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DESTROYPLAT );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ChangePlatStatus( LONG lID, DPlat::EPlatState Status, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_CHANGEPLATSTATUS );
	command.addShort ( lID );
	command.addByte ( (ULONG)Status );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayPlatSound( LONG lID, LONG lSound, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_PLAYPLATSOUND );
	command.addShort ( lID );
	command.addByte ( lSound );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoElevator( DElevator::EElevator Type, sector_t *pSector, LONG lSpeed, LONG lDirection, LONG lFloorDestDist, LONG lCeilingDestDist, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	// Since we still want to send direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = SERVER_AdjustElevatorDirection( lDirection );
	if ( lDirection == INT_MIN )
		return;

	NetCommand command ( SVC_DOELEVATOR );
	command.addByte ( Type );
	command.addShort ( lSectorID );
	command.addLong ( lSpeed );
	command.addByte ( lDirection );
	command.addLong ( lFloorDestDist );
	command.addLong ( lCeilingDestDist );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyElevator( LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DESTROYELEVATOR );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_StartElevatorSound( LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_STARTELEVATORSOUND );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoPillar( DPillar::EPillar Type, sector_t *pSector, LONG lFloorSpeed, LONG lCeilingSpeed, LONG lFloorTarget, LONG lCeilingTarget, LONG Crush, bool Hexencrush, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	NetCommand command ( SVC_DOPILLAR );
	command.addByte ( Type );
	command.addShort ( lSectorID );
	command.addLong ( lFloorSpeed );
	command.addLong ( lCeilingSpeed );
	command.addLong ( lFloorTarget );
	command.addLong ( lCeilingTarget );
	command.addByte ( clamp<LONG>(Crush,-128,127) );
	command.addByte ( Hexencrush );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyPillar( LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DESTROYPILLAR );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoWaggle( bool bCeiling, sector_t *pSector, LONG lOriginalDistance, LONG lAccumulator, LONG lAccelerationDelta, LONG lTargetScale, LONG lScale, LONG lScaleDelta, LONG lTicker, LONG lState, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID;

	lSectorID = LONG( pSector - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	NetCommand command ( SVC_DOWAGGLE );
	command.addByte ( !!bCeiling );
	command.addShort ( lSectorID );
	command.addLong ( lOriginalDistance );
	command.addLong ( lAccumulator );
	command.addLong ( lAccelerationDelta );
	command.addLong ( lTargetScale );
	command.addLong ( lScale );
	command.addLong ( lScaleDelta );
	command.addLong ( lTicker );
	command.addByte ( lState );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyWaggle( LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DESTROYWAGGLE );
	command.addShort ( lID );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdateWaggle( LONG lID, LONG lAccumulator, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC_UPDATEWAGGLE );
	command.setUnreliable( true );
	command.addShort( lID );
	command.addLong( lAccumulator );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoRotatePoly( LONG lSpeed, LONG lPolyNum, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DOROTATEPOLY );
	command.addLong ( lSpeed );
	command.addShort ( lPolyNum );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyRotatePoly( LONG lPolyNum, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DESTROYROTATEPOLY );
	command.addShort ( lPolyNum );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoMovePoly( LONG lXSpeed, LONG lYSpeed, LONG lPolyNum, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DOMOVEPOLY );
	command.addLong ( lXSpeed );
	command.addLong ( lYSpeed );
	command.addShort ( lPolyNum );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyMovePoly( LONG lPolyNum, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DESTROYMOVEPOLY );
	command.addShort ( lPolyNum );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoPolyDoor( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lSpeed, LONG lPolyNum, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DOPOLYDOOR );
	command.addByte ( lType );
	command.addLong ( lXSpeed );
	command.addLong ( lYSpeed );
	command.addLong ( lSpeed );
	command.addShort ( lPolyNum );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyPolyDoor( LONG lPolyNum, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DESTROYPOLYDOOR );
	command.addShort ( lPolyNum );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPolyDoorSpeedPosition( LONG lPolyNum, LONG lXSpeed, LONG lYSpeed, LONG lX, LONG lY, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETPOLYDOORSPEEDPOSITION );
	command.addShort ( lPolyNum );
	command.addLong ( lXSpeed );
	command.addLong ( lYSpeed );
	command.addLong ( lX );
	command.addLong ( lY );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPolyDoorSpeedRotation( LONG lPolyNum, LONG lSpeed, LONG lAngle, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETPOLYDOORSPEEDROTATION );
	command.addShort ( lPolyNum );
	command.addLong ( lSpeed );
	command.addLong ( lAngle );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayPolyobjSound( LONG lPolyNum, bool PolyMode, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_PLAYPOLYOBJSOUND );
	command.addShort ( lPolyNum );
	command.addByte ( PolyMode );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_StopPolyobjSound( LONG lPolyNum, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC2_STOPPOLYOBJSOUND );
	command.addShort ( lPolyNum );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPolyobjPosition( LONG lPolyNum, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	FPolyObj	*pPoly;

	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
		return;

	NetCommand command ( SVC_SETPOLYOBJPOSITION );
	command.addShort ( lPolyNum );
	command.addLong ( pPoly->StartSpot.x );
	command.addLong ( pPoly->StartSpot.y );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPolyobjRotation( LONG lPolyNum, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	FPolyObj	*pPoly;

	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
		return;

	NetCommand command ( SVC_SETPOLYOBJROTATION );
	command.addShort ( lPolyNum );
	command.addLong ( pPoly->angle );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Earthquake( AActor *pCenter, LONG lIntensity, LONG lDuration, LONG lTemorRadius, FSoundID Quakesound, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pCenter) )
		return;

	const char *pszQuakeSound = S_GetName( Quakesound );

	NetCommand command ( SVC_EARTHQUAKE );
	command.addShort ( pCenter->lNetID );
	command.addByte ( lIntensity );
	command.addShort ( lDuration );
	command.addShort ( lTemorRadius );
	command.addString ( pszQuakeSound );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SyncJoinQueue( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SyncJoinQueue command;
	TArray<ServerCommands::JoinSlot> joinSlots;
	joinSlots.Reserve( JOINQUEUE_GetSize() );

	for ( unsigned int i = 0; i < JOINQUEUE_GetSize(); ++i )
	{
		joinSlots[i].player = JOINQUEUE_GetSlotAt( i ).player;
		joinSlots[i].team = JOINQUEUE_GetSlotAt( i ).team;
	}

	command.SetSlots( joinSlots );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PushToJoinQueue( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( JOINQUEUE_GetSize() > 0 )
	{
		const JoinSlot& slot = JOINQUEUE_GetSlotAt( JOINQUEUE_GetSize() - 1 );
		NetCommand command ( SVC2_PUSHTOJOINQUEUE );
		command.addByte( slot.player );
		command.addByte( slot.team );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_RemoveFromJoinQueue( unsigned int index, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC2_REMOVEFROMJOINQUEUE );
	command.addByte( index );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoScroller( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lAffectee, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DOSCROLLER );
	command.addByte ( lType );
	command.addLong ( lXSpeed );
	command.addLong ( lYSpeed );
	command.addLong ( lAffectee );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetScroller( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lTag, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETSCROLLER );
	command.addByte ( lType );
	command.addLong ( lXSpeed );
	command.addLong ( lYSpeed );
	command.addShort ( lTag );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetWallScroller( LONG lId, LONG lSidechoice, LONG lXSpeed, LONG lYSpeed, LONG lWhere, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_SETWALLSCROLLER );
	command.addLong ( lId );
	command.addByte ( lSidechoice );
	command.addLong ( lXSpeed );
	command.addLong ( lYSpeed );
	command.addLong ( lWhere );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoFlashFader( float fR1, float fG1, float fB1, float fA1, float fR2, float fG2, float fB2, float fA2, float fTime, ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_DOFLASHFADER );
	command.addFloat ( fR1 );
	command.addFloat ( fG1 );
	command.addFloat ( fB1 );
	command.addFloat ( fA1 );
	command.addFloat ( fR2 );
	command.addFloat ( fG2 );
	command.addFloat ( fB2 );
	command.addFloat ( fA2 );
	command.addFloat ( fTime );
	command.addByte ( ulPlayer );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_GenericCheat( ULONG ulPlayer, ULONG ulCheat, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command ( SVC_GENERICCHEAT );
	command.addByte ( ulPlayer );
	command.addByte ( ulCheat );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetCameraToTexture( AActor *pCamera, char *pszTexture, LONG lFOV, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ((!EnsureActorHasNetID (pCamera) ) ||
		( pszTexture == NULL ))
	{
		return;
	}

	NetCommand command ( SVC_SETCAMERATOTEXTURE );
	command.addShort ( pCamera->lNetID );
	command.addString ( pszTexture );
	command.addByte ( lFOV );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_CreateTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulPal1, ULONG ulPal2, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	const bool bIsEdited = SERVER_IsTranslationEdited ( ulTranslation );

	NetCommand command ( SVC_CREATETRANSLATION );
	command.addShort ( ulTranslation );
	command.addByte ( bIsEdited );
	command.addByte ( ulStart );
	command.addByte ( ulEnd );
	command.addByte ( ulPal1 );
	command.addByte ( ulPal2 );
	command.sendCommandToClients ( ulPlayerExtra, flags );

}
//*****************************************************************************
//
void SERVERCOMMANDS_CreateTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulR1, ULONG ulG1, ULONG ulB1, ULONG ulR2, ULONG ulG2, ULONG ulB2, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	const bool bIsEdited = SERVER_IsTranslationEdited ( ulTranslation );

	NetCommand command ( SVC_CREATETRANSLATION2 );
	command.addShort ( ulTranslation );
	command.addByte ( bIsEdited );
	command.addByte ( ulStart );
	command.addByte ( ulEnd );
	command.addByte ( ulR1 );
	command.addByte ( ulG1 );
	command.addByte ( ulB1 );
	command.addByte ( ulR2 );
	command.addByte ( ulG2 );
	command.addByte ( ulB2 );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ReplaceTextures( int iFromname, int iToname, int iTexFlags, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::ReplaceTextures command;
	command.SetFromTexture( iFromname );
	command.SetToTexture( iToname );
	command.SetTextureFlags( iTexFlags );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorLink( ULONG ulSector, int iArg1, int iArg2, int iArg3, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetSectorLink command;
	command.SetSector( &sectors[ulSector] );
	command.SetTag( iArg1 );
	command.SetCeiling( iArg2 );
	command.SetMoveType( iArg3 );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoPusher( ULONG ulType, line_t *pLine, int iMagnitude, int iAngle, AActor *pSource, int iAffectee, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	const int iLineNum = pLine ? static_cast<ULONG>( pLine - lines ) : -1;
	const LONG lSourceNetID = pSource ? pSource->lNetID : -1;

	NetCommand command ( SVC_DOPUSHER );
	command.addByte ( ulType );
	command.addShort ( iLineNum );
	command.addLong ( iMagnitude );
	command.addLong ( iAngle );
	command.addShort ( lSourceNetID );
	command.addShort ( iAffectee );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_AdjustPusher( int iTag, int iMagnitude, int iAngle, ULONG ulType, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC_ADJUSTPUSHER );
	command.addShort ( iTag );
	command.addLong ( iMagnitude );
	command.addLong ( iAngle );
	command.addByte ( ulType );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}


//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_SetPlayerHazardCount ( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command ( SVC2_SETPLAYERHAZARDCOUNT );
	command.addByte ( ulPlayer );
	command.addShort ( players[ulPlayer].hazardcount );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
// [EP] All the clients must change their own mugshot state.
void SERVERCOMMANDS_SetMugShotState ( const char *statename )
{
	NetCommand command( SVC2_SETMUGSHOTSTATE );
	command.addString( statename );
	command.sendCommandToClients();
}

//*****************************************************************************
// [Dusk] Used in map resets to move a 3d midtexture moves without sector it's attached to.
void SERVERCOMMANDS_Scroll3dMidtexture ( sector_t* sector, fixed_t move, bool ceiling, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command ( SVC2_SCROLL3DMIDTEX );
	command.addByte ( sector - sectors );
	command.addLong ( move );
	command.addByte ( ceiling );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
void SERVERCOMMANDS_SetPlayerLogNumber ( const ULONG ulPlayer, const int Arg0, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	NetCommand command ( SVC2_SETPLAYERLOGNUMBER );
	command.addByte ( ulPlayer );
	command.addShort ( Arg0 );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetCVar( const FBaseCVar &CVar, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC2_SETCVAR );
	command.addString( CVar.GetName() );
	command.addString( CVar.GetGenericRep (CVAR_String).String );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDefaultSkybox( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	NetCommand command( SVC2_SETDEFAULTSKYBOX );
	command.addShort( ( level.DefaultSkybox != NULL ) ? level.DefaultSkybox->lNetID : -1 );
	command.sendCommandToClients( ulPlayerExtra, flags );
}
//*****************************************************************************
void SERVERCOMMANDS_SRPUserStartAuthentication ( const ULONG ulClient )
{
	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	CLIENT_s *pClient = SERVER_GetClient ( ulClient );
	NetCommand command ( SVC2_SRP_USER_START_AUTHENTICATION );
	command.addString ( pClient->username.GetChars() );
	command.sendCommandToClients ( ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
void SERVERCOMMANDS_SRPUserProcessChallenge ( const ULONG ulClient )
{
	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	CLIENT_s *pClient = SERVER_GetClient ( ulClient );
	NetCommand command ( SVC2_SRP_USER_PROCESS_CHALLENGE );
	command.addByte ( pClient->salt.Size() );
	for ( unsigned int i = 0; i < pClient->salt.Size(); ++i )
		command.addByte ( pClient->salt[i] );
	command.addShort ( pClient->bytesB.Size() );
	for ( unsigned int i = 0; i < pClient->bytesB.Size(); ++i )
		command.addByte ( pClient->bytesB[i] );
	command.sendCommandToClients ( ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
void SERVERCOMMANDS_SRPUserVerifySession ( const ULONG ulClient )
{
	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	CLIENT_s *pClient = SERVER_GetClient ( ulClient );
	NetCommand command ( SVC2_SRP_USER_VERIFY_SESSION );
	command.addShort ( pClient->bytesHAMK.Size() );
	for ( unsigned int i = 0; i < pClient->bytesHAMK.Size(); ++i )
		command.addByte ( pClient->bytesHAMK[i] );
	command.sendCommandToClients ( ulClient, SVCF_ONLYTHISCLIENT );
}

 //*****************************************************************************
void SERVERCOMMANDS_ShootDecal ( const FDecalTemplate* tpl, AActor* actor, fixed_t z, angle_t angle, fixed_t tracedist,
	bool permanent, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( EnsureActorHasNetID( actor ) == false )
		return;

	NetCommand command ( SVC2_SHOOTDECAL );
	command.addName( tpl->GetName() );
	command.addShort( actor->lNetID );
	command.addShort( z >> FRACBITS );
	command.addShort( angle >> FRACBITS );
	command.addLong( tracedist );
	command.addByte( permanent );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
void APathFollower::SyncWithClient ( const ULONG ulClient )
{
	if ( !EnsureActorHasNetID (this) )
		return;

	NetCommand command( SVC2_SYNCPATHFOLLOWER );
	command.addShort( this->lNetID );
	command.addShort( this->CurrNode ? this->CurrNode->lNetID : -1 );
	command.addShort( this->PrevNode ? this->PrevNode->lNetID : -1 );
	command.addFloat( this->Time );
	command.sendCommandToOneClient( ulClient );
}
