//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Deals with precaching requests from client effects
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//
//#include "cbase.h"
//#include "fx.h"
#include "clienteffectprecachesystem.h"
#include "particles/particles.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CClientEffect* s_pClientEffectHead = NULL;

CClientEffect::CClientEffect(void)
{
	if (s_pClientEffectHead) 
	{
		m_pNextClientEffect = s_pClientEffectHead;

	}
	s_pClientEffectHead = this;
}