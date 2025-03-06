//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMERULES_H
#define GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "platform.h"
#include "irecipientfilter.h"

// Debug history should be disabled in release builds
//#define DISABLE_DEBUG_HISTORY	

//#include "items.h"
class ITakeDamageInfo;
class CCommand;
class CAmmoDef;
class CTacticalMissionManager;
class CViewVectors;
class KeyValues;
class CGameTrace;
typedef CGameTrace trace_t;
class IHandleEntity;
class IServerEntity;
class CBaseEntity;
class C_BaseEntity;
class CBaseCombatWeapon;
class C_BaseCombatWeapon;
class CBaseCombatCharacter;
class C_BaseCombatCharacter;
class CBasePlayer;
class C_BasePlayer;
class CItem;
class C_Item;

// Autoaiming modes
enum
{
	AUTOAIM_NONE = 0,		// No autoaim at all.
	AUTOAIM_ON,				// Autoaim is on.
	AUTOAIM_ON_CONSOLE,		// Autoaim is on, including enhanced features for Console gaming (more assistance, etc)
};

// weapon respawning return codes
enum
{	
	GR_NONE = 0,
	
	GR_WEAPON_RESPAWN_YES,
	GR_WEAPON_RESPAWN_NO,
	
	GR_AMMO_RESPAWN_YES,
	GR_AMMO_RESPAWN_NO,
	
	GR_ITEM_RESPAWN_YES,
	GR_ITEM_RESPAWN_NO,

	GR_PLR_DROP_GUN_ALL,
	GR_PLR_DROP_GUN_ACTIVE,
	GR_PLR_DROP_GUN_NO,

	GR_PLR_DROP_AMMO_ALL,
	GR_PLR_DROP_AMMO_ACTIVE,
	GR_PLR_DROP_AMMO_NO,
};

// Player relationship return codes
enum
{
	GR_NOTTEAMMATE = 0,
	GR_TEAMMATE,
	GR_ENEMY,
	GR_ALLY,
	GR_NEUTRAL,
};

#endif // GAMERULES_H
