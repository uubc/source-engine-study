//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Physics simulation for non-havok/ipion objects
//
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"


#include "player.h"
#include "ai_basenpc.h"
#include "mempool.h"
#include "engine/IEngineSound.h"
#include "datacache/imdlcache.h"
#include "ispatialpartition.h"
#include "tier0/vprof.h"
#include "movevars_shared.h"
#include "hierarchy.h"
#include "trains.h"
#include "vphysicsupdateai.h"
#include "tier0/vcrmode.h"
#include "pushentity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
//
// CBaseEntity methods
//
//-----------------------------------------------------------------------------

void CBaseEntity::PerformCustomPhysics( Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity )
{
	// If you're going to use custom physics, you need to implement this!
	Assert(0);
}

void CBaseEntity::StartGroundContact(IServerEntity* ground)
{
	GetEngineObject()->AddFlag(FL_ONGROUND);
	//	Msg( "+++ %s starting contact with ground %s\n", GetClassname(), ground->GetClassname() );
}

void CBaseEntity::EndGroundContact(IServerEntity* ground)
{
	GetEngineObject()->RemoveFlag(FL_ONGROUND);
	//	Msg( "--- %s ending contact with ground %s\n", GetClassname(), ground->GetClassname() );
}
