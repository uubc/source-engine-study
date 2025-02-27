//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ENGINE_IENGINETRACE_H
#define ENGINE_IENGINETRACE_H
#ifdef _WIN32
#pragma once
#endif

#include "basehandle.h"
#include "utlvector.h" //need CUtlVector for IEngineTrace::GetBrushesIn*()
#include "mathlib/vector4d.h"
#include "model_types.h"
#include "ihandleentity.h"

class Vector;
struct Ray_t;
class CGameTrace;
typedef CGameTrace trace_t;
class ICollideable;
class QAngle;
class CTraceListData;
class CPhysCollide;
struct cplane_t;

//-----------------------------------------------------------------------------
// Classes are expected to inherit these + implement the ShouldHitEntity method
//-----------------------------------------------------------------------------

// This is the one most normal traces will inherit from
class CTraceFilter : public ITraceFilter
{
public:
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_EVERYTHING;
	}
};

//-----------------------------------------------------------------------------
// A standard filter to be applied to just about everything.
//-----------------------------------------------------------------------------
inline bool StandardFilterRules(IHandleEntity* pHandleEntity, int fContentsMask)
{
	// Static prop case...
	if (pHandleEntity->IsStaticProp()) {
		return true;
	}

	SolidType_t solid = pHandleEntity->GetEngineObject()->GetSolid();

	if ((pHandleEntity->GetModelType() != mod_brush) || (solid != SOLID_BSP && solid != SOLID_VPHYSICS))
	{
		if ((fContentsMask & CONTENTS_MONSTER) == 0)
			return false;
	}

	// This code is used to cull out tests against see-thru entities
	if (!(fContentsMask & CONTENTS_WINDOW) && pHandleEntity->IsTransparent())
		return false;

	// FIXME: this is to skip BSP models that are entities that can be 
	// potentially moved/deleted, similar to a monster but doors don't seem to 
	// be flagged as monsters
	// FIXME: the FL_WORLDBRUSH looked promising, but it needs to be set on 
	// everything that's actually a worldbrush and it currently isn't
	if (!(fContentsMask & CONTENTS_MOVEABLE) && (pHandleEntity->GetEngineObject()->GetMoveType() == MOVETYPE_PUSH))// !(touch->flags & FL_WORLDBRUSH) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//
// Shared client/server trace filter code
//
//-----------------------------------------------------------------------------
inline bool PassServerEntityFilter(const IHandleEntity* pTouch, const IHandleEntity* pPass)
{
	if (!pPass)
		return true;

	if (pTouch == pPass)
		return false;

	if (pTouch->IsStaticProp() || pPass->IsStaticProp())
		return true;

	// don't clip against own missiles
	if (pTouch->GetEngineObject()->GetOwnerEntity() == pPass->GetEngineObject())
		return false;

	// don't clip against owner
	if (pPass->GetEngineObject()->GetOwnerEntity() == pTouch->GetEngineObject())
		return false;


	return true;
}

typedef bool (*ShouldHitFunc_t)(IHandleEntity* pHandleEntity, int contentsMask);

//-----------------------------------------------------------------------------
// traceline methods
//-----------------------------------------------------------------------------
class CTraceFilterSimple : public CTraceFilter
{
public:
	// It does have a base, but we'll never network anything below here..
	typedef CTraceFilterSimple ThisClass;

	//-----------------------------------------------------------------------------
// Simple trace filter
//-----------------------------------------------------------------------------
	CTraceFilterSimple(const IHandleEntity* passedict, int collisionGroup,
		ShouldHitFunc_t pExtraShouldHitFunc = NULL)
	{
		m_pPassEnt = passedict;
		m_collisionGroup = collisionGroup;
		m_pExtraShouldHitCheckFunction = pExtraShouldHitFunc;
	}
	//-----------------------------------------------------------------------------
// The trace filter!
//-----------------------------------------------------------------------------
	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		if (!StandardFilterRules(pHandleEntity, contentsMask))
			return false;

		if (m_pPassEnt)
		{
			if (!PassServerEntityFilter(pHandleEntity, m_pPassEnt))
			{
				return false;
			}
		}

		// Don't test if the game code tells us we should ignore this collision...
		if (pHandleEntity->IsStaticProp())
			return false;
		if (!pHandleEntity->ShouldCollide(m_collisionGroup, contentsMask))
			return false;
		if (!pHandleEntity->GetEntityList()->GetGameRules()->ShouldCollide(m_collisionGroup, pHandleEntity->GetEngineObject()->GetCollisionGroup()))
			return false;
		if (m_pExtraShouldHitCheckFunction &&
			(!(m_pExtraShouldHitCheckFunction(pHandleEntity, contentsMask))))
			return false;

		return true;
	}
	virtual void SetPassEntity(const IHandleEntity* pPassEntity) { m_pPassEnt = pPassEntity; }
	virtual void SetCollisionGroup(int iCollisionGroup) { m_collisionGroup = iCollisionGroup; }

	const IHandleEntity* GetPassEntity(void) { return m_pPassEnt; }

private:
	const IHandleEntity* m_pPassEnt;
	int m_collisionGroup;
	ShouldHitFunc_t m_pExtraShouldHitCheckFunction;

};

class CTraceFilterSkipTwoEntities : public CTraceFilterSimple
{
public:
	// It does have a base, but we'll never network anything below here..
	typedef CTraceFilterSimple BaseClass;
	typedef CTraceFilterSkipTwoEntities ThisClass;;

	//-----------------------------------------------------------------------------
// Trace filter that skips two entities
//-----------------------------------------------------------------------------
	CTraceFilterSkipTwoEntities(const IHandleEntity* passentity, const IHandleEntity* passentity2, int collisionGroup) :
		BaseClass(passentity, collisionGroup), m_pPassEnt2(passentity2)
	{
	}
	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		Assert(pHandleEntity);
		if (!PassServerEntityFilter(pHandleEntity, m_pPassEnt2))
			return false;

		return BaseClass::ShouldHitEntity(pHandleEntity, contentsMask);
	}
	virtual void SetPassEntity2(const IHandleEntity* pPassEntity2) { m_pPassEnt2 = pPassEntity2; }

private:
	const IHandleEntity* m_pPassEnt2;
};

class CTraceFilterSimpleList : public CTraceFilterSimple
{
public:
	//-----------------------------------------------------------------------------
// Trace filter that can take a list of entities to ignore
//-----------------------------------------------------------------------------
	CTraceFilterSimpleList(int collisionGroup) :
		CTraceFilterSimple(NULL, collisionGroup)
	{
	}
	//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		if (m_PassEntities.Find(pHandleEntity) != m_PassEntities.InvalidIndex())
			return false;

		return CTraceFilterSimple::ShouldHitEntity(pHandleEntity, contentsMask);
	}

	//-----------------------------------------------------------------------------
// Purpose: Add an entity to my list of entities to ignore in the trace
//-----------------------------------------------------------------------------
	virtual void AddEntityToIgnore(IHandleEntity* pEntity)
	{
		m_PassEntities.AddToTail(pEntity);
	}
protected:
	CUtlVector<IHandleEntity*>	m_PassEntities;
};

class CTraceFilterOnlyNPCsAndPlayer : public CTraceFilterSimple
{
public:
	CTraceFilterOnlyNPCsAndPlayer(const IHandleEntity* passentity, int collisionGroup)
		: CTraceFilterSimple(passentity, collisionGroup)
	{
	}

	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_ENTITIES_ONLY;
	}

	//-----------------------------------------------------------------------------
// Purpose: Trace filter that only hits NPCs and the player
//-----------------------------------------------------------------------------
	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		if (CTraceFilterSimple::ShouldHitEntity(pHandleEntity, contentsMask))
		{
			if (pHandleEntity->IsStaticProp())
				return false;

			if (pHandleEntity->GetEntityList()->GetGameRules()->ShouldHitAsNPC(pHandleEntity)) {
				return true;
			}

			return (pHandleEntity->IsNPC() || pHandleEntity->IsPlayer());
		}
		return false;
	}
};

class CTraceFilterNoNPCsOrPlayer : public CTraceFilterSimple
{
public:
	CTraceFilterNoNPCsOrPlayer(const IHandleEntity* passentity, int collisionGroup)
		: CTraceFilterSimple(passentity, collisionGroup)
	{
	}

	//-----------------------------------------------------------------------------
// Purpose: Trace filter that only hits anything but NPCs and the player
//-----------------------------------------------------------------------------
	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		if (CTraceFilterSimple::ShouldHitEntity(pHandleEntity, contentsMask))
		{
			if (pHandleEntity->IsStaticProp())
				return false;

			if (pHandleEntity->GetEntityList()->GetGameRules()->ShouldHitAsNPC(pHandleEntity)) {
				return false;
			}

			return (!pHandleEntity->IsNPC() && !pHandleEntity->IsPlayer());
		}
		return false;
	}
};

//-----------------------------------------------------------------------------
// Purpose: Custom trace filter used for NPC LOS traces
//-----------------------------------------------------------------------------
class CTraceFilterLOS : public CTraceFilterSkipTwoEntities
{
public:
	//-----------------------------------------------------------------------------
// Purpose: Custom trace filter used for NPC LOS traces
//-----------------------------------------------------------------------------
	CTraceFilterLOS(IHandleEntity* pHandleEntity, int collisionGroup, IHandleEntity* pHandleEntity2 = NULL) :
		CTraceFilterSkipTwoEntities(pHandleEntity, pHandleEntity2, collisionGroup)
	{
	}
	//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		if (!pHandleEntity->BlocksLOS())
			return false;

		return CTraceFilterSimple::ShouldHitEntity(pHandleEntity, contentsMask);
	}
};

class CTraceFilterSkipClassname : public CTraceFilterSimple
{
public:
	//-----------------------------------------------------------------------------
// Trace filter that can take a classname to ignore
//-----------------------------------------------------------------------------
	CTraceFilterSkipClassname(const IHandleEntity* passentity, const char* pchClassname, int collisionGroup) :
		CTraceFilterSimple(passentity, collisionGroup), m_pchClassname(pchClassname)
	{
	}
	//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		if (pHandleEntity->IsStaticProp() || pHandleEntity->ClassMatches(m_pchClassname))
			return false;

		return CTraceFilterSimple::ShouldHitEntity(pHandleEntity, contentsMask);
	}

private:

	const char* m_pchClassname;
};

class CTraceFilterSkipTwoClassnames : public CTraceFilterSkipClassname
{
public:
	// It does have a base, but we'll never network anything below here..
	typedef CTraceFilterSkipClassname BaseClass;
	typedef CTraceFilterSkipTwoClassnames ThisClass;;

	//-----------------------------------------------------------------------------
// Trace filter that skips two classnames
//-----------------------------------------------------------------------------
	CTraceFilterSkipTwoClassnames(const IHandleEntity* passentity, const char* pchClassname, const char* pchClassname2, int collisionGroup) :
		BaseClass(passentity, pchClassname, collisionGroup), m_pchClassname2(pchClassname2)
	{
	}
	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		if (pHandleEntity->IsStaticProp() || pHandleEntity->ClassMatches(m_pchClassname2))
			return false;

		return BaseClass::ShouldHitEntity(pHandleEntity, contentsMask);
	}

private:
	const char* m_pchClassname2;
};

class CTraceFilterSimpleClassnameList : public CTraceFilterSimple
{
public:
	//-----------------------------------------------------------------------------
// Trace filter that can take a list of entities to ignore
//-----------------------------------------------------------------------------
	CTraceFilterSimpleClassnameList(const IHandleEntity* passentity, int collisionGroup) :
		CTraceFilterSimple(passentity, collisionGroup)
	{
	}
	//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		if (pHandleEntity->IsStaticProp())
			return false;

		for (int i = 0; i < m_PassClassnames.Count(); ++i)
		{
			if (pHandleEntity->ClassMatches(m_PassClassnames[i]))
				return false;
		}

		return CTraceFilterSimple::ShouldHitEntity(pHandleEntity, contentsMask);
	}

	//-----------------------------------------------------------------------------
// Purpose: Add an entity to my list of entities to ignore in the trace
//-----------------------------------------------------------------------------
	virtual void AddClassnameToIgnore(const char* pchClassname)
	{
		m_PassClassnames.AddToTail(pchClassname);
	}
private:
	CUtlVector<const char*>	m_PassClassnames;
};

class CTraceFilterChain : public CTraceFilter
{
public:
	CTraceFilterChain(ITraceFilter* pTraceFilter1, ITraceFilter* pTraceFilter2)
	{
		m_pTraceFilter1 = pTraceFilter1;
		m_pTraceFilter2 = pTraceFilter2;
	}
	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		bool bResult1 = true;
		bool bResult2 = true;

		if (m_pTraceFilter1)
			bResult1 = m_pTraceFilter1->ShouldHitEntity(pHandleEntity, contentsMask);

		if (m_pTraceFilter2)
			bResult2 = m_pTraceFilter2->ShouldHitEntity(pHandleEntity, contentsMask);

		return (bResult1 && bResult2);
	}

private:
	ITraceFilter* m_pTraceFilter1;
	ITraceFilter* m_pTraceFilter2;
};

class CTraceFilterEntitiesOnly : public ITraceFilter
{
public:
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_ENTITIES_ONLY;
	}
};


//-----------------------------------------------------------------------------
// Classes need not inherit from these
//-----------------------------------------------------------------------------
class CTraceFilterWorldOnly : public ITraceFilter
{
public:
	bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		return false;
	}
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_WORLD_ONLY;
	}
};

class CTraceFilterWorldAndPropsOnly : public ITraceFilter
{
public:
	bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		return false;
	}
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_EVERYTHING;
	}
};

class CTraceFilterHitAll : public CTraceFilter
{
public:
	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{ 
		return true; 
	}
};

#endif // ENGINE_IENGINETRACE_H
