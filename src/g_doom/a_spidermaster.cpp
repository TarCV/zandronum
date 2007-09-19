#include "templates.h"
#include "actor.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
// [BC] New #includes.
#include "network.h"
#include "sv_commands.h"

static FRandom pr_spidrefire ("SpidRefire");

void A_SpidRefire (AActor *self)
{
	// keep firing unless target got out of sight
	A_FaceTarget (self);

	// [BC] Client spider masterminds continue to fire until told by the server to stop.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	if (pr_spidrefire() < 10)
		return;

	if (!self->target
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, 0) )
	{
		self->SetState (self->SeeState);

		// [BC] If we're the server, tell clients to update this thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( self, STATE_SEE );
	}
}

void A_Metal (AActor *self)
{
	S_Sound (self, CHAN_BODY, "spider/walk", 1, ATTN_IDLE);
	A_Chase (self);
}
