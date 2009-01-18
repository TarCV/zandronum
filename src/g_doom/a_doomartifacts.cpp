#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_local.h"
#include "gi.h"
#include "gstrings.h"
#include "s_sound.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "p_effect.h"
#include "cl_demo.h"
#include "a_doomglobal.h"
#include "announcer.h"

// [BC] Random Powerup ------------------------------------------------------

#define	RPF_MEGASPHERE				1
#define	RPF_SOULSPHERE				2
#define	RPF_GUARDSPHERE				4
#define	RPF_PARTIALINVISIBILITY		8
#define	RPF_TIMEFREEZESPHERE		16
#define	RPF_INVISIBILITY			32
#define	RPF_DOOMSPHERE				64
#define	RPF_TURBOSPHERE				128
#define	RPF_ALL						( RPF_MEGASPHERE|RPF_SOULSPHERE|RPF_GUARDSPHERE| \
									  RPF_PARTIALINVISIBILITY|RPF_TIMEFREEZESPHERE|RPF_INVISIBILITY | \
									  RPF_DOOMSPHERE|RPF_TURBOSPHERE )

class ARandomPowerup : public AInventory
{
	DECLARE_ACTOR (ARandomPowerup, AInventory)
public:
	virtual bool	Use (bool pickup);
	void			Serialize( FArchive &arc );

	void	PostBeginPlay( )
	{
		Super::PostBeginPlay( );

		ulPowerupFlags = args[0];
		if ( ulPowerupFlags == 0 )
			ulPowerupFlags |= RPF_ALL;
	}

	bool	IsFrameAllowed( ULONG ulFrame )
	{
		// No time freeze spheres in multiplayer games.
		if (( ulFrame == 4 ) && ( NETWORK_GetState( ) != NETSTATE_SINGLE ))
			return ( false );

		return !!( ulPowerupFlags & ( 1 << ulFrame ));
	}

	ULONG	ulCurrentFrame;

protected:
	const char *PickupMessage ();

	ULONG	ulPowerupFlags;
};

void A_RandomPowerupFrame( AActor *pActor )
{
	ULONG	ulFrame;
	ARandomPowerup	*pRandomPowerup;

	pRandomPowerup = static_cast<ARandomPowerup *>( pActor );

	ulFrame = (ULONG)( pActor->state - pActor->SpawnState );
	if ( pRandomPowerup->IsFrameAllowed( ulFrame ) == false )
		pActor->SetState( pActor->state->NextState );
	else
		pRandomPowerup->ulCurrentFrame = ulFrame;
}

FState ARandomPowerup::States[] =
{
	S_BRIGHT (MEGA, 'A',	6, A_RandomPowerupFrame			, &States[1]),
	S_BRIGHT (SOUL, 'A',	6, A_RandomPowerupFrame			, &States[2]),
	S_BRIGHT (GARD, 'A',	6, A_RandomPowerupFrame			, &States[3]),
	S_BRIGHT (PINS, 'A',	6, A_RandomPowerupFrame			, &States[4]),
	S_BRIGHT (TIME, 'A',	6, A_RandomPowerupFrame			, &States[5]),
	S_BRIGHT (SINV, 'A',	6, A_RandomPowerupFrame			, &States[6]),
	S_BRIGHT (DOOM, 'A',	6, A_RandomPowerupFrame			, &States[7]),
	S_BRIGHT (TURB, 'A',	6, A_RandomPowerupFrame			, &States[0]),
};

IMPLEMENT_ACTOR (ARandomPowerup, Doom, 5039, 176)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM|MF_NOGRAVITY)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_ALWAYSPICKUP)
	PROP_Inventory_MaxAmount (0)
	PROP_FlagsNetwork( NETFL_UPDATEARGUMENTS|NETFL_SPECIALPICKUP )
	PROP_RenderFlags( RF_RANDOMPOWERUPHACK )
	PROP_SpawnState (0)
END_DEFAULTS

bool ARandomPowerup::Use (bool pickup)
{
	AInventory		*pItem;
	const PClass	*pType = NULL;
	bool			bReturnValue = false;

	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return ( false );
	}

	switch ( ulCurrentFrame )
	{
	// Megasphere.
	case 0:

		pType = PClass::FindClass( "Megasphere" );
		break;
	// Soulsphere.
	case 1:

		pType = PClass::FindClass( "Soulsphere" );
		break;
	// Guardsphere.
	case 2:

		pType = PClass::FindClass( "Guardsphere" );
		break;
	// Partial invisibility.
	case 3:

		pType = PClass::FindClass( "Blursphere" );
		break;
	// Time freeze.
	case 4:

		pType = PClass::FindClass( "TimeFreezeSphere" );
		break;
	// Invisibility.
	case 5:

		pType = PClass::FindClass( "InvisibilitySphere" );
		break;
	// Doomsphere.
	case 6:

		pType = PClass::FindClass( "Doomsphere" );
		break;
	// Turbosphere.
	case 7:

		pType = PClass::FindClass( "Turbosphere" );
		break;
	}

	pItem = static_cast<AInventory *>( Spawn( pType, Owner->x, Owner->y, Owner->z, ALLOW_REPLACE ));
	if ( pItem != NULL )
	{
		bReturnValue = pItem->TryPickup( Owner );
		if ( bReturnValue )
		{
			// [BC] If the item has an announcer sound, play it.
			if ( Owner->CheckLocalView( consoleplayer ))
				ANNOUNCER_PlayEntry( cl_announcer, pItem->PickupAnnouncerEntry( ));

			// [BC] Tell the client that he successfully picked up the item.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
				( Owner->player ) &&
				( this->ulNetworkFlags & NETFL_SPECIALPICKUP ))
			{
				SERVERCOMMANDS_GiveInventory( ULONG( Owner->player - players ), pItem );

				if (( ItemFlags & IF_QUIET ) == false )
					SERVERCOMMANDS_DoInventoryPickup( ULONG( Owner->player - players ), pItem->GetClass( )->TypeName.GetChars( ), pItem->PickupMessage( ));
			}
		}

		if (( pItem->ObjectFlags & OF_EuthanizeMe ) == false )
			pItem->Destroy( );
	}

	return ( bReturnValue );
}

void ARandomPowerup::Serialize( FArchive &arc )
{
	Super::Serialize( arc );
	arc << (DWORD &)ulCurrentFrame << (DWORD &)ulPowerupFlags;
}

const char *ARandomPowerup::PickupMessage( )
{
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return ( NULL );
	}

	switch ( ulCurrentFrame )
	{
	case 0:

		return GStrings( "GOTMSPHERE" );
	case 1:

		return GStrings( "GOTSUPER" );
	case 2:

		return GStrings( "PICKUP_GUARDSPHERE" );
	case 3:

		return GStrings( "GOTINVIS" );
	case 4:

		return GStrings( "PICKUP_TIMEFREEZER" );
	case 5:

		return GStrings( "PICKUP_INVISIBILITY" );
	case 6:

		return GStrings( "PICKUP_DOOMSPHERE" );
	case 7:

		return GStrings( "PICKUP_TURBOSPHERE" );
	}

	return ( "Random powerup error" );
}

// Hissy --------------------------------------------------------------------
// Hissy isn't really an artifact, but it's going here anyway.

class AHissy : public AActor
{
	DECLARE_ACTOR( AHissy, AActor )
public:
	void Tick( )
	{
		angle_t		Angle;
		angle_t		DeltaAngle;
		AActor		*pConsolePlayer;

		// Run the default actor logic.
		Super::Tick( );

		// Always watch the consoleplayer.
		pConsolePlayer = players[consoleplayer].mo;
		if (( playeringame[consoleplayer] == false ) || ( pConsolePlayer == NULL ))
			return;

		// Find the angle between Hissy and the console player.
		Angle = R_PointToAngle2( this->x, this->y, pConsolePlayer->x, pConsolePlayer->y );
		DeltaAngle = Angle - this->angle;

		if (( DeltaAngle  < ( ANG90/18 )) || ( DeltaAngle > (unsigned)-( ANG90/18 )))
			this->angle = Angle;
		else if ( DeltaAngle < ANG180 )
			this->angle += ( ANG90/18 );
		else
			this->angle -= ( ANG90/18 );
	}
};

FState AHissy::States[] =
{
	S_NORMAL( HISY, 'B',	-1,	NULL,				NULL ),
	S_NORMAL( HISY, 'A',	-1,	NULL,				NULL ),
};

IMPLEMENT_ACTOR( AHissy, Doom, 5057, 0 )
	PROP_RadiusFixed( 20 )
	PROP_HeightFixed( 26 )
	PROP_Flags( MF_SOLID )
	PROP_SpawnState( 0 )
END_DEFAULTS
