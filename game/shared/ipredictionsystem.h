//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IPREDICTIONSYSTEM_H
#define IPREDICTIONSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

//#include "predictable_entity.h"
#include "cbase.h"

class IHandleEntity;

class CDisablePredictionFiltering
{
public:
	CDisablePredictionFiltering( bool disable = true )
	{
		m_bDisabled = disable;
		if ( m_bDisabled )
		{
			EntityList()->PushDisableSuppress();
		}
	}

	~CDisablePredictionFiltering( void )
	{
		if ( m_bDisabled )
		{
			EntityList()->PopDisableSuppress();
		}
	}
private:
	bool	m_bDisabled;
};

#endif // IPREDICTIONSYSTEM_H
