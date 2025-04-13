//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_baseentity.h"
#ifdef WIN32
#include <typeinfo>
#endif
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pNewPosition - 
//			*pNewVelocity - 
//			*pNewAngles - 
//			*pNewAngVelocity - 
//-----------------------------------------------------------------------------
void C_BaseEntity::PerformCustomPhysics( Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity )
{
	// If you're going to use custom physics, you need to implement this!
	Assert(0);
}

//-----------------------------------------------------------------------------
// Purpose: Not yet supported on client .dll
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void C_BaseEntity::StartTouch( IClientEntity *pOther )
{
	// notify parent
//	if ( m_pParent != NULL )
//		m_pParent->StartTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Call touch function if one is set
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void C_BaseEntity::Touch( IClientEntity *pOther )
{ 
	if ( m_pfnTouch ) 
		(this->*m_pfnTouch)( pOther );

	// notify parent of touch
//	if ( m_pParent != NULL )
//		m_pParent->Touch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Call end touch
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void C_BaseEntity::EndTouch( IClientEntity *pOther )
{
	// notify parent
//	if ( m_pParent != NULL )
//	{
//		m_pParent->EndTouch( pOther );
//	}
}

void C_BaseEntity::StartGroundContact(IClientEntity* ground)
{
	GetEngineObject()->AddFlag(FL_ONGROUND);
	//	Msg( "+++ %s starting contact with ground %s\n", GetClassname(), ground->GetClassname() );
}

void C_BaseEntity::EndGroundContact(IClientEntity* ground)
{
	GetEngineObject()->RemoveFlag(FL_ONGROUND);
	//	Msg( "--- %s ending contact with ground %s\n", GetClassname(), ground->GetClassname() );
}



