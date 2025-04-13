//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "engine/IEngineSound.h"
#include "mempool.h"
#include "movevars_shared.h"
#include "utlrbtree.h"
#include "tier0/vprof.h"
#include "entitydatainstantiator.h"
#include "positionwatcher.h"
#include "movetype_push.h"
#include "vphysicsupdateai.h"
#include "igamesystem.h"
#include "utlmultilist.h"
#include "tier1/callqueue.h"

#ifdef PORTAL
	#include "portal_util_shared.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//void WatchPositionChanges( CBaseEntity *pWatcher, CBaseEntity *pMovingEntity )
//{
//	pMovingEntity->AddWatcherToEntity( pWatcher, POSITIONWATCHER );
//}

//void RemovePositionWatcher( CBaseEntity *pWatcher, CBaseEntity *pMovingEntity )
//{
//	pMovingEntity->RemoveWatcherFromEntity( pWatcher, POSITIONWATCHER );
//}

//void ReportPositionChanged( CBaseEntity *pMovedEntity )
//{
//	if (pMovedEntity)
//	{
//		pMovedEntity->NotifyPositionChanged();
//	}
//}

//void WatchVPhysicsStateChanges( CBaseEntity *pWatcher, CBaseEntity *pPhysicsEntity )
//{
//	pPhysicsEntity->AddWatcherToEntity( pWatcher, VPHYSICSWATCHER );
//}

//void RemoveVPhysicsStateWatcher( CBaseEntity *pWatcher, CBaseEntity *pPhysicsEntity )
//{
//	pPhysicsEntity->RemoveWatcherFromEntity( pWatcher, VPHYSICSWATCHER );
//}

//void ReportVPhysicsStateChanged( IPhysicsObject *pPhysics, CBaseEntity *pEntity, bool bAwake )
//{
//	if (pEntity)
//	{
//		pEntity->NotifyVPhysicsStateChanged( pPhysics, bAwake );
//	}
//}



//-----------------------------------------------------------------------------
// For debugging
//-----------------------------------------------------------------------------

//#ifdef GAME_DLL
//
//void SpewLinks()
//{
//	int nCount = 0;
//	for ( IServerEntity *pClass = EntityList()->FirstEnt(); pClass != NULL; pClass = EntityList()->NextEnt(pClass) )
//	{
//		if ( pClass /*&& !pClass->IsDormant()*/ )
//		{
//			servertouchlink_t *root = (servertouchlink_t* )pClass->GetEngineObject()->GetDataObject( TOUCHLINK );
//			if ( root )
//			{
//
//				// check if the edict is already in the list
//				for ( servertouchlink_t *link = root->nextLink; link != root; link = link->nextLink )
//				{
//					++nCount;
//					Msg("[%d] (%d) Link %d (%s) -> %d (%s)\n", nCount, pClass->IsDormant(),
//						pClass->entindex(), pClass->GetClassname(),
//						EntityList()->GetBaseEntityFromHandle(link->entityTouched)->entindex(), EntityList()->GetBaseEntityFromHandle(link->entityTouched)->GetClassname() );
//				}
//			}
//		}
//	}
//}
//
//#endif

//-----------------------------------------------------------------------------
// Purpose: Returns the mask of what is solid for the given entity
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseEntity::PhysicsSolidMaskForEntity( void ) const
{
	return MASK_SOLID;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseEntity::ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity )
{
	// Stop if on ground.
	if ( trace.plane.normal.z > 0.7 )			// Floor
	{
		// Get the total velocity (player + conveyors, etc.)
		VectorAdd(GetEngineObject()->GetAbsVelocity(), GetEngineObject()->GetBaseVelocity(), vecVelocity );

		// Verify that we have an entity.
		CBaseEntity *pEntity = (CBaseEntity*)trace.m_pEnt;
		Assert( pEntity );

		// Are we on the ground?
		if ( vecVelocity.z < (this->GetEngineObject()->GetActualGravity() * gpGlobals->frametime ) )
		{
			Vector vecAbsVelocity = GetEngineObject()->GetAbsVelocity();
			vecAbsVelocity.z = 0.0f;
			GetEngineObject()->SetAbsVelocity( vecAbsVelocity );
		}

		if ( pEntity->IsStandable() )
		{
			GetEngineObject()->SetGroundEntity( pEntity->GetEngineObject() );
		}
	}
}

// Remove this as ground entity for all object resting on this object
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
//void CBaseEntity::WakeRestingObjects()
//{
//	// Unset this as ground entity for everything resting on this object
//	//  This calls endgroundcontact for everything on the list
//	GetEngineObject()->PhysicsRemoveGroundList();
//}


