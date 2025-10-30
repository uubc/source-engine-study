//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef HL1MP_GAMERULES_H
#define HL1MP_GAMERULES_H
#pragma once


#include "gamerules.h"
#include "teamplay_gamerules.h"

extern ConVar sk_mp_dmg_multiplier ;

#ifdef CLIENT_DLL
	#define CHL1MPWorld C_HL1MPWorld
#endif

class CHL1MPWorld : public CTeamplayWorld
{
public:
	DECLARE_CLASS( CHL1MPWorld, CTeamplayWorld );

#ifdef CLIENT_DLL
	DECLARE_CLIENTCLASS();
#else
	DECLARE_SERVERCLASS();
#endif

	CHL1MPWorld();
	virtual ~CHL1MPWorld();

#ifdef CLIENT_DLL

	virtual float	GetViewModelFOV(void);

	virtual int		GetDeathMessageStartHeight(void);

	int GetKillCamMode() const { return OBS_MODE_NONE; }
	int GetKillCamTarget1() const { return 0; }
#endif // CLIENT_DLL

#ifdef GAME_DLL
	virtual void	Precache(void);
	virtual void	LevelInit();
	virtual void	LevelShutdown();

	virtual void	ClientActive(int pEdict, bool bLoadGame);
	virtual void	ClientPutInServer(int pEdict, const char* playername);
	
#endif // GAME_DLL

	virtual void CreateStandardEntities( void );

	virtual bool IsTeamplay( void )
	{
		return m_bTeamPlayEnabled;
	}

	virtual float GetAmmoDamage( IHandleEntity *pAttacker, IHandleEntity *pVictim, int nAmmoType );
    virtual float GetDamageMultiplier( void );    

	virtual bool IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer )
	{ 
		return true; 
	}
	bool ClientCommand( CBaseEntity *pEdict, const CCommand &args, int nClientIndex);

#ifdef CLIENT_DLL
#else
	virtual const char *GetGameDescription( void ) { return "Half-Life Deathmatch: Source"; }  // this is the game name that gets seen in the server browser

	virtual void StartGameFrame(void);

	virtual void Think ( void );

	virtual void GoToIntermission( void );

	virtual void InitDefaultAIRelationships( void );

	virtual float FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon );
	virtual float FlItemRespawnTime( CItem *pItem );

	virtual const char *SetDefaultPlayerTeam( CBasePlayer *pPlayer );
	virtual void InitHUD( CBasePlayer *pPlayer );
	virtual void ChangePlayerTeam( CBasePlayer *pPlayer, const char *pTeamName, bool bKill, bool bGib );
	virtual void ClientSettingsChanged( CBasePlayer *pPlayer );
	virtual void RespawnPlayer(CBaseEntity* pEdict, bool fCopyCorpse);
	virtual int GetTeamIndex( const char * pName );
#endif

private:

	const char *TeamWithFewestPlayers( void );

	CNetworkVar( bool, m_bTeamPlayEnabled );
};


inline CHL1MPWorld* HL1MPRules()
{
	return (CHL1MPWorld*)EntityList()->GetBaseEntity(0);
}


#endif
