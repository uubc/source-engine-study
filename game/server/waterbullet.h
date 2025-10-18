//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that simulates bullets that are underwater.
//
//=============================================================================//

#ifndef WEAPON_WATERBULLET_H
#define WEAPON_WATERBULLET_H
#ifdef _WIN32
#pragma once
#endif

#define WATER_BULLET_BUBBLES_PER_INCH 0.05f

//=========================================================
//=========================================================
class CWaterBullet : public CBaseEntity
{
	DECLARE_CLASS( CWaterBullet, CBaseEntity);

public:
	bool IsBaseAnimating() { return true; }
	void Precache();
	void Spawn( const Vector &vecOrigin, const Vector &vecDir );
	void Touch( IServerEntity *pOther );
	void BulletThink();

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
};

#endif // WEAPON_WATERBULLET_H
