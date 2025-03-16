//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( CLIENTMODE_NORMAL_H )
#define CLIENTMODE_NORMAL_H
#ifdef _WIN32
#pragma once
#endif

#include "iclientmode.h"
#include "GameEventListener.h"
#include <baseviewport.h>

class CBaseHudChat;
class CBaseHudWeaponSelection;
class CViewSetup;
class C_BaseEntity;
class C_BasePlayer;

namespace vgui
{
class Panel;
}

//=============================================================================
// HPE_BEGIN:
// [tj] Moved this from the .cpp file so derived classes could access it
//=============================================================================
 
 
//=============================================================================
// HPE_END
//=============================================================================

class CReplayReminderPanel;



#endif // CLIENTMODE_NORMAL_H

