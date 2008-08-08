#ifndef __A_HEXENGLOBAL_H__
#define __A_HEXENGLOBAL_H__

#include "d_player.h"

class AHeresiarch : public AActor
{
	DECLARE_ACTOR (AHeresiarch, AActor)
public:
	const PClass *StopBall;

	void Serialize (FArchive &arc);
	void Die (AActor *source, AActor *inflictor);
};

class AHolySpirit : public AActor
{
	DECLARE_CLASS (AHolySpirit, AActor)
public:
	bool Slam (AActor *thing);
	bool SpecialBlastHandling (AActor *source, fixed_t strength);
	bool IsOkayToAttack (AActor *link);
};

class AFighterWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AFighterWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

class AClericWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AClericWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

class AMageWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AMageWeapon, AWeapon);
public:
	bool TryPickup (AActor *toucher);
};

#endif //__A_HEXENGLOBAL_H__
