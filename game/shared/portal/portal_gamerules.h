//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Portal.
//
//=============================================================================//

#ifdef PORTAL_MP



#include "portal_mp_gamerules.h" //redirect to multiplayer gamerules in multiplayer builds



#else

#ifndef PORTAL_GAMERULES_H
#define PORTAL_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "gamerules.h"
#include "hl2_gamerules.h"

#ifdef CLIENT_DLL
	#define CPortalGameWorld C_PortalGameWorld
	namespace vgui
	{
		typedef unsigned long HScheme;
	}
#endif

#if defined ( CLIENT_DLL )
#include "steam/steam_api.h"
#endif

class CPortalGameWorld : public CHalfLife2World
{
public:
	DECLARE_CLASS(CPortalGameWorld, CHalfLife2World);
	
	virtual bool	ShouldCollide( int collisionGroup0, int collisionGroup1 );
	virtual bool	ShouldUseRobustRadiusDamage(CBaseEntity *pEntity);
#ifndef CLIENT_DLL
	virtual bool	ShouldAutoAim( CBasePlayer *pPlayer, CBaseEntity *target );
	virtual float	GetAutoAimScale( CBasePlayer *pPlayer );
#endif

#ifdef CLIENT_DLL
	virtual bool IsBonusChallengeTimeBased( void );
#endif

private:
	// Rules change for the mega physgun
	CNetworkVar( bool, m_bMegaPhysgun );

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS(); // This makes datatables able to access our private vars.

	CPortalGameWorld();

	virtual void	Init();
	virtual void	LevelInit();


	int GetKillCamMode() const { return m_nKillCamMode; }
	int GetKillCamTarget1() const { return m_nKillCamTarget1; }

	int m_nKillCamMode = OBS_MODE_NONE;
	int m_nKillCamTarget1 = 0;
	int m_nKillCamTarget2 = 0;
#else
public:

	DECLARE_SERVERCLASS(); // This makes datatables able to access our private vars.

	CPortalGameWorld();
	virtual ~CPortalGameWorld() {}

	virtual void			Precache(void);
	virtual void			LevelInit();
	virtual void			LevelShutdown();

	virtual void			ClientActive(int pEdict, bool bLoadGame);
	virtual void			ClientPutInServer(int pEdict, const char* playername);

	virtual void			Think( void );

	virtual bool			ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void			RespawnPlayer(CBaseEntity* pEdict, bool fCopyCorpse);
	virtual void			AfterPlayerSpawn( CBasePlayer *pPlayer );

	virtual void			InitDefaultAIRelationships( void );
	virtual const char*		AIClassText(int classType);
	virtual const char *GetGameDescription( void ) { return "Portal"; }
	virtual void			StartGameFrame(void);

	// Ammo
	virtual void			PlayerThink( CBasePlayer *pPlayer );
	virtual float			GetAmmoDamage( IHandleEntity *pAttacker, IHandleEntity *pVictim, int nAmmoType );

	virtual bool			ShouldBurningPropsEmitLight();

	bool ShouldRemoveRadio( void );
	
public:

	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );

	bool	MegaPhyscannonActive( void ) { return m_bMegaPhysgun;	}

private:

	int						DefaultFOV( void ) { return 75; }
#endif
};


//-----------------------------------------------------------------------------
// Gets us at the Half-Life 2 game rules
//-----------------------------------------------------------------------------
inline CPortalGameWorld* PortalGameRules()
{
	return (CPortalGameWorld*)EntityList()->GetBaseEntity(0);
}

#ifdef CLIENT_DLL
#define SCREEN_FILE		"scripts/vgui_screens.txt"
#endif // CLIENT_DLL


#endif // PORTAL_GAMERULES_H
#endif
