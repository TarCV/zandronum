//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004 Brad Carney
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
// Filename: sv_ban.h
//
// Description: Contains variables and routines related to the server ban
// of the program.
//
//-----------------------------------------------------------------------------

#ifndef __SV_BAN_H__
#define __SV_BAN_H__

#include "sv_main.h"

//*****************************************************************************
//	DEFINES

#define	MAX_SERVER_BANS			64
#define	BANFILE_REPARSE_TIME	( TICRATE * 60 * 10 )

//*****************************************************************************
//	STRUCTURES

typedef struct
{
	// The IP address that is banned in char form. Can be a number, or a wildcards.
	char		szBannedIP[4][4];

	// Comment regarding the banned address.
	char		szComment[128];

} BAN_t;

//*****************************************************************************
//	PROTOTYPES

void	SERVERBAN_Tick( void );
bool	SERVERBAN_IsIPBanned( char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 );
ULONG	SERVERBAN_DoesBanExist( char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 );
void	SERVERBAN_AddBan( char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3, char *pszPlayerName, char *pszComment );
bool	SERVERBAN_StringToBan( char *pszAddress, char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 );
void	SERVERBAN_ClearBans( void );
//LONG	SERVERBAN_GetNumBans( void );
BAN_t	SERVERBAN_GetBan( ULONG ulIdx );

//*****************************************************************************
//	EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Bool, sv_enforcebans );
EXTERN_CVAR( String, sv_banfile );

#endif	// __SV_BAN_H__
