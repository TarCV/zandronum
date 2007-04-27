//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003-2006 Brad Carney
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
// Date created:  1/14/06
//
//
// Filename: survival.h
//
// Description: 
//
//-----------------------------------------------------------------------------

#ifndef __SURVIVAL_H__
#define __SURVIVAL_H__

#include "c_cvars.h"

//*****************************************************************************
//	DEFINES

typedef enum
{
	SURVS_WAITINGFORPLAYERS,
	SURVS_COUNTDOWN,
	SURVS_INPROGRESS,
	SURVS_MISSIONFAILED,
	SURVS_NEWMAP,

} SURVIVALSTATE_e;

//*****************************************************************************
//	PROTOTYPES

void	SURVIVAL_Construct( void );
void	SURVIVAL_Tick( void );

ULONG	SURVIVAL_CountActivePlayers( bool bLiving );
void	SURVIVAL_StartCountdown( ULONG ulTicks );
void	SURVIVAL_DoFight( void );
//void	SURVIVAL_DoMissionFailed( void );

// Access functions
ULONG	SURVIVAL_GetCountdownTicks( void );
void	SURVIVAL_SetCountdownTicks( ULONG ulTicks );

SURVIVALSTATE_e	SURVIVAL_GetState( void );
void			SURVIVAL_SetState( SURVIVALSTATE_e State );

//*****************************************************************************
//  EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Int, sv_survivalcountdowntime )

#endif	// __SURVIVAL_H__
