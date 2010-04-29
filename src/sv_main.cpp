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
// Filename: sv_main.cpp
//
// Description: Contains variables and routines related to the server portion
// of the program.
//
// Possible improvements:
//	- Globally replace SERVER_CalcNumPlayers with SERVER_CountPlayers.
//
//-----------------------------------------------------------------------------

#include <stdarg.h>
#include <time.h>

#include "networkheaders.h"
#include "MD5Checksum.h"
#include "MD5ChecksumDefines.h"

#include "../upnpnat/upnpnat.h"

// [BB] Needed for timeGetTime() in SERVER_Tick().
#ifdef _MSC_VER
#include <mmsystem.h>
#endif 

#include "a_doomglobal.h"
#include "a_pickups.h"
#include "botcommands.h"
#include "callvote.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "duel.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "s_sound.h"
#include "gi.h"
#include "d_net.h"
#include "g_game.h"
#include "p_local.h"
#include "sv_main.h"
#include "sv_ban.h"
#include "i_system.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "m_argv.h"
#include "m_random.h"
#include "network.h"
#include "possession.h"
#include "s_sndseq.h"
#include "version.h"
#include "m_menu.h"
#include "v_text.h"
#include "w_wad.h"
#include "p_acs.h"
#include "p_effect.h"
#include "m_cheat.h"
#include "stats.h"
#include "team.h"
#include "chat.h"
#include "v_palette.h"
#include "v_video.h"
#include "templates.h"
#include "joinqueue.h"
#include "invasion.h"
#include "lastmanstanding.h"
#include "survival.h"
#include "sv_commands.h"
#include "sv_save.h"
#include "sv_rcon.h"
#include "gamemode.h"
#include "domination.h"

//*****************************************************************************
//	MISC CRAP THAT SHOULDN'T BE HERE BUT HAS TO BE BECAUSE OF SLOPPY CODING

FPolyObj	*GetPolyobj( int polyNum );
FPolyObj	*GetPolyobjByIndex( ULONG ulPoly );

void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags );
void SERVERCONSOLE_ReListPlayers( void );

EXTERN_CVAR( Bool, sv_cheats );

//*****************************************************************************
//	PROTOTYPES

static	bool	server_StartChat( BYTESTREAM_s *pByteStream );
static	bool	server_EndChat( BYTESTREAM_s *pByteStream );
static	bool	server_Ignore( BYTESTREAM_s *pByteStream );
static	bool	server_Say( BYTESTREAM_s *pByteStream );
static	bool	server_ClientMove( BYTESTREAM_s *pByteStream );
static	bool	server_MissingPacket( BYTESTREAM_s *pByteStream );
static	bool	server_UpdateClientPing( BYTESTREAM_s *pByteStream );
static	bool	server_WeaponSelect( BYTESTREAM_s *pByteStream );
static	bool	server_Taunt( BYTESTREAM_s *pByteStream );
static	bool	server_Spectate( BYTESTREAM_s *pByteStream );
static	bool	server_RequestJoin( BYTESTREAM_s *pByteStream );
static	bool	server_RequestRCON( BYTESTREAM_s *pByteStream );
static	bool	server_RCONCommand( BYTESTREAM_s *pByteStream );
static	bool	server_Suicide( BYTESTREAM_s *pByteStream );
static	bool	server_ChangeTeam( BYTESTREAM_s *pByteStream );
static	bool	server_SpectateInfo( BYTESTREAM_s *pByteStream );
static	bool	server_GenericCheat( BYTESTREAM_s *pByteStream );
static	bool	server_GiveCheat( BYTESTREAM_s *pByteStream );
static	bool	server_SummonCheat( BYTESTREAM_s *pByteStream, LONG lType );
static	bool	server_ReadyToGoOn( BYTESTREAM_s *pByteStream );
static	bool	server_ChangeDisplayPlayer( BYTESTREAM_s *pByteStream );
static	bool	server_AuthenticateLevel( BYTESTREAM_s *pByteStream );
static	bool	server_CallVote( BYTESTREAM_s *pByteStream );
static	bool	server_InventoryUseAll( BYTESTREAM_s *pByteStream );
static	bool	server_InventoryUse( BYTESTREAM_s *pByteStream );
static	bool	server_InventoryDrop( BYTESTREAM_s *pByteStream );
static	bool	server_Puke( BYTESTREAM_s *pByteStream );
static	bool	server_MorphCheat( BYTESTREAM_s *pByteStream );
static	bool	server_CheckForClientMinorCommandFlood( ULONG ulClient );

// [RC]
#ifdef CREATE_PACKET_LOG
static  void	server_LogPacket( BYTESTREAM_s *pByteStream, NETADDRESS_s Address, const char *pszReason );
#endif

//*****************************************************************************
//	VARIABLES

// Global array of clients.
static	CLIENT_s		g_aClients[MAXPLAYERS];

// The last client we received a packet from.
static	LONG			g_lCurrentClient;

// Number of ticks that have passed since start of... level?
static	LONG			g_lGameTime = 0;

#ifndef NO_SERVER_GUI
// Storage for commands issued through various menu options to be executed all at once.
static	TArray<FString>	g_ServerCommandQueue;
#endif

// Timer for restarting the map.
static	LONG			g_lMapRestartTimer;

// Buffer that we use for handling packet loss.
static	NETBUFFER_s		g_PacketLossBuffer;

// List of IP addresses that may connect to full servers.
static	IPList			g_AdminIPList;

// List of IP address that we want to ignore for a short amount of time.
static	QueryIPQueue	g_floodProtectionIPQueue( 10 );

// [BB] String to verify that the ban list was actually sent from the master server.
// It's generated randomly on startup by the server.
static	FString			g_MasterBanlistVerificationString;

// Statistics.
static	LONG		g_lTotalServerSeconds = 0;
static	LONG		g_lTotalNumPlayers = 0;
static	LONG		g_lMaxNumPlayers = 0;
static	LONG		g_lTotalNumFrags = 0;
static	QWORD		g_qwTotalOutboundDataTransferred = 0;
static	LONG		g_lMaxOutboundDataTransfer = 0;
static	LONG		g_lCurrentOutboundDataTransfer = 0;
static	LONG		g_lOutboundDataTransferLastSecond = 0;
static	QWORD		g_qwTotalInboundDataTransferred = 0;
static	LONG		g_lMaxInboundDataTransfer = 0;
static	LONG		g_lCurrentInboundDataTransfer = 0;
static	LONG		g_lInboundDataTransferLastSecond = 0;

// This is the current font the "screen" is using when it displays messages.
static	char		g_szCurrentFont[16];
static	char		g_szScriptActiveFont[16];

// This is the music the loaded map is currently using.
static	char		g_szMapMusic[16];

// Maximum packet size.
static	ULONG		g_ulMaxPacketSize = 0;

// List of all translations edited by level scripts.
static	TArray<EDITEDTRANSLATION_s>		g_EditedTranslationList;

// [RC] File to log packets to.
#ifdef CREATE_PACKET_LOG
static	FILE		*PacketLogFile = NULL;
static	IPList		g_HackerIPList;

CUSTOM_CVAR( String, sv_hackerlistfile, "hackerlist.txt", CVAR_ARCHIVE )
{
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( !(g_HackerIPList.clearAndLoadFromFile( sv_hackerlistfile.GetGenericRep( CVAR_String ).String ) ) )
		Printf( "%s", g_HackerIPList.getErrorMessage() );
}

#endif

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( String, sv_motd, "", CVAR_ARCHIVE )
CVAR( Bool, sv_defaultdmflags, true, 0 )
CVAR( Bool, sv_forcepassword, false, CVAR_ARCHIVE )
CVAR( Bool, sv_forcejoinpassword, false, CVAR_ARCHIVE )
CVAR( Bool, sv_showlauncherqueries, false, CVAR_ARCHIVE )
CVAR( Bool, sv_timestamp, false, CVAR_ARCHIVE )
CVAR( Int, sv_timestampformat, 0, CVAR_ARCHIVE )
CVAR( Int, sv_colorstripmethod, 0, CVAR_ARCHIVE )
CVAR( Bool, sv_disallowbots, false, CVAR_ARCHIVE )
CVAR( Bool, sv_minimizetosystray, true, CVAR_ARCHIVE )
CVAR( Int, sv_queryignoretime, 10, CVAR_ARCHIVE )
CVAR( Bool, sv_markchatlines, false, CVAR_ARCHIVE )
CVAR( Bool, sv_nokill, false, CVAR_ARCHIVE )
CVAR( Bool, sv_nodrop, false, CVAR_ARCHIVE )
CVAR( Bool, sv_pure, true, CVAR_SERVERINFO | CVAR_LATCH )
CVAR( Int, sv_maxclientsperip, 2, CVAR_ARCHIVE )

CUSTOM_CVAR( String, sv_adminlistfile, "adminlist.txt", CVAR_ARCHIVE )
{
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( !(g_AdminIPList.clearAndLoadFromFile( sv_adminlistfile.GetGenericRep( CVAR_String ).String ) ) )
		Printf( "%s", g_AdminIPList.getErrorMessage() );
}

//*****************************************************************************
//
CUSTOM_CVAR( Int, sv_maxclients, MAXPLAYERS, CVAR_ARCHIVE )
{
	if ( self < 0 )
		self = 0;

	if ( self > MAXPLAYERS )
	{
		Printf( "sv_maxclients must be less than or equal to %d.\n", MAXPLAYERS );
		self = MAXPLAYERS;
	}
}

//*****************************************************************************
//
CUSTOM_CVAR( Int, sv_maxplayers, MAXPLAYERS, CVAR_ARCHIVE )
{
	if ( self < 0 )
		self = 0;

	if ( self > MAXPLAYERS )
	{
		Printf( "sv_maxplayers must be less than or equal to %d.\n", MAXPLAYERS );
		self = MAXPLAYERS;
	}
}

//*****************************************************************************
//
CUSTOM_CVAR( String, sv_password, "password", CVAR_ARCHIVE )
{
	if ( strlen( self ) > 0 && strlen( self ) <= 4 )
	{
		Printf( "sv_password must be greater than 4 chars in length!\n" );
		self = "password";
	}
}

//*****************************************************************************
//
CUSTOM_CVAR( String, sv_joinpassword, "password", CVAR_ARCHIVE )
{
	if ( strlen( self ) > 0 && strlen( self ) <= 4 )
	{
		Printf( "sv_joinpassword must be greater than 4 chars in length!\n" );
		self = "password";
	}
}

//*****************************************************************************
//
CUSTOM_CVAR( String, sv_rconpassword, "", CVAR_ARCHIVE )
{
	if ( strlen( self ) > 0 && strlen( self ) <= 4 )
	{
		Printf( "sv_rconpassword must be greater than 4 chars in length!\n" );
		self = "";
	}
}

//*****************************************************************************
//
CUSTOM_CVAR( Int, sv_maxpacketsize, 1400, CVAR_ARCHIVE )
{
	if ( self > MAX_UDP_PACKET )
	{
		Printf( "sv_maxpacketsize cannot exceed %d bytes.\n", MAX_UDP_PACKET );
		self = MAX_UDP_PACKET;
	}

	if (( gamestate != GS_STARTUP ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
		Printf( "The server must be restarted before this change will take effect.\n" );
}

//*****************************************************************************
//	FUNCTIONS

void SERVER_Construct( void )
{
    char		*pszPort;
	char		*pszMaxClients;
	ULONG		ulIdx;
	USHORT		usPort;

	// Check if the user wants to use an alternate port for the server.
	pszPort = Args->CheckValue( "-port" );
    if ( pszPort )
    {
       usPort = atoi( pszPort );
       Printf( PRINT_HIGH, "Server using alternate port %i.\n", usPort );
    }
	else 
	   usPort = DEFAULT_SERVER_PORT;

	// Set up a socket and network message buffer.
	NETWORK_Construct( usPort, false );

	// [BB] Forward the external port with UPnP.
	if ( Args->CheckParm ( "-upnp" ) )
	{
		UPNPNAT nat;
		nat.init(5,10);

		int externalPort = NETWORK_GetLocalPort();

		if ( Args->CheckValue( "-upnp" ) != NULL )
			externalPort = atoi( Args->CheckValue( "-upnp" ) );

		if( !nat.discovery() )
		{
			Printf( "NAT discovery error: %s\n", nat.get_last_error() );
		}
		else if ( !nat.add_port_mapping("test",NETWORK_AddressToStringIgnorePort( NETWORK_GetCachedLocalAddress() ), externalPort, NETWORK_GetLocalPort(),"UDP") )
		{
			Printf( "Error adding port mapping: %s\n",nat.get_last_error() );
		}
		else
			Printf( "Sucessfully added port mapping.\n" );
	}

	// Initizlize the playeringame array (is this necessary?).
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ++ulIdx )
		playeringame[ulIdx] = false;

	NETWORK_InitBuffer( &g_PacketLossBuffer, MAX_UDP_PACKET, BUFFERTYPE_WRITE );
	NETWORK_ClearBuffer( &g_PacketLossBuffer );

	// Initialize clients.
	g_ulMaxPacketSize = sv_maxpacketsize;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
    {
		NETWORK_InitBuffer( &g_aClients[ulIdx].PacketBuffer, MAX_UDP_PACKET, BUFFERTYPE_WRITE );
		NETWORK_ClearBuffer( &g_aClients[ulIdx].PacketBuffer );

		// Initialize the saved packet buffer.
		NETWORK_InitBuffer( &g_aClients[ulIdx].SavedPacketBuffer, g_ulMaxPacketSize * PACKET_BUFFER_SIZE, BUFFERTYPE_WRITE );
		NETWORK_ClearBuffer( &g_aClients[ulIdx].SavedPacketBuffer );

		// Initialize the unreliable packet buffer.
		NETWORK_InitBuffer( &g_aClients[ulIdx].UnreliablePacketBuffer, MAX_UDP_PACKET, BUFFERTYPE_WRITE );
		NETWORK_ClearBuffer( &g_aClients[ulIdx].UnreliablePacketBuffer );

		// This is currently an open slot.
		g_aClients[ulIdx].State = CLS_FREE;
	}

	// If they used "-host <#>", make <#> the max number of players.
	pszMaxClients = Args->CheckValue( "-host" );
	if ( pszMaxClients )
		sv_maxclients = atoi( pszMaxClients );

#ifndef NO_SERVER_GUI
	g_ServerCommandQueue.Clear( );
#endif

	// [BB] Initialize g_MasterBanlistVerificationString.
	{
		FString randomString;
		for ( int i = 0; i < 100; ++i )
			randomString += static_cast<char>(M_Random( ));

		CMD5Checksum::GetMD5( reinterpret_cast<const BYTE*>(randomString.GetChars()), randomString.Len(), g_MasterBanlistVerificationString );
	}

	g_lMapRestartTimer = 0;

	sprintf( g_szCurrentFont, "SmallFont" );
	sprintf( g_szScriptActiveFont, "SmallFont" );

#ifdef CREATE_PACKET_LOG

	// [RC] Create the packet log.
	FString logfilename;
	time_t clock;
	struct tm *lt;
	time (&clock);
	lt = localtime (&clock);
	if (lt != NULL)
		logfilename.Format("packetLog_%d_%02d_%02d-%02d_%02d_%02d.txt", lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
	else
		logfilename.Format("packetLog.txt");

	if ( (PacketLogFile = fopen (logfilename, "w")) )
	{
		FString output;
		UCVarValue		Val;
		Val = sv_hostname.GetGenericRep( CVAR_String );
		output.Format("Packet logging started...\n\ton: %d/%d/%d, at %02d:%02d:%02d\n\tat server: %s\n\nDo not distribute this file to the public!\n=====================================================================\n",
			lt->tm_mon + 1, lt->tm_mday, lt->tm_year + 1900, lt->tm_hour, lt->tm_min, lt->tm_sec,  Val.String);
		fputs(output, PacketLogFile);
		Printf("Packet logging enabled. DO NOT DISTRIBUTE THIS EXE TO THE PUBLIC! -- Rivecoder\n");
	}
	else
		Printf("Could not start the packet log.\n"), 

#endif

	// Call SERVER_Destruct() when Skulltag closes.
	atterm( SERVER_Destruct );

	// Setup the child modules.
	SERVER_MASTER_Construct( );
	SERVER_SAVE_Construct( );
	SERVER_RCON_Construct( );
}

//*****************************************************************************
//
void SERVER_Destruct( void )
{
	ULONG	ulIdx;

	// Free the packet loss buffer.
	NETWORK_FreeBuffer( &g_PacketLossBuffer );

	// Free the clients' buffers.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		NETWORK_FreeBuffer( &g_aClients[ulIdx].PacketBuffer );
		NETWORK_FreeBuffer( &g_aClients[ulIdx].SavedPacketBuffer );
		NETWORK_FreeBuffer( &g_aClients[ulIdx].UnreliablePacketBuffer );
	}

#ifdef CREATE_PACKET_LOG
	if ( PacketLogFile )
		fclose( PacketLogFile );
#endif
}

//DWORD	g_LastMS, g_LastSec, g_FrameCount, g_LastCount, g_LastTic;

void			SERVERCONSOLE_UpdateStatistics( void );

//*****************************************************************************
//
void SERVERCONSOLE_UpdateScoreboard( void );
void SERVER_Tick( void )
{
	LONG			lNowTime;
	LONG			lNewTics;
	LONG			lPreviousTics;
	LONG			lCurTics;
	ULONG			ulIdx;

	I_DoSelect();
	lPreviousTics = g_lGameTime / (( 1.0 / (double)35.75 ) * 1000.0 );

	lNowTime = I_MSTime( );
	lNewTics = lNowTime / (( 1.0 / (double)35.75 ) * 1000.0 );

	lCurTics = lNewTics - lPreviousTics;
	while ( lCurTics <= 0 )
	{
		// [BB] Recieve packets whenever possible (not only once each tic) to allow
		// for an accurate ping measurement.
		SERVER_GetPackets( );

		I_Sleep( 1 );
		lNowTime = I_MSTime( );
		lNewTics = lNowTime / (( 1.0 / (double)35.75 ) * 1000.0 );
		lCurTics = lNewTics - lPreviousTics;
	}

#ifdef NO_SERVER_GUI
	// console input
	char *cmd = I_ConsoleInput();
	if (cmd)
		AddCommandString (cmd);
	fflush(stdout);
#else
	// Execute any commands that have been issued through server menus.
	while ( g_ServerCommandQueue.Size( ))
		SERVER_DeleteCommand( );
#endif
	
	int iOldTime = level.time;
	while ( lCurTics-- )
	{
		//DObject::BeginFrame ();

		// Recieve packets.
		SERVER_GetPackets( );

		G_Ticker ();

		// Update the scoreboard if we have a new second to display.
		if ( timelimit && (( level.time % TICRATE ) == 0 ) && ( level.time != iOldTime ))
		{
			SERVERCONSOLE_UpdateScoreboard( );
			iOldTime = level.time;
		}

		if ( g_lMapRestartTimer > 0 )
		{
			if ( --g_lMapRestartTimer == 0 )
			{
				char	szString[128];

				sprintf( szString, "map %s", level.mapname );
				AddCommandString( szString );
			}
		}

		// Drop anyone who's been disconnected.
		SERVER_CheckTimeouts( );

		// Send out player's true position, etc.
		SERVER_WriteCommands( );

		// Check everyone's PacketBuffer for anything that needs to be sent.
		SERVER_SendOutPackets( );

		// Potentially send an update to the master server.
		SERVER_MASTER_Tick( );

		// Time out any old RCON sessions.
		SERVER_RCON_Tick( );

		// Broadcast the server signal so it can be detected on a LAN.
		SERVER_MASTER_Broadcast( );

		// Potentially re-parse the banfile.
		SERVERBAN_Tick( );

		// Print stats and get out.
		FStat::PrintStat( );

		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( SERVER_IsValidClient( ulIdx ) == false ) || ( players[ulIdx].bSpectating ))
				continue;

			if ( g_aClients[ulIdx].lLastMoveTick != gametic )
			{
				g_aClients[ulIdx].lOverMovementLevel--;
//					Printf( "%s: -- (%d)\n", players[ulIdx].userinfo.netname, g_aClients[ulIdx].lOverMovementLevel );
			}
		}

		gametic++;
		maketic++;

		// Do some statistic stuff every second.
		if (( gametic % TICRATE ) == 0 )
		{
			// Increase the number of seconds the server has been active.
			g_lTotalServerSeconds++;

			// Count the number of active players.
			LONG lCurrentNumPlayers = 0;
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( SERVER_IsValidClient( ulIdx ) == false )
					continue;

				g_lTotalNumPlayers++; // Divided by g_lTotalServerSeconds to form an average.
				lCurrentNumPlayers++;
			}

			// Check for new peak records!
			if ( lCurrentNumPlayers > g_lMaxNumPlayers )
				g_lMaxNumPlayers = lCurrentNumPlayers;
			if ( g_lCurrentOutboundDataTransfer > g_lMaxOutboundDataTransfer )
				g_lMaxOutboundDataTransfer = g_lCurrentOutboundDataTransfer;
			if ( g_lCurrentInboundDataTransfer > g_lMaxInboundDataTransfer )
				g_lMaxInboundDataTransfer = g_lCurrentInboundDataTransfer;

			// Update "current" outbound data.
			g_lOutboundDataTransferLastSecond = g_lCurrentOutboundDataTransfer;
			g_lCurrentOutboundDataTransfer = 0;
			g_lInboundDataTransferLastSecond = g_lCurrentInboundDataTransfer;
			g_lCurrentInboundDataTransfer = 0;

			// Update the form.
			SERVERCONSOLE_UpdateStatistics( );
		}

		//DObject::EndFrame ();
	}
/*
	if ( 1 )
	{
		QWORD ms = I_MSTime ();
		DWORD howlong = DWORD(ms - g_LastMS);
		if (howlong > 0)
		{
			DWORD thisSec = ms/1000;

			if (g_LastSec < thisSec)
			{
//					Printf( "%2lu ms (%3lu fps)\n", howlong, g_LastCount );

				g_LastCount = g_FrameCount / (thisSec - g_LastSec);
				g_LastSec = thisSec;
				g_FrameCount = 0;
			}
			g_FrameCount++;
		}
		g_LastMS = ms;
	}
*/
	g_lGameTime = lNowTime;

	// [BB] Remove IP adresses from g_floodProtectionIPQueue that have been in there long enough.
	g_floodProtectionIPQueue.adjustHead ( g_lGameTime / 1000 );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( SERVER_IsValidClient( ulIdx ) == false ) || ( players[ulIdx].bSpectating ))
			continue;

		if ( g_aClients[ulIdx].lOverMovementLevel >= MAX_OVERMOVEMENT_LEVEL )
			SERVER_KickPlayer( ulIdx, "Abnormal level of movement commands detected!" );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVER_SendOutPackets( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ( NETWORK_CalcBufferSize( &g_aClients[ulIdx].PacketBuffer ) > 0 )
			SERVER_SendClientPacket( ulIdx, true );

		if ( NETWORK_CalcBufferSize( &g_aClients[ulIdx].UnreliablePacketBuffer ) > 0 )
			SERVER_SendClientPacket( ulIdx, false );
	}
}

//*****************************************************************************
//
void SERVER_SendClientPacket( ULONG ulClient, bool bReliable )
{
	NETBUFFER_s		*pBuffer;
    NETBUFFER_s		TempBuffer;
	BYTE			abData[MAX_UDP_PACKET];
	CLIENT_s		*pClient;

	pClient = SERVER_GetClient( ulClient );
	if ( pClient == NULL )
		return;

	TempBuffer.pbData = abData;
	TempBuffer.ulMaxSize = sizeof( abData );
	TempBuffer.ulCurrentSize = 0;
	TempBuffer.BufferType = BUFFERTYPE_WRITE;
	TempBuffer.ByteStream.pbStream = abData;
	TempBuffer.ByteStream.pbStreamEnd = TempBuffer.ByteStream.pbStream + TempBuffer.ulMaxSize;

	if ( bReliable )
		pBuffer = &pClient->PacketBuffer;
	else
		pBuffer = &pClient->UnreliablePacketBuffer;

	pBuffer->ulCurrentSize = NETWORK_CalcBufferSize( pBuffer );
	if ( bReliable )
	{
		pClient->SavedPacketBuffer.ulCurrentSize = NETWORK_CalcBufferSize( &pClient->SavedPacketBuffer );

		// If we've reached the end of our reliable packets buffer, start writing at the beginning.
		if (( pClient->SavedPacketBuffer.ulCurrentSize + pClient->PacketBuffer.ulCurrentSize ) >= pClient->SavedPacketBuffer.ulMaxSize )
		{
			pClient->SavedPacketBuffer.ByteStream.pbStream = pClient->SavedPacketBuffer.pbData;
			pClient->SavedPacketBuffer.ulCurrentSize = 0;
		}

		// Save where the beginning is and the size of each packet within the reliable packets
		// buffer.
		pClient->lPacketBeginning[( pClient->ulPacketSequence ) % PACKET_BUFFER_SIZE] = pClient->SavedPacketBuffer.ulCurrentSize;
		pClient->lPacketSize[( pClient->ulPacketSequence ) % PACKET_BUFFER_SIZE] = pClient->PacketBuffer.ulCurrentSize;
		pClient->lPacketSequence[( pClient->ulPacketSequence ) % PACKET_BUFFER_SIZE] = pClient->ulPacketSequence;

		// Write what we want to send out to our reliable packets buffer, so that it can be
		// retransmitted later if necessary.
		if ( pClient->PacketBuffer.ulCurrentSize )
			NETWORK_WriteBuffer( &pClient->SavedPacketBuffer.ByteStream, pClient->PacketBuffer.pbData, pClient->PacketBuffer.ulCurrentSize );

		// Write the header to our temporary buffer.
		NETWORK_WriteByte( &TempBuffer.ByteStream, SVC_HEADER );
		NETWORK_WriteLong( &TempBuffer.ByteStream, pClient->ulPacketSequence );

		pClient->ulPacketSequence++;
	}
	else
	{
		// Write the header to our temporary buffer.
		NETWORK_WriteByte( &TempBuffer.ByteStream, SVC_UNRELIABLEPACKET );
	}

	// Write the body of the message to our temporary buffer.
	pBuffer->ulCurrentSize = pBuffer->ByteStream.pbStream - pBuffer->pbData;
	if ( pBuffer->ulCurrentSize != 0 )
		NETWORK_WriteBuffer( &TempBuffer.ByteStream, pBuffer->pbData, pBuffer->ulCurrentSize );

	// Finally, send the packet, and clear the buffer.
	NETWORK_LaunchPacket( &TempBuffer, pClient->Address );
	NETWORK_ClearBuffer( pBuffer );
}

//*****************************************************************************
//
void SERVER_CheckClientBuffer( ULONG ulClient, ULONG ulSize, bool bReliable )
{
	NETBUFFER_s	*pBuffer;
	CLIENT_s	*pClient;

	pClient = SERVER_GetClient( ulClient );
	if ( pClient == NULL )
		return;

	if ( bReliable )
		pBuffer = &pClient->PacketBuffer;
	else
		pBuffer = &pClient->UnreliablePacketBuffer;

	// Make sure we have enough room for the upcoming message. If not, send
	// out the current buffer and clear the packet.
	pBuffer->ulCurrentSize = pBuffer->ByteStream.pbStream - pBuffer->pbData;
	if (( pBuffer->ulCurrentSize + ( ulSize + 5 )) >= SERVER_GetMaxPacketSize( ))
	{
		if ( debugfile )
			fprintf( debugfile, "Launching premature packet: %d\n", bReliable );

		// Lanch the packet so we can prepare another.
		SERVER_SendClientPacket( ulClient, bReliable );
	}
}

//*****************************************************************************
//
LONG SERVER_FindFreeClientSlot( void )
{
	ULONG	ulIdx;
	ULONG	ulNumSlots;

	// [BB] Even if maxclients is reached, we allow one additional connection from localhost.
	// The check (SERVER_CalcNumPlayers( ) >= sv_maxclients) for a non local connection attempt
	// is done in SERVER_SetupNewConnection.
	ulNumSlots = sv_maxclients + 1;

	// If the number of players in the game is greater than or equal to the number of current
	// players, disallow the join.
	if ( SERVER_CalcNumPlayers( ) >= ulNumSlots )
		return ( -1 );

	// Look for a free player slot.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( g_aClients[ulIdx].State == CLS_FREE ) && ( playeringame[ulIdx] == false ))
			return ( ulIdx );
	}

	// Didn't find an available slot.
	return ( -1 );
}

//*****************************************************************************
//
LONG SERVER_FindClientByAddress( NETADDRESS_s Address )
{
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
	   if ( g_aClients[ulIdx].State == CLS_FREE )
	       continue;

	   // If the client's address matches the given IP, return the client's index.
	   if ( NETWORK_CompareAddress( g_aClients[ulIdx].Address, Address, false ))
	       return ( ulIdx );
	}
	
	return ( -1 );
}

//*****************************************************************************
//
CLIENT_s *SERVER_GetClient( ULONG ulIdx )
{
	if ( ulIdx >= MAXPLAYERS )
		return ( NULL );

	return ( &g_aClients[ulIdx] );
}

//*****************************************************************************
// [BB] Returns the number of clients that are somehow connected to the server.
// They don't need to be actually in the game yet, to count as connected client.
ULONG SERVER_CalcNumConnectedClients( void )
{
	ULONG	ulNumConnectedClients = 0;

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_GetClient( ulIdx )->State != CLS_FREE )
			ulNumConnectedClients++;
	}

	return ( ulNumConnectedClients );
}

//*****************************************************************************
//
ULONG SERVER_CalcNumPlayers( void )
{
	return SERVER_CountPlayers( true );
}

//*****************************************************************************
//
ULONG SERVER_CountPlayers( bool bCountBots )
{
	ULONG	ulNumPlayers = 0;

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] && ( !players[ulIdx].bIsBot || ( players[ulIdx].bIsBot && bCountBots )))
			ulNumPlayers++;
	}

	return ( ulNumPlayers );
}

//*****************************************************************************
//
ULONG SERVER_CalcNumNonSpectatingPlayers( ULONG ulExcludePlayer )
{
	ULONG	ulIdx;
	ULONG	ulNumPlayers = 0;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( PLAYER_IsTrueSpectator( &players[ulIdx] )) ||
			( ulIdx == ulExcludePlayer ))
		{
			continue;
		}

		ulNumPlayers++;
	}

	return ( ulNumPlayers );
}

//*****************************************************************************
//
void SERVER_CheckTimeouts( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
		{
			if ( ( g_aClients[ulIdx].State != CLS_FREE )
			     && ( ( gametic - g_aClients[ulIdx].ulLastCommandTic ) >= ( CLIENT_TIMEOUT * TICRATE ) ) )
			{
				Printf( "Unfinished connection from %s timed out.\n", NETWORK_AddressToString( g_aClients[ulIdx].Address ) );
				SERVER_DisconnectClient( ulIdx, false, false );
			}
			continue;
		}

		// If we haven't gotten a packet from this client in CLIENT_TIMEOUT seconds,
		// disconnect him.
		if (( gametic - g_aClients[ulIdx].ulLastCommandTic ) >= ( CLIENT_TIMEOUT * TICRATE ))
		{
		    SERVER_DisconnectClient( ulIdx, true, true );
			continue;
		}

		if ( players[ulIdx].bSpectating )
			continue;

		// Also check to see if the client is lagging.
		if (( gametic - g_aClients[ulIdx].ulLastCommandTic ) >= TICRATE )
		{
			// Have not heard from the client in at least one second; mark him as
			// lagging and tell clients.
			if ( players[ulIdx].bLagging == false )
			{
				players[ulIdx].bLagging = true;
				SERVERCOMMANDS_SetPlayerLaggingStatus( ulIdx );
			}
		}
		else
		{
			// Player is no longer lagging. Tell clients.
			if ( players[ulIdx].bLagging )
			{
				players[ulIdx].bLagging = false;
				SERVERCOMMANDS_SetPlayerLaggingStatus( ulIdx );
			}
		}
	}
}

//*****************************************************************************
//

#ifdef	_DEBUG
CVAR( Bool, sv_emulatepacketloss, false, 0 )
#endif

void SERVER_GetPackets( void )
{
	BYTESTREAM_s	*pByteStream;

	while ( NETWORK_GetPackets( ) > 0 )
	{
		// Set up our byte stream.
		pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;
		pByteStream->pbStream = NETWORK_GetNetworkMessageBuffer( )->pbData;
		pByteStream->pbStreamEnd = pByteStream->pbStream + NETWORK_GetNetworkMessageBuffer( )->ulCurrentSize;

		// [RC]
#ifdef CREATE_PACKET_LOG
		pByteStream->pbStreamBeginning = pByteStream->pbStream;
		pByteStream->bPacketAlreadyLogged = false;

		// We've already had trouble from this IP, so log all of his traffic.
		if ( g_HackerIPList.isIPInList( NETWORK_GetFromAddress( ) ) )
		{
			FString outString;
			outString.Format("Alleged hacker (first offense: %s)", g_HackerIPList.getEntryComment(  NETWORK_GetFromAddress( ) ));
			server_LogPacket( pByteStream, NETWORK_GetFromAddress( ), outString.GetChars() );
		}

#endif

		// We've gotten a packet. Try to figure out if it's from a connected client.
		g_lCurrentClient = SERVER_FindClientByAddress( NETWORK_GetFromAddress( ));

		// Packet is not from an existing client; must be someone trying to connect!
		if ( g_lCurrentClient == -1 )
		{
			SERVER_DetermineConnectionType( pByteStream );
			continue;
		}

#ifdef	_DEBUG
		// Emulate packet loss for debugging.
		if ( sv_emulatepacketloss )
		{
			if (( M_Random( ) < 170 ) && gamestate == GS_LEVEL )
				break;
		}
#endif

		// Parse the information sent by the clients.
		SERVER_ParsePacket( pByteStream );

		// Invalidate this.
		g_lCurrentClient = -1;
	}
}

//*****************************************************************************
//
EXTERN_CVAR( Bool, sv_logfiletimestamp )
void SERVER_SendChatMessage( ULONG ulPlayer, ULONG ulMode, const char *pszString )
{
	// [BB] Ignore any chat messages with invalid chat mode. This is crucial because
	// the rest of the code assumes that only valid chat modes are used.
	if ( ( ulMode == CHATMODE_NONE ) || ( ulMode >= NUM_CHATMODES ) )
		return;

	bool	bFordidChatToPlayers;

	// Potentially prevent spectators from talking to active players during LMS games.
	if (( teamlms || lastmanstanding ) &&
		(( lmsspectatorsettings & LMS_SPF_CHAT ) == false ) &&
		( ulPlayer < MAXPLAYERS ) &&
		( players[ulPlayer].bSpectating ) &&
		( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ))
	{
		bFordidChatToPlayers = true;
	}
	else
		bFordidChatToPlayers = false;

	FString cleanedChatString = pszString;

	// [BB] Remove any kind of trailing crap.
	V_RemoveTrailingCrapFromFString ( cleanedChatString );

	// [BB] If the chat string is empty now, it only contained crap and is ignored.
	if ( cleanedChatString.IsEmpty() )
		return;

	// [BB] Replace the pointer to the chat string, with the cleaned version.
	// This way the code below doesn't need to be altered.
	pszString = cleanedChatString.GetChars();

	SERVERCOMMANDS_PlayerSay( ulPlayer, pszString, ulMode, bFordidChatToPlayers );

	// [BB] This is to make the lines readily identifiable, necessary
	// for MiX-MaN's IRC server control tool for example.
	bool sv_logfiletimestampOldValue = sv_logfiletimestamp;
	if( sv_markchatlines )
	{
		Printf( "CHAT " );
		sv_logfiletimestamp = false;
	}
	// Print this message in the server's local window.
	if ( strnicmp( "/me", pszString, 3 ) == 0 )
	{
		pszString += 3;
		if ( ulPlayer == MAXPLAYERS )
			Printf( "* <server>%s\n", pszString );
		else
			Printf( "* %s%s\n", players[ulPlayer].userinfo.netname, pszString );
	}
	else
	{
		if ( ulPlayer == MAXPLAYERS )
			Printf( "<server>: %s\n", pszString );
		else
			Printf( "%s: %s\n", players[ulPlayer].userinfo.netname, pszString );
	}
	if( sv_markchatlines && sv_logfiletimestampOldValue )
		sv_logfiletimestamp = true;
}

//*****************************************************************************
//
void SERVER_RequestClientToAuthenticate( ULONG ulClient )
{
	NETWORK_ClearBuffer( &g_aClients[ulClient].PacketBuffer );
	NETWORK_WriteByte( &g_aClients[ulClient].PacketBuffer.ByteStream, SVCC_AUTHENTICATE );
	NETWORK_WriteString( &g_aClients[ulClient].PacketBuffer.ByteStream, level.mapname );

	// Send the packet off.
	SERVER_SendClientPacket( ulClient, true );
}

//*****************************************************************************
//
void SERVER_AuthenticateClientLevel( BYTESTREAM_s *pByteStream )
{
	if ( SERVER_PerformAuthenticationChecksum( pByteStream ) == false )
	{
		SERVER_ClientError( g_lCurrentClient, NETWORK_ERRORCODE_AUTHENTICATIONFAILED );
		return;
	}

	// The client has now had his level authenticated.
	g_aClients[g_lCurrentClient].State = CLS_AUTHENTICATED;

	// Tell the client his level was authenticated.
	NETWORK_ClearBuffer( &g_aClients[g_lCurrentClient].PacketBuffer );
	NETWORK_WriteByte( &g_aClients[g_lCurrentClient].PacketBuffer.ByteStream, SVCC_MAPLOAD );
	// [BB] Also tell him the game mode, otherwise the client can't decide whether 3D floors should be spawned or not.
	NETWORK_WriteByte( &g_aClients[g_lCurrentClient].PacketBuffer.ByteStream, GAMEMODE_GetCurrentMode( ) );
	
	// Send the packet off.
	SERVER_SendClientPacket( g_lCurrentClient, true );
}

//*****************************************************************************
//
bool SERVER_PerformAuthenticationChecksum( BYTESTREAM_s *pByteStream )
{
	MapData		*pMap;
	FString		serverVertexString;
	FString		serverLinedefString;
	FString		serverSidedefString;
	FString		serverSectorString;
	FString		serverBehaviorString;
	FString		clientVertexString;
	FString		clientLinedefString;
	FString		clientSidedefString;
	FString		clientSectorString;
	FString		clientBehaviorString;

	// [BB] Open the map. Since we are already using the map, we won't get a NULL pointer.
	pMap = P_OpenMapData( level.mapname );

	// Generate checksums for the map lumps.
	NETWORK_GenerateMapLumpMD5Hash( pMap, ML_VERTEXES, serverVertexString );
	NETWORK_GenerateMapLumpMD5Hash( pMap, ML_LINEDEFS, serverLinedefString );
	NETWORK_GenerateMapLumpMD5Hash( pMap, ML_SIDEDEFS, serverSidedefString );
	NETWORK_GenerateMapLumpMD5Hash( pMap, ML_SECTORS, serverSectorString );
	if ( pMap->HasBehavior ) // ML_BEHAVIOR
		NETWORK_GenerateMapLumpMD5Hash( pMap, ML_BEHAVIOR, serverBehaviorString );
	else
		serverBehaviorString = "";

	// Free the map pointer, we don't need it anymore.
	delete ( pMap );

	// Read in the client's checksum strings.
	clientVertexString = NETWORK_ReadString( pByteStream );
	clientLinedefString = NETWORK_ReadString( pByteStream );
	clientSidedefString = NETWORK_ReadString( pByteStream );
	clientSectorString = NETWORK_ReadString( pByteStream );
	clientBehaviorString = NETWORK_ReadString( pByteStream );

	// Checksums did not match! Therefore, the level authentication has failed.
	if (( serverVertexString.Compare( clientVertexString ) != 0 ) ||
		( serverLinedefString.Compare( clientLinedefString ) != 0 ) ||
		( serverSidedefString.Compare( clientSidedefString ) != 0 ) ||
		( serverSectorString.Compare( clientSectorString ) != 0 ) ||
		( serverBehaviorString.Compare( clientBehaviorString ) != 0 ))
	{
		return ( false );
	}

	return ( true );
}

//*****************************************************************************
//
void SERVERCONSOLE_ReListPlayers( void );
void SERVER_ConnectNewPlayer( BYTESTREAM_s *pByteStream )
{
	UCVarValue							Val;
	LONG								lCommand;
	ULONG								ulIdx;
	PLAYERSAVEDINFO_t					*pSavedInfo;
	ULONG								ulState;
	ULONG								ulCountdownTicks;
	AInventory							*pInventory;
	DDoor								*pDoor;
	DPlat								*pPlat;
	DFloor								*pFloor;
	DElevator							*pElevator;
	DWaggleBase							*pWaggle;
	DPillar								*pPillar;
	DCeiling							*pCeiling;
	DScroller							*pScroller;
	TThinkerIterator<DDoor>				DoorIterator;
	TThinkerIterator<DPlat>				PlatIterator;
	TThinkerIterator<DFloor>			FloorIterator;
	TThinkerIterator<DElevator>			ElevatorIterator;
	TThinkerIterator<DWaggleBase>		WaggleIterator;
	TThinkerIterator<DPillar>			PillarIterator;
	TThinkerIterator<DCeiling>			CeilingIterator;
	TThinkerIterator<DScroller>			ScrollerIterator;

	// If the client hasn't authenticated his level, don't accept this connection.
	if ( g_aClients[g_lCurrentClient].State < CLS_AUTHENTICATED )
	{
		if ( g_aClients[g_lCurrentClient].State == CLS_AUTHENTICATED_BUT_OUTDATED_MAP )
			SERVER_RequestClientToAuthenticate( g_lCurrentClient );
		else
			SERVER_ClientError( g_lCurrentClient, NETWORK_ERRORCODE_AUTHENTICATIONFAILED );
		return;
	}

	// This player is now in the game.
	playeringame[g_lCurrentClient] = true;

	// [BB] If necessary, spawn a voodoo doll for the player.
	if ( COOP_PlayersVoodooDollsNeedToBeSpawned ( g_lCurrentClient ) )
	{
		// [BB] players[g_lCurrentClient].mo already should be NULL, I'm not sure if the code works if it's not NULL.
		APlayerPawn* pOldPawn = players[g_lCurrentClient].mo;
		players[g_lCurrentClient].mo = NULL;
		COOP_SpawnVoodooDollsForPlayerIfNecessary ( g_lCurrentClient );
		// [BB] It is important that mo doesn't point to the voodoo doll, otherwise G_CooperativeSpawnPlayer called later
		// removes the MF_SOLID flag from the doll because it calls G_CheckSpot.
		players[g_lCurrentClient].mo = pOldPawn;
	}

	// Check and see if this player should spawn as a spectator.
	players[g_lCurrentClient].bSpectating = (( PLAYER_ShouldSpawnAsSpectator( &players[g_lCurrentClient] )) || ( g_aClients[g_lCurrentClient].bWantStartAsSpectator ));

	// Don't restart the map! There's people here!
	g_lMapRestartTimer = 0;

	g_aClients[g_lCurrentClient].ulLastGameTic = gametic;
	g_aClients[g_lCurrentClient].ulLastCommandTic = gametic;

	// Clear out the client's netbuffer.
	NETWORK_ClearBuffer( &g_aClients[g_lCurrentClient].PacketBuffer );

	// Tell the client that we're about to send him a snapshot of the level.
	SERVERCOMMANDS_BeginSnapshot( g_lCurrentClient );

	// Send welcome message.
	SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "Version %s Server\n", DOTVERSIONSTR );

	// Send consoleplayer number.
	SERVERCOMMANDS_SetConsolePlayer( g_lCurrentClient );

	// Send dmflags.
	SERVERCOMMANDS_SetGameDMFlags( g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// Send skill level.
	SERVERCOMMANDS_SetGameSkill( g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// Send special settings like teamplay and deathmatch.
	SERVERCOMMANDS_SetGameMode( g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// Send timelimit, fraglimit, etc.
	SERVERCOMMANDS_SetGameModeLimits( g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// If this is LMS, send the allowed weapons.
	if ( lastmanstanding || teamlms )
	{
		SERVERCOMMANDS_SetLMSAllowedWeapons( g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		SERVERCOMMANDS_SetLMSSpectatorSettings( g_lCurrentClient, SVCF_ONLYTHISCLIENT );
	}

	// If this is CTF or ST, tell the client whether or not we're in simple mode.
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_USETEAMITEM )
		SERVERCOMMANDS_SetSimpleCTFSTMode( g_lCurrentClient, SVCF_ONLYTHISCLIENT );
/*
	// Send the map name, and have the client load it.
	SERVERCOMMANDS_MapLoad( g_lCurrentClient, SVCF_ONLYTHISCLIENT );
*/
	// Send the map music.
	SERVERCOMMANDS_SetMapMusic( SERVER_GetMapMusic( ), g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// Send the message of the day.
	Val = sv_motd.GetGenericRep( CVAR_String );
	FString motd = Val.String;

	// [BB] This server doesn't enforce the special lump authentication. Inform the client about it!
	if ( !sv_pure )
		motd += "\n\nThis server doesn't enforce the special lump authentication.\nEnter at your own risk.\n";

	if ( motd.IsNotEmpty() )
		SERVERCOMMANDS_PrintMOTD( motd.GetChars(), g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// [BB] Client is using the "join full server from localhost" hack. Inform him about it!
	if ( ( SERVER_CalcNumPlayers( ) > static_cast<unsigned> (sv_maxclients) ) )
		SERVERCOMMANDS_PrintMOTD( "Emergency!\n\nYou are joining from localhost even though the server is full.\nDo whatever is necessary to clean the situation and disconnect afterwards.\n", g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// If we're in a duel or LMS mode, tell him the state of the game mode.
	if ( duel || lastmanstanding || teamlms || possession || teampossession || survival || invasion )
	{
		if ( duel )
		{
			ulState = DUEL_GetState( );
			ulCountdownTicks = DUEL_GetCountdownTicks( );
		}
		else if ( survival )
		{
			ulState = SURVIVAL_GetState( );
			ulCountdownTicks = SURVIVAL_GetCountdownTicks( );
		}
		else if ( invasion )
		{
			ulState = INVASION_GetState( );
			ulCountdownTicks = INVASION_GetCountdownTicks( );
		}
		else if ( possession || teampossession )
		{
			ulState = POSSESSION_GetState( );
			if ( ulState == (PSNSTATE_e)PSNS_ARTIFACTHELD )
				ulCountdownTicks = POSSESSION_GetArtifactHoldTicks( );
			else
				ulCountdownTicks = POSSESSION_GetCountdownTicks( );
		}
		else
		{
			ulState = LASTMANSTANDING_GetState( );
			ulCountdownTicks = LASTMANSTANDING_GetCountdownTicks( );
		}

		SERVERCOMMANDS_SetGameModeState( ulState, ulCountdownTicks, g_lCurrentClient, SVCF_ONLYTHISCLIENT );

		// Also, if we're in invasion mode, tell the client what wave we're on.
		if ( invasion )
			SERVERCOMMANDS_SetInvasionWave( g_lCurrentClient, SVCF_ONLYTHISCLIENT );
	}

	// In a game mode that involves teams, potentially decide a team for him.
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
	{
		// The server will decide his team!
		if ( dmflags2 & DF2_NO_TEAM_SELECT )
		{
			players[g_lCurrentClient].bOnTeam = true;
			players[g_lCurrentClient].ulTeam = TEAM_ChooseBestTeamForPlayer( );

		}
		else
			players[g_lCurrentClient].bOnTeam = false;
	}

	lCommand = NETWORK_ReadByte( pByteStream );
	if ( lCommand != CLC_USERINFO )
	{
		SERVER_ClientError( g_lCurrentClient, NETWORK_ERRORCODE_FAILEDTOSENDUSERINFO );
		return;
	}

	// Read in the user's userinfo. If it returns false, the player was kicked for flooding
	// (though this shouldn't happen anymore).
	if ( SERVER_GetUserInfo( pByteStream, false ) == false )
		return;

	// Apparently, we know the client is in the game, but the 
	// client dosent know this. So instead of calling multiple 
	// P_SpawnPlayers and messing ourselves up, we just re-send
	// the spawn data. Make sense?
	if (( g_aClients[g_lCurrentClient].State == CLS_SPAWNED ) && ( players[g_lCurrentClient].mo != NULL ))
		SERVERCOMMANDS_SpawnPlayer( g_lCurrentClient, PST_REBORNNOINVENTORY, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
	else
	{
		players[g_lCurrentClient].playerstate = PST_ENTER;

		// [BB] Spawn the player at an appropriate start.
		GAMEMODE_SpawnPlayer ( g_lCurrentClient );
	}

	// Tell the client of any lines that have been altered since the level start.
	SERVER_UpdateLines( g_lCurrentClient );

	// Tell the client of any sides that have been altered since the level start.
	SERVER_UpdateSides( g_lCurrentClient );

	// Tell the client of any sectors that have been altered since the level start.
	SERVER_UpdateSectors( g_lCurrentClient );

	// Tell the client about any active doors.
	while (( pDoor = DoorIterator.Next( )))
		pDoor->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active plats.
	while (( pPlat = PlatIterator.Next( )))
		pPlat->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active floors.
	while (( pFloor = FloorIterator.Next( )))
		pFloor->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active elevators.
	while (( pElevator = ElevatorIterator.Next( )))
		pElevator->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active waggles.
	while (( pWaggle = WaggleIterator.Next( )))
		pWaggle->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active pillars.
	while (( pPillar = PillarIterator.Next( )))
		pPillar->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active ceilings.
	while (( pCeiling = CeilingIterator.Next( )))
		pCeiling->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active scrollers.
	while (( pScroller = ScrollerIterator.Next( )))
		pScroller->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active rotate polyobjects.
	{
		DRotatePoly	*pRotatePoly;
		TThinkerIterator<DRotatePoly>	Iterator;

		while (( pRotatePoly = Iterator.Next( )))
			pRotatePoly->UpdateToClient( g_lCurrentClient );
	}

	// Tell the client about any move polyobjects.
	{
		DMovePoly	*pMovePoly;
		TThinkerIterator<DMovePoly>		Iterator;

		while (( pMovePoly = Iterator.Next( )))
			pMovePoly->UpdateToClient( g_lCurrentClient );
	}

	// Tell the client about any active door polyobjects.
	{
		DPolyDoor	*pPolyDoor;
		TThinkerIterator<DPolyDoor>		Iterator;

		while (( pPolyDoor = Iterator.Next( )))
			pPolyDoor->UpdateToClient( g_lCurrentClient );
	}

	// Send a snapshot of the level.
	SERVER_SendFullUpdate( g_lCurrentClient );

	// If we need to start this client's enter scripts, do that now.
	if ( g_aClients[g_lCurrentClient].bRunEnterScripts )
	{
		FBehavior::StaticStartTypedScripts( SCRIPT_Enter, players[g_lCurrentClient].mo, true );
		g_aClients[g_lCurrentClient].bRunEnterScripts = false;
	}

	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_USETEAMITEM )
	{
		// In ST/CTF games, let the incoming player know who has flags/skulls.
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( SERVER_IsValidClient( ulIdx ) == false )
				continue;

			// Player shouldn't have a flag/skull if he's not on a team...
			if (( players[ulIdx].bOnTeam == false ) || ( players[ulIdx].mo == NULL ))
				continue;

			// See if this player is carrying the opponents flag/skull.
			pInventory = TEAM_FindOpposingTeamsItemInPlayersInventory ( &players[ulIdx] );
			if ( pInventory )
				SERVERCOMMANDS_GiveInventory( ulIdx, pInventory, g_lCurrentClient, SVCF_ONLYTHISCLIENT );

			// See if the player is carrying the white flag in OFCTF.
			pInventory = players[ulIdx].mo->FindInventory( PClass::FindClass( "WhiteFlag" ));
			if (( oneflagctf ) && ( pInventory ))
				SERVERCOMMANDS_GiveInventory( ulIdx, pInventory, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		}

		// Also let the client know if flags/skulls are on the ground.
		for ( ulIdx = 0; ulIdx < teams.Size( ); ulIdx++ )
			SERVERCOMMANDS_SetTeamReturnTicks( ulIdx, TEAM_GetReturnTicks( ulIdx ), g_lCurrentClient, SVCF_ONLYTHISCLIENT );

		SERVERCOMMANDS_SetTeamReturnTicks( teams.Size( ), TEAM_GetReturnTicks( teams.Size( ) ), g_lCurrentClient, SVCF_ONLYTHISCLIENT );
	}

	// If we're playing terminator, potentially tell the client who's holding the terminator
	// artifact.
	if ( terminator )
	{
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] == false ) || ( players[ulIdx].mo == NULL ))
				continue;

			pInventory = players[ulIdx].mo->FindInventory( PClass::FindClass( "PowerTerminatorArtifact" ));
			if ( pInventory )
				SERVERCOMMANDS_GiveInventory( ulIdx, pInventory, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		}
	}

	// If we're playing possession/team possession, potentially tell the client who's holding
	// the possession artifact.
	if ( possession || teampossession )
	{
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] == false ) || ( players[ulIdx].mo == NULL ))
				continue;

			pInventory = players[ulIdx].mo->FindInventory( PClass::FindClass( "PowerPossessionArtifact" ));
			if ( pInventory )
				SERVERCOMMANDS_GiveInventory( ulIdx, pInventory, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		}
	}

	// Check and see if this is a disconnected player. If so, restore his fragcount.
	pSavedInfo = SERVER_SAVE_GetSavedInfo( players[g_lCurrentClient].userinfo.netname, g_aClients[g_lCurrentClient].Address );
	if (( pSavedInfo ) && ( g_aClients[g_lCurrentClient].bWantNoRestoreFrags == false ))
	{
		PLAYER_SetFragcount( &players[g_lCurrentClient], pSavedInfo->lFragCount, false, false );
		PLAYER_SetWins( &players[g_lCurrentClient], pSavedInfo->lWinCount );
		players[g_lCurrentClient].lPointCount = pSavedInfo->lPointCount;

		if ( teamgame )
			SERVERCOMMANDS_SetPlayerPoints( g_lCurrentClient );

		// [RC] Also restore his playing time. This should agree with 'restore frags' as a whole, clean slate option.
		players[g_lCurrentClient].ulTime = pSavedInfo->ulTime;
	}

	// If this player is on a team, tell all the other clients that a team has been selected
	// for him.
	if ( players[g_lCurrentClient].bOnTeam )
		SERVERCOMMANDS_SetPlayerTeam( g_lCurrentClient );

	if ( g_aClients[g_lCurrentClient].State != CLS_SPAWNED )
	{
		if ( players[g_lCurrentClient].bSpectating )
			SERVER_Printf( PRINT_HIGH, "%s \\c-has connected.\n", players[g_lCurrentClient].userinfo.netname );
		else
			SERVER_Printf( PRINT_HIGH, "%s \\c-entered the game.\n", players[g_lCurrentClient].userinfo.netname );
	}

	BOTCMD_SetLastJoinedPlayer( players[g_lCurrentClient].userinfo.netname );

	// Tell the bots that a new players has joined the game!
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] == false )
			continue;

		if ( players[ulIdx].pSkullBot )
			players[ulIdx].pSkullBot->PostEvent( BOTEVENT_PLAYER_JOINEDGAME );
	}

	if ( g_aClients[g_lCurrentClient].State != CLS_SPAWNED )
		SERVERCONSOLE_ReListPlayers( );

	// Update this client's state. He's in the game now!
	g_aClients[g_lCurrentClient].State = CLS_SPAWNED;

	// [RC] Update clients using the RCON utility.
	SERVER_RCON_UpdateInfo( SVRCU_PLAYERDATA );

	// Tell the client that the snapshot is done.
	SERVERCOMMANDS_EndSnapshot( g_lCurrentClient );

	// [RC] Clients may wish to ignore this new player.
	SERVERCOMMANDS_PotentiallyIgnorePlayer( g_lCurrentClient );
}

//*****************************************************************************
//
void SERVER_DetermineConnectionType( BYTESTREAM_s *pByteStream )
{
	ULONG	ulFlags;
	ULONG	ulTime;
	LONG	lCommand;

	// If either this IP is in our flood protection queue, or the queue is full (DOS), ignore the request.
	if ( g_floodProtectionIPQueue.isFull( ) || g_floodProtectionIPQueue.addressInQueue( NETWORK_GetFromAddress( )))
		return;

	lCommand = NETWORK_ReadByte( pByteStream );

	// [BB] It's absolutely crucial that we only handle the first command in a packet
	// that comes from someone that's not a client yet. Otherwise a single malformed
	// packet can be used to bombard the server with connection requests, freezing the server for about 5 seconds.

	// End of message.
	if ( lCommand == -1 )
		return;

	// If it's not a launcher querying the server, it must be a client.
	if ( lCommand != CLCC_ATTEMPTCONNECTION )
	{
		switch ( lCommand )
		{		
		// [RC] An RCON utility is trying to connect to/control this server.
		case CLRC_BEGINCONNECTION:
		case CLRC_PASSWORD:
		case CLRC_COMMAND:
		case CLRC_PONG:
		case CLRC_DISCONNECT:

			SERVER_RCON_ParseMessage( NETWORK_GetFromAddress( ), lCommand, pByteStream );
			return;
		// Launcher is querying this server.
		case LAUNCHER_SERVER_CHALLENGE:

			// Read in three more bytes, because it was a long that was sent to us.
			NETWORK_ReadByte( pByteStream );
			NETWORK_ReadByte( pByteStream );
			NETWORK_ReadByte( pByteStream );

			// Read in what the query wants to know.
			ulFlags = NETWORK_ReadLong( pByteStream );

			// Read in the time the launcher sent us.
			ulTime = NETWORK_ReadLong( pByteStream );

			// Received launcher query!
			if ( sv_showlauncherqueries )
				Printf( "Launcher challenge from: %s\n", NETWORK_AddressToString( NETWORK_GetFromAddress( )));

			SERVER_MASTER_SendServerInfo( NETWORK_GetFromAddress( ), ulFlags, ulTime, false );
			return;
		// [RC] Master server is sending us the holy banlist.
		case MASTER_SERVER_BANLIST:

			if ( NETWORK_CompareAddress( NETWORK_GetFromAddress( ), SERVER_MASTER_GetMasterAddress( ), false ))
			{
				FString MasterBanlistVerificationString = NETWORK_ReadString( pByteStream );
				if ( SERVER_GetMasterBanlistVerificationString().Compare ( MasterBanlistVerificationString ) == 0 )
					SERVERBAN_ReadMasterServerBans( pByteStream );
				else
					Printf ( "Banlist with wrong verification string received. Ignoring\n" );
			}
			return;
		// Ignore; possibly a client who thinks he's still in a game, but isn't.
		case CLC_USERINFO:
		case CLC_QUIT:
		case CLC_STARTCHAT:
		case CLC_ENDCHAT:
		case CLC_ENTERCONSOLE:
		case CLC_EXITCONSOLE:
		case CLC_IGNORE:
		case CLC_SAY:
		case CLC_CLIENTMOVE:
		case CLC_MISSINGPACKET:
		case CLC_PONG:
		case CLC_WEAPONSELECT:
		case CLC_TAUNT:
		case CLC_SPECTATE:
		case CLC_REQUESTJOIN:
		case CLC_REQUESTRCON:
		case CLC_RCONCOMMAND:
		case CLC_SUICIDE:
		case CLC_CHANGETEAM:
		case CLC_SPECTATEINFO:
		case CLC_GENERICCHEAT:
		case CLC_GIVECHEAT:
		case CLC_SUMMONCHEAT:
		case CLC_READYTOGOON:
		case CLC_CHANGEDISPLAYPLAYER:
		case CLC_AUTHENTICATELEVEL:
		case CLC_CALLVOTE:
		case CLC_VOTEYES:
		case CLC_VOTENO:
		case CLC_INVENTORYUSEALL:
		case CLC_INVENTORYUSE:
		case CLC_INVENTORYDROP:
		case CLC_SUMMONFRIENDCHEAT:
		case CLC_SUMMONFOECHEAT: 
		case CLC_PUKE:
		case CLC_MORPHEX:

			// [BB] After a map change with the CCMD map, legitimate clients may get caught by
			// this. Since the packet is completely ignored anyway, there is no need to ban the
			// IP for ten seconds.
			/*
			Printf( "CLC command (%d) from someone not in game (%s). Ignoring IP for 10 seconds.\n", static_cast<int> (lCommand), NETWORK_AddressToString( NETWORK_GetFromAddress( )));
			// [BB] Block all further challenges of this IP for ten seconds to prevent log flooding.
			g_floodProtectionIPQueue.addAddress ( NETWORK_GetFromAddress( ), g_lGameTime / 1000 );
			*/

			return;
		// [BB] 200 was CLCC_ATTEMPTCONNECTION in 97d-beta4.3 and earlier versions.
		case 200: 
			Printf( "Challenge (%d) from (%s). Likely an old client (97d-beta4.3 or older) trying to connect. Informing the client and ignoring IP for 10 seconds.\n", static_cast<int> (lCommand), NETWORK_AddressToString( NETWORK_GetFromAddress( )));
			// [BB] Block all further challenges of this IP for ten seconds to prevent log flooding.
			g_floodProtectionIPQueue.addAddress ( NETWORK_GetFromAddress( ), g_lGameTime / 1000 );
			// [BB] Try to tell the client in a 97d-beta4.3 compatible way, that his version is too old.
			{
				NETBUFFER_s	TempBuffer;

				NETWORK_InitBuffer( &TempBuffer, MAX_UDP_PACKET, BUFFERTYPE_WRITE );
				NETWORK_ClearBuffer( &TempBuffer );

				// Make sure the packet has a packet header. The client is expecting this!
				NETWORK_WriteHeader( &TempBuffer.ByteStream, 0 /* = SVC_HEADER in 97d-beta4.3 */ );
				NETWORK_WriteLong( &TempBuffer.ByteStream, 0 );

				NETWORK_WriteByte( &TempBuffer.ByteStream, 254 /* = NETWORK_ERROR in 97d-beta4.3 */ );
				NETWORK_WriteByte( &TempBuffer.ByteStream, 1 /* = NETWORK_ERRORCODE_WRONGVERSION in 97d-beta4.3 */ );
				FString versionStringMessage;
				versionStringMessage.Format ( "%s\nYou are way out of the loop :)", DOTVERSIONSTR );
				NETWORK_WriteString( &TempBuffer.ByteStream, versionStringMessage.GetChars() );

				NETWORK_LaunchPacket( &TempBuffer, NETWORK_GetFromAddress( ) );
				NETWORK_FreeBuffer( &TempBuffer );
			}

			return;
		default:

			Printf( "Unknown challenge (%d) from %s. Ignoring IP for 10 seconds.\n", static_cast<int> (lCommand), NETWORK_AddressToString( NETWORK_GetFromAddress( )));
			// [BB] Block all further challenges of this IP for ten seconds to prevent log flooding.
			g_floodProtectionIPQueue.addAddress ( NETWORK_GetFromAddress( ), g_lGameTime / 1000 );

#ifdef CREATE_PACKET_LOG
			server_LogPacket(pByteStream,  NETWORK_GetFromAddress( ), "Unknown connection challenge.");
#endif
			return;
		}
	}

	// Don't handle connection attempts from clients if we're in intermission.
	if ( gamestate != GS_LEVEL )
		return;

	// Setup a new player (setup CLIENT_t and player_t)
	SERVER_SetupNewConnection( pByteStream, true );
}

//*****************************************************************************
//
void SERVER_SetupNewConnection( BYTESTREAM_s *pByteStream, bool bNewPlayer )
{
	LONG			lClient;
	FString			clientVersion;
	FString			clientPassword;
	char			szServerPassword[MAX_NETWORK_STRING];
	LONG			lClientNetworkGameVersion;
	char			szAddress[4][4];
	UCVarValue		Val;
	ULONG			ulIdx;
	NETADDRESS_s	AddressFrom;
	bool			bAdminClientConnecting;

	// Grab the IP address of the packet we've just received.
	AddressFrom = NETWORK_GetFromAddress( );

	// [RC] Prevent the fakeplayers exploit. 
	{
		// [RC] Count how many other clients are using this IP.
		ULONG ulOtherClientsFromIP = 0;
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( NETWORK_CompareAddress( AddressFrom, g_aClients[ulIdx].Address, true ) )
				ulOtherClientsFromIP++;
		}

		// [RC] Kick if necessary.
		if ( sv_maxclientsperip > 0 && (static_cast<LONG>(ulOtherClientsFromIP) >= sv_maxclientsperip) )
		{
			// Printf( "Connection from %s refused: too many connections from that IP. (sv_maxclientsperip is %d.)", NETWORK_AddressToString( AddressFrom ),  sv_maxclientsperip );
			SERVER_ConnectionError( AddressFrom, "Too many connections from this IP.", NETWORK_ERRORCODE_TOOMANYCONNECTIONSFROMIP );
			return;
		}
	}

	if ( bNewPlayer )
	{
		// [BB]: In case of emergency you should be able to join your own server,
		// even if it is full. Here we check, if the connection request comes from
		// localhost, i.e. 127.0.0.1. If it does, we allow a connect even in case of
		// SERVER_CalcNumPlayers( ) >= sv_maxclients as long as SERVER_FindFreeClientSlot( )
		// finds a free slot.
		bAdminClientConnecting = false;
		if ( g_AdminIPList.isIPInList ( AddressFrom ) )
			bAdminClientConnecting = true;

		// Try to find a player slot for the connecting client.
		lClient = SERVER_FindFreeClientSlot( );

		// If the server is full, send him a packet saying that it is.
		if (( lClient == -1 ) || ( SERVER_CalcNumPlayers( ) >= static_cast<unsigned> (sv_maxclients) && !bAdminClientConnecting ))
		{
			// Tell the client a packet saying the server is full.
			SERVER_ConnectionError( AddressFrom, "Server is full.", NETWORK_ERRORCODE_SERVERISFULL );

			// User sent the version, password, start as spectator, restore frags, and network protcol version along with the challenge.
			NETWORK_ReadString( pByteStream );
			NETWORK_ReadString( pByteStream );
			NETWORK_ReadByte( pByteStream );
			NETWORK_ReadByte( pByteStream );
			NETWORK_ReadByte( pByteStream );
			// [BB] Lump authentication string.
			NETWORK_ReadString( pByteStream );
			return;
		}
	}
	else
		lClient = g_lCurrentClient;

	if ( g_aClients[lClient].State == CLS_SPAWNED )
		SERVER_DisconnectClient( lClient, false, true );

	// Read in the client version info.
	clientVersion = NETWORK_ReadString( pByteStream );

	// Read in the client's password.
	clientPassword = NETWORK_ReadString( pByteStream );
	clientPassword.ToUpper();

	// Read in whether or not the client wants to start as a spectator.
	g_aClients[lClient].bWantStartAsSpectator = !!NETWORK_ReadByte( pByteStream );

	// Read in whether or not the client wants to start as a spectator.
	g_aClients[lClient].bWantNoRestoreFrags = !!NETWORK_ReadByte( pByteStream );

	// Read in the client's network game version.
	lClientNetworkGameVersion = NETWORK_ReadByte( pByteStream );

	NETWORK_ClearBuffer( &g_aClients[lClient].PacketBuffer );
	NETWORK_ClearBuffer( &g_aClients[lClient].SavedPacketBuffer );
	NETWORK_ClearBuffer( &g_aClients[lClient].UnreliablePacketBuffer );
	for ( ulIdx = 0; ulIdx < PACKET_BUFFER_SIZE; ulIdx++ )
	{
		g_aClients[lClient].lPacketBeginning[ulIdx] = 0;
		g_aClients[lClient].lPacketSize[ulIdx] = 0;
		g_aClients[lClient].lPacketSequence[ulIdx] = 0;
	}
	g_aClients[lClient].ulPacketSequence = 0;
	memset( g_aClients[lClient].SavedPacketBuffer.pbData, -1, sizeof ( g_aClients[lClient].SavedPacketBuffer.pbData ));
	memset( g_aClients[lClient].UnreliablePacketBuffer.pbData, -1, sizeof ( g_aClients[lClient].UnreliablePacketBuffer.pbData ));

	// Who is connecting?
	Printf( "Connect (v%s): %s\n", clientVersion.GetChars(), NETWORK_AddressToString( NETWORK_GetFromAddress( )));

	// Setup the client.
	g_aClients[lClient].State = CLS_CHALLENGE;
	g_aClients[lClient].Address = AddressFrom;

	{
		// Make sure the version matches.
		if ( stricmp( clientVersion.GetChars(), DOTVERSIONSTR ) != 0 )
		{
			SERVER_ClientError( lClient, NETWORK_ERRORCODE_WRONGVERSION );
#ifdef CREATE_PACKET_LOG
			server_LogPacket(pByteStream,  NETWORK_GetFromAddress( ), "Wrong version.");
#endif
			return;
		}
	}

	// Make sure the network game version matches.
	if ( NETGAMEVERSION != lClientNetworkGameVersion )
	{
		SERVER_ClientError( lClient, NETWORK_ERRORCODE_WRONGPROTOCOLVERSION );
#ifdef CREATE_PACKET_LOG
		server_LogPacket(pByteStream,  NETWORK_GetFromAddress( ), "Wrong netcode version.");
#endif

		return;
	}

	// Client is now in the loop, prevent from a early timeout
	// [BB] Only do this if this is actually a new player connecting. Otherwise it
	// could be abused to keep non-finished connections alive.
	if ( bNewPlayer )
		g_aClients[lClient].ulLastCommandTic = g_aClients[lClient].ulLastGameTic = gametic;

	// Check if we require a password to join this server.
	Val = sv_password.GetGenericRep( CVAR_String );
	if (( sv_forcepassword ) && ( strlen( Val.String )))
	{
		// Store password in temporary buffer (becuase we strupr it).
		strcpy( szServerPassword, Val.String );

		// Check their password against ours (both not case sensitive).
		if ( strcmp( strupr( szServerPassword ), clientPassword.GetChars() ) != 0 )
		{
			// Client has the wrong password! GET THE FUCK OUT OF HERE!
			SERVER_ClientError( lClient, NETWORK_ERRORCODE_WRONGPASSWORD );
			return;
		}
	}

	// Check if this IP has been banned.
	NETWORK_AddressToIPStringArray( g_aClients[lClient].Address, szAddress );
	if (( sv_enforcebans ) && ( SERVERBAN_IsIPBanned( szAddress )))
	{
		// Client has been banned! GET THE FUCK OUT OF HERE!
		SERVER_ClientError( lClient, NETWORK_ERRORCODE_BANNED );
		return;
	}

	if ( sv_pure && strcmp ( NETWORK_ReadString( pByteStream ), g_lumpsAuthenticationChecksum.GetChars() ) )
	{
		// Client fails the lump authentication.
		SERVER_ClientError( lClient, NETWORK_ERRORCODE_AUTHENTICATIONFAILED );
		return;
	}

	// Client is now connected to the server.
	g_aClients[lClient].State = CLS_CONNECTED;

	// Reset stats, and a couple other things.
	players[lClient].fragcount = 0;
	players[lClient].killcount = 0;
	players[lClient].lPointCount = 0;
	players[lClient].ulWins = 0;
	PLAYER_ResetSpecialCounters ( &players[lClient] );
	players[lClient].ulDeathCount = 0;
	players[lClient].ulTime = 0;
	players[lClient].bSpectating = false;
	players[lClient].bDeadSpectator = false;
	players[lClient].ulTeam = teams.Size( );
	players[lClient].bOnTeam = false;

	g_aClients[lClient].bRCONAccess = false;
	g_aClients[lClient].ulDisplayPlayer = lClient;
	g_aClients[lClient].commandInstances.clear();
	g_aClients[lClient].minorCommandInstances.clear();
	for ( ulIdx = 0; ulIdx < MAX_CHATINSTANCE_STORAGE; ulIdx++ )
		g_aClients[lClient].lChatInstances[ulIdx] = 0;
	g_aClients[lClient].ulLastChatInstance = 0;
	for ( ulIdx = 0; ulIdx < MAX_USERINFOINSTANCE_STORAGE; ulIdx++ )
		g_aClients[lClient].lUserInfoInstances[ulIdx] = 0;
	g_aClients[lClient].ulLastUserInfoInstance = 0;
	g_aClients[lClient].ulLastChangeTeamTime = 0;
	g_aClients[lClient].ulLastSuicideTime = 0;
	g_aClients[lClient].lLastPacketLossTick = 0;
	g_aClients[lClient].lLastMoveTick = 0;
	g_aClients[lClient].lOverMovementLevel = 0;
	g_aClients[lClient].bRunEnterScripts = false;
	g_aClients[lClient].szSkin[0] = 0;
	g_aClients[lClient].IgnoredAddresses.clear();

	// [BB] Inform the client that he is connected and needs to authenticate the map.
	SERVER_RequestClientToAuthenticate( lClient );
}

//*****************************************************************************
//
bool SERVER_GetUserInfo( BYTESTREAM_s *pByteStream, bool bAllowKick )
{
	ULONG		ulFlags;
	player_t	*pPlayer;
	FString		nameString;
	char		szSkin[64];
	char		szClass[64];
	char		szOldPlayerName[64];
	ULONG		ulUserInfoInstance;
	// [BB] We may only kick the player after completely parsing the current message,
	// otherwise we'll get network parsing errors.
	bool		bKickPlayer = false;
	FString kickReason;

	pPlayer = &players[g_lCurrentClient];

	if ( bAllowKick )
	{
		ulUserInfoInstance = ( ++g_aClients[g_lCurrentClient].ulLastUserInfoInstance % MAX_USERINFOINSTANCE_STORAGE );
		g_aClients[g_lCurrentClient].lUserInfoInstances[ulUserInfoInstance] = gametic;

		// If this is the second time a player has updated their userinfo in a 7 tick interval (~1/5 of a second, ~1/5 of a second update interval),
		// kick him.
		if (( g_aClients[g_lCurrentClient].lUserInfoInstances[( ulUserInfoInstance + MAX_USERINFOINSTANCE_STORAGE - 1 ) % MAX_USERINFOINSTANCE_STORAGE] ) > 0 )
		{
			if ( ( g_aClients[g_lCurrentClient].lUserInfoInstances[ulUserInfoInstance] ) -
				( g_aClients[g_lCurrentClient].lUserInfoInstances[( ulUserInfoInstance + MAX_USERINFOINSTANCE_STORAGE - 1 ) % MAX_USERINFOINSTANCE_STORAGE] )
				<= 7 )
			{
				bKickPlayer = true;
				kickReason = "User info change flood.";
			}
		}

		// If this is the third time a player has updated their userinfo in a 42 tick interval (~1.5 seconds, ~.75 second update interval),
		// kick him.
		if (( g_aClients[g_lCurrentClient].lUserInfoInstances[( ulUserInfoInstance + MAX_USERINFOINSTANCE_STORAGE - 2 ) % MAX_USERINFOINSTANCE_STORAGE] ) > 0 )
		{
			if ( ( g_aClients[g_lCurrentClient].lUserInfoInstances[ulUserInfoInstance] ) -
				( g_aClients[g_lCurrentClient].lUserInfoInstances[( ulUserInfoInstance + MAX_USERINFOINSTANCE_STORAGE - 2 ) % MAX_USERINFOINSTANCE_STORAGE] )
				<= 42 )
			{
				bKickPlayer = true;
				kickReason = "User info change flood.";
			}
		}

		// If this is the fourth time a player has updated their userinfo in a 105 tick interval (~3 seconds, ~1 second interval ),
		// kick him.
		if (( g_aClients[g_lCurrentClient].lUserInfoInstances[( ulUserInfoInstance + MAX_USERINFOINSTANCE_STORAGE - 3 ) % MAX_USERINFOINSTANCE_STORAGE] ) > 0 )
		{
			if ( ( g_aClients[g_lCurrentClient].lUserInfoInstances[ulUserInfoInstance] ) -
				( g_aClients[g_lCurrentClient].lUserInfoInstances[( ulUserInfoInstance + MAX_USERINFOINSTANCE_STORAGE - 3 ) % MAX_USERINFOINSTANCE_STORAGE] )
				<= 105 )
			{
				bKickPlayer = true;
				kickReason = "User info change flood.";
			}
		}
	}

	// Read in which userinfo entries the client is sending us.
	ulFlags = NETWORK_ReadShort( pByteStream );

	// Read in the player's name.
	if ( ulFlags & USERINFO_NAME )
	{
		sprintf( szOldPlayerName, "%s", pPlayer->userinfo.netname );
		nameString = NETWORK_ReadString( pByteStream );

		if ( nameString.Len() > MAXPLAYERNAME )
			nameString.Truncate(MAXPLAYERNAME);

		// [RC] Remove bad characters from their username.
		strcpy( pPlayer->userinfo.netname, nameString.GetChars() );
		V_CleanPlayerName(nameString);

		// The user really shouldn't have an invalid name unless they are using a hacked executable.
		if ( nameString.Compare( pPlayer->userinfo.netname ) != 0 )
		{
			bKickPlayer = true;
			kickReason = "User name contains illegal characters." ;
		}

		// [BB] It's extremely critical that we NEVER use the uncleaned player name!
		// If this is omitted, for example the RCON password can be optained by just using
		// a tampered name.
		strcpy( pPlayer->userinfo.netname, nameString.GetChars() );

		if ( g_aClients[g_lCurrentClient].State == CLS_SPAWNED )
		{
			char	szPlayerNameNoColor[32];
			char	szOldPlayerNameNoColor[32];

			sprintf( szPlayerNameNoColor, "%s", pPlayer->userinfo.netname );
			sprintf( szOldPlayerNameNoColor, "%s", szOldPlayerName );

			V_ColorizeString( szPlayerNameNoColor );
			V_ColorizeString( szOldPlayerNameNoColor );
			V_RemoveColorCodes( szPlayerNameNoColor );
			V_RemoveColorCodes( szOldPlayerNameNoColor );

			if ( stricmp( szPlayerNameNoColor, szOldPlayerNameNoColor ) != 0 )
			{
				SERVER_Printf( PRINT_HIGH, "%s \\c-is now known as %s\n", szOldPlayerName, pPlayer->userinfo.netname );

				// [RC] Update clients using the RCON utility.
				SERVER_RCON_UpdateInfo( SVRCU_PLAYERDATA );
			}
		}
	}

	// Read in gender, color, and aim distance.
	// [BB] Make sure that the gender is valid.
	if ( ulFlags & USERINFO_GENDER )
		pPlayer->userinfo.gender = clamp ( NETWORK_ReadByte( pByteStream ), 0, 2 );
	if ( ulFlags & USERINFO_COLOR )
		pPlayer->userinfo.color = NETWORK_ReadLong( pByteStream );
	if ( ulFlags & USERINFO_AIMDISTANCE )
		pPlayer->userinfo.aimdist = NETWORK_ReadLong( pByteStream );

	// Read in the player's skin, and make sure it's valid.
	if ( ulFlags & USERINFO_SKIN )
		strncpy( szSkin, NETWORK_ReadString( pByteStream ), 63 );
	else
		szSkin[0] = '\0';

	// Read in the player's railgun color.
	if ( ulFlags & USERINFO_RAILCOLOR )
		pPlayer->userinfo.lRailgunTrailColor = NETWORK_ReadLong( pByteStream );

	// Read in the player's handicap.
	if ( ulFlags & USERINFO_HANDICAP )
	{
		pPlayer->userinfo.lHandicap = NETWORK_ReadByte( pByteStream );
		if ( pPlayer->userinfo.lHandicap < 0 )
			pPlayer->userinfo.lHandicap = 0;
		else if ( pPlayer->userinfo.lHandicap > deh.MaxSoulsphere )
			pPlayer->userinfo.lHandicap = deh.MaxSoulsphere;
	}

	// If this is a Hexen game, read in the player's class.
	if ( ulFlags & USERINFO_PLAYERCLASS )
		strncpy( szClass, NETWORK_ReadString( pByteStream ), 63 );
	else
		szClass[0] = 0;

	if ( szClass[0] )
	{
		pPlayer->userinfo.PlayerClass = D_PlayerClassToInt( szClass );

		// If the player class is changed, we also have to reset cls.
		// Otherwise cls will not be reinitialized in P_SpawnPlayer. 
		pPlayer->cls = NULL;
	}

	// Now that we've (maybe) read in the player's class information, try to find a skin
	// for him based on his class.
	if ( szSkin[0] )
	{
		// Store the name of the skin the client gave us, so others can view the skin
		// even if the server doesn't have the skin loaded.
		strncpy( g_aClients[g_lCurrentClient].szSkin, szSkin, MAX_SKIN_NAME + 1 );

		// [BB] This can't be done if PlayerClass == -1, but shouldn't be necessary anyway,
		// since it's done as soon as the player is spawned in P_SpawnPlayer.
		if ( pPlayer->userinfo.PlayerClass != -1 )
			pPlayer->userinfo.skin = R_FindSkin( szSkin, pPlayer->userinfo.PlayerClass );
	}

	if ( bKickPlayer )
	{
		SERVER_KickPlayer( g_lCurrentClient, kickReason.GetChars() );
		return ( false );
	}

	// Now send out the player's userinfo out to other players.
	SERVERCOMMANDS_SetPlayerUserInfo( g_lCurrentClient, ulFlags, g_lCurrentClient, SVCF_SKIPTHISCLIENT );

	// Also, update the scoreboard.
	SERVERCONSOLE_UpdatePlayerInfo( g_lCurrentClient, UDF_NAME );

	// Success!
	return ( true );
}

//*****************************************************************************
//
void SERVER_ConnectionError( NETADDRESS_s Address, const char *pszMessage, ULONG ulErrorCode )
{
	NETBUFFER_s	TempBuffer;

	NETWORK_InitBuffer( &TempBuffer, MAX_UDP_PACKET, BUFFERTYPE_WRITE );
	NETWORK_ClearBuffer( &TempBuffer );

	// Display error message locally in the console.
	Printf( "Denied connection for %s: %s\n", NETWORK_AddressToString( Address ), pszMessage );

	// Make sure the packet has a packet header. The client is expecting this!
	NETWORK_WriteHeader( &TempBuffer.ByteStream, SVC_HEADER );
	NETWORK_WriteLong( &TempBuffer.ByteStream, 0 );

	NETWORK_WriteByte( &TempBuffer.ByteStream, SVCC_ERROR );
	NETWORK_WriteByte( &TempBuffer.ByteStream, ulErrorCode );

//	NETWORK_LaunchPacket( TempBuffer, Address, true );
	NETWORK_LaunchPacket( &TempBuffer, Address );
	NETWORK_FreeBuffer( &TempBuffer );
}

//*****************************************************************************
//
void SERVER_ClientError( ULONG ulClient, ULONG ulErrorCode )
{
	NETWORK_WriteByte( &g_aClients[ulClient].PacketBuffer.ByteStream, SVCC_ERROR );
	NETWORK_WriteByte( &g_aClients[ulClient].PacketBuffer.ByteStream, ulErrorCode );

	// Display error message locally in the console.
	switch ( ulErrorCode )
	{
	case NETWORK_ERRORCODE_WRONGPASSWORD:

		Printf( "Incorrect password.\n" );
		break;
	case NETWORK_ERRORCODE_WRONGVERSION:

		Printf( "Incorrect version.\n" );

		// Tell the client what version this server using.
		NETWORK_WriteString( &g_aClients[ulClient].PacketBuffer.ByteStream, DOTVERSIONSTR );
		break;
	case NETWORK_ERRORCODE_BANNED:

		Printf( "Client banned.\n" );

		// Tell the client why he was banned, and when his ban expires.
		NETWORK_WriteString( &g_aClients[ulClient].PacketBuffer.ByteStream, SERVERBAN_GetBanList( )->getEntryComment( g_aClients[ulClient].Address ));
		NETWORK_WriteLong( &g_aClients[ulClient].PacketBuffer.ByteStream, (LONG) SERVERBAN_GetBanList( )->getEntryExpiration( g_aClients[ulClient].Address ));
		break;
	case NETWORK_ERRORCODE_AUTHENTICATIONFAILED:

		Printf( "Authentication failed.\n" );
		break;
	case NETWORK_ERRORCODE_FAILEDTOSENDUSERINFO:

		Printf( "Failed to send userinfo.\n" );
		break;
	}

	// Send the packet off.
	SERVER_SendClientPacket( ulClient, true );

	Printf( "%s \\c-disconnected.\n", NETWORK_AddressToString( g_aClients[ulClient].Address ));

	memset( &g_aClients[ulClient].Address, 0, sizeof( g_aClients[ulClient].Address ));
	g_aClients[ulClient].State = CLS_FREE;
	g_aClients[ulClient].ulLastGameTic = 0;
	playeringame[ulClient] = false;
}

//*****************************************************************************
//
void SERVER_SendFullUpdate( ULONG ulClient )
{
	AActor						*pActor;
	ULONG						ulIdx;
	player_t*					pPlayer;
	AInventory					*pInventory;
	TThinkerIterator<AActor>	Iterator;

	// Send active players to the client.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( ulClient == ulIdx ) || ( playeringame[ulIdx] == false ))
			continue;

		pPlayer = &players[ulIdx];
		if ( pPlayer->mo == NULL )
			continue;

		SERVERCOMMANDS_SpawnPlayer( ulIdx, PST_REBORNNOINVENTORY, ulClient, SVCF_ONLYTHISCLIENT );
		SERVERCOMMANDS_SetPlayerUserInfo( ulIdx, USERINFO_ALL, ulClient, SVCF_ONLYTHISCLIENT );

		// Also send this player's team.
		if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
			SERVERCOMMANDS_SetPlayerTeam( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );

		// Check if we need to tell the incoming player about any powerups this player may have.
		// [BB] Also tell about all the ammo this player has.
		for ( pInventory = pPlayer->mo->Inventory; pInventory != NULL; pInventory = pInventory->Inventory )
		{
			if ( pInventory->IsKindOf( RUNTIME_CLASS( APowerup )))
			{
				SERVERCOMMANDS_GivePowerup( ulIdx, static_cast<APowerup *>( pInventory ), ulClient, SVCF_ONLYTHISCLIENT );
				if (( pInventory->IsKindOf( RUNTIME_CLASS( APowerInvulnerable ))) &&
					(( pPlayer->mo->effects & FX_VISIBILITYFLICKER ) || ( pPlayer->mo->effects & FX_RESPAWNINVUL )))
				{
					SERVERCOMMANDS_PlayerRespawnInvulnerability( ulIdx );
				}
			}
			else if ( pInventory->IsKindOf( RUNTIME_CLASS( AAmmo )))
				SERVERCOMMANDS_GiveInventory( ulIdx, pInventory, ulClient, SVCF_ONLYTHISCLIENT );
		}

		// Also if this player is currently dead, let the incoming player know that.
		if ( pPlayer->mo->health <= 0 )
			SERVERCOMMANDS_ThingIsCorpse( pPlayer->mo, ulClient, SVCF_ONLYTHISCLIENT );
		// [BB] SERVERCOMMANDS_SpawnPlayer instructs the client to spawn a player
		// with default health. So we still need to send the correct current health value
		// (if the player is not dead).
		else
			SERVERCOMMANDS_SetPlayerHealth( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );

		// If the client receiving the update is a spectator, send the player's
		// ready weapon.
		if ( PLAYER_IsTrueSpectator( &players[ulClient] ))
			SERVERCOMMANDS_WeaponChange( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );

		// [BB] It's possible that the MaxHealth property was changed dynamically with ACS, so send it.
		SERVERCOMMANDS_SetPlayerMaxHealth( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );
	}

	// Server may have already picked a team for the incoming player. If so, tell him!
	if (( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) && players[ulClient].bOnTeam )
		SERVERCOMMANDS_SetPlayerTeam( ulClient, ulClient, SVCF_ONLYTHISCLIENT );

	// [BB] This game mode uses teams, so inform the incoming player about the scores/wins/frags of the teams.
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS )
	{
		for ( ulIdx = 0; ulIdx < teams.Size( ); ulIdx++ )
		{
			if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS )
				SERVERCOMMANDS_SetTeamWins( ulIdx, TEAM_GetWinCount( ulIdx ), false, ulClient, SVCF_ONLYTHISCLIENT );
			else if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNPOINTS )
				SERVERCOMMANDS_SetTeamScore( ulIdx, TEAM_GetScore( ulIdx ), false, ulClient, SVCF_ONLYTHISCLIENT );
			else if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNFRAGS )
				SERVERCOMMANDS_SetTeamFrags( ulIdx, TEAM_GetFragCount( ulIdx ), false, ulClient, SVCF_ONLYTHISCLIENT );
		}
	}

	// [BB] Tell individual player scores/wins/frags/kills to the client.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] == false )
			continue;

		// [BB] In all cooperative game modes players get kills, otherwise they get frags
		// (even if the game mode is not won with frags).
		if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_COOPERATIVE )
			SERVERCOMMANDS_SetPlayerKillCount( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );
		else
			SERVERCOMMANDS_SetPlayerFrags( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );

		if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNWINS )
			SERVERCOMMANDS_SetPlayerWins( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );
		else if ( GAMEMODE_GetFlags(GAMEMODE_GetCurrentMode()) & GMF_PLAYERSEARNPOINTS )
			SERVERCOMMANDS_SetPlayerPoints( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );
	}

	// Send Domination State
	if ( domination )
	{
		SERVERCOMMANDS_SetDominationState( ulClient, SVCF_ONLYTHISCLIENT );
	}

	// If we're in duel mode, tell the client how many duels have taken place.
	if ( duel )
		SERVERCOMMANDS_SetDuelNumDuels( ulClient, SVCF_ONLYTHISCLIENT );

	// Send the level time.
	if ( timelimit )
		SERVERCOMMANDS_SetMapTime( ulClient, SVCF_ONLYTHISCLIENT );

	// Go through all the items on the map, and tell the client to spawn those of which
	// are important.
    while (( pActor = Iterator.Next( )))
    {
		// If the actor doesn't have a network ID, don't spawn it (it
		// probably isn't important).
		if ( pActor->lNetID == -1 )
			continue;

		// Don't spawn players, items about to be deleted, inventory items
		// that have an owner, or items that the client spawns himself.
		// [BB] Don't spawn dead actors that are not corpses.
		// [BB] The ( pActor->health <= 0 ) check is necessary, otherwise
		// things like DeadDemon are not spawned.
		if (( pActor->IsKindOf( RUNTIME_CLASS( APlayerPawn ))) ||
			( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HoldAndDestroy") ) ||	// S_HOLDANDDESTROY
			( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("Held") ) || // S_HELD
			( pActor->ulNetworkFlags & NETFL_ALLOWCLIENTSPAWN ) ||
			(( pActor->health <= 0 ) && (( pActor->flags & MF_COUNTKILL ) == false ) && ( pActor->InDeathState( ))))
		{
			continue;
		}

		// [BB] Don't spawn things hidden by AActor::HideOrDestroyIfSafe().
		// The clients don't need them at all, since the server will tell
		// them to spawn a new actor during GAME_ResetMap anyway.
		if ( !( pActor->IsKindOf( RUNTIME_CLASS( AInventory ) ) )
		     && ( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideIndefinitely") ) // S_HIDEINDEFINITELY 
		   )
		{
			continue;
		}

		// Spawn a missile. Missiles must be handled differently because they have
		// velocity.
		if ( pActor->flags & MF_MISSILE )
		{
			SERVERCOMMANDS_SpawnMissile( pActor, ulClient, SVCF_ONLYTHISCLIENT );
		}
		// Tell the client to spawn this thing.
		else
		{
			SERVERCOMMANDS_SpawnThing( pActor, ulClient, SVCF_ONLYTHISCLIENT );
			// [BB] If the thing is not at its spawn point, let the client know about the spawn point.
			if ( ( pActor->x != pActor->SpawnPoint[0] )
				|| ( pActor->y != pActor->SpawnPoint[1] )
				|| ( pActor->z != pActor->SpawnPoint[2] )
				)
			{
				SERVERCOMMANDS_SetThingSpawnPoint( pActor, ulClient, SVCF_ONLYTHISCLIENT );
			}

			// [BB] Since the monster movement is client side, the client needs to be
			// informed about the momentum and the current state. If the frame is not
			// set, the client thinks the actor is in its spawn state.
			{

				if ( (pActor->InSpawnState() == false)
					 && !(( pActor->health <= 0 ) && ( pActor->flags & MF_COUNTKILL )) // [BB] Corpses are handled later.
					 )
				{
					SERVERCOMMANDS_SetThingFrame( pActor, pActor->state, ulClient, SVCF_ONLYTHISCLIENT, false );
				}

				ULONG ulBits = 0;

				if ( pActor->momx != 0 )
					ulBits |= CM_MOMX;

				if ( pActor->momy != 0 )
					ulBits |= CM_MOMY;

				if ( pActor->momz != 0 )
					ulBits |= CM_MOMZ;

				if ( ulBits != 0 )
					SERVERCOMMANDS_MoveThingExact( pActor, ulBits, ulClient, SVCF_ONLYTHISCLIENT );
			}

			// If it's important to update this thing's arguments, do that now.
			// [BB] Wouldn't it be better, if this is done for all things, for which
			// at least one of the arguments is not equal to zero?
			// [BC] It's not necessarily important for clients to know this, such
			// as with invasion spawners. You can do it if you want, though! It would
			// probably save headache later on.
			//if ( pActor->ulNetworkFlags & NETFL_UPDATEARGUMENTS )
			// [BB] I don't want to export NETFL_UPDATEARGUMENTS to DECORATE, so we have
			// to tell the clients all the arguments.
			if ( ( pActor->args[0] != 0 )
				|| ( pActor->args[1] != 0 )
				|| ( pActor->args[2] != 0 )
				|| ( pActor->args[3] != 0 )
				|| ( pActor->args[4] != 0 ) )
				SERVERCOMMANDS_SetThingArguments( pActor, ulClient, SVCF_ONLYTHISCLIENT );

			// [BB] Some things like AMovingCamera rely on the AActor tid.
			// So tell it to the client. I have no idea if this has unwanted side
			// effects. Has to be checked.
			if ( pActor->tid != 0 )
				SERVERCOMMANDS_SetThingTID( pActor, ulClient, SVCF_ONLYTHISCLIENT );

			// If this thing's translation has been altered, tell the client.
			if ( pActor->Translation != 0 )
				SERVERCOMMANDS_SetThingTranslation( pActor, ulClient, SVCF_ONLYTHISCLIENT );

			// This item has been picked up, and is in its hidden, respawn state. Let
			// the client know that.
			if (( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideDoomish") ) ||	// S_HIDEDOOMISH
				( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideSpecial") ) ||	// S_HIDESPECIAL
				( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideIndefinitely") ))
			{
				SERVERCOMMANDS_HideThing( pActor, ulClient, SVCF_ONLYTHISCLIENT );
			}

			// Let the clients know if an object is dormant or not.
			if ( pActor->IsActive( ) == false )
				SERVERCOMMANDS_ThingDeactivate( pActor, NULL, ulClient, SVCF_ONLYTHISCLIENT );

			// Update the water level of the actor, but not if it's a player!
			if (( pActor->waterlevel > 0 ) && ( pActor->player == NULL ))
				SERVERCOMMANDS_SetThingWaterLevel( pActor, ulClient, SVCF_ONLYTHISCLIENT );

			// Update the actor's speed if it's changed.
			if ( pActor->Speed != pActor->GetDefault( )->Speed )
				SERVERCOMMANDS_SetThingProperty( pActor, APROP_Speed, ulClient, SVCF_ONLYTHISCLIENT  );

			// [BB] Update the actor's RenderStyle if it's changed.
			if ( pActor->RenderStyle.AsDWORD != pActor->GetDefault( )->RenderStyle.AsDWORD )
				SERVERCOMMANDS_SetThingProperty( pActor, APROP_RenderStyle, ulClient, SVCF_ONLYTHISCLIENT  );

			// [BB] Update the actor's alpha if it's changed.
			if ( pActor->alpha != pActor->GetDefault( )->alpha )
				SERVERCOMMANDS_SetThingProperty( pActor, APROP_Alpha, ulClient, SVCF_ONLYTHISCLIENT  );

			// If any of this actor's flags have changed during the course of the level, notify
			// the client.
/*
			if ( pActor->flags != pActor->GetDefault( )->flags )
				SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS, ulClient, SVCF_ONLYTHISCLIENT );
			if ( pActor->flags2 != pActor->GetDefault( )->flags2 )
				SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS2, ulClient, SVCF_ONLYTHISCLIENT );
			if ( pActor->flags3 != pActor->GetDefault( )->flags3 )
				SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS3, ulClient, SVCF_ONLYTHISCLIENT );
			if ( pActor->flags4 != pActor->GetDefault( )->flags4 )
				SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS4, ulClient, SVCF_ONLYTHISCLIENT );
			if ( pActor->flags5 != pActor->GetDefault( )->flags5 )
				SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS5, ulClient, SVCF_ONLYTHISCLIENT );
			if ( pActor->ulSTFlags != pActor->GetDefault( )->ulSTFlags )
				SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGSST, ulClient, SVCF_ONLYTHISCLIENT );
*/
			// [BB] Now that the ammo amount from weapon pickups is handled on the server
			// this shouldn't be necessary anymore. Remove after thorough testing.
			// If this is a weapon, tell the client how much ammo it gives.
			//if ( pActor->IsKindOf( RUNTIME_CLASS( AWeapon )))
			//	SERVERCOMMANDS_SetWeaponAmmoGive( pActor, ulClient, SVCF_ONLYTHISCLIENT );
		}

		// Check and see if it's important that the client know the angle of the object.
		if ( pActor->angle != 0 )
			SERVERCOMMANDS_SetThingAngle( pActor, ulClient, SVCF_ONLYTHISCLIENT );

		// Spawned monster is a corpse.
		if (( pActor->health <= 0 ) && ( pActor->flags & MF_COUNTKILL ))
			SERVERCOMMANDS_ThingIsCorpse( pActor, ulClient, SVCF_ONLYTHISCLIENT );
    }

	// Tell clients the found/total item count.
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_COOPERATIVE )
	{
		SERVERCOMMANDS_SetMapNumFoundItems( ulClient, SVCF_ONLYTHISCLIENT );
		SERVERCOMMANDS_SetMapNumTotalItems( ulClient, SVCF_ONLYTHISCLIENT );
	}

	// Also let the client know about any cameras set to textures.
	FCanvasTextureInfo::UpdateToClient( ulClient );

	// Send out any translations that have been edited since the start of the level.
	for ( ulIdx = 0; ulIdx < g_EditedTranslationList.Size( ); ulIdx++ )
	{
		if ( g_EditedTranslationList[ulIdx].ulType == DLevelScript::PCD_TRANSLATIONRANGE1 )
			SERVERCOMMANDS_CreateTranslation( g_EditedTranslationList[ulIdx].ulIdx, g_EditedTranslationList[ulIdx].ulStart, g_EditedTranslationList[ulIdx].ulEnd, g_EditedTranslationList[ulIdx].ulPal1, g_EditedTranslationList[ulIdx].ulPal2 );
		else
			SERVERCOMMANDS_CreateTranslation( g_EditedTranslationList[ulIdx].ulIdx, g_EditedTranslationList[ulIdx].ulStart, g_EditedTranslationList[ulIdx].ulEnd, g_EditedTranslationList[ulIdx].ulR1, g_EditedTranslationList[ulIdx].ulG1, g_EditedTranslationList[ulIdx].ulB1, g_EditedTranslationList[ulIdx].ulR2, g_EditedTranslationList[ulIdx].ulG2, g_EditedTranslationList[ulIdx].ulB2 );
	}

	// [BB] If the sky differs from the standard sky, let the client know about it.
	if ( level.info 
	     && ( ( stricmp( level.skypic1, level.info->skypic1 ) != 0 )
	          || ( stricmp( level.skypic2, level.info->skypic2 ) != 0 ) )
	   )
	{
		SERVERCOMMANDS_SetMapSky( ulClient, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVER_WriteCommands( void )
{
	AActor		*pActor;
	ULONG		ulIdx;
	
	// Ping clients and stuff.
	SERVER_SendHeartBeat( );

	// Don't need to update origin every tic. The server sends origin and velocity of a 
	// player and the client always knows origin on the next tic.
	if (( gametic % 3 ) == 0 )
	{
		// See if any players need to be updated to clients.
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] == false )
				continue;

			// [BB] You can be watching through the eyes of someone, even if you are not a spectator.
			// If this player is watching through the eyes of another player, send this
			// player some extra info about that player to make for a better watching
			// experience.
			if (( g_aClients[ulIdx].ulDisplayPlayer != ulIdx ) &&
				( g_aClients[ulIdx].ulDisplayPlayer <= MAXPLAYERS ) &&
				( players[g_aClients[ulIdx].ulDisplayPlayer].mo != NULL ))
			{
				SERVERCOMMANDS_UpdatePlayerExtraData( ulIdx, g_aClients[ulIdx].ulDisplayPlayer );
			}

			// Spectators can move around freely, without us telling it what to do (lag-less).
			if ( players[ulIdx].bSpectating ) 
			{
				// Don't send this to bots.
				if ((( gametic % ( 3 * TICRATE )) == 0 ) && ( players[ulIdx].bIsBot == false ) ) 
				{
					// Just send them one byte to let them know they're still alive.
					SERVERCOMMANDS_Nothing( ulIdx );
				}

				continue;
			}

			pActor = players[ulIdx].mo;
			if ( pActor == NULL )
				continue;

			SERVERCOMMANDS_MovePlayer( ulIdx, ulIdx, SVCF_SKIPTHISCLIENT );
			SERVERCOMMANDS_MoveLocalPlayer( ulIdx );
		}
	}

	// Once every four seconds, update each player's ping.
	if (( gametic % ( 4 * TICRATE )) == 0 )
	{
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( SERVER_IsValidClient( ulIdx ) == false )
				continue;

			// Tell everyone this player's ping.
			SERVERCOMMANDS_UpdatePlayerPing( ulIdx );

			// Also, update the scoreboard.
			SERVERCONSOLE_UpdatePlayerInfo( ulIdx, UDF_PING );
		}
	}

	// Once every minute, update the level time.
	if (( gametic % ( 60 * TICRATE )) == 0 )
		SERVERCOMMANDS_SetMapTime( );
}

//*****************************************************************************
//
bool SERVER_IsValidClient( ULONG ulClient )
{
	// Don't transmit data to players not in the game, or bots.
	if (( ulClient >= MAXPLAYERS ) || ( playeringame[ulClient] == false ) || ( players[ulClient].pSkullBot ))
		return ( false );

	return ( true );
}

//*****************************************************************************
//
bool SERVER_IsValidPlayer( ULONG ulPlayer )
{
	// Only transmit data about valid players.
	if (( ulPlayer >= MAXPLAYERS ) || ( playeringame[ulPlayer] == false ))
		return ( false );

	return ( true );
}

//*****************************************************************************
//
bool SERVER_IsValidPlayerWithMo( ULONG ulPlayer )
{
	return ( SERVER_IsValidPlayer ( ulPlayer ) && players[ulPlayer].mo );
}

//*****************************************************************************
//
void SERVER_DisconnectClient( ULONG ulClient, bool bBroadcast, bool bSaveInfo )
{
	ULONG	ulIdx;

	if ( bBroadcast )
	{
		if (( gametic - g_aClients[ulClient].ulLastCommandTic ) == ( CLIENT_TIMEOUT * 35 ) && players[ulClient].userinfo.netname )
			SERVER_Printf( PRINT_HIGH, "%s \\c-timed out.\n", players[ulClient].userinfo.netname );
		else if ( players[ulClient].userinfo.netname )
			SERVER_Printf( PRINT_HIGH, "client %s \\c-disconnected.\n", players[ulClient].userinfo.netname );
		else
			Printf( "%s \\c-disconnected.\n", NETWORK_AddressToString( g_aClients[ulClient].Address ));
	}

	// [BB] Morphed players need to be unmorphed before disconnecting.
	if (players[ulClient].morphTics)
		P_UndoPlayerMorph (&players[ulClient], &players[ulClient]);

	// Inform the other clients that this player has been disconnected.
	SERVERCOMMANDS_DisconnectPlayer( ulClient );

	// Potentially back up the player's score in the game, so that if he rejoins, he hasn't
	// lost everything.
	if ( bSaveInfo )
	{
		PLAYERSAVEDINFO_t	Info;

		Info.Address		= g_aClients[ulClient].Address;
		Info.lFragCount		= players[ulClient].fragcount;
		Info.lPointCount	= players[ulClient].lPointCount;
		Info.lWinCount		= players[ulClient].ulWins;
		Info.ulTime			= players[ulClient].ulTime; // [RC] Save time
		sprintf( Info.szName, "%s", players[ulClient].userinfo.netname );

		SERVER_SAVE_SaveInfo( &Info );
	}
	else
	{
		PLAYERSAVEDINFO_t	*pInfo;

		pInfo = SERVER_SAVE_GetSavedInfo( players[ulClient].userinfo.netname, g_aClients[ulClient].Address );
		if ( pInfo )
		{
			pInfo->Address.abIP[0] = 0;
			pInfo->Address.abIP[1] = 0;
			pInfo->Address.abIP[2] = 0;
			pInfo->Address.abIP[3] = 0;
			pInfo->Address.usPort = 0;
			pInfo->bInitialized = false;
			pInfo->lFragCount = 0;
			pInfo->lPointCount = 0;
			pInfo->lWinCount = 0;
			pInfo->szName[0] = 0;
		}
	}

	// If this player was eligible to get an assist, cancel that.
	TEAM_CancelAssistsOfPlayer ( ulClient );

	// Destroy the actor attached to the player.
	if ( players[ulClient].mo )
	{
		// [BB] Stop all scripts of the player that are still running.
		FBehavior::StaticStopMyScripts ( players[ulClient].mo );
		// If he's disconnecting while carrying an important item like a flag, etc., make sure he, 
		// drops it before he leaves.
		players[ulClient].mo->DropImportantItems( true );

		players[ulClient].mo->Destroy( );
		players[ulClient].mo = NULL;
	}
	// [BB] Destroy all voodoo dolls of that player.
	COOP_DestroyVoodooDollsOfPlayer ( ulClient );

	memset( &g_aClients[ulClient].Address, 0, sizeof( g_aClients[ulClient].Address ));
	g_aClients[ulClient].State = CLS_FREE;
	g_aClients[ulClient].ulLastGameTic = 0;
	playeringame[ulClient] = false;

	// Run the disconnect scripts now that the player is leaving.
	if (( players[ulClient].bSpectating == false ) ||
		( players[ulClient].bDeadSpectator ))
	{
		FBehavior::StaticStartTypedScripts( SCRIPT_Disconnect, NULL, true, ulClient );
	}

	// Redo the scoreboard.
	SERVERCONSOLE_ReListPlayers( );

	// [RC] Update clients using the RCON utility.
	SERVER_RCON_UpdateInfo( SVRCU_PLAYERDATA );

	// Clear the client's buffers.
	NETWORK_ClearBuffer( &g_aClients[ulClient].PacketBuffer );
	NETWORK_ClearBuffer( &g_aClients[ulClient].UnreliablePacketBuffer );
	NETWORK_ClearBuffer( &g_aClients[ulClient].SavedPacketBuffer );

	// Tell the join queue module that a player has left the game.
	if ( PLAYER_IsTrueSpectator( &players[ulClient] ) == false )
		JOINQUEUE_PlayerLeftGame( true );
	else
		JOINQUEUE_SpectatorLeftGame( ulClient );

	// If this player was the enemy of another bot, tell the bot.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) || ( players[ulIdx].pSkullBot == NULL ))
			continue;

		if ( players[ulIdx].pSkullBot->m_ulPlayerEnemy == ulClient )
			players[ulIdx].pSkullBot->m_ulPlayerEnemy = MAXPLAYERS;
	}

	// If nobody's left on the server, zero out the scores.
	if (( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) && ( SERVER_CalcNumPlayers( ) == 0 ))
	{
		for ( ULONG i = 0; i < teams.Size( ); i++ )
		{
			TEAM_SetScore( i, 0, false );
			TEAM_SetFragCount( i, 0, false );
			TEAM_SetDeathCount( i, 0 );
			TEAM_SetWinCount( i, 0, false );
		}

		// Also, potentially restart the map after 5 minutes.
		g_lMapRestartTimer = TICRATE * 60 * 5;

		// If playing Domination reset ownership
		if ( domination )
			DOMINATION_Reset();
	}
}

//*****************************************************************************
//
void SERVER_SendHeartBeat( void )
{
	ULONG	ulIdx;

	// Ping clients once every second.
	if (( gametic % ( 1 * TICRATE )) || ( gamestate == GS_INTERMISSION ))
		return;

	SERVERCOMMANDS_Ping( I_MSTime( ));

	// Save the time we pinged them at. The client's ping can be calculated
	// subtracting the client's pong from this.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		g_aClients[ulIdx].ulLastGameTic = I_MSTime( );
}

//*****************************************************************************
//
void STACK_ARGS SERVER_Printf( ULONG ulPrintLevel, const char *pszString, ... )
{
	va_list		argptr;
	char		szStringBuf[1024];

	va_start( argptr, pszString );
#ifdef _MSC_VER
	_vsnprintf( szStringBuf, 1024 - 1, pszString, argptr );
#else
	vsnprintf( szStringBuf, 1024 - 1, pszString, argptr );
#endif
	va_end( argptr );

	// Print message locally in console window.
	Printf( "%s", szStringBuf );

	// Send the message out to clients for them to print.
	SERVERCOMMANDS_Print( szStringBuf, ulPrintLevel );
}

//*****************************************************************************
//
void STACK_ARGS SERVER_PrintfPlayer( ULONG ulPrintLevel, ULONG ulPlayer, const char *pszString, ... )
{
	va_list		argptr;
	char		szStringBuf[1024];

	va_start( argptr, pszString );
#ifdef _MSC_VER
	_vsnprintf( szStringBuf, 1024 - 1, pszString, argptr );
#else
	vsnprintf( szStringBuf, 1024 - 1, pszString, argptr );
#endif
	va_end( argptr );

	// Send the message out to the player to print.
	SERVERCOMMANDS_Print( szStringBuf, ulPrintLevel, ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVER_UpdateSectors( ULONG ulClient )
{
	ULONG							ulIdx;
	sector_t						*pSector;
	FPolyObj						*pPoly;
	TThinkerIterator<DFireFlicker>	FireFlickerIterator;
	DFireFlicker					*pFireFlicker;
	TThinkerIterator<DFlicker>		FlickerIterator;
	DFlicker						*pFlicker;
	TThinkerIterator<DLightFlash>	LightFlashIterator;
	DLightFlash						*pLightFlash;
	TThinkerIterator<DStrobe>		StrobeIterator;
	DStrobe							*pStrobe;
	TThinkerIterator<DGlow>			GlowIterator;
	DGlow							*pGlow;
	TThinkerIterator<DGlow2>		Glow2Iterator;
	DGlow2							*pGlow2;
	TThinkerIterator<DPhased>		PhasedIterator;
	DPhased							*pPhased;

	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	for ( ulIdx = 0; static_cast<signed> (ulIdx) < numsectors; ulIdx++ )
	{
		pSector = &sectors[ulIdx];

		// Check and see if flats need to be updated.
		if ( pSector->bFlatChange )
			SERVERCOMMANDS_SetSectorFlat( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );

		// Update the floor heights.
		if ( pSector->bFloorHeightChange )
			SERVERCOMMANDS_SetSectorFloorPlane( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );

		// Update the ceiling heights.
		if ( pSector->bCeilingHeightChange )
			SERVERCOMMANDS_SetSectorCeilingPlane( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );

		// Update the panning.
		if (( pSector->GetXOffset(sector_t::ceiling) != 0 ) ||
			( pSector->GetYOffset(sector_t::ceiling,false) != 0 ) ||
			( pSector->GetXOffset(sector_t::floor) != 0 ) ||
			( pSector->GetYOffset(sector_t::floor,false) != 0 ))
		{
			SERVERCOMMANDS_SetSectorPanning( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );
		}

		// Update the sector color.
		if (( pSector->ColorMap->Color.r != 255 ) ||
			( pSector->ColorMap->Color.g != 255 ) ||
			( pSector->ColorMap->Color.b != 255 ) ||
			( pSector->ColorMap->Desaturate != 0 ))
		{
			SERVERCOMMANDS_SetSectorColor( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );
		}

		// Update the sector fade.
		if (( pSector->ColorMap->Fade.r != 0 ) ||
			( pSector->ColorMap->Fade.g != 0 ) ||
			( pSector->ColorMap->Fade.b != 0 ))
		{
			SERVERCOMMANDS_SetSectorFade( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );
		}

		// Update the sector's ceiling/floor rotation.
		if (( pSector->GetAngle(sector_t::ceiling,false) != 0 ) || ( pSector->GetAngle(sector_t::floor,false) != 0 ))
			SERVERCOMMANDS_SetSectorRotation( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );

		// Update the sector's ceiling/floor scale.
		if (( pSector->GetXScale(sector_t::ceiling) != FRACUNIT ) ||
			( pSector->GetYScale(sector_t::ceiling) != FRACUNIT ) ||
			( pSector->GetXScale(sector_t::floor) != FRACUNIT ) ||
			( pSector->GetYScale(sector_t::floor) != FRACUNIT ))
		{
			SERVERCOMMANDS_SetSectorScale( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );
		}

		// Update the sector's friction.
		if (( pSector->friction != ORIG_FRICTION ) ||
			( pSector->movefactor != ORIG_FRICTION_FACTOR ))
		{
			SERVERCOMMANDS_SetSectorFriction( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );
		}

		// Update the sector's angle/y-offset.
		if (( pSector->planes[sector_t::ceiling].xform.base_angle != 0 ) ||
			( pSector->planes[sector_t::ceiling].xform.base_yoffs != 0 ) ||
			( pSector->planes[sector_t::floor].xform.base_angle != 0 ) ||
			( pSector->planes[sector_t::floor].xform.base_yoffs != 0 ))
		{
			SERVERCOMMANDS_SetSectorAngleYOffset( ulIdx );
		}

		// Update the sector's gravity.
		if ( pSector->gravity != 1.0f )
			SERVERCOMMANDS_SetSectorGravity( ulIdx );

		// Update the sector's light level.
		if ( pSector->bLightChange )
			SERVERCOMMANDS_SetSectorLightLevel( ulIdx, ulClient, SVCF_ONLYTHISCLIENT );

		// Update the sector's reflection.
		if (( pSector->ceiling_reflect != 0.0f ) ||
			( pSector->floor_reflect != 0.0f ))
		{
			SERVERCOMMANDS_SetSectorReflection( ulIdx );
		}
	}

	for ( ulIdx = 0; static_cast<signed> (ulIdx) <= po_NumPolyobjs; ulIdx++ )
	{
		pPoly = GetPolyobjByIndex( ulIdx );
		if ( pPoly == NULL )
			continue;

		// Tell client if the position has been changed.
		if ( pPoly->bMoved )
			SERVERCOMMANDS_SetPolyobjPosition( pPoly->tag, ulClient, SVCF_ONLYTHISCLIENT );

		// Tell client if the rotation has been changed.
		if ( pPoly->bRotated )
			SERVERCOMMANDS_SetPolyobjRotation( pPoly->tag, ulClient, SVCF_ONLYTHISCLIENT );
	}

	// Check for various sector light effects. If we find any, tell the client about them.
	while (( pFireFlicker = FireFlickerIterator.Next( )) != NULL )
		pFireFlicker->UpdateToClient( ulClient );
	while (( pFlicker = FlickerIterator.Next( )) != NULL )
		pFlicker->UpdateToClient( ulClient );
	while (( pLightFlash = LightFlashIterator.Next( )) != NULL )
		pLightFlash->UpdateToClient( ulClient );
	while (( pStrobe = StrobeIterator.Next( )) != NULL )
		pStrobe->UpdateToClient( ulClient );
	while (( pGlow = GlowIterator.Next( )) != NULL )
		pGlow->UpdateToClient( ulClient );
	while (( pGlow2 = Glow2Iterator.Next( )) != NULL )
		pGlow2->UpdateToClient( ulClient );
	while (( pPhased = PhasedIterator.Next( )) != NULL )
		pPhased->UpdateToClient( ulClient );
}

//*****************************************************************************
//
void SERVER_UpdateLines( ULONG ulClient )
{
	ULONG		ulLine;

	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	for ( ulLine = 0; ulLine < (ULONG)numlines; ulLine++ )
	{
		// Have any of the textures changed?
		if ( lines[ulLine].ulTexChangeFlags )
			SERVERCOMMANDS_SetLineTexture( ulLine, ulClient, SVCF_ONLYTHISCLIENT );

		// Is the alpha of this line altered?
		if ( lines[ulLine].Alpha != lines[ulLine].SavedAlpha )
			SERVERCOMMANDS_SetLineAlpha( ulLine, ulClient, SVCF_ONLYTHISCLIENT );

		// Has the line's blocking status or the ML_ADDTRANS setting changed?
		if ( lines[ulLine].flags != lines[ulLine].SavedFlags )
			SERVERCOMMANDS_SetSomeLineFlags( ulLine, ulClient, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVER_UpdateSides( ULONG ulClient )
{
	ULONG		ulSide;

	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	for ( ulSide = 0; ulSide < (ULONG)numsides; ulSide++ )
	{
		// Have the side's flags changed?
		if ( sides[ulSide].Flags != sides[ulSide].SavedFlags )
			SERVERCOMMANDS_SetSideFlags( ulSide, ulClient, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVER_ReconnectNewLevel( const char *pszMapName )
{
	ULONG	ulIdx;

	// Tell clients to reconnect, and that the upcoming map will be pszMapName.
	SERVERCOMMANDS_MapNew( pszMapName );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] == false )
			continue;

		// Get rid of bots.
		if ( players[ulIdx].bIsBot )
		{
			BOTS_RemoveBot( ulIdx, false );
			continue;
		}

		// Make sure the commands we sent out get sent right away.
		SERVER_SendClientPacket( ulIdx, true );

		// Disconnect the client.
		SERVER_DisconnectClient( ulIdx, false, false );
	}
}

//*****************************************************************************
//
void SERVER_LoadNewLevel( const char *pszMapName )
{
	ULONG		ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		// Initialize this client's buffer.
		SERVER_SendClientPacket( ulIdx, true );
	}

	// Tell the client to authenticate his level.
	SERVERCOMMANDS_MapAuthenticate( pszMapName );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		// Send out the packet and clear out the client's buffer.
		SERVER_SendClientPacket( ulIdx, true );
	}

	// [BB] The clients who are authenticated, but still didn't finish loading
	// the map are not covered by the code above and need special treatment.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_GetClient( ulIdx )->State == CLS_AUTHENTICATED )
			SERVER_GetClient( ulIdx )->State = CLS_AUTHENTICATED_BUT_OUTDATED_MAP;
	}
}

//*****************************************************************************
//
void SERVER_KickAllPlayers( const char *pszReason )
{
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) )
			SERVER_KickPlayer( ulIdx, pszReason ? pszReason : "No reason given." );
	}
}

//*****************************************************************************
//
void SERVER_KickPlayer( ULONG ulPlayer, const char *pszReason )
{
	ULONG	ulIdx;
	char	szKickString[512];
	char	szName[64];

	// Make sure the target is valid and applicable.
	if (( ulPlayer >= MAXPLAYERS ) || ( !playeringame[ulPlayer] ))
		return;

	sprintf( szName, "%s", players[ulPlayer].userinfo.netname );
	V_RemoveColorCodes( szName );

	// Build the full kick string.
	sprintf( szKickString, "\\ci%s\\ci was kicked from the server! Reason: %s\n", szName, pszReason );
	Printf( "%s", szKickString );

	// Rebuild the string that will be displayed to clients. This time, color codes are allowed.
	sprintf( szKickString, "\\ci%s\\ci was kicked from the server! Reason: %s\n", players[ulPlayer].userinfo.netname, pszReason );

	// Send the message out to all clients.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		SERVER_PrintfPlayer( PRINT_HIGH, ulIdx, szKickString );
	}

	// If we're kicking a bot, just remove him.
	if ( players[ulPlayer].bIsBot )
		BOTS_RemoveBot( ulPlayer, true );
	else
	{
		// Tell the player that he's been kicked.
		SERVERCOMMANDS_ConsolePlayerKicked( ulPlayer );
		SERVER_SendClientPacket( ulPlayer, true );

		// Tell the other players that this player has been kicked.
		SERVER_DisconnectClient( ulPlayer, true, false );
	}
}

//*****************************************************************************
//
void SERVER_KickPlayerFromGame( ULONG ulPlayer, const char *pszReason )
{
	ULONG	ulIdx;
	char	szKickString[512];
	char	szName[64];

	// Make sure the target is valid and applicable.
	if (( ulPlayer >= MAXPLAYERS ) || ( !playeringame[ulPlayer] ))
		return;

	sprintf( szName, "%s", players[ulPlayer].userinfo.netname );
	V_RemoveColorCodes( szName );

	// Build the full kick string.
	sprintf( szKickString, "\\ci%s\\ci has been forced to spectate! Reason: %s\n", szName, pszReason );
	Printf( "%s", szKickString );

	// Rebuild the string that will be displayed to clients. This time, color codes are allowed.
	sprintf( szKickString, "\\ci%s\\ci has been forced to spectate! Reason: %s\n", players[ulPlayer].userinfo.netname, pszReason );

	// Already a spectator! This should not happen.
	if ( PLAYER_IsTrueSpectator( &players[ulPlayer] ))
		return;

	// Make this player a spectator.
	PLAYER_SetSpectator( &players[ulPlayer], true, false );

	// Send the message out to all clients.
	SERVERCOMMANDS_PlayerIsSpectator( ulPlayer );
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		SERVER_PrintfPlayer( PRINT_HIGH, ulIdx, szKickString );
	}
}

//*****************************************************************************
//
void SERVER_AddCommand( const char *pszCommand )
{
#ifdef NO_SERVER_GUI
	char* command = new char[strlen(pszCommand)+1];
	memcpy(command, pszCommand, strlen(pszCommand)+1);
	AddCommandString( command );
	delete[] command;
#else
	g_ServerCommandQueue.Push( pszCommand );
#endif
}

//*****************************************************************************
//
void SERVER_DeleteCommand( void )
{
#ifndef NO_SERVER_GUI
	AddCommandString( (char *)g_ServerCommandQueue[0].GetChars( ));
	g_ServerCommandQueue.Delete( 0 );
#endif
}

//*****************************************************************************
//
bool SERVER_IsEveryoneReadyToGoOn( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ) == false )
			continue;

		if ( players[ulIdx].bReadyToGoOn == false )
			return ( false );
	}

	return ( true );
}

//*****************************************************************************
//
bool SERVER_IsPlayerVisible( ULONG ulPlayer, ULONG ulPlayer2 )
{
	// Can ulPlayer see ulPlayer2?
	if (( teamlms || lastmanstanding ) &&
		(( lmsspectatorsettings & LMS_SPF_VIEW ) == false ) &&
		( players[ulPlayer].bSpectating ) &&
		( g_aClients[ulPlayer].ulDisplayPlayer == ulPlayer ) &&
		( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ))
	{
		return ( false );
	}

	// Passed all checks!
	return ( true );
}

//*****************************************************************************
//
bool SERVER_IsPlayerAllowedToKnowHealth( ULONG ulPlayer, ULONG ulPlayer2 )
{
	// No bodies? Definitely not!
	if (( players[ulPlayer].mo == NULL ) || ( players[ulPlayer2].mo == NULL ))
		return ( false );

	// If these players's are not teammates, then disallow knowledge of health.
	if ( players[ulPlayer].mo->IsTeammate( players[ulPlayer2].mo ) == false )
		return ( false );

	// Passed all checks!
	return ( true );
}

//*****************************************************************************
// 
// [RC] Is this player ignoring players at this address?
// Returns 0 (if not), -1 (if indefinitely), or the tics until expiration (if temporarily).
//
LONG SERVER_GetPlayerIgnoreTic( ULONG ulPlayer, NETADDRESS_s Address )
{
	// Remove all expired entries first. ([RC] We could combine the loops, but not all of the old entries would be removed.)
	for ( std::list<STORED_QUERY_IP_s>::iterator i = SERVER_GetClient( ulPlayer )->IgnoredAddresses.begin(); i != SERVER_GetClient( ulPlayer )->IgnoredAddresses.end( ); )
	{
		if (( i->lNextAllowedGametic != -1 ) && ( i->lNextAllowedGametic <= gametic ))
			i = SERVER_GetClient( ulPlayer )->IgnoredAddresses.erase( i ); // Returns a new iterator.
		else
			++i;
	}

	// Search for entries with this address.
	for ( std::list<STORED_QUERY_IP_s>::iterator i = SERVER_GetClient( ulPlayer )->IgnoredAddresses.begin(); i != SERVER_GetClient( ulPlayer )->IgnoredAddresses.end(); ++i )
	{
		if ( NETWORK_CompareAddress( i->Address, Address, true ))
		{
			if ( i->lNextAllowedGametic != -1 )
				return i->lNextAllowedGametic - gametic;
			else
				return -1;
		}
	}

	return 0;
}

//*****************************************************************************
//
const char *SERVER_GetCurrentFont( void )
{
	return ( g_szCurrentFont );
}

//*****************************************************************************
//
void SERVER_SetCurrentFont( const char *pszFont )
{
	sprintf( g_szCurrentFont, "%s", pszFont );
}

//*****************************************************************************
//
const char *SERVER_GetScriptActiveFont( void )
{
	return ( g_szScriptActiveFont );
}

//*****************************************************************************
//
void SERVER_SetScriptActiveFont( const char *pszFont )
{
	sprintf( g_szScriptActiveFont, "%s", pszFont );
}

//*****************************************************************************
//
LONG SERVER_AdjustDoorDirection( LONG lDirection )
{
	// Not a valid door direction.
	if (( lDirection < -1 ) || ( lDirection > 2 ))
		return ( INT_MIN );

	return ( lDirection + 1 );
}

//*****************************************************************************
//
LONG SERVER_AdjustFloorDirection( LONG lDirection )
{
	// Not a valid floor direction.
	if (( lDirection < -1 ) || ( lDirection > 1 ))
		return ( INT_MIN );

	return ( lDirection + 1 );
}

//*****************************************************************************
//
LONG SERVER_AdjustCeilingDirection( LONG lDirection )
{
	// Not a valid floor direction.
	if (( lDirection < -1 ) || ( lDirection > 1 ))
		return ( INT_MIN );

	return ( lDirection + 1 );
}

//*****************************************************************************
//
LONG SERVER_AdjustElevatorDirection( LONG lDirection )
{
	// Not a valid floor direction.
	if (( lDirection < -1 ) || ( lDirection > 1 ))
		return ( INT_MIN );

	return ( lDirection + 1 );
}

//*****************************************************************************
//
ULONG SERVER_GetMaxPacketSize( void )
{
	return ( g_ulMaxPacketSize );
}

//*****************************************************************************
//
char *SERVER_GetMapMusic( void )
{
	return ( g_szMapMusic );
}

//*****************************************************************************
//
IPList *SERVER_GetAdminList( void )
{
	return &g_AdminIPList;
}

//*****************************************************************************
//
const FString& SERVER_GetMasterBanlistVerificationString( void )
{
	return g_MasterBanlistVerificationString;
}
//*****************************************************************************
//
void SERVER_SetMapMusic( const char *pszMusic )
{
	if ( pszMusic )
		sprintf( g_szMapMusic, "%s", pszMusic );
	else
		g_szMapMusic[0] = 0;
}

//*****************************************************************************
//
void SERVER_ResetInventory( ULONG ulClient )
{
	AInventory	*pInventory;

	if (( ulClient >= MAXPLAYERS ) ||
		( playeringame[ulClient] == false ) ||
		( players[ulClient].mo == NULL ))
	{
		return;
	}

	// First, tell the client to delete his inventory.
	SERVERCOMMANDS_DestroyAllInventory( ulClient, ulClient, SVCF_ONLYTHISCLIENT );

	// Then, go through the client's inventory, and tell the client to give himself
	// each item.
	for ( pInventory = players[ulClient].mo->Inventory; pInventory != NULL; pInventory = pInventory->Inventory )
	{
		if ( pInventory->IsKindOf( RUNTIME_CLASS( AAmmo )))
			continue;

		// [BB] The collection of weapon pieces is handled on the server, so there is
		// no need to tell the client of its WeaponHolders. In fact a weapon holder
		// without PieceMask and PieceWeapon properly set is completely useless.
		if ( pInventory->IsKindOf( PClass::FindClass( "WeaponHolder" )) )
			continue;

		if ( pInventory->IsKindOf( RUNTIME_CLASS( APowerup )))
		{
			// [BB] All clients need to be informed about some special iventory kinds.
			if ( pInventory->IsKindOf( PClass::FindClass( "PowerTerminatorArtifact" ))
				 || pInventory->IsA( PClass::FindClass( "PowerPossessionArtifact" )) )
				SERVERCOMMANDS_GivePowerup( ulClient, static_cast<APowerup *>( pInventory ) );
			else
				SERVERCOMMANDS_GivePowerup( ulClient, static_cast<APowerup *>( pInventory ), ulClient, SVCF_ONLYTHISCLIENT );
			if (( pInventory->IsKindOf( RUNTIME_CLASS( APowerInvulnerable ))) &&
				(( players[ulClient].mo->effects & FX_VISIBILITYFLICKER ) || ( players[ulClient].mo->effects & FX_RESPAWNINVUL )))
			{
				SERVERCOMMANDS_PlayerRespawnInvulnerability( ulClient );
			}
		}
		else
		{
			// [BB] We want to give things here, but giving an item with an amount of 0
			// will cause the client to destory the item on his side. For unknown reasons
			// some custom weapons only have an amount of zero after a map change
			// (Fist and NewPistol in KDiZD for example) and will be lost on the client
			// side. Raising the amount to one fixes the problem. This is only a workaraound,
			// one should identify the reason for the zero amount.
			if ( pInventory->IsKindOf( RUNTIME_CLASS( AWeapon )) && pInventory->Amount == 0 )
				pInventory->Amount = 1;

			// [BB] All clients need to be informed about some special iventory kinds.
			if ( pInventory->IsKindOf( RUNTIME_CLASS( ATeamItem )) )
				SERVERCOMMANDS_GiveInventory( ulClient, pInventory );
			else
				SERVERCOMMANDS_GiveInventory( ulClient, pInventory, ulClient, SVCF_ONLYTHISCLIENT );
			// [BB] The armor display has to be updated seperately, otherwise
			// the client thinks the armor is green and its amount is equal to 1.
			if ( (pInventory->IsKindOf( RUNTIME_CLASS( AArmor ))) )
				SERVERCOMMANDS_SetPlayerArmor( ulClient );
		}
	}

	for ( pInventory = players[ulClient].mo->Inventory; pInventory != NULL; pInventory = pInventory->Inventory )
	{
		if ( pInventory->IsKindOf( RUNTIME_CLASS( AAmmo )) == false )
			continue;

		SERVERCOMMANDS_GiveInventory( ulClient, pInventory, ulClient, SVCF_ONLYTHISCLIENT );
	}
	// [BB]: After giving back the inventory, inform the player about which weapon he is using.
	// This at least partly fixes the "Using unknown weapon type" bug.
	SERVERCOMMANDS_WeaponChange( ulClient, ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVER_AddEditedTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulPal1, ULONG ulPal2 )
{
	EDITEDTRANSLATION_s	Translation;

	Translation.ulIdx = ulTranslation;
	Translation.ulStart = ulStart;
	Translation.ulEnd = ulEnd;
	Translation.ulPal1 = ulPal1;
	Translation.ulPal2 = ulPal2;
	Translation.ulType = DLevelScript::PCD_TRANSLATIONRANGE1;

	g_EditedTranslationList.Push( Translation );
}

//*****************************************************************************
//
void SERVER_AddEditedTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulR1, ULONG ulG1, ULONG ulB1, ULONG ulR2, ULONG ulG2, ULONG ulB2 )
{
	EDITEDTRANSLATION_s	Translation;

	Translation.ulIdx = ulTranslation;
	Translation.ulStart = ulStart;
	Translation.ulEnd = ulEnd;
	Translation.ulR1 = ulR1;
	Translation.ulG1 = ulG1;
	Translation.ulB1 = ulB1;
	Translation.ulR2 = ulR2;
	Translation.ulG2 = ulG2;
	Translation.ulB2 = ulB2;
	Translation.ulType = DLevelScript::PCD_TRANSLATIONRANGE2;

	g_EditedTranslationList.Push( Translation );
}

//*****************************************************************************
//
void SERVER_RemoveEditedTranslation( ULONG ulTranslation )
{
	const int numEntries = g_EditedTranslationList.Size();

	for ( int i = numEntries; i > 0; --i )
	{
		if ( g_EditedTranslationList[i-1].ulIdx == ulTranslation )
		{
			g_EditedTranslationList.Delete ( i - 1 );
		}
	}
}

//*****************************************************************************
//
bool SERVER_IsTranslationEdited( ULONG ulTranslation )
{
	for ( int i = 0; i < g_EditedTranslationList.Size(); ++i )
	{
		if ( g_EditedTranslationList[i].ulIdx == ulTranslation )
			return true;
	}
	return false;
}

//*****************************************************************************
//
void SERVER_ClearEditedTranslations( void )
{
	g_EditedTranslationList.Clear( );
}

//*****************************************************************************
//
void SERVER_ErrorCleanup( void )
{
	ULONG	ulIdx;
	char	szString[16];

	// Disconnect all the clients.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_IsValidClient( ulIdx ))
			SERVER_KickPlayer( ulIdx, "Server encountered an error." );
	}

	// [BC] Remove all the bots from this game.
	BOTS_RemoveAllBots( false );

	// Reload the map.
	sprintf( szString, "map %s", level.mapname );
	AddCommandString( szString );
}

//*****************************************************************************
//
void SERVER_IgnoreIP( NETADDRESS_s Address )
{
	g_floodProtectionIPQueue.addAddress( Address, g_lGameTime / 1000 );
}

//*****************************************************************************
//*****************************************************************************
//
LONG SERVER_STATISTIC_GetTotalSecondsElapsed( void )
{
	return ( g_lTotalServerSeconds );
}

//*****************************************************************************
//
LONG SERVER_STATISTIC_GetTotalPlayers( void )
{
	return ( g_lTotalNumPlayers );
}

//*****************************************************************************
//
LONG SERVER_STATISTIC_GetMaxNumPlayers( void )
{
	return ( g_lMaxNumPlayers );
}

//*****************************************************************************
//
LONG SERVER_STATISTIC_GetTotalFrags( void )
{
	return ( g_lTotalNumFrags );
}

//*****************************************************************************
//
void SERVER_STATISTIC_AddToTotalFrags( void )
{
	g_lTotalNumFrags++;
}

//*****************************************************************************
//
QWORD SERVER_STATISTIC_GetTotalOutboundDataTransferred( void )
{
	return ( g_qwTotalOutboundDataTransferred );
}

//*****************************************************************************
//
LONG SERVER_STATISTIC_GetPeakOutboundDataTransfer( void )
{
	return ( g_lMaxOutboundDataTransfer );
}

//*****************************************************************************
//
void SERVER_STATISTIC_AddToOutboundDataTransfer( ULONG ulNumBytes )
{
	g_qwTotalOutboundDataTransferred += ulNumBytes;
	g_lCurrentOutboundDataTransfer += ulNumBytes;

	SERVERCONSOLE_UpdateStatistics( );
}

//*****************************************************************************
//
LONG SERVER_STATISTIC_GetCurrentOutboundDataTransfer( void )
{
	return ( g_lOutboundDataTransferLastSecond );
}

//*****************************************************************************
//
QWORD SERVER_STATISTIC_GetTotalInboundDataTransferred( void )
{
	return ( g_qwTotalInboundDataTransferred );
}

//*****************************************************************************
//
LONG SERVER_STATISTIC_GetPeakInboundDataTransfer( void )
{
	return ( g_lMaxInboundDataTransfer );
}

//*****************************************************************************
//
void SERVER_STATISTIC_AddToInboundDataTransfer( ULONG ulNumBytes )
{
	g_qwTotalInboundDataTransferred += ulNumBytes;
	g_lCurrentInboundDataTransfer += ulNumBytes;

	SERVERCONSOLE_UpdateStatistics( );
}

//*****************************************************************************
//
LONG SERVER_STATISTIC_GetCurrentInboundDataTransfer( void )
{
	return ( g_lInboundDataTransferLastSecond );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVER_ParsePacket( BYTESTREAM_s *pByteStream )
{
	bool	bPlayerKicked;
	LONG	lCommand;

	while ( 1 )	 
	{  
		lCommand = NETWORK_ReadByte( pByteStream );

		// End of message.
		if ( lCommand == -1 )
			break;

		bPlayerKicked = false;
		switch ( lCommand )
		{
		case CLCC_ATTEMPTCONNECTION:

			// Client is trying to connect to the server, but is disconnected on his end.
			SERVER_SetupNewConnection( pByteStream, false );
			break;
		case CLCC_ATTEMPTAUTHENTICATION:

			// Client is attempting to authenticate his level.
			SERVER_AuthenticateClientLevel( pByteStream );
			break;
		case CLCC_REQUESTSNAPSHOT:

			// Client has gotten a connection from the server, and is sending userinfo.
			SERVER_ConnectNewPlayer( pByteStream );
			break;
		default:

			// [BB] Only authenticated clients are allowed to send the commands handled in
			// SERVER_ProcessCommand(). If we would let non authenticated clients do this,
			// this could be abused to keep non finished connections alive.
			if ( g_aClients[g_lCurrentClient].State < CLS_AUTHENTICATED )
			{
				// [BB] Under these special, rare circumstances valid clients can send illegal commands.
				if ( g_aClients[g_lCurrentClient].State != CLS_AUTHENTICATED_BUT_OUTDATED_MAP )
					Printf( "Illegal command (%d) from non-authenticated client (%s).\n", static_cast<int> (lCommand), NETWORK_AddressToString( NETWORK_GetFromAddress( ) ) );

				// [BB] Ignore the rest of the packet, it can't be valid.
				while ( NETWORK_ReadByte( pByteStream ) != -1 );
				break;
			}


			// This returns true if the player was kicked as a result.
			if ( SERVER_ProcessCommand( lCommand, pByteStream ))
				return;

			break;
		}
	}
}

//*****************************************************************************
//
bool SERVER_ProcessCommand( LONG lCommand, BYTESTREAM_s *pByteStream )
{
	switch ( lCommand )
	{
	case CLC_USERINFO:

		// Client is sending us his userinfo.
		SERVER_GetUserInfo( pByteStream, true );
		break;
	case CLC_QUIT:

		// Client has left the game.
		SERVER_DisconnectClient( g_lCurrentClient, true, true );
		break;
	case CLC_STARTCHAT:
	case CLC_ENDCHAT:
	case CLC_ENTERCONSOLE:
	case CLC_EXITCONSOLE:

		// [BB] If the client is flooding the server with commands, the client is
		// kicked and we don't need to handle the command.
		if ( server_CheckForClientMinorCommandFlood ( g_lCurrentClient ) == true )
			return ( true );

		if ( lCommand == CLC_STARTCHAT )
		{
			// Client is beginning to type.
			players[g_lCurrentClient].bChatting = true;

			// Tell clients about the change in this player's chatting status.
			SERVERCOMMANDS_SetPlayerChatStatus( g_lCurrentClient );
		}
		else if ( lCommand == CLC_ENDCHAT )
		{
			// Client is done talking.
			players[g_lCurrentClient].bChatting = false;

			// Tell clients about the change in this player's chatting status.
			SERVERCOMMANDS_SetPlayerChatStatus( g_lCurrentClient );
		}
		else if ( lCommand == CLC_ENTERCONSOLE )
		{

			// Player has entered the console - give him an icon.
			players[g_lCurrentClient].bInConsole = true;
			SERVERCOMMANDS_SetPlayerConsoleStatus( g_lCurrentClient );
		}
		else if ( lCommand == CLC_EXITCONSOLE )
		{
			// Player has left the console - remove his icon.
			players[g_lCurrentClient].bInConsole = false;
			SERVERCOMMANDS_SetPlayerConsoleStatus( g_lCurrentClient );
		}

		return false;
	case CLC_IGNORE:

		// Player whishes to ignore / unignore someone.
		return ( server_Ignore( pByteStream ) );
	case CLC_SAY:

		// Client is talking.
		return ( server_Say( pByteStream ));
	case CLC_CLIENTMOVE:
		{
			bool	bPlayerKicked;

			// Client is sending movement information.
			bPlayerKicked = server_ClientMove( pByteStream );

			if ( g_aClients[g_lCurrentClient].lLastMoveTick == gametic )
				g_aClients[g_lCurrentClient].lOverMovementLevel++;

			g_aClients[g_lCurrentClient].lLastMoveTick = gametic;
			return ( bPlayerKicked );
		}
		break;
	case CLC_MISSINGPACKET:

		// Client is missing a packet; it's our job to resend it!
		return ( server_MissingPacket( pByteStream ));
	case CLC_PONG:

		// Ping response from client.
		return ( server_UpdateClientPing( pByteStream ));
	case CLC_WEAPONSELECT:

		// Client has sent a weapon change.
		return ( server_WeaponSelect( pByteStream ));
	case CLC_TAUNT:

		// Client is taunting! Broadcast it to other clients.
		return ( server_Taunt( pByteStream ));
	case CLC_SPECTATE:

		// Client now wishes to spectate.
		return ( server_Spectate( pByteStream ));
	case CLC_REQUESTJOIN:

		// Client wishes to join the game after spectating.
		return ( server_RequestJoin( pByteStream ));
	case CLC_REQUESTRCON:

		// Client is attempting to gain remote control access to the server.
		return ( server_RequestRCON( pByteStream ));
	case CLC_RCONCOMMAND:

		// Client is sending a remote control command to the server.
		return ( server_RCONCommand( pByteStream ));
	case CLC_SUICIDE:

		// Client wishes to kill himself.
		return ( server_Suicide( pByteStream ));
	case CLC_CHANGETEAM:

		// Client wishes to change his team.
		return ( server_ChangeTeam( pByteStream ));
	case CLC_SPECTATEINFO:

		// Client is sending special spectator information.
		return ( server_SpectateInfo( pByteStream ));
	case CLC_GENERICCHEAT:

		// Client is attempting to use a cheat. Only legal if sv_cheats is enabled.
		return ( server_GenericCheat( pByteStream ));
	case CLC_GIVECHEAT:

		// Client is attempting to use the give cheat. Only legal if sv_cheats is enabled.
		return ( server_GiveCheat( pByteStream ));
	case CLC_SUMMONCHEAT:
	case CLC_SUMMONFRIENDCHEAT:
	case CLC_SUMMONFOECHEAT:

		// Client is attempting to use a summon cheat. Only legal if sv_cheats is enabled.
		return ( server_SummonCheat( pByteStream, lCommand ));
	case CLC_READYTOGOON:

		// Users can only toggle if they haven't yet, and we must be in intermission.
		if ( gamestate != GS_INTERMISSION || players[g_lCurrentClient].bReadyToGoOn )
			return ( false );

		// Toggle this player (specator)'s "ready to go on" status.
		// [RC] Now a permanent choice.
		players[g_lCurrentClient].bReadyToGoOn = true;

		if ( SERVER_IsEveryoneReadyToGoOn( ) == false )
			SERVERCOMMANDS_SetPlayerReadyToGoOnStatus( g_lCurrentClient );

		return false;
	case CLC_CHANGEDISPLAYPLAYER:

		// Client is changing the player whose eyes he is looking through.
		return ( server_ChangeDisplayPlayer( pByteStream ));
	case CLC_AUTHENTICATELEVEL:

		// Client is attempting to authenticate his level.
		return ( server_AuthenticateLevel( pByteStream ));
	case CLC_CALLVOTE:

		// Client wishes to call a vote.
		return ( server_CallVote( pByteStream ));
	case CLC_VOTEYES:

		// Client wishes to vote "yes" on the current vote.
		CALLVOTE_VoteYes( g_lCurrentClient );
		return ( false );
	case CLC_VOTENO:

		// Client wishes to vote "no" on the current vote.
		CALLVOTE_VoteNo( g_lCurrentClient );
		return ( false );
	case CLC_INVENTORYUSEALL:

		// Client wishes to use all inventory items he has.
		return ( server_InventoryUseAll( pByteStream ));
	case CLC_INVENTORYUSE:

		// Client wishes to use a specfic inventory item he has.
		return ( server_InventoryUse( pByteStream ));
	case CLC_INVENTORYDROP:

		// Client wishes to drop a specfic inventory item he has.
		return ( server_InventoryDrop( pByteStream ));
	case CLC_PUKE:

		// [BB] Client wishes to puke a scipt.
		return ( server_Puke( pByteStream ));
	case CLC_MORPHEX:

		// [BB] Client wishes to morph to a certain class.
		return ( server_MorphCheat( pByteStream ));
	default:

		Printf( PRINT_HIGH, "SERVER_ParseCommands: Unknown client message: %d\n", static_cast<int> (lCommand) );
        return ( true );
	}

	// Return false if the player was not kicked as a result of processing
	// this command.
	return ( false );
}


//*****************************************************************************
//
// [RC] Finds the first player (or, optionally, bot) with the given name; returns MAXPLAYERS if none were found.
ULONG SERVER_GetPlayerIndexFromName( const char *pszName, bool bIgnoreColors, bool bReturnBots )
{
	char	szPlayerName[64];

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] == false )
			continue;

		// Optionally remove the color codes from the player name.
		if ( bIgnoreColors )
		{
			sprintf( szPlayerName, "%s", players[ulIdx].userinfo.netname );
			V_RemoveColorCodes( szPlayerName );
		}

		if ( stricmp( bIgnoreColors ? szPlayerName : players[ulIdx].userinfo.netname, pszName ) == 0 )
		{
			if ( !players[ulIdx].bIsBot || bReturnBots )
				return ulIdx;
		}
	}

	// None found.
	return MAXPLAYERS;
}

//*****************************************************************************
//
LONG SERVER_GetCurrentClient( void )
{
	return ( g_lCurrentClient );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVER_GiveInventoryToPlayer( const player_t *player, AInventory *pInventory )
{
	if ( (player == NULL) || (player->mo == NULL) || (NETWORK_GetState( ) != NETSTATE_SERVER) )
		return;

	SERVERCOMMANDS_GiveInventoryNotOverwritingAmount( player->mo, pInventory );
}

//*****************************************************************************
// [BB]
void SERVER_HandleWeaponStateJump( ULONG ulPlayer, FState *pState, LONG lPosition )
{
	if ( SERVER_IsValidPlayer( ulPlayer ) == false )
		return;

	AWeapon *pReadyWeapon = players[ulPlayer].ReadyWeapon;

	if ( pReadyWeapon == false )
		return;

	// [BB] First set the weapon sprite of the player according to the jump.
	SERVERCOMMANDS_SetPlayerPSprite( ulPlayer, pState, lPosition );

	// [BB] During the time it takes for the server to instruct the client to do the jump
	// the client possible executes code pointers it shouldn't because the client just ignores
	// the jump when executing A_Jump. Typically this can mess the local ammo prediction done
	// by the client (KF7Soviet from Goldeneye64TCv12.pk3 gives a good example of this). As
	// workaround we resync the clients ammo count every time a jump is done. Not really nice,
	// but should get the job done.
	if ( pReadyWeapon->Ammo1 )
		SERVERCOMMANDS_GiveInventory( ulPlayer, static_cast<AInventory *>( pReadyWeapon->Ammo1 ));
	if ( pReadyWeapon->Ammo2 )
		SERVERCOMMANDS_GiveInventory( ulPlayer, static_cast<AInventory *>( pReadyWeapon->Ammo2 ));
}

//*****************************************************************************
//
static bool server_Ignore( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTargetIdx = NETWORK_ReadByte( pByteStream );
	bool	bIgnore = !!NETWORK_ReadByte( pByteStream );
	LONG	lTicks = NETWORK_ReadLong( pByteStream );

	if ( !SERVER_IsValidClient( ulTargetIdx ))
		return false;

	// First, remove any entries using this IP.
	NETADDRESS_s AddressToIgnore  = SERVER_GetClient( ulTargetIdx )->Address;
	for ( std::list<STORED_QUERY_IP_s>::iterator i = SERVER_GetClient( g_lCurrentClient )->IgnoredAddresses.begin(); i != SERVER_GetClient( g_lCurrentClient )->IgnoredAddresses.end( ); )
	{
		if ( NETWORK_CompareAddress( i->Address, AddressToIgnore, true ))
			i = SERVER_GetClient( g_lCurrentClient )->IgnoredAddresses.erase( i ); // Returns a new iterator.
		else
			++i;
	}

	// Now, add the new entry. If an entry had existed before, this "updates" it.
	if ( bIgnore )
	{
		STORED_QUERY_IP_s Entry;
		Entry.Address = AddressToIgnore;
		Entry.lNextAllowedGametic = ( lTicks == -1 ) ? lTicks : ( gametic + lTicks );

		SERVER_GetClient( g_lCurrentClient )->IgnoredAddresses.push_back( Entry );
	}

	return false;
}

//*****************************************************************************
//
static bool server_CheckForClientCommandFlood( ULONG ulClient )
{
	// [BB] If a client issues more commands in floodWindowLength seconds than
	// his commandInstances can hold, he is temporarily banned.
	const LONG floodWindowLength = 60;
	if ( g_aClients[ulClient].commandInstances.getOldestEntry() > 0 )
	{
		if ( ( gametic - g_aClients[ulClient].commandInstances.getOldestEntry() ) <= floodWindowLength * TICRATE )
		{
			SERVERBAN_BanPlayer( ulClient, "10min", "Client command flood." );
			return ( true );
		}
	}
	// [BB] If this is the last command he may do in the given time frame, warn him.
	if ( g_aClients[ulClient].commandInstances.getOldestEntry( 1 ) > 0 )
	{
		if ( ( gametic - g_aClients[ulClient].commandInstances.getOldestEntry( 1 ) ) <= floodWindowLength * TICRATE )
			SERVER_PrintfPlayer( PRINT_HIGH, ulClient, "Stop flooding the server with commands or you will be temporarily banned.\n" );
	}
	g_aClients[ulClient].commandInstances.put ( gametic );

	return ( false );
}

//*****************************************************************************
// [BB] Shares quite a bit of code with server_CheckForClientCommandFlood, but
// we probably don't win enough if it's rewritten to cut down the code duplication here.
static bool server_CheckForClientMinorCommandFlood( ULONG ulClient )
{
	// [BB] If a client issues more commands in floodWindowLength seconds than
	// his commandInstances can hold, he is temporarily banned.
	const LONG floodWindowLength = 10;
	if ( g_aClients[ulClient].minorCommandInstances.getOldestEntry() > 0 )
	{
		if ( ( gametic - g_aClients[ulClient].minorCommandInstances.getOldestEntry() ) <= floodWindowLength * TICRATE )
		{
			SERVERBAN_BanPlayer( ulClient, "10min", "Client command flood." );
			return ( true );
		}
	}
	g_aClients[ulClient].minorCommandInstances.put ( gametic );

	return ( false );
}

//*****************************************************************************
//
static bool server_Say( BYTESTREAM_s *pByteStream )
{
	ULONG ulPlayer = g_lCurrentClient;

	// Read in the chat mode (normal, team, etc.)
	ULONG ulChatMode = NETWORK_ReadByte( pByteStream );

	// Read in the chat string.
	const char	*pszChatString = NETWORK_ReadString( pByteStream );

	// [RC] Are this player's chats ignored?
	if ( players[ulPlayer].bIgnoreChat )
		return ( false );

	//==========================
	// Check for chat flooding.
	//==========================

	bool bMutePlayer = false;
	ULONG ulChatInstance = ( ++g_aClients[ulPlayer].ulLastChatInstance % MAX_CHATINSTANCE_STORAGE );
	g_aClients[ulPlayer].lChatInstances[ulChatInstance] = gametic;

	// Mute the player if this is the...

	// ...second time he has chatted in a 7 tick interval (~1/5 of a second, ~1/5 of a second chat interval).
	if ( ( g_aClients[ulPlayer].lChatInstances[ulChatInstance] ) -
		( g_aClients[ulPlayer].lChatInstances[( ulChatInstance + MAX_CHATINSTANCE_STORAGE - 1 ) % MAX_CHATINSTANCE_STORAGE] )
		<= 7 )
	{
		bMutePlayer = true;
	}

	// ..third time he has chatted in a 42 tick interval (~1.5 seconds, ~.75 second chat interval).
	if ( ( g_aClients[ulPlayer].lChatInstances[ulChatInstance] ) -
		( g_aClients[ulPlayer].lChatInstances[( ulChatInstance + MAX_CHATINSTANCE_STORAGE - 2 ) % MAX_CHATINSTANCE_STORAGE] )
		<= 42 )
	{
		bMutePlayer = true;
	}

	// ...fourth time he has chatted in a 105 tick interval (~3 seconds, ~1 second interval).
	if ( ( g_aClients[ulPlayer].lChatInstances[ulChatInstance] ) -
		( g_aClients[ulPlayer].lChatInstances[( ulChatInstance + MAX_CHATINSTANCE_STORAGE - 3 ) % MAX_CHATINSTANCE_STORAGE] )
		<= 105 )
	{
		bMutePlayer = true;
	}

	// Mute the player.
	if ( bMutePlayer )
	{
		players[ulPlayer].bIgnoreChat = true;
		players[ulPlayer].lIgnoreChatTicks = 15 * TICRATE;
		SERVER_PrintfPlayer( PRINT_HIGH, ulPlayer, "Please refrain from chatting so much. You've been muted for 15 seconds.\n" );
		return ( false );
	}
	// Or, relay the chat message onto clients.
	else
	{
		SERVER_SendChatMessage( ulPlayer, ulChatMode, pszChatString );
		return ( false );
	}
}

//*****************************************************************************
//
static bool server_ClientMove( BYTESTREAM_s *pByteStream )
{
	player_t		*pPlayer;
	ticcmd_t		*pCmd;
	angle_t			Angle;
	angle_t			Pitch;
	ULONG			ulGametic;
	ULONG			ulBits;
	USHORT			usActorNetworkIndex;
	const PClass	*pType;
	AInventory		*pInventory;
//	ULONG			ulIdx;

	pPlayer = &players[g_lCurrentClient];
	pCmd = &pPlayer->cmd;

	// Read in the client's gametic.
	ulGametic = NETWORK_ReadLong( pByteStream );

	// Read in the information the client is sending us.
	ulBits = NETWORK_ReadByte( pByteStream );

	if ( ulBits & CLIENT_UPDATE_YAW )
		pCmd->ucmd.yaw = NETWORK_ReadShort( pByteStream );
	else
		pCmd->ucmd.yaw = 0;

	if ( ulBits & CLIENT_UPDATE_PITCH )
		pCmd->ucmd.pitch = NETWORK_ReadShort( pByteStream );
	else
		pCmd->ucmd.pitch = 0;

	if ( ulBits & CLIENT_UPDATE_ROLL )
		pCmd->ucmd.roll = NETWORK_ReadShort( pByteStream );
	else
		pCmd->ucmd.roll = 0;

	if ( ulBits & CLIENT_UPDATE_BUTTONS )
		pCmd->ucmd.buttons = ( ulBits & CLIENT_UPDATE_BUTTONS_LONG ) ? NETWORK_ReadLong( pByteStream ) : NETWORK_ReadByte( pByteStream );
	else
		pCmd->ucmd.buttons = 0;

	if ( ulBits & CLIENT_UPDATE_FORWARDMOVE )
		pCmd->ucmd.forwardmove = NETWORK_ReadShort( pByteStream );
	else
		pCmd->ucmd.forwardmove = 0;

	if ( ulBits & CLIENT_UPDATE_SIDEMOVE )
		pCmd->ucmd.sidemove = NETWORK_ReadShort( pByteStream );
	else
		pCmd->ucmd.sidemove = 0;

	if ( ulBits & CLIENT_UPDATE_UPMOVE )
		pCmd->ucmd.upmove = NETWORK_ReadShort( pByteStream );
	else
		pCmd->ucmd.upmove = 0;

	// Always read in the angle and pitch.
	Angle = NETWORK_ReadLong( pByteStream );
	Pitch = NETWORK_ReadLong( pByteStream );

	// If the client is attacking, he always sends the name of the weapon he's using.
	if ( pCmd->ucmd.buttons & BT_ATTACK )
	{
		usActorNetworkIndex = NETWORK_ReadShort( pByteStream );
		if ( pPlayer->ReadyWeapon )
		{
			// If the name of the weapon the client is using doesn't match the name of the
			// weapon we think he's using, do something to rectify the situation.
			if ( pPlayer->ReadyWeapon->GetClass( )->getActorNetworkIndex() != usActorNetworkIndex )
			{
				pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
				if (( pType ) && ( pType->IsDescendantOf( RUNTIME_CLASS( AWeapon ))))
				{
					if ( pPlayer->mo )
					{
						pInventory = pPlayer->mo->FindInventory( pType );
						if ( pInventory )
						{
							pPlayer->PendingWeapon = static_cast<AWeapon *>( pInventory );

							// Update other spectators with this info.
							SERVERCOMMANDS_SetPlayerPendingWeapon( g_lCurrentClient, g_lCurrentClient, SVCF_SKIPTHISCLIENT );
						}
//						else if ( g_ulWeaponCheckGracePeriodTicks == 0 )
//						{
//							SERVER_KickPlayer( g_lCurrentClient, "Using unowned weapon." );
//							return ( true );
//						}
					}
				}
				else
				{
					if( usActorNetworkIndex == 0 )
					{
						// [BB] For some reason the clients think he as no ready weapon, 
						// but the server thinks he as one. Although this should not happen,
						// we make a workaround for this here. Just tell the client to bring
						// up the weapon, the server thinks he is using.
						SERVERCOMMANDS_WeaponChange( g_lCurrentClient, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
					}
					else{
						SERVER_KickPlayer( g_lCurrentClient, "Using unknown weapon type." );
						return ( true );
					}
				}
			}
		}
	}

	// Don't timeout.
	g_aClients[g_lCurrentClient].ulLastCommandTic = gametic;
	g_aClients[g_lCurrentClient].ulClientGameTic = ulGametic;

	if ( gamestate == GS_LEVEL )
	{
		if ( pPlayer->mo )
		{
			pPlayer->mo->angle = Angle;
			pPlayer->mo->pitch = Pitch;

			// Makes sure the pitch is valid (should we kick them if it's not?)
			if ( pPlayer->mo->pitch < ( -ANGLE_1 * 90 ))
				pPlayer->mo->pitch = -ANGLE_1*90;
			else if ( pPlayer->mo->pitch > ( ANGLE_1 * 90 ))
				pPlayer->mo->pitch = ( ANGLE_1 * 90 );

			P_PlayerThink( pPlayer );
			if ( pPlayer->mo )
				pPlayer->mo->Tick( );
		}
	}

	// If CLC_ENDCHAT got missed, and the player is doing stuff, then obviously he is no longer chatting.

	// [RC] This actually isn't necessarily true. By using a joystick, a player can both move and chat.
	// I'm not going to change it though, because since they can move, they shouldn't be protected by the llama medal. Also, it'd confuse people.
	if (( pPlayer->bChatting ) &&
		(( pCmd->ucmd.buttons != 0 ) ||
		( pCmd->ucmd.forwardmove != 0 ) ||
		( pCmd->ucmd.sidemove != 0 ) ||
		( pCmd->ucmd.upmove != 0 )))
	{
		pPlayer->bChatting = false;
		SERVERCOMMANDS_SetPlayerChatStatus( g_lCurrentClient );
	}

	return ( false );
}

//*****************************************************************************
//
static bool server_MissingPacket( BYTESTREAM_s *pByteStream )
{
	ULONG	ulIdx;
	LONG	lPacket;
	LONG	lLastPacket;
	bool	bFullUpdateRequired;

	// If this client just requested missing packets, ignore the request.
	if ( gametic <= ( g_aClients[g_lCurrentClient].lLastPacketLossTick + ( TICRATE / 4 )))
	{
		while ( NETWORK_ReadLong( pByteStream ) != -1 )
			;

		return ( false );
	}

	// Keep reading in packets until we hit -1.
	lLastPacket = -1;
	while (( lPacket = NETWORK_ReadLong( pByteStream )) != -1 )
	{
		// The missing packet sequence must be sent to us in ascending order. If it's not,
		// the server could potentially go into an infinite loop, or be lagged heavily.
		if (( lPacket <= lLastPacket ) || ( lPacket < 0 ))
		{
			while ( NETWORK_ReadLong( pByteStream ) != -1 )
				;

			SERVER_KickPlayer( g_lCurrentClient, "Invalid missing packet request." );
			return ( true );
		}
		lLastPacket = lPacket;

		// Search through all PACKET_BUFFER_SIZE of the stored packets. We're looking for the packet that
		// that we want to send to the client by matching the sequences. If we cannot find
		// the packet, then we much send a full update to the client.
		bFullUpdateRequired = true;
		for ( ulIdx = 0; ulIdx < PACKET_BUFFER_SIZE; ulIdx++ )
		{
			if ( g_aClients[g_lCurrentClient].lPacketSequence[ulIdx] == lPacket )
			{
				bFullUpdateRequired = false;
				break;
			}
		}

		// We could not find the correct packet to resend to the client. We must send him
		// a full update.
		if  ( bFullUpdateRequired )
		{
			// Do we really need to print this? Nah.
//			Printf( "*** Sequence screwed for client %d, %s!\n", g_lCurrentClient, players[g_lCurrentClient].userinfo.netname );
			SERVER_KickPlayer( g_lCurrentClient, "Too many missed packets." );
			return ( true );
		}

		// Now that we've found the missed packet that we need to send to the client,
		// send it.
		NETWORK_ClearBuffer( &g_PacketLossBuffer );
		NETWORK_WriteHeader( &g_PacketLossBuffer.ByteStream, SVC_HEADER );
		NETWORK_WriteLong( &g_PacketLossBuffer.ByteStream, lPacket );
		if ( g_aClients[g_lCurrentClient].lPacketSize[ulIdx] )
		{
			NETWORK_WriteBuffer( &g_PacketLossBuffer.ByteStream,
				g_aClients[g_lCurrentClient].SavedPacketBuffer.pbData + g_aClients[g_lCurrentClient].lPacketBeginning[ulIdx],
				g_aClients[g_lCurrentClient].lPacketSize[ulIdx] );
		}
//		else
//			Printf( "server_MissingPacket: WARNING! Packet %d has no size!\n", ulIdx );
		NETWORK_LaunchPacket( &g_PacketLossBuffer, g_aClients[g_lCurrentClient].Address );
	}

	// Mark this client as having requested missing packets.
	g_aClients[g_lCurrentClient].lLastPacketLossTick = gametic;

	return ( false );
}

//*****************************************************************************
//
static bool server_UpdateClientPing( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPing;

	ulPing = NETWORK_ReadLong( pByteStream );

	ULONG currentPing = (I_MSTime( ) - ulPing);
	const ULONG ticLength = 1000 / TICRATE;
	player_t *p = &players[g_lCurrentClient];
	// [BB] Lag spike, reset the averaging.
	if ( labs (currentPing - p->ulPing) > 3.5 * ticLength )
	{
		p->ulPing = currentPing;
		p->ulPingAverages = 0;
	}
	else
	{
		ULONG oldPing = p->ulPing;
		ULONG ulPingAverages = p->ulPingAverages;
		p->ulPing = ( p->ulPingAverages * p->ulPing + currentPing ) / ( 1 + p->ulPingAverages );
		// [BB] The most recent ping measurement should always have a noticeable influence on the average ping.
		if ( p->ulPingAverages < 20 )
			p->ulPingAverages++;
	}

	return ( false );
}

//*****************************************************************************
//
static bool server_WeaponSelect( BYTESTREAM_s *pByteStream )
{
	USHORT			usActorNetworkIndex;
	const PClass	*pType;
	AInventory		*pInventory;

	// Read in the identification of the weapon the player is selecting.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// If the player doesn't have a body, break out.
	if ( players[g_lCurrentClient].mo == NULL )
		return ( false );

	// Try to find the class that corresponds to the name of the weapon the client
	// is sending us. If it doesn't exist, or the class isn't a type of weapon, boot
	// them.
	pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
	if (( pType == NULL ) ||
		( pType->IsDescendantOf( RUNTIME_CLASS( AWeapon )) == false ))
	{
		SERVER_KickPlayer( g_lCurrentClient, "Tried to switch to unknown weapon type." );
		return ( true );
	}

	pInventory = players[g_lCurrentClient].mo->FindInventory( pType );
	if ( pInventory == NULL )
	{
//		SERVER_KickPlayer( g_lCurrentClient, "Tried to select unowned weapon." );
//		return ( true );
		return ( false );
	}

	// Finally, switch the player's pending weapon.
	players[g_lCurrentClient].PendingWeapon = static_cast<AWeapon *>( pInventory );

	// [BB] Tell the other clients about the change. This should fix the spectator bug and the railgun pistol sound bug.
	SERVERCOMMANDS_SetPlayerPendingWeapon( g_lCurrentClient, g_lCurrentClient, SVCF_SKIPTHISCLIENT );

	return ( false );
}

//*****************************************************************************
//
static bool server_Taunt( BYTESTREAM_s *pByteStream )
{
	// Don't taunt if we're not in a level!
	if ( gamestate != GS_LEVEL )
		return ( false );

	// Spectating players and dead players cannot taunt.
	if (( players[g_lCurrentClient].bSpectating ) ||
		( players[g_lCurrentClient].health <= 0 ) ||
		( players[g_lCurrentClient].mo == NULL ) ||
		( i_compatflags & COMPATF_DISABLETAUNTS ))
	{
		return ( false );
	}

	// [BB] If the client is flooding the server with commands, the client is
	// kicked and we don't need to handle the command.
	if ( server_CheckForClientMinorCommandFlood ( g_lCurrentClient ) == true )
		return ( true );

	SERVERCOMMANDS_PlayerTaunt( g_lCurrentClient, g_lCurrentClient, SVCF_SKIPTHISCLIENT );

	return ( false );
}

//*****************************************************************************
//
static bool server_Spectate( BYTESTREAM_s *pByteStream )
{
	ULONG	ulIdx;

	// Already a spectator!
	if ( PLAYER_IsTrueSpectator( &players[g_lCurrentClient] ))
		return ( false );

	// Make this player a spectator.
	PLAYER_SetSpectator( &players[g_lCurrentClient], true, false );

	// Tell the other players to mark this player as a spectator.
	SERVERCOMMANDS_PlayerIsSpectator( g_lCurrentClient );

	// Tell this player everyone's weapon.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( players[ulIdx].ReadyWeapon == NULL ))
		{
			continue;
		}

		SERVERCOMMANDS_WeaponChange( ulIdx, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
	}

	return ( false );
}

//*****************************************************************************
//
static bool server_RequestJoin( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Val;
	FString		clientJoinPassword;

	// Read in the join password.
	clientJoinPassword = NETWORK_ReadString( pByteStream );
	clientJoinPassword.ToUpper();

	// Player can't rejoin game if he's not spectating!
	if (( playeringame[g_lCurrentClient] == false ) || ( players[g_lCurrentClient].bSpectating == false ))
		return ( false );

	// Player can't rejoin their LMS/survival game if they are dead.
	if (( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_DEADSPECTATORS ) && ( players[g_lCurrentClient].bDeadSpectator ))
		return ( false );

	// If we're forcing a join password, kick him if it doesn't match.
	Val = sv_joinpassword.GetGenericRep( CVAR_String );
	if (( sv_forcejoinpassword ) && ( strlen( Val.String )))
	{
		char	szServerJoinPassword[64];

		// Store password in temporary buffer (becuase we strupr it).
		strcpy( szServerJoinPassword, Val.String );

		// Check their password against ours (both not case sensitive).
		if ( strcmp( strupr( szServerJoinPassword ), clientJoinPassword.GetChars() ) != 0 )
		{
			// Tell the client that the password didn't match.
			SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "Incorrect join password.\n" );
			return ( false );
		}
	}

	// If there aren't currently any slots available, just put the person in line.
	Val = sv_maxplayers.GetGenericRep( CVAR_Int );
	if (( duel && DUEL_CountActiveDuelers( ) >= 2 ) ||
		( static_cast<signed> (SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS )) >= Val.Int ) ||
		( SURVIVAL_GetState( ) == SURVS_INPROGRESS ) ||
		( SURVIVAL_GetState( ) == SURVS_MISSIONFAILED ) ||
		(( lastmanstanding || teamlms ) && (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) || ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE ))) ||
		( INVASION_PreventPlayersFromJoining() ) )
	{
		JOINSLOT_t	JoinSlot;

		JoinSlot.ulPlayer = g_lCurrentClient;
		JoinSlot.ulTeam = teams.Size( );
		JOINQUEUE_AddPlayer( JoinSlot );

		// Tell the client what his position in line is.
		SERVERCOMMANDS_SetQueuePosition( g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		return ( false );
	}

	// Everything's okay! Go ahead and join!
	PLAYER_SpectatorJoinsGame ( &players[g_lCurrentClient] );
	// [BB] It's possible that you are watching through the eyes of someone else
	// upon joining. Doesn't hurt to reset it.
	g_aClients[g_lCurrentClient].ulDisplayPlayer = g_lCurrentClient;
	if ( ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) &&
		( !( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_TEAMGAME ) || ( TemporaryTeamStarts.Size( ) == 0 ) ) )
	{
		players[g_lCurrentClient].bOnTeam = true;
		players[g_lCurrentClient].ulTeam = TEAM_ChooseBestTeamForPlayer( );

		// If this player is on a team, tell all the other clients that a team has been selected
		// for him.
		if ( players[g_lCurrentClient].bOnTeam )
			SERVERCOMMANDS_SetPlayerTeam( g_lCurrentClient );
	}

	SERVER_Printf( PRINT_HIGH, "%s \\c-joined the game.\n", players[g_lCurrentClient].userinfo.netname );

	// Update this player's info on the scoreboard.
	SERVERCONSOLE_UpdatePlayerInfo( g_lCurrentClient, UDF_FRAGS );

	return ( false );
}

//*****************************************************************************
//
static bool server_RequestRCON( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Val;
	const char	*pszUserPassword;

	Val = sv_rconpassword.GetGenericRep( CVAR_String );

	// If the user password matches our PW, and we have a PW set, give him RCON access.
	pszUserPassword = NETWORK_ReadString( pByteStream );

	// [BB] If the client is flooding the server with commands, the client is
	// kicked and we don't need to handle the command.
	if ( server_CheckForClientCommandFlood ( g_lCurrentClient ) == true )
		return ( true );

	if (( strlen( Val.String )) && ( strcmp( Val.String, pszUserPassword ) == 0 ))
	{
		g_aClients[g_lCurrentClient].bRCONAccess = true;
		SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "RCON access granted.\n" );
		Printf( "RCON access for %s is granted!\n", players[g_lCurrentClient].userinfo.netname );
	}
	else
	{
		g_aClients[g_lCurrentClient].bRCONAccess = false;
		SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "Incorrect RCON password.\n" );
		Printf( "Incorrect RCON password attempt from %s.\n", players[g_lCurrentClient].userinfo.netname );
	}

	return ( false );
}

//*****************************************************************************
//
static bool server_RCONCommand( BYTESTREAM_s *pByteStream )
{
	const char	*pszCommand;

	// Read in the command the user sent us.
	pszCommand = NETWORK_ReadString( pByteStream );

	// If they don't have RCON access, quit.
	if ( !g_aClients[g_lCurrentClient].bRCONAccess )
		return ( false );

	Printf( "-> %s (RCON by %s - %s)\n", pszCommand, players[g_lCurrentClient].userinfo.netname, NETWORK_AddressToString( SERVER_GetClient( g_lCurrentClient )->Address ));

	// Set the RCON player so that output displays on his end.
	CONSOLE_SetRCONPlayer( g_lCurrentClient );

	// Now, execute the player's command.
	C_DoCommand( pszCommand );

	// Turn off displaying output on his end.
	CONSOLE_SetRCONPlayer( MAXPLAYERS );

	return ( false );
}

//*****************************************************************************
//
static bool server_Suicide( BYTESTREAM_s *pByteStream )
{
	// Spectators cannot suicide.
	if ( players[g_lCurrentClient].bSpectating || playeringame[g_lCurrentClient] == false )
		return ( false );

	// Don't allow suiciding during a duel.
	if ( duel && ( DUEL_GetState( ) == DS_INDUEL ))
		return ( false );

	// If this player has tried to suicide recently, ignore the request.
	if ( gametic < static_cast<signed> ( g_aClients[g_lCurrentClient].ulLastSuicideTime + ( TICRATE * 10 )))
		return ( false );

	// [BB] The server may forbid suiciding completely.
	if ( sv_nokill )
	{
		SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient(), "Suiciding is not allowed in this server.\n" );
		return ( false );
	}

	g_aClients[g_lCurrentClient].ulLastSuicideTime = gametic;

	cht_Suicide( &players[g_lCurrentClient] );

	return ( false );
}

//*****************************************************************************
//
static bool server_ChangeTeam( BYTESTREAM_s *pByteStream )
{
	LONG		lDesiredTeam;
	bool		bOnTeam, bAutoSelectTeam = false;
	UCVarValue	Val;
	FString		clientJoinPassword;

	// Read in the join password.
	clientJoinPassword = NETWORK_ReadString( pByteStream );
	clientJoinPassword.ToUpper();

	lDesiredTeam = NETWORK_ReadByte( pByteStream );
	if ( playeringame[g_lCurrentClient] == false )
		return ( false );

	// Not in a level.
	if ( gamestate != GS_LEVEL )
		return ( false );

	// Not a teamgame.
	if ( !( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_PLAYERSONTEAMS ) )
		return ( false );

	// Player can't rejoin their LMS game if they are dead.
	if ( teamlms && ( players[g_lCurrentClient].bDeadSpectator ))
		return ( false );

	// "No team change" dmflag is set. Ignore this.
	if ( players[g_lCurrentClient].bOnTeam && ( dmflags2 & DF2_NO_TEAM_SWITCH ))
		return ( false );

	// If this player has tried to change teams recently, ignore the request.
	if (!( ( lastmanstanding || teamlms ) && ( LASTMANSTANDING_GetState( ) == LMSS_COUNTDOWN ) ) && gametic < static_cast<signed> ( g_aClients[g_lCurrentClient].ulLastChangeTeamTime + ( TICRATE * 10 )))
		return ( false );

	g_aClients[g_lCurrentClient].ulLastChangeTeamTime = gametic;

	// If the team isn't valid, just pick the best team for the player to be on.
	if ( TEAM_CheckIfValid( lDesiredTeam ) == false )
	{
		lDesiredTeam = TEAM_ChooseBestTeamForPlayer( );
		bAutoSelectTeam = true;
	}

	// If the desired team matches our current team, break out.
	if (( players[g_lCurrentClient].bOnTeam ) && ( lDesiredTeam == static_cast<signed> (players[g_lCurrentClient].ulTeam) ))
	{
		SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "You are already on the %s team!\n", TEAM_GetName( lDesiredTeam ));
		return ( false );
	}

	// Don't allow players to switch teams in the middle of a team LMS game.
	if ( teamlms && ( players[g_lCurrentClient].bSpectating == false ) && (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) || ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE )))
	{
		SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "You cannot switch teams in the middle of a match!\n" );
		return ( false );
	}

	// If we're forcing a join password, kick him if it doesn't match.
	Val = sv_joinpassword.GetGenericRep( CVAR_String );
	if (( sv_forcejoinpassword ) && ( strlen( Val.String )))
	{
		char	szServerJoinPassword[64];

		// Store password in temporary buffer (becuase we strupr it).
		strcpy( szServerJoinPassword, Val.String );

		// Check their password against ours (both not case sensitive).
		if ( strcmp( strupr( szServerJoinPassword ), clientJoinPassword.GetChars() ) != 0 )
		{
			// Tell the client that the password didn't match.
			SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "Incorrect join password.\n" );
			return ( false );
		}
	}

	Val = sv_maxplayers.GetGenericRep( CVAR_Int );
	if (( players[g_lCurrentClient].bSpectating ) && 
		(( static_cast<signed> (SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS )) >= Val.Int ) ||		
		( teamlms && ( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) && ( PLAYER_IsTrueSpectator( &players[g_lCurrentClient] )))))
	{
		JOINSLOT_t	JoinSlot;

		JoinSlot.ulPlayer = g_lCurrentClient;
		if ( bAutoSelectTeam ) // [RC] If the player chose to autoselect his team, postpone that until he actually joins.
			JoinSlot.ulTeam = teams.Size( );
		else
			JoinSlot.ulTeam = lDesiredTeam;
		JOINQUEUE_AddPlayer( JoinSlot );

		// Tell the client what his position in line is.
		SERVERCOMMANDS_SetQueuePosition( g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		return ( false );
	}

	// If this player was eligible to get an assist, cancel that.
	TEAM_CancelAssistsOfPlayer ( static_cast<unsigned>( g_lCurrentClient ) );

	// Don't allow him to "take" flags or skulls with him. If he was carrying any,
	// spawn what he was carrying on the ground.
	players[g_lCurrentClient].mo->DropImportantItems( false );

	// [BB] Morphed players need to be unmorphed before changing teams.
	if ( players[g_lCurrentClient].morphTics )
		P_UndoPlayerMorph ( &players[g_lCurrentClient], &players[g_lCurrentClient] );

	// Save this. This will determine our message.
	bOnTeam = players[g_lCurrentClient].bOnTeam;

	// Set the new team.
	PLAYER_SetTeam( &players[g_lCurrentClient], lDesiredTeam, true );

	// Player was on a team, so tell everyone that he's changing teams.
	if ( bOnTeam )
	{
		SERVER_Printf( PRINT_HIGH, "%s \\c-defected to the \\c%c%s \\c-team.\n", players[g_lCurrentClient].userinfo.netname, V_GetColorChar( TEAM_GetTextColor( players[g_lCurrentClient].ulTeam )), TEAM_GetName( players[g_lCurrentClient].ulTeam ));
	}
	// Otherwise, tell everyone he's joining a team.
	else
	{
		SERVER_Printf( PRINT_HIGH, "%s \\c-joined the \\c%c%s \\c-team.\n", players[g_lCurrentClient].userinfo.netname, V_GetColorChar( TEAM_GetTextColor( players[g_lCurrentClient].ulTeam )), TEAM_GetName( players[g_lCurrentClient].ulTeam ));
	}

	if ( players[g_lCurrentClient].mo )
	{
		SERVERCOMMANDS_DestroyThing( players[g_lCurrentClient].mo );

		players[g_lCurrentClient].mo->Destroy( );
		players[g_lCurrentClient].mo = NULL;
	}

	// Now respawn the player at the appropriate spot. Set the player state to
	// PST_REBORNNOINVENTORY so everything (weapons, etc.) is cleared.
	// [BB] If the player was a spectator, we have to set the state to
	// PST_ENTERNOINVENTORY. Otherwise the enter scripts are not executed.
	if ( players[g_lCurrentClient].bSpectating )
		players[g_lCurrentClient].playerstate = PST_ENTERNOINVENTORY;
	else
		players[g_lCurrentClient].playerstate = PST_REBORNNOINVENTORY;

	// Also, take away spectator status.
	players[g_lCurrentClient].bSpectating = false;
	players[g_lCurrentClient].bDeadSpectator = false;

	if ( teamgame )
		G_TeamgameSpawnPlayer( g_lCurrentClient, players[g_lCurrentClient].ulTeam, true );
	else
		G_DeathMatchSpawnPlayer( g_lCurrentClient, true );

	// Tell the join queue that a player "sort of" left the game.
	JOINQUEUE_PlayerLeftGame( false );

	// Update this player's info on the scoreboard.
	SERVERCONSOLE_UpdatePlayerInfo( g_lCurrentClient, UDF_FRAGS );

	return ( false );
}

//*****************************************************************************
//
static bool server_SpectateInfo( BYTESTREAM_s *pByteStream )
{
	player_t	*pPlayer;
	ULONG		ulGametic;

	pPlayer = &players[g_lCurrentClient];

	// Read in the client's gametic.
	ulGametic = NETWORK_ReadLong( pByteStream );

	if ( gamestate == GS_LEVEL )
	{
		P_PlayerThink( pPlayer );
		if ( pPlayer->mo )
			pPlayer->mo->Tick( );
	}

	// Don't timeout.
	g_aClients[g_lCurrentClient].ulLastCommandTic = gametic;
	g_aClients[g_lCurrentClient].ulClientGameTic = ulGametic;

	return ( false );
}

//*****************************************************************************
//
static bool server_GenericCheat( BYTESTREAM_s *pByteStream )
{
	ULONG	ulCheat;

	// Read in the cheat.
	ulCheat = NETWORK_ReadByte( pByteStream );

	// If it's legal, do the cheat.
	if (( sv_cheats ) ||
		(( ulCheat == CHT_CHASECAM ) &&	(( ( deathmatch == false ) && ( teamgame == false ) ) || (players[g_lCurrentClient].bSpectating) )))
	{
		cht_DoCheat( &players[g_lCurrentClient], ulCheat );

		// Tell clients about this cheat. [BB] CHT_MORPH is completely server side.
		if ( ulCheat != CHT_MORPH )
		{
			// [BB] You only need to notify the client who wants to use chasecam about it.
			// If you tell it to all clients, it looks weird for a client spying someone with chasecam on.
			if (( ulCheat == CHT_CHASECAM ) &&	( deathmatch == false ) && ( teamgame == false ))
				SERVERCOMMANDS_GenericCheat( g_lCurrentClient, ulCheat, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
			else
				SERVERCOMMANDS_GenericCheat( g_lCurrentClient, ulCheat );
		}
	}
	// If not, boot their ass!
	else
	{
		SERVER_KickPlayer( g_lCurrentClient, "Attempted to cheat with sv_cheats being false!" );
		return ( true );
	}

	return ( false );
}

//*****************************************************************************
//
static bool server_GiveCheat( BYTESTREAM_s *pByteStream )
{
	const char	*pszItemName;
	ULONG		ulAmount;

	// Read in the item name.
	pszItemName = NETWORK_ReadString( pByteStream );

	// Read in the amount.
	ulAmount = NETWORK_ReadByte( pByteStream );

	// If it's legal, do the cheat.
	if ( sv_cheats )
	{
		cht_Give( &players[g_lCurrentClient], pszItemName, ulAmount );
/*
		// Tell clients about this cheat.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			ULONG	ulIdx;

			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( SERVER_IsValidClient( ulIdx ) == false )
					continue;

				NETWORK_CheckBuffer( ulIdx, 3 + strlen( pszItemName ));
				NETWORK_WriteHeader( &g_aClients[ulIdx].PacketBuffer, SVC_GIVECHEAT );
				NETWORK_WriteByte( &g_aClients[ulIdx].PacketBuffer, g_lCurrentClient );
				NETWORK_WriteString( &g_aClients[ulIdx].PacketBuffer, pszItemName );
				NETWORK_WriteByte( &g_aClients[ulIdx].PacketBuffer, ulAmount );
			}
		}
*/
	}
	// If not, boot their ass!
	else
	{
		SERVER_KickPlayer( g_lCurrentClient, "Attempted to cheat with sv_cheats being false!" );
		return ( true );
	}

	return ( false );
}

//*****************************************************************************
//
static bool server_SummonCheat( BYTESTREAM_s *pByteStream, LONG lType )
{
	const char		*pszName;
	AActor			*pSource;
	const PClass	*pType;
	AActor			*pActor;

	// Read in the item name.
	pszName = NETWORK_ReadString( pByteStream );

	const bool bSetAngle = NETWORK_ReadByte( pByteStream );
	// [BB] The client only sends the angle, if it is supposed to be set.
	const SHORT sAngle = bSetAngle ? NETWORK_ReadShort( pByteStream ) : 0;

	pSource = players[g_lCurrentClient].mo;
	if ( pSource == NULL )
		return ( false );

	// If it's legal, do the cheat.
	if ( sv_cheats )
	{
		pType = PClass::FindClass( pszName );
		if (( pType == NULL ) || ( pType->ActorInfo == NULL ))
		{
			Printf( "server_SummonCheat: Couldn't find class: %s\n", pszName );
			return ( false );
		}

		// If it's a missile, spawn it as a player missile.
		if ( GetDefaultByType( pType )->flags & MF_MISSILE )
		{
			pActor = P_SpawnPlayerMissile( pSource, pType );
			if ( pActor )
				SERVERCOMMANDS_SpawnMissile( pActor );
		}
		else
		{
			const AActor	 *pDef = GetDefaultByType( pType );
					
			pActor = Spawn( pType, pSource->x + FixedMul( pDef->radius * 2 + pSource->radius, finecosine[pSource->angle>>ANGLETOFINESHIFT] ),
						pSource->y + FixedMul( pDef->radius * 2 + pSource->radius, finesine[pSource->angle>>ANGLETOFINESHIFT] ),
						pSource->z + 8 * FRACUNIT, ALLOW_REPLACE );

			// [BB] If this is the summonfriend cheat, we have to make the monster friendly.
			if (pActor != NULL && lType != CLC_SUMMONCHEAT)
			{
				if (lType == CLC_SUMMONFRIENDCHEAT)
				{
					if (pActor->CountsAsKill()) 
					{
						level.total_monsters--;

						// [BB] The monster is friendly, so we need to correct the number of monsters in invasion mode.
						INVASION_UpdateMonsterCount( pActor, true );
					}

					pActor->FriendPlayer = g_lCurrentClient + 1;
					pActor->flags |= MF_FRIENDLY;
					pActor->LastHeard = players[g_lCurrentClient].mo;
				}
				else
				{
					pActor->FriendPlayer = 0;
					pActor->flags &= ~MF_FRIENDLY;
				}
			}


			if ( pActor )
				SERVERCOMMANDS_SpawnThing( pActor );

			if ( bSetAngle )
			{
				pActor->angle = pSource->angle - (ANGLE_1 * sAngle);
				// [BB] If the angle is not zero, we have to inform the clients.
				if ( pActor->angle != 0 )
					SERVERCOMMANDS_SetThingAngle( pActor );
			}
		}
	}
	// If not, boot their ass!
	else
	{
		SERVER_KickPlayer( g_lCurrentClient, "Attempted to cheat with sv_cheats being false!" );
		return ( true );
	}

	return ( false );
}

//*****************************************************************************
//
static bool server_ChangeDisplayPlayer( BYTESTREAM_s *pByteStream )
{
	ULONG		ulDisplayPlayer;

	// Read in their display player.
	ulDisplayPlayer = NETWORK_ReadByte( pByteStream );
	if (( ulDisplayPlayer < MAXPLAYERS ) && playeringame[ulDisplayPlayer] )
		g_aClients[g_lCurrentClient].ulDisplayPlayer = ulDisplayPlayer;

	return ( false );
}

//*****************************************************************************
//
static bool server_AuthenticateLevel( BYTESTREAM_s *pByteStream )
{
	ULONG								ulState;
	ULONG								ulCountdownTicks;
	DDoor								*pDoor;
	DPlat								*pPlat;
	DFloor								*pFloor;
	DElevator							*pElevator;
	DWaggleBase							*pWaggle;
	DPillar								*pPillar;
	DCeiling							*pCeiling;
	DScroller							*pScroller;
	TThinkerIterator<DDoor>				DoorIterator;
	TThinkerIterator<DPlat>				PlatIterator;
	TThinkerIterator<DFloor>			FloorIterator;
	TThinkerIterator<DElevator>			ElevatorIterator;
	TThinkerIterator<DWaggleBase>		WaggleIterator;
	TThinkerIterator<DPillar>			PillarIterator;
	TThinkerIterator<DCeiling>			CeilingIterator;
	TThinkerIterator<DScroller>			ScrollerIterator;

	// [BB] Read the name of the map, the client is trying to authenticate.
	const FString mapnameString = NETWORK_ReadString( pByteStream );

	// [BB] The client sent us the authentication data of the wrong level.
	// This can happen if the map changes too quickly twice in a row: In that case
	// we get the authentication data for the first level from the client when
	// the server already loaded the second level.
	if ( stricmp ( mapnameString.GetChars(), level.mapname ) != 0 )
	{
		// [BB] This eats the authentication strings the client is sending, necessary
		// because we need to parse the packet completely.
		SERVER_PerformAuthenticationChecksum( pByteStream );

		// [BB] This authentication problem occures rarely. Make sure that a malicious client
		// can't abuse it for flooding.
		if ( server_CheckForClientCommandFlood ( g_lCurrentClient ) == true )
			return ( true );

		// [BB] Tell the client authenticate again.
		SERVERCOMMANDS_MapAuthenticate ( level.mapname, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		return ( false );
	}

	if ( SERVER_PerformAuthenticationChecksum( pByteStream ) == false )
	{
		SERVER_KickPlayer( g_lCurrentClient, "Level authentication failed." );
		return ( true );
	}

	// [BB] Update the client's state according to the successful authenticaion.
	if ( SERVER_GetClient( g_lCurrentClient )->State == CLS_SPAWNED_BUT_NEEDS_AUTHENTICATION )
		SERVER_GetClient( g_lCurrentClient )->State = CLS_SPAWNED;

	// Now that the level has been authenticated, send all the level data for the client.

	// Send skill level.
	SERVERCOMMANDS_SetGameSkill( g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// Send special settings like teamplay and deathmatch.
	SERVERCOMMANDS_SetGameMode( g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// Send timelimit, fraglimit, etc.
	SERVERCOMMANDS_SetGameModeLimits( g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// If this is LMS, send the allowed weapons.
	if ( lastmanstanding || teamlms )
	{
		SERVERCOMMANDS_SetLMSAllowedWeapons( g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		SERVERCOMMANDS_SetLMSSpectatorSettings( g_lCurrentClient, SVCF_ONLYTHISCLIENT );
	}

	// If this is CTF or ST, tell the client whether or not we're in simple mode.
	if ( GAMEMODE_GetFlags( GAMEMODE_GetCurrentMode( )) & GMF_USETEAMITEM )
		SERVERCOMMANDS_SetSimpleCTFSTMode( g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// Send the map name, and have the client load it.
	SERVERCOMMANDS_MapLoad( g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// Send the map music.
	SERVERCOMMANDS_SetMapMusic( SERVER_GetMapMusic( ), g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// If we're in a duel or LMS mode, tell him the state of the game mode.
	if ( duel || lastmanstanding || teamlms || possession || teampossession || survival || invasion )
	{
		if ( duel )
		{
			ulState = DUEL_GetState( );
			ulCountdownTicks = DUEL_GetCountdownTicks( );
		}
		else if ( survival )
		{
			ulState = SURVIVAL_GetState( );
			ulCountdownTicks = SURVIVAL_GetCountdownTicks( );
		}
		else if ( invasion )
		{
			ulState = INVASION_GetState( );
			ulCountdownTicks = INVASION_GetCountdownTicks( );
		}
		else if ( possession || teampossession )
		{
			ulState = POSSESSION_GetState( );
			if ( ulState == (PSNSTATE_e)PSNS_ARTIFACTHELD )
				ulCountdownTicks = POSSESSION_GetArtifactHoldTicks( );
			else
				ulCountdownTicks = POSSESSION_GetCountdownTicks( );
		}
		else
		{
			ulState = LASTMANSTANDING_GetState( );
			ulCountdownTicks = LASTMANSTANDING_GetCountdownTicks( );
		}

		SERVERCOMMANDS_SetGameModeState( ulState, ulCountdownTicks, g_lCurrentClient, SVCF_ONLYTHISCLIENT );

		// Also, if we're in invasion mode, tell the client what wave we're on.
		if ( invasion )
			SERVERCOMMANDS_SetInvasionWave( g_lCurrentClient, SVCF_ONLYTHISCLIENT );
	}

	// Tell the client of any lines that have been altered since the level start.
	SERVER_UpdateLines( g_lCurrentClient );

	// Tell the client of any sides that have been altered since the level start.
	SERVER_UpdateSides( g_lCurrentClient );

	// Tell the client of any sectors that have been altered since the level start.
	SERVER_UpdateSectors( g_lCurrentClient );

	// Tell the client about any active doors.
	while (( pDoor = DoorIterator.Next( )) != NULL )
		pDoor->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active plats.
	while (( pPlat = PlatIterator.Next( )) != NULL )
		pPlat->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active floors.
	while (( pFloor = FloorIterator.Next( )) != NULL )
		pFloor->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active elevators.
	while (( pElevator = ElevatorIterator.Next( )))
		pElevator->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active waggles.
	while (( pWaggle = WaggleIterator.Next( )))
		pWaggle->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active pillars.
	while (( pPillar = PillarIterator.Next( )) != NULL )
		pPillar->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active ceilings.
	while (( pCeiling = CeilingIterator.Next( )) != NULL )
		pCeiling->UpdateToClient( g_lCurrentClient );

	// Tell the client about any active scrollers.
	while (( pScroller = ScrollerIterator.Next( )) != NULL )
		pScroller->UpdateToClient( g_lCurrentClient );

	// Tell client to spawn themselves (this doesn't happen in the full update).
	if ( players[g_lCurrentClient].mo != NULL )
	{
		SERVERCOMMANDS_SpawnPlayer( g_lCurrentClient, PST_REBORNNOINVENTORY, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		// [BB] The player body spawned on the client has the default flag values.
		// If the actual body doesn't have default values (e.g. the player had the
		// fly cheat before tha map change), tell the client about the actual values.
		SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults ( players[g_lCurrentClient].mo, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		// [BB] Inform the player about its health, otherwise the client thinks that the local player has the default health.
		SERVERCOMMANDS_SetPlayerHealth( g_lCurrentClient, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		// [BB] It's possible that the MaxHealth property was changed dynamically with ACS, so send it.
		SERVERCOMMANDS_SetPlayerMaxHealth( g_lCurrentClient, g_lCurrentClient, SVCF_ONLYTHISCLIENT );

		/* [BB] Does not work with the latest ZDoom changes. Check if it's still necessary.
		// If the client has weapon pieces, tell them.
		if ( players[g_lCurrentClient].pieces )
			SERVERCOMMANDS_SetPlayerPieces( g_lCurrentClient, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
		*/

		// If the client is dead, tell them that.
		if ( players[g_lCurrentClient].mo->health <= 0 )
			SERVERCOMMANDS_ThingIsCorpse( players[g_lCurrentClient].mo, g_lCurrentClient, SVCF_ONLYTHISCLIENT );
	}

	// If this player travelled from the last level, we need to reset his inventory so that
	// he still has it on the client end.
	if ( G_AllowTravel( ))
		SERVER_ResetInventory( g_lCurrentClient );

	// Send a snapshot of the level.
	SERVER_SendFullUpdate( g_lCurrentClient );

	// If we need to start this client's enter scripts, do that now.
	if ( g_aClients[g_lCurrentClient].bRunEnterScripts )
	{
		FBehavior::StaticStartTypedScripts( SCRIPT_Enter, players[g_lCurrentClient].mo, true );
		g_aClients[g_lCurrentClient].bRunEnterScripts = false;
	}

	// [BL] To remove pistol fire-fights if the player is supposed to be unarmed clear his inventory
	if ( players[g_lCurrentClient].bUnarmed )
		SERVERCOMMANDS_DestroyAllInventory( g_lCurrentClient, g_lCurrentClient, SVCF_ONLYTHISCLIENT );

	// Finally, send out the packet.
	SERVER_SendClientPacket( g_lCurrentClient, true );

	return ( false );
}

//*****************************************************************************
//
static bool server_CallVote( BYTESTREAM_s *pByteStream )
{
	ULONG		ulVoteCmd;
	char		szCommand[128];

	// Read in the type of vote happening.
	ulVoteCmd = NETWORK_ReadByte( pByteStream );

	// Read in the parameters for the vote.
	FString		Parameters = NETWORK_ReadString( pByteStream );

	// Read in the reason for the vote.
	FString		Reason = NETWORK_ReadString( pByteStream );

	//==============
	// VERIFICATION
	//==============

	// [BB] If the client is flooding the server with commands, the client is
	// kicked and we don't need to handle the command.
	if ( server_CheckForClientCommandFlood ( g_lCurrentClient ) == true )
		return ( true );

	// Don't allow votes if the server has them disabled.
	if ( sv_nocallvote == 1 )
	{
		SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "This server has disabled voting.\n" );
		return false;
	}

	// Also, don't allow spectator votes if the server has them disabled.
	if ( sv_nocallvote == 2 && players[g_lCurrentClient].bSpectating )
	{
		SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "This server requires spectators to join the game to vote.\n" );
		return false;
	}

	// Make sure we have the required number of voters.
	if ( static_cast<int>( CALLVOTE_CountNumEligibleVoters( )) < sv_minvoters )
	{
		SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "This server requires at least %d eligible voters to call a vote.\n", static_cast<ULONG>( sv_minvoters ));
		return false;
	}

	// [BB] Further no votes, when not in a level, e.g. during intermission.
	if ( gamestate != GS_LEVEL )
	{
		SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "You can only vote during the game.\n" );
		return false;
	}

	// Check if the specific type of vote is allowed.
	bool bVoteAllowed = false;
	switch ( ulVoteCmd )
	{
	case VOTECMD_KICK:

		bVoteAllowed = !sv_nokickvote;
		sprintf( szCommand, "kick" );
		break;
	case VOTECMD_MAP:

		bVoteAllowed = !sv_nomapvote;
		sprintf( szCommand, "map" );
		break;
	case VOTECMD_CHANGEMAP:

		bVoteAllowed = !sv_nochangemapvote;
		sprintf( szCommand, "changemap" );
		break;
	case VOTECMD_FRAGLIMIT:

		bVoteAllowed = !sv_nofraglimitvote;
		sprintf( szCommand, "fraglimit" );
		break;
	case VOTECMD_TIMELIMIT:

		bVoteAllowed = !sv_notimelimitvote;
		sprintf( szCommand, "timelimit" );
		break;
	case VOTECMD_WINLIMIT:

		bVoteAllowed = !sv_nowinlimitvote;
		sprintf( szCommand, "winlimit" );
		break;
	case VOTECMD_DUELLIMIT:

		bVoteAllowed = !sv_noduellimitvote;
		sprintf( szCommand, "duellimit" );
		break;
	case VOTECMD_POINTLIMIT:

		bVoteAllowed = !sv_nopointlimitvote;
		sprintf( szCommand, "pointlimit" );
		break;
	default:

		return ( false );
	}

	// Begin the vote, if that type is allowed.
	if ( bVoteAllowed )
		CALLVOTE_BeginVote( szCommand, Parameters, Reason, g_lCurrentClient );
	else
		SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, "%s votes are disabled on this server.\n", szCommand );

	return ( false );
}

//*****************************************************************************
//
static bool server_InventoryUseAll( BYTESTREAM_s *pByteStream )
{
	if (gamestate == GS_LEVEL && !paused && SERVER_IsValidPlayerWithMo(g_lCurrentClient) )
	{
		AInventory *item = players[g_lCurrentClient].mo->Inventory;

		while (item != NULL)
		{
			AInventory *next = item->Inventory;
			if (item->ItemFlags & IF_INVBAR && !(item->IsKindOf(RUNTIME_CLASS(APuzzleItem))))
			{
				players[g_lCurrentClient].mo->UseInventory (item);
			}
			item = next;
		}
	}
	return ( false );
}

//*****************************************************************************
//
static bool server_InventoryUse( BYTESTREAM_s *pByteStream )
{
	USHORT usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	if (gamestate == GS_LEVEL && !paused && SERVER_IsValidPlayerWithMo(g_lCurrentClient) )
	{
		AInventory *item = players[g_lCurrentClient].mo->FindInventory (NETWORK_GetClassNameFromIdentification( usActorNetworkIndex));
		if (item != NULL)
		{
			players[g_lCurrentClient].mo->UseInventory (item);
		}
	}

	return ( false );
}

//*****************************************************************************
//
static bool server_InventoryDrop( BYTESTREAM_s *pByteStream )
{
	USHORT		usActorNetworkIndex = 0;
	AInventory	*pItem;

	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// [BB] The server may forbid dropping completely.
	if ( sv_nodrop )
	{
		SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient(), "Dropping items is not allowed in this server.\n" );
		return ( false );
	}

	if (gamestate == GS_LEVEL && !paused && SERVER_IsValidPlayerWithMo(g_lCurrentClient) )
	{
		pItem = players[g_lCurrentClient].mo->FindInventory( NETWORK_GetClassNameFromIdentification( usActorNetworkIndex ));
		if ( pItem )
			players[g_lCurrentClient].mo->DropInventory( pItem );
	}

	return ( false );
}

//*****************************************************************************
//
static bool server_Puke( BYTESTREAM_s *pByteStream )
{
	ULONG ulScript = NETWORK_ReadShort( pByteStream );
	ULONG ulArgn = NETWORK_ReadByte( pByteStream );
	int arg[3] = { 0, 0, 0 };
	for ( ULONG ulIdx = 0; ulIdx < ulArgn; ++ulIdx)
		arg[ulIdx] = NETWORK_ReadLong ( pByteStream );
	bool bAlways = !!NETWORK_ReadByte( pByteStream );

	// [BB] A normal client checks if the script is pukeable and only requests to puke pukeable scripts.
	// Thus if the requested script is not pukeable, the client was tampered with.
	if ( ACS_IsScriptPukeable ( ulScript ) == false )
	{
		// [BB] Trying to puke a non-pukeable script is treated as possible command flooding
		if ( server_CheckForClientCommandFlood ( g_lCurrentClient ) == true )
			return ( true );

		return ( false );
	}

	// [BB] Execute the script as if it was invoked by the puke command.
	P_StartScript (players[g_lCurrentClient].mo, NULL, ulScript, level.mapname, false,
		arg[0], arg[1], arg[2], bAlways, false, true);

	return ( false );
}

//*****************************************************************************
//
static bool server_MorphCheat( BYTESTREAM_s *pByteStream )
{
	const char *pszMorphClass = NETWORK_ReadString( pByteStream );

	// If it's legal, do the moprh.
	if ( sv_cheats )
	{
		const char *msg = cht_Morph (players + g_lCurrentClient, PClass::FindClass (pszMorphClass), false);
		FString messageString = *msg != '\0' ? msg : "Morph failed.";
		SERVER_PrintfPlayer( PRINT_HIGH, g_lCurrentClient, messageString.GetChars() );
	}
	// If not, boot their ass!
	else
	{
		SERVER_KickPlayer( g_lCurrentClient, "Attempted to cheat with sv_cheats being false!" );
		return ( true );
	}

	return ( false );
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( kick_idx )
{
	// Only the server can boot players!
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: kick_idx <player index> [reason]\nYou can get the list of players and indexes with the ccmd playerinfo.\n" );
		return;
	}

	ULONG ulIdx =  atoi(argv[1]);

	// [BB] Validity checks are done in SERVER_KickPlayer.
	// If we provided a reason, give it.
	if ( argv.argc( ) >= 3 )
		SERVER_KickPlayer( ulIdx, argv[2] );
	else
		SERVER_KickPlayer( ulIdx, "None given." );
	return;
}

CCMD( kick_ip )
{
	// Only the server can boot players!
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: kick_ip <ip:port> [reason]\nYou can get the list of players and indexes with the ccmd playerinfo.\n" );
		return;
	}

	NETADDRESS_s address;
	NETWORK_StringToAddress( argv[1], &address );
	ULONG ulIdx = SERVER_FindClientByAddress( address );

	// [BB] Validity checks are done in SERVER_KickPlayer.
	// If we provided a reason, give it.
	if ( argv.argc( ) >= 3 )
		SERVER_KickPlayer( ulIdx, argv[2] );
	else
		SERVER_KickPlayer( ulIdx, "None given." );

	return;
}

CCMD( kick )
{
	ULONG	ulIdx;
	char	szPlayerName[64];

	// Only the server can boot players!
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: kick <playername> [reason]\n" );
		return;
	}

	// Loop through all the players, and try to find one that matches the given name.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] == false )
			continue;

		// Removes the color codes from the player name so it appears as the server sees it in the window.
		sprintf( szPlayerName, "%s", players[ulIdx].userinfo.netname );
		V_RemoveColorCodes( szPlayerName );

		if ( stricmp( szPlayerName, argv[1] ) == 0 )
		{
			// If we provided a reason, give it.
			if ( argv.argc( ) >= 3 )
				SERVER_KickPlayer( ulIdx, argv[2] );
			else
				SERVER_KickPlayer( ulIdx, "None given." );

			return;
		}
	}

	// Didn't find a player that matches the name.
	Printf( "Unknown player: %s\n", argv[1] );
	return;
}

//*****************************************************************************
//
CCMD( kickfromgame_idx )
{
	// Only the server can boot players!
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: kickfromgame_idx <player index> [reason]\nYou can get the list of players and indexes with the ccmd playerinfo.\n" );
		return;
	}

	ULONG ulIdx =  atoi(argv[1]);

	// [BB] Validity checks are done in SERVER_KickPlayerFromGame.
	// If we provided a reason, give it.
	if ( argv.argc( ) >= 3 )
		SERVER_KickPlayerFromGame( ulIdx, argv[2] );
	else
		SERVER_KickPlayerFromGame( ulIdx, "None given." );
	return;
}

//*****************************************************************************
//
CCMD( kickfromgame )
{
	ULONG	ulIdx;
	char	szPlayerName[64];

	// Only the server can boot players!
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: kickfromgame <playername> [reason]\n" );
		return;
	}

	// Loop through all the players, and try to find one that matches the given name.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] == false )
			continue;

		// Removes the color codes from the player name so it appears as the server sees it in the window.
		sprintf( szPlayerName, "%s", players[ulIdx].userinfo.netname );
		V_RemoveColorCodes( szPlayerName );

		if ( stricmp( szPlayerName, argv[1] ) == 0 )
		{
			// Already a spectator!
			if ( PLAYER_IsTrueSpectator( &players[g_lCurrentClient] ))
				continue;

			// If we provided a reason, give it.
			if ( argv.argc( ) >= 3 )
				SERVER_KickPlayerFromGame( ulIdx, argv[2] );
			else
				SERVER_KickPlayerFromGame( ulIdx, "None given" );

			return;
		}
	}

	// Didn't find a player that matches the name.
	Printf( "Unknown player: %s\n", argv[1] );
	return;
}

//*****************************************************************************
#ifdef	_DEBUG
CCMD( testchecksum )
{
	char	szString[1024];
	FString	MD5String;
	char	append[256];

	memset( szString, 0, 1024 );
	memset( append, 0, 256 );
		
	for ( int i = 1; i <= argv.argc( ) - 1; ++i )
	{
		sprintf( append, "%s ", argv[i] );
		strcat( szString, append );
	}
	
	CMD5Checksum::GetMD5( (BYTE *)szString, 1024, MD5String );
	Printf( "%s\n", MD5String.GetChars() );
}

//*****************************************************************************
CCMD( testchecksumonlevel )
{
	LONG		lBaseLumpNum;
	LONG		lCurLumpNum;
	LONG		lLumpSize;
	FString		Checksum;
	FWadLump	Data;
	BYTE		*pbData;

	// This is the lump number of the current map we're on.
	lBaseLumpNum = Wads.GetNumForName( level.mapname );

	// Get the vertex lump.
	lCurLumpNum = lBaseLumpNum + ML_VERTEXES;
	lLumpSize = Wads.LumpLength( lCurLumpNum );

	// Open the vertex lump, and dump the data from it into our data buffer.
	Data = Wads.OpenLumpNum( lCurLumpNum );
	pbData = new BYTE[lLumpSize];
	Data.Read( pbData, lLumpSize );

	// Perform the checksum on our buffer, and free it.
	CMD5Checksum::GetMD5( pbData, lLumpSize, Checksum );
	delete[] pbData;

	// Now, send the vertex checksum string.
	Printf( "Verticies: %s\n", Checksum.GetChars() );

	// Get the linedefs lump.
	lCurLumpNum = lBaseLumpNum + ML_LINEDEFS;
	lLumpSize = Wads.LumpLength( lCurLumpNum );

	// Open the linedefs lump, and dump the data from it into our data buffer.
	Data = Wads.OpenLumpNum( lCurLumpNum );
	pbData = new BYTE[lLumpSize];
	Data.Read( pbData, lLumpSize );

	// Perform the checksum on our buffer, and free it.
	CMD5Checksum::GetMD5( pbData, lLumpSize, Checksum );
	delete[] pbData;

	// Now, send the linedefs checksum string.
	Printf( "Linedefs: %s\n", Checksum.GetChars() );

	// Get the sidedefs lump.
	lCurLumpNum = lBaseLumpNum + ML_SIDEDEFS;
	lLumpSize = Wads.LumpLength( lCurLumpNum );

	// Open the sidedefs lump, and dump the data from it into our data buffer.
	Data = Wads.OpenLumpNum( lCurLumpNum );
	pbData = new BYTE[lLumpSize];
	Data.Read( pbData, lLumpSize );

	// Perform the checksum on our buffer, and free it.
	CMD5Checksum::GetMD5( pbData, lLumpSize, Checksum );
	delete[] pbData;

	// Now, send the sidedefs checksum string.
	Printf( "Sidedefs: %s\n", Checksum.GetChars() );

	// Get the sectors lump.
	lCurLumpNum = lBaseLumpNum + ML_SECTORS;
	lLumpSize = Wads.LumpLength( lCurLumpNum );

	// Open the sectors lump, and dump the data from it into our data buffer.
	Data = Wads.OpenLumpNum( lCurLumpNum );
	pbData = new BYTE[lLumpSize];
	Data.Read( pbData, lLumpSize );

	// Perform the checksum on our buffer, and free it.
	CMD5Checksum::GetMD5( pbData, lLumpSize, Checksum );
	delete[] pbData;

	// Now, send the sectors checksum string.
	Printf( "Sectors: %s\n", Checksum.GetChars() );
}

//*****************************************************************************
CCMD( trytocrashclient )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( SERVER_IsValidClient( ulIdx ) == false )
				continue;

			SERVER_CheckClientBuffer( ulIdx, 256, true );
			NETWORK_WriteByte( &g_aClients[ulIdx].PacketBuffer.ByteStream, M_Random( ) % NUM_SERVER_COMMANDS );
			for ( ulIdx2 = 0; ulIdx2 < 512; ulIdx2++ )
				NETWORK_WriteByte( &g_aClients[ulIdx].PacketBuffer.ByteStream, M_Random( ));

		}
	}
}
#endif	// _DEBUG

#ifdef CREATE_PACKET_LOG

//*****************************************************************************
// [RC] Logs a suspicious packet to the log.
static void	server_LogPacket( BYTESTREAM_s *pByteStream, NETADDRESS_s Address, const char *pszReason )
{
	FString		logLine;
	BYTE		*OriginalPosition = pByteStream->pbStream;

	// Already logged this one.
	if ( pByteStream->bPacketAlreadyLogged)
		return;

	pByteStream->bPacketAlreadyLogged = true;

	Printf( "Logging packet from %s, because: %s.\n", NETWORK_AddressToString( Address ), pszReason);

	// Log all further packets from this IP.
	if ( !g_HackerIPList.isIPInList( Address ) )
	{
		char szAddress[4][4];

		itoa( Address.abIP[0], szAddress[0], 10 );
		itoa( Address.abIP[1], szAddress[1], 10 );
		itoa( Address.abIP[2], szAddress[2], 10 );
		itoa( Address.abIP[3], szAddress[3], 10 );
		std::string reason;
		reason = "Hacker";
		g_HackerIPList.addEntry( szAddress[0], szAddress[1], szAddress[2],  szAddress[3], "", pszReason, reason);
	}

	// Write the start of the log entry.
	logLine.Format("\nLogging packet from %s:", NETWORK_AddressToString( Address ));
	FString logfilename;
	time_t clock;
	struct tm *lt;
	time (&clock);
	lt = localtime (&clock);
	logLine.AppendFormat("\n\ton: %d/%d/%d, at %02d:%02d:%02d", lt->tm_mon + 1, lt->tm_mday, lt->tm_year + 1900, lt->tm_hour, lt->tm_min, lt->tm_sec); 
	logLine.AppendFormat("\n\treason: %s\n\n", pszReason);
	fputs( logLine , PacketLogFile );

	// Advance to the beginning of the packet.
	while ( pByteStream->pbStream != pByteStream->pbStreamBeginning )
		pByteStream->pbStream -= 1;

	// Next, log all the bytes, until the end.
	while ( pByteStream->pbStream != pByteStream->pbStreamEnd )
	{
		int Byte = *pByteStream->pbStream;
		pByteStream->pbStream += 1;		
		logLine.Format("%d ", Byte);

		fputs(logLine, PacketLogFile);
	}

	// Return the stream to where we were.
	while ( pByteStream->pbStream != OriginalPosition )
		pByteStream->pbStream -= 1;

	logLine.Format("\nEnd of packet.\n");
	fputs( logLine, PacketLogFile );
	fflush (PacketLogFile);
}
#endif
