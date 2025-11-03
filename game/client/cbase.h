//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CBASE_H
#define CBASE_H
#ifdef _WIN32
#pragma once
#endif

//struct IStudioHdr;
class C_BaseEntity;

#include <stdio.h>
#include <stdlib.h>

#include <tier0/platform.h>
#include <tier0/dbg.h>

#include <tier1/strtools.h>
#include <vstdlib/random.h>
#include <utlvector.h>
#include <const.h>
#include <icvar.h>

#include "string_t.h"

// These two have to be included very early
//#include <predictableid.h>
//#include <predictable_entity.h>
#include "ehandle.h"
typedef CHandle<C_BaseEntity> EHANDLE;
#include "recvproxy.h"
#include "engine/IEngineTrace.h"
#include "entitylist_base.h"
#include "gamerules.h"
// This is a precompiled header.  Include a bunch of common stuff.
// This is kind of ugly in that it adds a bunch of dependency where it isn't needed.
// But on balance, the compile time is much lower (even incrementally) once the precompiled
// headers contain these headers.
#include "precache_register.h"
//#include "c_basecombatweapon.h"
//#include "c_basecombatcharacter.h"
//#include "shared_classnames.h"
#include "baseentity_shared.h"
#include "c_baseplayer.h"
//#include "cliententitylist.h"
#include "itempents.h"
#include "vphysics_interface.h"
//#include "physics.h"
#include "c_recipientfilter.h"
//#include "cdll_client_int.h"
#include "worldsize.h"
#include "engine/ivmodelinfo.h"
#include "gamemovement.h"
#include "portal_util_shared.h"
#include <util_shared.h>
#include "cdll_util.h"

extern IClientWorld* g_pGameRules;
inline IClientWorld* GameRules() {
	return g_pGameRules;
}

abstract_class C_BaseEntityClassList
{
public:
	C_BaseEntityClassList();
	~C_BaseEntityClassList();
	virtual void LevelShutdown() = 0;

	C_BaseEntityClassList* m_pNextClassList;
};

template< class T >
class C_EntityClassList : public C_BaseEntityClassList
{
public:
	virtual void LevelShutdown() { m_pClassList = NULL; }

	void Insert(T* pEntity)
	{
		pEntity->m_pNext = m_pClassList;
		m_pClassList = pEntity;
	}

	void Remove(T* pEntity)
	{
		T** pPrev = &m_pClassList;
		T* pCur = *pPrev;
		while (pCur)
		{
			if (pCur == pEntity)
			{
				*pPrev = pCur->m_pNext;
				return;
			}
			pPrev = &pCur->m_pNext;
			pCur = *pPrev;
		}
	}

	static T* m_pClassList;
};

// Maximum size of entity list
#define INVALID_CLIENTENTITY_HANDLE CBaseHandle(NULL)

//-----------------------------------------------------------------------------
// Converts an IHandleEntity to an CBaseEntity
//-----------------------------------------------------------------------------
inline const CBaseEntity* EntityFromEntityHandle(const IHandleEntity* pConstHandleEntity)
{
	IHandleEntity* pHandleEntity = const_cast<IHandleEntity*>(pConstHandleEntity);

#ifdef CLIENT_DLL
	IClientUnknown* pUnk = (IClientUnknown*)pHandleEntity;
	return (CBaseEntity*)pUnk->GetBaseEntity();
#else
	if (staticpropmgr->IsStaticProp(pHandleEntity))
		return NULL;

	return (CBaseEntity*)pHandleEntity;
#endif
}

inline CBaseEntity* EntityFromEntityHandle(IHandleEntity* pHandleEntity)
{
#ifdef CLIENT_DLL
	IClientUnknown* pUnk = (IClientUnknown*)pHandleEntity;
	return (CBaseEntity*)pUnk->GetBaseEntity();
#else
	if (staticpropmgr->IsStaticProp(pHandleEntity))
		return NULL;

	return (CBaseEntity*)pHandleEntity;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline void DevMsgRT(PRINTF_FORMAT_STRING char const* pMsg, ...)
{
	if (gpGlobals->frametime != 0.0f)
	{
		va_list argptr;
		va_start(argptr, pMsg);
		// 
		{
			static char	string[1024];
			Q_vsnprintf(string, sizeof(string), pMsg, argptr);
			DevMsg(1, "%s", string);
		}
		// DevMsg( pMsg, argptr );
		va_end(argptr);
	}
}

#endif // CBASE_H
