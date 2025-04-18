//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
//  hud_msg.cpp
//
#include "cbase.h"
#include "clientmode.h"
#include "hudelement.h"
#include "KeyValues.h"
#include "vgui_controls/AnimationController.h"
#include "engine/IEngineSound.h"
#include <bitbuf.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/// USER-DEFINED SERVER MESSAGE HANDLERS

void CHud::MsgFunc_ResetHUD( bf_read &msg )
{
	ResetHUD();
}

void CHud::ResetHUD()
{
	// clear all hud data
	g_pGameRules->GetViewportAnimationController()->CancelAllAnimations();

	for ( int i = 0; i < m_HudList.Size(); i++ )
	{
		m_HudList[i]->Reset();
	}

	g_pGameRules->GetViewportAnimationController()->RunAllAnimationsToCompletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CHud::MsgFunc_SendAudio( bf_read &msg )
{
	char szString[2048];
	msg.ReadString( szString, sizeof(szString) );
	
	CLocalPlayerFilter filter;
	g_pSoundEmitterSystem->EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, szString );//C_BaseEntity::
}
