// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: i_net.c,v 1.2 1997/12/29 19:50:54 pekangas Exp $
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
//		Low-level networking code. Uses BSD sockets for UDP networking.
//
//-----------------------------------------------------------------------------


/* [Petteri] Check if compiling for Win32:	*/
#if defined(__WINDOWS__) || defined(__NT__) || defined(_MSC_VER) || defined(_WIN32)
#ifndef __WIN32__
#	define __WIN32__
#endif
#endif
/* Follow #ifdef __WIN32__ marks */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* [Petteri] Use Winsock for Win32: */
#ifdef __WIN32__
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	include <winsock.h>
#define USE_WINDOWS_DWORD
#else
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <errno.h>
#	include <unistd.h>
#	include <netdb.h>
#	include <sys/ioctl.h>
#endif

#include "doomtype.h"
#include "i_system.h"
#include "d_event.h"
#include "d_net.h"
#include "m_argv.h"
#include "m_alloc.h"
#include "m_swap.h"
#include "m_crc32.h"
#include "d_player.h"
#include "templates.h"
#include "c_console.h"
#include "st_start.h"

#include "doomstat.h"

#include "i_net.h"



/* [Petteri] Get more portable: */
#ifndef __WIN32__
typedef int SOCKET;
#define SOCKET_ERROR		-1
#define INVALID_SOCKET		-1
#define closesocket			close
#define ioctlsocket			ioctl
#define Sleep(x)			usleep (x * 1000)
#define WSAEWOULDBLOCK		EWOULDBLOCK
#define WSAECONNRESET		ECONNRESET
#define WSAGetLastError()	errno
#endif

#ifdef __WIN32__
#define IPPORT_USERRESERVED 5000
typedef int socklen_t;
#endif

//
// NETWORKING
//

static u_short DOOMPORT = (IPPORT_USERRESERVED + 29);
static SOCKET mysocket = INVALID_SOCKET;
static sockaddr_in sendaddress[MAXNETNODES];
static BYTE sendplayer[MAXNETNODES];

#ifdef __WIN32__
char	*neterror (void);
#else
#define neterror() strerror(errno)
#endif

enum
{
	PRE_CONNECT,
	PRE_DISCONNECT,
	PRE_ALLHERE,
	PRE_CONACK,
	PRE_ALLHEREACK,
	PRE_GO
};

// Set PreGamePacket.fake to this so that the game rejects any pregame packets
// after it starts. This translates to NCMD_SETUP|NCMD_MULTI.
#define PRE_FAKE 0x30

struct PreGamePacket
{
	BYTE fake;
	BYTE message;
	BYTE numnodes;
	union
	{
		BYTE consolenum;
		BYTE numack;
	};
	struct
	{
		u_long	address;
		u_short	port;
		BYTE	player;
		BYTE	pad;
	} machines[MAXNETNODES];
};

//
// UDPsocket
//
SOCKET UDPsocket (void)
{
	SOCKET s;
		
	// allocate a socket
	s = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
		I_FatalError ("can't create socket: %s", neterror ());

	return s;
}

//
// BindToLocalPort
//
void BindToLocalPort (SOCKET s, u_short port)
{
	int v;
	sockaddr_in address;

	memset (&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
						
	v = bind (s, (sockaddr *)&address, sizeof(address));
	if (v == SOCKET_ERROR)
		I_FatalError ("BindToPort: %s", neterror ());
}

int FindNode (sockaddr_in *address)
{
	int i;

	// find remote node number
	for (i = 0; i<doomcom.numnodes; i++)
		if (address->sin_addr.s_addr == sendaddress[i].sin_addr.s_addr
			&& address->sin_port == sendaddress[i].sin_port)
			break;

	if (i == doomcom.numnodes)
	{
		// packet is not from one of the players (new game broadcast?)
		i = -1;
	}
	return i;
}

//
// PacketSend
//
void PacketSend (void)
{
	int c;

	//printf ("sending %i\n",gametic);
	c = sendto (mysocket , (const char*)doomcom.data, doomcom.datalength
				,0,(sockaddr *)&sendaddress[doomcom.remotenode]
				,sizeof(sendaddress[doomcom.remotenode]));

	//	if (c == -1)
	//			I_Error ("SendPacket error: %s",strerror(errno));
}


//
// PacketGet
//
void PacketGet (void)
{
	int c;
	socklen_t fromlen;
	sockaddr_in fromaddress;
	int node;

	fromlen = sizeof(fromaddress);
	c = recvfrom (mysocket, (char*)doomcom.data, MAX_MSGLEN, 0
				  , (sockaddr *)&fromaddress, &fromlen);
	node = FindNode (&fromaddress);

	if (node >= 0 && c == SOCKET_ERROR)
	{
		int err = WSAGetLastError();

		if (err == WSAECONNRESET)
		{ // The remote node aborted unexpectedly, so pretend it sent an exit packet

			Printf (PRINT_BOLD, "The connection from %s was dropped\n",
				players[sendplayer[node]].userinfo.netname);

			doomcom.data[0] = 0x80;	// NCMD_EXIT
			c = 1;
		}
		else if (err != WSAEWOULDBLOCK)
		{
			I_Error ("GetPacket: %s", neterror ());
		}
		else
		{
			doomcom.remotenode = -1;		// no packet
			return;
		}
	}

	doomcom.remotenode = node;
	doomcom.datalength = (short)c;
}

sockaddr_in *PreGet (void *buffer, int bufferlen, bool noabort)
{
	static sockaddr_in fromaddress;
	socklen_t fromlen;
	int c;

	fromlen = sizeof(fromaddress);
	c = recvfrom (mysocket, (char *)buffer, bufferlen, 0,
		(sockaddr *)&fromaddress, &fromlen);

	if (c == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK || (noabort && err == WSAECONNRESET))
			return NULL;	// no packet
		I_Error ("PreGet: %s", neterror ());
	}
	return &fromaddress;
}

void PreSend (const void *buffer, int bufferlen, sockaddr_in *to)
{
	sendto (mysocket, (const char *)buffer, bufferlen, 0, (sockaddr *)to, sizeof(*to));
}

void BuildAddress (sockaddr_in *address, char *name)
{
	hostent *hostentry;		// host information entry
	u_short port;
	char *portpart;
	bool isnamed = false;
	int curchar;
	char c;

	address->sin_family = AF_INET;

	if ( (portpart = strchr (name, ':')) )
	{
		*portpart = 0;
		port = atoi (portpart + 1);
		if (!port)
		{
			Printf ("Weird port: %s (using %d)\n", portpart + 1, DOOMPORT);
			port = DOOMPORT;
		}
	}
	else
	{
		port = DOOMPORT;
	}
	address->sin_port = htons(port);

	for (curchar = 0; (c = name[curchar]) ; curchar++)
	{
		if ((c < '0' || c > '9') && c != '.')
		{
			isnamed = true;
			break;
		}
	}

	if (!isnamed)
	{
		address->sin_addr.s_addr = inet_addr (name);
		Printf ("Node number, %d address %s\n", doomcom.numnodes, name);
	}
	else
	{
		hostentry = gethostbyname (name);
		if (!hostentry)
			I_FatalError ("gethostbyname: couldn't find %s\n%s", name, neterror());
		address->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];
		Printf ("Node number %d hostname %s\n",
			doomcom.numnodes, hostentry->h_name);
	}

	if (portpart)
		*portpart = ':';
}

void CloseNetwork (void)
{
	if (mysocket != INVALID_SOCKET)
	{
		closesocket (mysocket);
		mysocket = INVALID_SOCKET;
	}
#ifdef __WIN32__
	WSACleanup ();
#endif
}

void StartNetwork (bool autoPort)
{
	u_long trueval = 1;
#ifdef __WIN32__
	WSADATA wsad;

	if (WSAStartup (0x0101, &wsad))
	{
		I_FatalError ("Could not initialize Windows Sockets");
	}
#endif

	atterm (CloseNetwork);

	netgame = true;
	multiplayer = true;
	
	// create communication socket
	mysocket = UDPsocket ();
	BindToLocalPort (mysocket, autoPort ? 0 : DOOMPORT);
	ioctlsocket (mysocket, FIONBIO, &trueval);
}

void WaitForPlayers (int i)
{
	if (i == Args.NumArgs() - 1)
		I_FatalError ("Not enough parameters after -net");

	StartNetwork (false);

	// parse player number and host list
	doomcom.consoleplayer = (short)(Args.GetArg (i+1)[0]-'1');
	Printf ("Console player number: %d\n", doomcom.consoleplayer);

	doomcom.numnodes = 1;		// this node for sure
		
	i++;
	while (++i < Args.NumArgs() && Args.GetArg (i)[0] != '-' && Args.GetArg (i)[0] != '+')
	{
		BuildAddress (&sendaddress[doomcom.numnodes], Args.GetArg (i));
		doomcom.numnodes++;
	}

	Printf ("Total players: %d\n", doomcom.numnodes);
		
	doomcom.id = DOOMCOM_ID;
	doomcom.numplayers = doomcom.numnodes;
}

void SendAbort (void)
{
	BYTE dis[2] = { PRE_FAKE, PRE_DISCONNECT };

	while (--doomcom.numnodes > 0)
	{
		PreSend (dis, 2, &sendaddress[doomcom.numnodes]);
		PreSend (dis, 2, &sendaddress[doomcom.numnodes]);
		PreSend (dis, 2, &sendaddress[doomcom.numnodes]);
		PreSend (dis, 2, &sendaddress[doomcom.numnodes]);
	}
}

bool Host_CheckForConnects (void *userdata)
{
	PreGamePacket packet;
	int numplayers = (int)(intptr_t)userdata;
	sockaddr_in *from;
	int node;

	while ( (from = PreGet (&packet, sizeof(packet), false)) )
	{
		if (packet.fake != PRE_FAKE)
		{
			continue;
		}
		switch (packet.message)
		{
		case PRE_CONNECT:
			node = FindNode (from);
			if (node == -1)
			{
				node = doomcom.numnodes++;
				sendaddress[node] = *from;
			}
			Printf ("Got connect from node %d\n", node);
			packet.message = PRE_CONACK;
			packet.numnodes = numplayers;
			packet.consolenum = node;
			PreSend (&packet, 4, from);
			ST_NetProgress (doomcom.numnodes);
			break;

		case PRE_DISCONNECT:
			node = FindNode (from);
			if (node >= 0)
			{
				Printf ("Got disconnect from node %d\n", node);
				doomcom.numnodes--;
				while (node < doomcom.numnodes)
				{
					sendaddress[node] = sendaddress[node+1];
					node++;
				}
				ST_NetProgress (doomcom.numnodes);
			}
			break;
		}
	}
	if (doomcom.numnodes < numplayers)
	{
		return false;
	}

	// It's possible somebody bailed out after all players were found.
	// Unfortunately, this isn't guaranteed to catch all of them.
	// Oh well. Better than nothing.
	while ( (from = PreGet (&packet, sizeof(packet), false)) )
	{
		if (packet.fake == PRE_FAKE && packet.message == PRE_DISCONNECT)
		{
			node = FindNode (from);
			if (node >= 0)
			{
				doomcom.numnodes--;
				while (node < doomcom.numnodes)
				{
					sendaddress[node] = sendaddress[node+1];
					node++;
				}
			}
			break;
		}
	}
	return doomcom.numnodes >= numplayers;
}

bool Host_SendAllHere (void *userdata)
{
	int *gotack = (int *)userdata;	// ackcount is at gotack[MAXNETNODES]
	PreGamePacket packet;
	int node;
	sockaddr_in *from;

	for (node = 1, packet.numack = 1; node < doomcom.numnodes; ++node)
	{
		packet.numack += gotack[node];
	}

	// Send out address information to all guests. Guests that have already
	// acknowledged receipt effectively get just a heartbeat packet.
	packet.fake = PRE_FAKE;
	packet.message = PRE_ALLHERE;
	for (node = 1; node < doomcom.numnodes; node++)
	{
		int machine, spot = 0;

		if (!gotack[node])
		{
			for (spot = 0, machine = 1; machine < doomcom.numnodes; machine++)
			{
				if (node != machine)
				{
					packet.machines[spot].address = sendaddress[machine].sin_addr.s_addr;
					packet.machines[spot].port = sendaddress[machine].sin_port;
					packet.machines[spot].player = node;

					spot++;	// fixes problem of new address replacing existing address in
							// array; it's supposed to increment the index before getting
							// and storing in the packet the next address.
				}
			}
			packet.numnodes = doomcom.numnodes - 2;
		}
		else
		{
			packet.numnodes = 0;
		}
		PreSend (&packet, 4 + spot*8, &sendaddress[node]);
	}

	// Check for replies.
	while ( (from = PreGet (&packet, sizeof(packet), false)) )
	{
		if (packet.fake == PRE_FAKE && packet.message == PRE_ALLHEREACK)
		{
			node = FindNode (from);
			if (node >= 0)
			{
				if (!gotack[node])
				{
					gotack[node] = true;
					gotack[MAXNETNODES]++;
				}
			}
			PreSend (&packet, 2, from);
		}
	}

	// If everybody has replied, then this loop can end.
	return gotack[MAXNETNODES] == doomcom.numnodes - 1;
}

void HostGame (int i)
{
	PreGamePacket packet;
	int numplayers;
	int node;
	int gotack[MAXNETNODES+1];

	if ((i == Args.NumArgs() - 1) || !(numplayers = atoi (Args.GetArg(i+1))))
	{	// No player count specified, assume 2
		numplayers = 2;
	}

	if (numplayers == 1)
	{ // Special case: Only 1 player, so don't bother starting the network
		netgame = false;
		multiplayer = true;
		doomcom.id = DOOMCOM_ID;
		doomcom.numplayers = doomcom.numnodes = 1;
		doomcom.consoleplayer = 0;
		return;
	}

	StartNetwork (false);

	// [JC] - this computer is starting the game, therefore it should
	// be the Net Arbitrator.
	doomcom.consoleplayer = 0;
	Printf ("Console player number: %d\n", doomcom.consoleplayer);

	doomcom.numnodes = 1;
	Printf ("Waiting for players...\n");

	atterm (SendAbort);

	ST_NetInit ("Waiting for players", numplayers);

	// Wait for numplayers-1 different connections
	if (!ST_NetLoop (Host_CheckForConnects, (void *)(intptr_t)numplayers))
	{
		exit (0);
	}

	// Now inform everyone of all machines involved in the game
	memset (gotack, 0, sizeof(gotack));
	Printf ("Sending all here\n");
	ST_NetInit ("Done waiting", 1);

	if (!ST_NetLoop (Host_SendAllHere, (void *)gotack))
	{
		exit (0);
	}

	popterm ();

	// Now go
	Printf ("Go\n");
	packet.fake = PRE_FAKE;
	packet.message = PRE_GO;
	for (node = 1; node < doomcom.numnodes; node++)
	{
		// If we send the packets eight times to each guest,
		// hopefully at least one of them will get through.
		for (int i = 8; i != 0; --i)
		{
			PreSend (&packet, 2, &sendaddress[node]);
		}
	}

	Printf ("Total players: %d\n", doomcom.numnodes);

	doomcom.id = DOOMCOM_ID;
	doomcom.numplayers = doomcom.numnodes;

	// On the host, each player's number is the same as its node number
	for (i = 0; i < doomcom.numnodes; ++i)
	{
		sendplayer[i] = i;
	}
}

// This routine is used by a guest to notify the host of its presence.
// Once that host acknowledges receipt of the notification, this routine
// is never called again.

bool Guest_ContactHost (void *userdata)
{
	sockaddr_in *from;
	PreGamePacket packet;

	// Let the host know we are here.
	packet.fake = PRE_FAKE;
	packet.message = PRE_CONNECT;
	PreSend (&packet, 2, &sendaddress[1]);

	// Listen for a reply.
	while ( (from = PreGet (&packet, sizeof(packet), true)) )
	{
		if (packet.fake == PRE_FAKE && packet.message == PRE_CONACK)
		{
			doomcom.consoleplayer = packet.consolenum;
			sendplayer[0] = packet.consolenum;
			Printf ("Console player number: %d\n", doomcom.consoleplayer);
			*(int *)userdata = packet.numnodes;		// Really # of players in the game.
			return true;
		}
	}

	// In case the progress bar could not be marqueed, bump it.
	ST_NetProgress (0);

	return false;
}

bool Guest_WaitForOthers (void *userdata)
{
	sockaddr_in *from;
	PreGamePacket packet;

	while ( (from = PreGet (&packet, sizeof(packet), false)) )
	{
		if (packet.fake != PRE_FAKE)
		{
			continue;
		}
		switch (packet.message)
		{
		case PRE_ALLHERE:
			if (doomcom.numnodes == 0)
			{
				int node;

				packet.numnodes = packet.numnodes;
				doomcom.numnodes = packet.numnodes + 2;
				for (node = 0; node < packet.numnodes; node++)
				{
					sendaddress[node+2].sin_addr.s_addr = packet.machines[node].address;
					sendaddress[node+2].sin_port = packet.machines[node].port;
					sendplayer[node+2] = packet.machines[node].player;

					// [JC] - fixes problem of games not starting due to
					// no address family being assigned to nodes stored in
					// sendaddress[] from the All Here packet.
					sendaddress[node+2].sin_family = AF_INET;
				}
			}
			ST_NetProgress (packet.numack);

			Printf ("Received All Here, sending ACK\n");
			packet.fake = PRE_FAKE;
			packet.message = PRE_ALLHEREACK;
			PreSend (&packet, 2, &sendaddress[1]);
			break;

		case PRE_GO:
			Printf ("Go\n");
			return true;

		case PRE_DISCONNECT:
			I_FatalError ("Host cancelled the game");
			break;
		}
	}

	return false;
}

void JoinGame (int i)
{
	int numplayers = 0;

	if ((i == Args.NumArgs() - 1) ||
		(Args.GetArg(i+1)[0] == '-') ||
		(Args.GetArg(i+1)[0] == '+'))
		I_FatalError ("You need to specify the host machine's address");

	StartNetwork (true);

	// Host is always node 1
	BuildAddress (&sendaddress[1], Args.GetArg(i+1));
	sendplayer[1] = 0;

	atterm (SendAbort);

	// Let host know we are here
	ST_NetInit ("Contacting host", 0);

	if (!ST_NetLoop (Guest_ContactHost, &numplayers))
	{
		exit (0);
	}

	// Wait for everyone else to connect
	Printf ("Total players: %d\n", numplayers);
	ST_NetInit ("Waiting for players", numplayers);

	// We know about ourself and the host; that's two players.
	ST_NetProgress (2);

	if (!ST_NetLoop (Guest_WaitForOthers, 0))
	{
		exit (0);
	}

	popterm ();

	Printf ("Total players: %d\n", doomcom.numnodes);

	doomcom.id = DOOMCOM_ID;
	doomcom.numplayers = doomcom.numnodes;
}

//
// I_InitNetwork
//
void I_InitNetwork (void)
{
	int i;
	char *v;

	memset (&doomcom, 0, sizeof(doomcom));

	// set up for network
	v = Args.CheckValue ("-dup");
	if (v)
	{
		doomcom.ticdup = clamp (atoi (v), 1, MAXTICDUP);
	}
	else
	{
		doomcom.ticdup = 1;
	}

	if (Args.CheckParm ("-extratic"))
		doomcom.extratics = 1;
	else
		doomcom.extratics = 0;

	v = Args.CheckValue ("-port");
	if (v)
	{
		DOOMPORT = atoi (v);
		Printf ("using alternate port %i\n", DOOMPORT);
	}

	// parse network game options,
	//		-net <consoleplayer> <host> <host> ...
	// -or-
	//		player 1: -host <numplayers>
	//		player x: -join <player 1's address>
	if ( (i = Args.CheckParm ("-net")) )
		WaitForPlayers (i);
	else if ( (i = Args.CheckParm ("-host")) )
		HostGame (i);
	else if ( (i = Args.CheckParm ("-join")) )
		JoinGame (i);
	else
	{
		// single player game
		netgame = false;
		multiplayer = false;
		doomcom.id = DOOMCOM_ID;
		doomcom.numplayers = doomcom.numnodes = 1;
		doomcom.consoleplayer = 0;
		return;
	}
}


void I_NetCmd (void)
{
	if (doomcom.command == CMD_SEND)
	{
		PacketSend ();
	}
	else if (doomcom.command == CMD_GET)
	{
		PacketGet ();
	}
	else
		I_Error ("Bad net cmd: %i\n",doomcom.command);
}

#ifdef __WIN32__
char *neterror (void)
{
	static char neterr[16];
	int			code;

	switch (code = WSAGetLastError ()) {
		case WSAEACCES:				return "EACCES";
		case WSAEADDRINUSE:			return "EADDRINUSE";
		case WSAEADDRNOTAVAIL:		return "EADDRNOTAVAIL";
		case WSAEAFNOSUPPORT:		return "EAFNOSUPPORT";
		case WSAEALREADY:			return "EALREADY";
		case WSAECONNABORTED:		return "ECONNABORTED";
		case WSAECONNREFUSED:		return "ECONNREFUSED";
		case WSAECONNRESET:			return "ECONNRESET";
		case WSAEDESTADDRREQ:		return "EDESTADDRREQ";
		case WSAEFAULT:				return "EFAULT";
		case WSAEHOSTDOWN:			return "EHOSTDOWN";
		case WSAEHOSTUNREACH:		return "EHOSTUNREACH";
		case WSAEINPROGRESS:		return "EINPROGRESS";
		case WSAEINTR:				return "EINTR";
		case WSAEINVAL:				return "EINVAL";
		case WSAEISCONN:			return "EISCONN";
		case WSAEMFILE:				return "EMFILE";
		case WSAEMSGSIZE:			return "EMSGSIZE";
		case WSAENETDOWN:			return "ENETDOWN";
		case WSAENETRESET:			return "ENETRESET";
		case WSAENETUNREACH:		return "ENETUNREACH";
		case WSAENOBUFS:			return "ENOBUFS";
		case WSAENOPROTOOPT:		return "ENOPROTOOPT";
		case WSAENOTCONN:			return "ENOTCONN";
		case WSAENOTSOCK:			return "ENOTSOCK";
		case WSAEOPNOTSUPP:			return "EOPNOTSUPP";
		case WSAEPFNOSUPPORT:		return "EPFNOSUPPORT";
		case WSAEPROCLIM:			return "EPROCLIM";
		case WSAEPROTONOSUPPORT:	return "EPROTONOSUPPORT";
		case WSAEPROTOTYPE:			return "EPROTOTYPE";
		case WSAESHUTDOWN:			return "ESHUTDOWN";
		case WSAESOCKTNOSUPPORT:	return "ESOCKTNOSUPPORT";
		case WSAETIMEDOUT:			return "ETIMEDOUT";
		case WSAEWOULDBLOCK:		return "EWOULDBLOCK";
		case WSAHOST_NOT_FOUND:		return "HOST_NOT_FOUND";
		case WSANOTINITIALISED:		return "NOTINITIALISED";
		case WSANO_DATA:			return "NO_DATA";
		case WSANO_RECOVERY:		return "NO_RECOVERY";
		case WSASYSNOTREADY:		return "SYSNOTREADY";
		case WSATRY_AGAIN:			return "TRY_AGAIN";
		case WSAVERNOTSUPPORTED:	return "VERNOTSUPPORTED";
		case WSAEDISCON:			return "EDISCON";

		default:
			sprintf (neterr, "%d", code);
			return neterr;
	}
}
#endif
