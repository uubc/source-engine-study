//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <KeyValues.h>
#include "portal_weapon_parse.h"
#include "ammodef.h"

#ifdef PORTAL
FileWeaponInfo_t* CreateWeaponInfo()
{
	return new CPortalSWeaponInfo;
}
#endif // PORTAL

CPortalSWeaponInfo::CPortalSWeaponInfo()
{
	m_iPlayerDamage = 0;
}


void CPortalSWeaponInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

	m_iPlayerDamage = pKeyValuesData->GetInt( "damage", 0 );
}


