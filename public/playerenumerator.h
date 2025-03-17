//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PLAYERENUMERATOR_H
#define PLAYERENUMERATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "basehandle.h"
#include "ispatialpartition.h"

class CPlayerEnumerator : public IPartitionEnumerator
{
	typedef CPlayerEnumerator ThisClass;;
public:
	//Forced constructor
	CPlayerEnumerator(IEntityList* pEntityList, float radius, Vector vecOrigin )
	{
		m_pEntityList = pEntityList;
		m_flRadiusSquared = radius * radius;
		m_vecOrigin = vecOrigin;
		m_Objects.RemoveAll();
	}

	int	GetObjectCount() { return m_Objects.Size(); }

	IHandleEntity *GetObject( int index )
	{
		if ( index < 0 || index >= GetObjectCount() )
			return NULL;

		return m_pEntityList->GetBaseEntityFromHandle(m_Objects[ index ]);
	}

	//Actual work code
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		IHandleEntity* pEnt = pHandleEntity;//EntityList()->GetBaseEntityFromHandle(pHandleEntity->GetRefEHandle());
		if ( pEnt == NULL )
			return ITERATION_CONTINUE;

		if ( !pEnt->IsPlayer() )
			return ITERATION_CONTINUE;

		Vector	deltaPos = pEnt->GetEngineObject()->GetAbsOrigin() - m_vecOrigin;
		if ( deltaPos.LengthSqr() > m_flRadiusSquared )
			return ITERATION_CONTINUE;

		CBaseHandle h;
		h = pEnt;
		m_Objects.AddToTail( h );

		return ITERATION_CONTINUE;
	}

public:
	//Data members
	float	m_flRadiusSquared;
	Vector m_vecOrigin;
	IEntityList* m_pEntityList = NULL;
	CUtlVector<CBaseHandle> m_Objects;
};

#endif // PLAYERENUm_ObjectsMERATOR_H