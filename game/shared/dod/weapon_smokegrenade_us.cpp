//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasegrenade.h"

#ifdef CLIENT_DLL
	#define CWeaponSmokeGrenadeUS C_WeaponSmokeGrenadeUS
#else
	#include "dod_smokegrenade_us.h"	//the thing that we throw
#endif

class CWeaponSmokeGrenadeUS : public CWeaponDODBaseGrenade
{
public:
	DECLARE_CLASS( CWeaponSmokeGrenadeUS, CWeaponDODBaseGrenade );
	DECLARE_NETWORKCLASS();
#ifdef CLIENT_DLL
	DECLARE_PREDICTABLE();
#endif // CLIENT_DLL

	CWeaponSmokeGrenadeUS() {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_SMOKE_US; }

#ifndef CLIENT_DLL

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime = GRENADE_FUSE_LENGTH )
	{
        CDODSmokeGrenadeUS::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer );
	}

#endif

	virtual float GetDetonateTimerLength( void ) { return 10; }

private:
	CWeaponSmokeGrenadeUS( const CWeaponSmokeGrenadeUS & ) {}
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSmokeGrenadeUS, DT_WeaponSmokeGrenadeUS )

BEGIN_NETWORK_TABLE(CWeaponSmokeGrenadeUS, DT_WeaponSmokeGrenadeUS)
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponSmokeGrenadeUS )
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_smoke_us, CWeaponSmokeGrenadeUS );
PRECACHE_WEAPON_REGISTER( weapon_smoke_us );