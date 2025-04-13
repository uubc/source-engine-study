//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PHYSICS_SHARED_H
#define PHYSICS_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "networkvar.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "engine/IEngineTrace.h"
#include "iserverentity.h"
#include "icliententity.h"

class IPhysics;
class IPhysicsEnvironment;
class IPhysicsSurfaceProps;
class IPhysicsCollision;
class IPhysicsObject;
class IPhysicsObjectPairHash;
class ISoundPatch;
struct objectparams_t;
class CPhysCollide;
struct solid_t;
struct vcollide_t;
class IVPhysicsKeyHandler;

extern ConVar sv_portal_collision_sim_bounds_x;
extern ConVar sv_portal_collision_sim_bounds_y;
extern ConVar sv_portal_collision_sim_bounds_z;
extern ConVar sv_portal_trace_vs_world;
extern ConVar sv_portal_trace_vs_displacements;
extern ConVar sv_portal_trace_vs_holywall;
extern ConVar sv_portal_trace_vs_staticprops;
extern ConVar sv_use_transformed_collideables;
//extern IPhysicsObject		*g_PhysWorldObject;
//extern IPhysics				*physics;
//extern IPhysicsCollision	*physcollision;
//extern IPhysicsEnvironment	*physenv;
//#ifdef PORTAL
//extern IPhysicsEnvironment	*physenv_main;
//#endif
//extern IPhysicsSurfaceProps *physprops;
//extern IPhysicsObjectPairHash *g_EntityCollisionHash;

//extern const objectparams_t g_PhysDefaultObjectParams;

// Compute enough energy of a reference mass travelling at speed
// makes numbers more intuitive


//void PhysFrictionSound( IHandleEntity *pEntity, IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit );
void PhysFrictionEffect(IEntityList* pEntityList, Vector &vecPos, Vector vecVel, float energy, int surfaceProps, int surfacePropsHit );

// Create a vphysics object based on a model
IPhysicsObject *PhysModelCreate( IHandleEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles, solid_t *pSolid = NULL );

IPhysicsObject *PhysModelCreateBox( IHandleEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, bool isStatic );
IPhysicsObject *PhysModelCreateOBB( IHandleEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, const QAngle &angle, bool isStatic );

// Create a vphysics object based on a BSP model (unmoveable)
IPhysicsObject *PhysModelCreateUnmoveable( IHandleEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles );

// Create a vphysics object based on an existing collision model
IPhysicsObject *PhysModelCreateCustom( IHandleEntity *pEntity, const CPhysCollide *pModel, const Vector &origin, const QAngle &angles, const char *pName, bool isStatic, solid_t *pSolid = NULL );

// Create a bbox collision model (these may be shared among entities, they are auto-deleted at end of level. do not manage)
CPhysCollide *PhysCreateBbox(IEntityList* pEntityList, const Vector &mins, const Vector &maxs );

// Create a vphysics sphere object
IPhysicsObject *PhysSphereCreate( IHandleEntity *pEntity, float radius, const Vector &origin, solid_t &solid );

// Destroy a physics object created using PhysModelCreate...()
void PhysDestroyObject(IEntityList* pEntityList, IPhysicsObject *pObject, IHandleEntity *pEntity = NULL );



// create the world physics objects
IPhysicsObject *PhysCreateWorld_Shared( IHandleEntity *pWorld, vcollide_t *pWorldCollide, const objectparams_t &defaultParams );

// parse the parameters for a single solid from the model's collision data
bool PhysModelParseSolid( solid_t &solid, IHandleEntity *pEntity, int modelIndex );
// parse the parameters for a solid matching a particular index
bool PhysModelParseSolidByIndex( solid_t &solid, IHandleEntity *pEntity, int modelIndex, int solidIndex );

void PhysParseSurfaceData( class IPhysicsSurfaceProps *pProps, class IFileSystem *pFileSystem );

// fill out this solid_t with the AABB defaults (high inertia/no rotation)
void PhysGetDefaultAABBSolid( solid_t &solid );



void PhysForceClearVelocity( IPhysicsObject *pPhys );
bool PhysHasContactWithOtherInDirection( IPhysicsObject *pPhysics, const Vector &dir );

void PrecachePhysicsSounds(IEntityList* pEntityList);


//=============================================================================
//
// Physics Game Trace
//
class CPhysicsGameTrace : public IPhysicsGameTrace
{
public:

	void VehicleTraceRay(const Ray_t& ray, void* pVehicle, trace_t* pTrace);
	void VehicleTraceRayWithWater(const Ray_t& ray, void* pVehicle, trace_t* pTrace);
	bool VehiclePointInWater(const Vector& vecPoint, void* pVehicle);
};

//-----------------------------------------------------------------------------
// Singleton access
//-----------------------------------------------------------------------------
extern IVPhysicsKeyHandler* g_pSolidSetup;
extern const objectparams_t g_PhysDefaultObjectParams;

//-----------------------------------------------------------------------------
// Purpose: Keeps track of original positions of any entities that are being possibly pushed
//  and handles restoring positions for those objects if the push is aborted
//-----------------------------------------------------------------------------
class CPhysicsPushedEntities
{
public:

	DECLARE_CLASS_NOBASE(CPhysicsPushedEntities);

	CPhysicsPushedEntities(void);

	// Purpose: Tries to rotate an entity hierarchy, returns the blocker if any
	IServerEntity* PerformRotatePush(IServerEntity* pRoot, float movetime);

	// Purpose: Tries to linearly push an entity hierarchy, returns the blocker if any
	IServerEntity* PerformLinearPush(IServerEntity* pRoot, float movetime);

	int			CountMovedEntities() { return m_rgMoved.Count(); }
	void		StoreMovedEntities(physicspushlist_t& list);
	void		BeginPush(IServerEntity* pRootEntity);

protected:

	// describes the per-frame incremental motion of a rotating MOVETYPE_PUSH
	struct RotatingPushMove_t
	{
		Vector		origin;
		matrix3x4_t	startLocalToWorld;
		matrix3x4_t	endLocalToWorld;
		QAngle		amove;		// delta orientation
	};

	// Pushers + their original positions also (for touching triggers)
	struct PhysicsPusherInfo_t
	{
		IServerEntity* m_pEntity;
		Vector				m_vecStartAbsOrigin;
	};

	// Pushed entities + various state related to them being pushed
	struct PhysicsPushedInfo_t
	{
		IServerEntity* m_pEntity;
		Vector				m_vecStartAbsOrigin;
		trace_t				m_Trace;
		bool				m_bBlocked;
		bool				m_bPusherIsGround;
	};

	// Adds the specified entity to the list
	void	AddEntity(IServerEntity* ent);

	// If a move fails, restores all entities to their original positions
	void	RestoreEntities();

	// Compute the direction to move the rotation blocker
	void	ComputeRotationalPushDirection(IServerEntity* pBlocker, const RotatingPushMove_t& rotPushMove, Vector* pMove, IServerEntity* pRoot);

	// Speculatively checks to see if all entities in this list can be pushed
	bool SpeculativelyCheckPush(PhysicsPushedInfo_t& info, const Vector& vecAbsPush, bool bRotationalPush);

	// Speculatively checks to see if all entities in this list can be pushed
	virtual bool SpeculativelyCheckRotPush(const RotatingPushMove_t& rotPushMove, IServerEntity* pRoot);

	// Speculatively checks to see if all entities in this list can be pushed
	virtual bool	SpeculativelyCheckLinearPush(const Vector& vecAbsPush);

	// Registers a blockage
	IServerEntity* RegisterBlockage();

	// Some fixup for objects pushed by rotating objects
	virtual void	FinishRotPushedEntity(IServerEntity* pPushedEntity, const RotatingPushMove_t& rotPushMove);

	// Commits the speculative movement
	void	FinishPush(bool bIsRotPush = false, const RotatingPushMove_t* pRotPushMove = NULL);

	// Generates a list of all entities potentially blocking all pushers
	void	GenerateBlockingEntityList();
	void	GenerateBlockingEntityListAddBox(const Vector& vecMoved);

	// Purpose: Gets a list of all entities hierarchically attached to the root 
	void	SetupAllInHierarchy(IServerEntity* pParent);

	// Unlink + relink the pusher list so we can actually do the push
	void	UnlinkPusherList(int* pPusherHandles);
	void	RelinkPusherList(int* pPusherHandles);

	// Causes all entities in the list to touch triggers from their prev position
	void	FinishPushers();

	// Purpose: Rotates the root entity, fills in the pushmove structure
	void	RotateRootEntity(IServerEntity* pRoot, float movetime, RotatingPushMove_t& rotation);

	// Purpose: Linearly moves the root entity
	void	LinearlyMoveRootEntity(IServerEntity* pRoot, float movetime, Vector* pAbsPushVector);

	bool	IsPushedPositionValid(IServerEntity* pBlocker);

protected:

	CUtlVector<PhysicsPusherInfo_t>	m_rgPusher;
	CUtlVector<PhysicsPushedInfo_t>	m_rgMoved;
	int								m_nBlocker;
	bool							m_bIsUnblockableByPlayer;
	Vector							m_rootPusherStartLocalOrigin;
	QAngle							m_rootPusherStartLocalAngles;
	float							m_rootPusherStartLocaltime;
	float							m_flMoveTime;

	friend class CPushBlockerEnum;
};

class CTraceFilterPushMove : public CTraceFilterSimple
{
	typedef CTraceFilterSimple BaseClass;
	typedef CTraceFilterPushMove ThisClass;;

public:
	CTraceFilterPushMove(IServerEntity* pEntity, int nCollisionGroup)
		: CTraceFilterSimple(pEntity, nCollisionGroup)
	{
		m_pRootParent = (IServerEntity*)pEntity->GetEngineObject()->GetRootMoveParent()->GetOuter();
	}

	bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		Assert(dynamic_cast<IServerEntity*>(pHandleEntity));
		IServerEntity* pTestEntity = static_cast<IServerEntity*>(pHandleEntity);
		if (!pTestEntity)
			return false;

		if (pTestEntity->GetEngineObject()->EntityHasMatchingRootParent(m_pRootParent ? m_pRootParent->GetEngineObject() : NULL))
			return false;

		if (pTestEntity->GetEngineObject()->GetMoveType() == MOVETYPE_VPHYSICS &&
			pTestEntity->GetEngineObject()->VPhysicsGetObject() && pTestEntity->GetEngineObject()->VPhysicsGetObject()->IsMoveable())
			return false;

		return BaseClass::ShouldHitEntity(pHandleEntity, contentsMask);
	}

private:

	IServerEntity* m_pRootParent;
};

extern CPhysicsPushedEntities *g_pPushedEntities;

#endif // PHYSICS_SHARED_H
