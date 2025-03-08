//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"  
#include "weapon_dodfullauto.h"

#if defined( CLIENT_DLL )
	#define CDODFullAutoPunchWeapon C_DODFullAutoPunchWeapon
#endif

class CDODFullAutoPunchWeapon : public CDODFullAutoWeapon
{
public:
	DECLARE_CLASS( CDODFullAutoPunchWeapon, CDODFullAutoWeapon );
	DECLARE_NETWORKCLASS(); 
#ifdef CLIENT_DLL
	DECLARE_PREDICTABLE();
#endif // CLIENT_DLL

	CDODFullAutoPunchWeapon() {}

	virtual void Spawn( void );
	virtual void SecondaryAttack( void );
	virtual bool Reload( void );
	virtual void ItemBusyFrame( void );
	
	virtual const char *GetSecondaryDeathNoticeName( void ) { return "punch"; }

	virtual DODWeaponID GetWeaponID( void ) const { return WEAPON_NONE; }

private:
	CDODFullAutoPunchWeapon( const CDODFullAutoPunchWeapon & );
};