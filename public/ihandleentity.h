//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef IHANDLEENTITY_H
#define IHANDLEENTITY_H
#ifdef _WIN32
#pragma once
#endif
#include "platform.h"
#include "const.h"
#include "string_t.h"
#include "tier1/utlvector.h" //need CUtlVector for IEngineTrace::GetBrushesIn*()
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "mathlib/vector4d.h"
#include "mathlib/vmatrix.h"
#include "engine/ICollideable.h"
#include "model_types.h"
#include "gamerules.h"
#include "iclientvirtualreality.h"

class IEngineObjectServer;
class IEngineObjectClient;
class IEngineObject;
class IServerEntity;
class IClientEntity;
class IHandleEntity;
class CBaseHandle;
class IEntityFactory;
class IEntityList;
class IServerEntityList;
class IClientEntityList;
class datamap_t;
struct PS_SD_Static_SurfaceProperties_t;
class CTraceListData;
class CPhysCollide;
class IStudioHdr;
class IPhysics;
class IPhysicsEnvironment;
class IPhysicsSurfaceProps;
class IPhysicsObject;
class IPhysicsCollision;
class IPhysSaveRestoreBlockHandler;
class IPhysicsGameTrace;
class IPhysicsObjectPairHash;
struct EmitSound_t;
class IVModelInfo;
class IEffects;

//-----------------------------------------------------------------------------
// A ray...
//-----------------------------------------------------------------------------

struct Ray_t
{
	VectorAligned  m_Start;	// starting point, centered within the extents
	VectorAligned  m_Delta;	// direction + length of the ray
	VectorAligned  m_StartOffset;	// Add this to m_Start to get the actual ray start
	VectorAligned  m_Extents;	// Describes an axis aligned box extruded along a ray
	bool	m_IsRay;	// are the extents zero?
	bool	m_IsSwept;	// is delta != 0?

	void Init(Vector const& start, Vector const& end)
	{
		VectorSubtract(end, start, m_Delta);

		m_IsSwept = (m_Delta.LengthSqr() != 0);

		VectorClear(m_Extents);
		m_IsRay = true;

		// Offset m_Start to be in the center of the box...
		VectorClear(m_StartOffset);
		VectorCopy(start, m_Start);
	}

	void Init(Vector const& start, Vector const& end, Vector const& mins, Vector const& maxs)
	{
		VectorSubtract(end, start, m_Delta);

		m_IsSwept = (m_Delta.LengthSqr() != 0);

		VectorSubtract(maxs, mins, m_Extents);
		m_Extents *= 0.5f;
		m_IsRay = (m_Extents.LengthSqr() < 1e-6);

		// Offset m_Start to be in the center of the box...
		VectorAdd(mins, maxs, m_StartOffset);
		m_StartOffset *= 0.5f;
		VectorAdd(start, m_StartOffset, m_Start);
		m_StartOffset *= -1.0f;
	}

	// compute inverse delta
	Vector InvDelta() const
	{
		Vector vecInvDelta;
		for (int iAxis = 0; iAxis < 3; ++iAxis)
		{
			if (m_Delta[iAxis] != 0.0f)
			{
				vecInvDelta[iAxis] = 1.0f / m_Delta[iAxis];
			}
			else
			{
				vecInvDelta[iAxis] = FLT_MAX;
			}
		}
		return vecInvDelta;
	}

private:
};

//-----------------------------------------------------------------------------
// The standard trace filter... NOTE: Most normal traces inherit from CTraceFilter!!!
//-----------------------------------------------------------------------------
enum TraceType_t
{
	TRACE_EVERYTHING = 0,
	TRACE_WORLD_ONLY,				// NOTE: This does *not* test static props!!!
	TRACE_ENTITIES_ONLY,			// NOTE: This version will *not* test static props
	TRACE_EVERYTHING_FILTER_PROPS,	// NOTE: This version will pass the IHandleEntity for props through the filter, unlike all other filters
};

abstract_class ITraceFilter
{
public:
	virtual bool ShouldHitEntity(IHandleEntity * pEntity, int contentsMask) = 0;
	virtual TraceType_t	GetTraceType() const = 0;
};

//-----------------------------------------------------------------------------
// Enumeration interface for EnumerateLinkEntities
//-----------------------------------------------------------------------------
abstract_class IEntityEnumerator
{
public:
	// This gets called with each handle
	virtual bool EnumEntity(IHandleEntity * pHandleEntity) = 0;
};

//-----------------------------------------------------------------------------
// Interface the engine exposes to the game DLL
//-----------------------------------------------------------------------------
#define INTERFACEVERSION_ENGINETRACE_SERVER	"EngineTraceServer003"
#define INTERFACEVERSION_ENGINETRACE_CLIENT	"EngineTraceClient003"
abstract_class IEngineTrace
{
public:
	// Returns the contents mask + entity at a particular world-space position
	virtual int		GetPointContents(const Vector & vecAbsPosition, IHandleEntity * *ppEntity = NULL) = 0;

	// Get the point contents, but only test the specific entity. This works
	// on static props and brush models.
	//
	// If the entity isn't a static prop or a brush model, it returns CONTENTS_EMPTY and sets
	// bFailed to true if bFailed is non-null.
	virtual int		GetPointContents_Collideable(ICollideable* pCollide, const Vector& vecAbsPosition) = 0;

	// Traces a ray against a particular entity
	virtual void	ClipRayToEntity(const Ray_t& ray, unsigned int fMask, IHandleEntity* pEnt, trace_t* pTrace) = 0;

	// Traces a ray against a particular entity
	virtual void	ClipRayToCollideable(const Ray_t& ray, unsigned int fMask, ICollideable* pCollide, trace_t* pTrace) = 0;

	// A version that simply accepts a ray (can work as a traceline or tracehull)
	virtual void	TraceRay(const Ray_t& ray, unsigned int fMask, ITraceFilter* pTraceFilter, trace_t* pTrace) = 0;

	// A version that sets up the leaf and entity lists and allows you to pass those in for collision.
	virtual void	SetupLeafAndEntityListRay(const Ray_t& ray, CTraceListData& traceData) = 0;
	virtual void    SetupLeafAndEntityListBox(const Vector& vecBoxMin, const Vector& vecBoxMax, CTraceListData& traceData) = 0;
	virtual void	TraceRayAgainstLeafAndEntityList(const Ray_t& ray, CTraceListData& traceData, unsigned int fMask, ITraceFilter* pTraceFilter, trace_t* pTrace) = 0;

	// A version that sweeps a collideable through the world
	// abs start + abs end represents the collision origins you want to sweep the collideable through
	// vecAngles represents the collision angles of the collideable during the sweep
	virtual void	SweepCollideable(ICollideable* pCollide, const Vector& vecAbsStart, const Vector& vecAbsEnd,
		const QAngle& vecAngles, unsigned int fMask, ITraceFilter* pTraceFilter, trace_t* pTrace) = 0;

	// Enumerates over all entities along a ray
	// If triggers == true, it enumerates all triggers along a ray
	virtual void	EnumerateEntities(const Ray_t& ray, bool triggers, IEntityEnumerator* pEnumerator) = 0;

	// Same thing, but enumerate entitys within a box
	virtual void	EnumerateEntities(const Vector& vecAbsMins, const Vector& vecAbsMaxs, IEntityEnumerator* pEnumerator) = 0;

	// Convert a handle entity to a collideable.  Useful inside enumer
	virtual ICollideable* GetCollideable(IHandleEntity* pEntity) = 0;

	// HACKHACK: Temp for performance measurments
	virtual int GetStatByIndex(int index, bool bClear) = 0;


	//finds brushes in an AABB, prone to some false positives
	virtual void GetBrushesInAABB(const Vector& vMins, const Vector& vMaxs, CUtlVector<int>* pOutput, int iContentsMask = 0xFFFFFFFF) = 0;

	//Creates a CPhysCollide out of all displacements wholly or partially contained in the specified AABB
	virtual CPhysCollide* GetCollidableFromDisplacementsInAABB(const Vector& vMins, const Vector& vMaxs) = 0;

	//retrieve brush planes and contents, returns true if data is being returned in the output pointers, false if the brush doesn't exist
	virtual bool GetBrushInfo(int iBrush, CUtlVector<Vector4D>* pPlanesOut, int* pContentsOut) = 0;

	virtual bool PointOutsideWorld(const Vector& ptTest) = 0; //Tests a point to see if it's outside any playable area

	// Walks bsp to find the leaf containing the specified point
	virtual int GetLeafContainingPoint(const Vector& ptTest) = 0;
};

class IEngineWorld : public IEngineTrace {
public:

};

class IEnginePlayer {
public:

};

class IEnginePortal {
public:
	virtual bool IsActivated() const = 0;
	virtual bool IsPortal2() const = 0;
	virtual bool IsActivedAndLinked(void) const = 0;
	virtual bool IsReadyToSimulate(void) const = 0;
	virtual const IEngineObject* AsEngineObject() const = 0;
	virtual const VMatrix& MatrixThisToLinked() const = 0;
	virtual const cplane_t& GetPortalPlane() const = 0;
	virtual const IEnginePortal* GetLinkedPortal() const = 0;
	virtual bool RayIsInPortalHole(const Ray_t& ray) const = 0;
	virtual void TraceRay(const Ray_t& ray, unsigned int fMask, ITraceFilter* pTraceFilter, trace_t* pTrace, bool bTraceHolyWall = true) const = 0;
	virtual void TraceEntity(IHandleEntity* pEntity, const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, ITraceFilter* pFilter, trace_t* ptr) const = 0;
	virtual const PS_SD_Static_SurfaceProperties_t& GetSurfaceProperties() const = 0;
};

class IEngineShadowClone {
public:

};

class IEngineVehicle {
public:

};

class IEngineRope {
public:

};

class IEngineGhost {
public:

};

class IEngineObject {
public:
	virtual datamap_t* GetDataDescMap(void) const = 0;
	virtual const CBaseHandle& GetRefEHandle() const = 0;
	virtual IEntityList* GetEntityList() const = 0;
	virtual int entindex() const = 0;
	virtual const string_t& GetClassname() const = 0;
	virtual const string_t& GetGlobalname() const = 0;
	virtual int GetFlags(void) const = 0;
	virtual void AddEFlags(int nEFlagMask) = 0;
	virtual void RemoveEFlags(int nEFlagMask) = 0;
	virtual bool IsEFlagSet(int nEFlagMask) const = 0;
	virtual IEngineObject* GetMoveParent(void) const = 0;
	//virtual void SetMoveParent(IEngineObjectServer* hMoveParent) = 0;
	virtual IEngineObject* GetRootMoveParent() const = 0;
	virtual IEngineObject* FirstMoveChild(void) const = 0;
	//virtual void SetFirstMoveChild(IEngineObjectServer* hMoveChild) = 0;
	virtual IEngineObject* NextMovePeer(void) const = 0;
	virtual int GetModelIndex(void) const = 0;
	virtual string_t GetModelName(void) const = 0;
	virtual int GetModelType() const = 0;
	virtual const model_t* GetModel(void) const = 0;
	virtual IStudioHdr* GetModelPtr(void) const = 0;
	virtual float GetModelScale() const = 0;
	virtual void AddSolidFlags(int flags) = 0;
	virtual ICollideable* GetCollideable() = 0;
	virtual SolidType_t GetSolid() const = 0;
	virtual bool IsSolidFlagSet(int flagMask) const = 0;
	virtual bool IsMarkedForDeletion(void) = 0;
	virtual bool ComputeHitboxSurroundingBox(Vector* pVecWorldMins, Vector* pVecWorldMaxs) = 0;
	virtual bool ComputeEntitySpaceHitboxSurroundingBox(Vector* pVecWorldMins, Vector* pVecWorldMaxs) = 0;
	virtual void CollisionRulesChanged() = 0;
	virtual const Vector& GetAbsOrigin(void) const = 0;
	virtual const QAngle& GetAbsAngles(void) const = 0;
	virtual const Vector& GetAbsVelocity() const = 0;
	virtual const Vector& GetLocalOrigin(void) const = 0;
	virtual const QAngle& GetLocalAngles(void) const = 0;
	virtual const Vector& GetLocalVelocity() const = 0;
	virtual void GetVectors(Vector* forward, Vector* right, Vector* up) const = 0;
	virtual IHandleEntity* GetHandleEntity() const = 0;
	virtual const Vector& WorldAlignMins() const = 0;
	virtual const Vector& WorldAlignMaxs() const = 0;
	virtual const Vector& WorldAlignSize() const = 0;
	virtual void WorldSpaceAABB(Vector* pWorldMins, Vector* pWorldMaxs) const = 0;
	virtual const Vector& OBBMins() const = 0;
	virtual const Vector& OBBMaxs() const = 0;
	virtual const Vector& OBBSize() const = 0;
	virtual const Vector& GetCollisionOrigin() const = 0;
	virtual const QAngle& GetCollisionAngles() const = 0;
	virtual int GetCollisionGroup() const = 0;
	virtual const matrix3x4_t& CollisionToWorldTransform() const = 0;
	virtual const matrix3x4_t& EntityToWorldTransform() const = 0;
	virtual MoveType_t GetMoveType() const = 0;
	virtual int GetWaterLevel() const = 0;
	virtual IPhysicsObject* VPhysicsGetObject(void) const = 0;
	virtual bool IsRagdoll() const = 0;
	virtual IEngineObject* GetOwnerEntity(void) const = 0;
	virtual IEngineObject* GetEffectEntity(void) const = 0;
	virtual void SetGravity(float flGravity) = 0;
	virtual float GetGravity(void) const = 0;
	virtual int	LookupAttachment(const char* pAttachmentName) = 0;
	virtual bool GetAttachment(int number, Vector& origin, QAngle& angles) = 0;
	virtual bool GetAttachment(int number, matrix3x4_t& matrix) = 0;

	virtual bool IsEngineObjectServer() const = 0;
	virtual IEngineObjectServer* AsEngineObjectServer() = 0;
	virtual bool IsEngineObjectClient() const = 0;
	virtual IEngineObjectClient* AsEngineObjectClient() = 0;

	virtual bool IsWorld() = 0;
	virtual IEngineWorld* AsEngineWorld() = 0;
	virtual const IEngineWorld* AsEngineWorld() const = 0;
	virtual bool IsPlayer() = 0;
	virtual IEnginePlayer* AsEnginePlayer() = 0;
	virtual const IEnginePlayer* AsEnginePlayer() const = 0;
	virtual bool IsPortal() = 0;
	virtual IEnginePortal* AsEnginePortal() = 0;
	virtual const IEnginePortal* AsEnginePortal() const = 0;
	virtual bool IsShadowClone() = 0;
	virtual IEngineShadowClone* AsEngineShadowClone() = 0;
	virtual const IEngineShadowClone* AsEngineShadowClone() const = 0;
	virtual bool IsVehicle() = 0;
	virtual IEngineVehicle* AsEngineVehicle() = 0;
	virtual const IEngineVehicle* AsEngineVehicle() const = 0;
	virtual bool IsRope() = 0;
	virtual IEngineRope* AsEngineRope() = 0;
	virtual const IEngineRope* AsEngineRope() const = 0;
	virtual bool IsGhost() = 0;
	virtual IEngineGhost* AsEngineGhost() = 0;
	virtual const IEngineGhost* AsEngineGhost() const = 0;
};

class ITakeDamageInfo {
public:
	// Inflictor is the weapon or rocket (or player) that is dealing the damage.
	virtual IHandleEntity* GetInflictor() const = 0;
	virtual void SetInflictor(IHandleEntity* pInflictor) = 0;

	// Weapon is the weapon that did the attack.
	// For hitscan weapons, it'll be the same as the inflictor. For projectile weapons, the projectile 
	// is the inflictor, and this contains the weapon that created the projectile.
	virtual IHandleEntity* GetWeapon() const = 0;
	virtual void SetWeapon(IHandleEntity* pWeapon) = 0;

	// Attacker is the character who originated the attack (like a player or an AI).
	virtual IHandleEntity* GetAttacker() const = 0;
	virtual void SetAttacker(IHandleEntity* pAttacker) = 0;

	virtual float GetDamage() const = 0;
	virtual void SetDamage(float flDamage) = 0;
	virtual float GetMaxDamage() const = 0;
	virtual void SetMaxDamage(float flMaxDamage) = 0;
	virtual void ScaleDamage(float flScaleAmount) = 0;
	virtual void AddDamage(float flAddAmount) = 0;
	virtual void SubtractDamage(float flSubtractAmount) = 0;
	virtual float GetDamageBonus() const = 0;
	virtual void SetDamageBonus(float flBonus) = 0;

	virtual float GetBaseDamage() const = 0;
	virtual bool BaseDamageIsValid() const = 0;

	virtual Vector GetDamageForce() const = 0;
	virtual void SetDamageForce(const Vector& damageForce) = 0;
	virtual void ScaleDamageForce(float flScaleAmount) = 0;

	virtual Vector GetDamagePosition() const = 0;
	virtual void SetDamagePosition(const Vector& damagePosition) = 0;

	virtual Vector GetReportedPosition() const = 0;
	virtual void SetReportedPosition(const Vector& reportedPosition) = 0;

	virtual int GetDamageType() const = 0;
	virtual void SetDamageType(int bitsDamageType) = 0;
	virtual void AddDamageType(int bitsDamageType) = 0;
	virtual int GetDamageCustom(void) const = 0;
	virtual void SetDamageCustom(int iDamageCustom) = 0;
	virtual int GetDamageStats(void) const = 0;
	virtual void SetDamageStats(int iDamageStats) = 0;
	virtual void SetForceFriendlyFire(bool bValue) = 0;
	virtual bool IsForceFriendlyFire(void) const = 0;

	virtual int GetAmmoType() const = 0;
	virtual void SetAmmoType(int iAmmoType) = 0;
	virtual const char* GetAmmoName() const = 0;

	virtual int GetPlayerPenetrationCount() const = 0;
	virtual void SetPlayerPenetrationCount(int iPlayerPenetrationCount) = 0;

	virtual int GetDamagedOtherPlayers() const = 0;
	virtual void SetDamagedOtherPlayers(int iVal) = 0;

	virtual void Set(IHandleEntity* pInflictor, IHandleEntity* pAttacker, float flDamage, int bitsDamageType, int iKillType = 0) = 0;
	virtual void Set(IHandleEntity* pInflictor, IHandleEntity* pAttacker, IHandleEntity* pWeapon, float flDamage, int bitsDamageType, int iKillType = 0) = 0;
	virtual void Set(IHandleEntity* pInflictor, IHandleEntity* pAttacker, const Vector& damageForce, const Vector& damagePosition, float flDamage, int bitsDamageType, int iKillType = 0, Vector* reportedPosition = NULL) = 0;
	virtual void Set(IHandleEntity* pInflictor, IHandleEntity* pAttacker, IHandleEntity* pWeapon, const Vector& damageForce, const Vector& damagePosition, float flDamage, int bitsDamageType, int iKillType = 0, Vector* reportedPosition = NULL) = 0;

};

abstract_class IHandleWorld{
public:

	virtual void	Init() = 0;
	virtual void	Shutdown() = 0;
	// Level init, shutdown
	virtual void	LevelInit() = 0;
	virtual void	LevelInitPreEntity() = 0;
	virtual void	LevelInitPostEntity() = 0;
	// The level is shutdown in two parts
	virtual void	LevelShutdownPreEntity() = 0;
	virtual void	LevelShutdownPostEntity() = 0;
	virtual void	LevelShutdown() = 0;

	// Damage Queries - these need to be implemented by the various subclasses (single-player, multi-player, etc).
	// The queries represent queries against damage types and properties.
	virtual bool	Damage_IsTimeBased(int iDmgType) = 0;			// Damage types that are time-based.
	virtual bool	Damage_ShouldGibCorpse(int iDmgType) = 0;		// Damage types that gib the corpse.
	virtual bool	Damage_ShowOnHUD(int iDmgType) = 0;			// Damage types that have client HUD art.
	virtual bool	Damage_NoPhysicsForce(int iDmgType) = 0;		// Damage types that don't have to supply a physics force & position.
	virtual bool	Damage_ShouldNotBleed(int iDmgType) = 0;		// Damage types that don't make the player bleed.
	//Temp: These will go away once DamageTypes become enums.
	virtual int		Damage_GetTimeBased(void) = 0;				// Actual bit-fields.
	virtual int		Damage_GetShouldGibCorpse(void) = 0;
	virtual int		Damage_GetShowOnHud(void) = 0;
	virtual int		Damage_GetNoPhysicsForce(void) = 0;
	virtual int		Damage_GetShouldNotBleed(void) = 0;

// Ammo Definitions
	//CAmmoDef* GetAmmoDef();


	virtual bool ShouldCollide(int collisionGroup0, int collisionGroup1) = 0;
	virtual bool ShouldHitAsNPC(IHandleEntity* pHandleEntity) = 0;
	virtual int DefaultFOV(void) = 0;
	// Get the view vectors for this mod.
	virtual const CViewVectors* GetViewVectors() const = 0;

	virtual float GetDamageMultiplier(void) = 0;
	// Functions to verify the single/multiplayer status of a game
	virtual bool IsMultiplayer(void) = 0;// is this a multiplayer game? (either coop or deathmatch)
	virtual const unsigned char* GetEncryptionKey() = 0;
	virtual bool InRoundRestart(void) = 0;
	//Allow thirdperson camera.
	virtual bool AllowThirdPersonCamera(void) = 0;
	virtual void ClientCommandKeyValues(int pEntity, KeyValues* pKeyValues) = 0;
	// IsConnectedUserInfoChangeAllowed allows the clients to change
	// cvars with the FCVAR_NOT_CONNECTED rule if it returns true
	virtual const char* GetGameTypeName(void) = 0;
	virtual int GetGameType(void) = 0;
	virtual bool ShouldDrawHeadLabels() = 0;
	virtual void ClientSpawned(int  pPlayer) = 0;
	virtual void OnFileReceived(const char* fileName, unsigned int transferID) = 0;
	virtual bool IsHolidayActive( /*EHoliday*/ int eHoliday) const = 0;
	virtual void DebugDrawLine(const Vector& vecAbsStart, const Vector& vecAbsEnd, int r, int g, int b, bool test, float duration) = 0;
};

abstract_class IHandlePlayer{
public:

};

abstract_class IHandleNPC{
public:

};

// An IHandleEntity-derived class can go into an entity list and use ehandles.
class SINGLE_INHERITANCE IHandleEntity
{
public:
	virtual ~IHandleEntity() {}
	virtual int entindex() const = 0;
	virtual datamap_t* GetDataDescMap(void) const = 0;
	virtual void SetRefEHandle( const CBaseHandle &handle ) = 0;
	virtual const CBaseHandle& GetRefEHandle() const = 0;
	virtual IEntityFactory* GetEntityFactory() const { return NULL; }
	virtual IEntityList* GetEntityList() const { return NULL; }
	virtual IEngineObject* GetEngineObject() { return NULL; }
	virtual const IEngineObject* GetEngineObject() const { return NULL; }
	virtual bool IsServerEntity() { return false; }
	virtual IServerEntity* AsServerEntity() { return NULL; }
	virtual bool IsClientEntity() { return false; }
	virtual IClientEntity* AsClientEntity() { return NULL; }
	virtual void PostConstructor(const char* szClassname, int iForceEdictIndex) {}
	virtual bool Init(int entnum, int iSerialNum) { return true; }
	virtual void AfterInit() {};
	virtual const string_t& GetEntityName() const { static string_t s; return s; }
	virtual char const* GetClassname(void) const { return NULL; }
	virtual bool ClassMatches(const char* pszClassOrWildcard) { return false; }
	virtual char const* GetDebugName(void) const { return NULL; }
	virtual	bool ShouldCollide(int collisionGroup, int contentsMask) const { return false; }
	virtual bool TestCollision(const Ray_t& ray, unsigned int mask, trace_t& trace) { return false; }
	virtual	bool TestHitboxes(const Ray_t& ray, unsigned int fContentsMask, trace_t& tr) { return false; }
	virtual void ComputeWorldSpaceSurroundingBox(Vector* pWorldMins, Vector* pWorldMaxs) {}
	virtual bool ShouldSavePhysics() { return false; }
	virtual bool CreateVPhysics() { return false; }
	virtual bool IsWorld() const { return false; }
	virtual IHandleWorld* AsHandleWorld() { return NULL; }
	virtual bool IsStaticProp() const { return false; }
	virtual bool IsBSPModel() const { return false; }
	virtual bool IsCombatCharacter(void) const { return false; }
	virtual bool IsNPC(void) const { return false; }
	virtual bool IsPlayer(void) const { return false; }
	virtual bool IsLocalPlayer(void) const { return false; }
	virtual IHandlePlayer* AsHandlePlayer() { return NULL; }
	virtual bool IsAlive(void) { return false; }
	virtual bool IsStandable() const { return false; }
	virtual bool IsTransparent() const { return false; }
	virtual bool BlocksLOS(void) { return false; }
	virtual const Vector& WorldSpaceCenter() const { return *(Vector*)0; }
	virtual int GetTeamNumber(void) const { return 0; }
	virtual int GetMaxHealth() const { return 0; }
	virtual int GetHealth() const { return 0; }
	virtual const char& GetTakeDamage() const { return *(char*)0; }
	virtual float GetAttackDamageScale(IHandleEntity* pVictim) { return 0.0f; }
	virtual Vector EyePosition(void) { return *(Vector*)0; } // position of eyes
	virtual const QAngle& EyeAngles(void) { return *(QAngle*)0; }// Direction of eyes in world space
	virtual void EyeVectors(Vector* pForward, Vector* pRight = NULL, Vector* pUp = NULL) { AngleVectors(EyeAngles(), pForward, pRight, pUp); }
	virtual const QAngle& LocalEyeAngles(void) { return *(QAngle*)0; }	// Direction of eyes in local space (pl.v_angle)
	virtual Vector EarPosition(void) { return EyePosition(); }// position of ears
	virtual Vector Weapon_ShootPosition() { return EyePosition(); }
	virtual ITraceFilter* GetBeamTraceFilter(void) { return NULL; }
};

abstract_class IEntityCallBack{
public:
	virtual void AfterCreated(IHandleEntity* pEntity) = 0;
	virtual void BeforeDestroy(IHandleEntity* pEntity) = 0;
};

abstract_class IEntityFactory
{
public:
	virtual int GetEngineObjectType() = 0;
	virtual IHandleEntity * Create(IEntityList* pEntityList, int iForceEdictIndex,int iSerialNum ,IEntityCallBack* pCallBack) = 0;//const char* pClassName, 
	virtual void Destroy(IHandleEntity* pEntity) = 0;
	virtual const char* GetMapClassName() = 0;
	virtual const char* GetDllClassName() = 0;
	virtual size_t GetEntitySize() = 0;
	virtual int RequiredEdictIndex() = 0;
	virtual bool IsNetworkable() = 0;
	IEntityFactory* m_pNext = NULL;
};

// This is the glue that hooks .MAP entity class names to our CPP classes
abstract_class IEntityFactoryDictionary
{
public:
	virtual void InstallFactory(IEntityFactory * pFactory) = 0;
	virtual void UninstallFactory(IEntityFactory* pFactory) = 0;
	virtual IHandleEntity* Create(IEntityList* pEntityList, const char* pClassName , int iForceEdictIndex, int iSerialNum, IEntityCallBack* pCallBack) = 0;
	virtual void Destroy(IHandleEntity* pEntity) = 0;
	virtual IEntityFactory* FindFactory(const char* pClassName) = 0;
	virtual const char* GetMapClassName(const char* pClassName) = 0;
	virtual const char* GetDllClassName(const char* pClassName) = 0;
	virtual size_t		GetEntitySize(const char* pClassName) = 0;
	virtual int RequiredEdictIndex(const char* pClassName) = 0;
	virtual bool IsNetworkable(const char* pClassName) = 0;
	virtual const char* GetCannonicalName(const char* pClassName) = 0;
	virtual void ReportEntitySizes() = 0;
	virtual void DumpEntityFactories() = 0;
};

abstract_class IPooledStringAllocer{
public:
	virtual string_t AllocPooledString(const char* pStr) = 0;
};

abstract_class IEntityList : public IPooledStringAllocer
{
public:
	virtual IServerEntityList* AsServerEntityList() = 0;
	virtual IClientEntityList* AsClientEntityList() = 0;
	virtual IHandleEntity * CreateEntityByName(const char* className, int iForceEdictIndex = -1, int iSerialNum = -1) = 0;
	virtual void DestroyEntity(IHandleEntity* pEntity) = 0;
	virtual IHandleEntity* GetBaseEntityFromHandle(CBaseHandle hEnt) const = 0;
	virtual IHandleEntity* GetBaseEntity(int entnum) const = 0;
	virtual IHandleEntity* FindHandleEntityByName(IHandleEntity* pStartEntity, string_t iszName) = 0;
	virtual IEngineWorld* GetEngineWorld() = 0;
	virtual IHandleWorld* GetWorld() = 0;
	virtual int GetPortalCount() = 0;
	virtual IEnginePortal* GetPortal(int index) = 0;
	virtual void AddDirtyEntity(IEngineObject* pEntity) = 0;
	virtual IPhysics* Physics() = 0;
	virtual IPhysicsEnvironment* PhysGetEnv() = 0;
	virtual IPhysicsSurfaceProps* PhysGetProps() = 0;
	virtual IPhysicsObject* PhysGetWorldObject() = 0;
	virtual IPhysicsCollision* PhysGetCollision() = 0;
	virtual IPhysSaveRestoreBlockHandler* PhysSaveRestoreBlockHandler() = 0;
	virtual IPhysicsGameTrace* IPhysGameTrace() = 0;
	virtual IPhysicsObjectPairHash* PhysGetEntityCollisionHash() = 0;
	virtual IVModelInfo* GetModelInfo() = 0;
	virtual IEffects* GetEffects() = 0;
	virtual string_t AllocPooledString(const char* pStr) = 0;
	//-----------------------------------------------------------------------------
// Shared random number generators for shared/predicted code:
// whenever generating random numbers in shared/predicted code, these functions
// have to be used. Each call should specify a unique "sharedname" string that
// seeds the random number generator. In loops make sure the "additionalSeed"
// is increased with the loop counter, otherwise it will always return the
// same random number
//-----------------------------------------------------------------------------
	virtual float SharedRandomFloat(const char* sharedname, float flMinVal, float flMaxVal, int additionalSeed = 0) = 0;
	virtual int SharedRandomInt(const char* sharedname, int iMinVal, int iMaxVal, int additionalSeed = 0) = 0;
	virtual Vector SharedRandomVector(const char* sharedname, float minVal, float maxVal, int additionalSeed = 0) = 0;
	virtual QAngle SharedRandomAngle(const char* sharedname, float minVal, float maxVal, int additionalSeed = 0) = 0;
};

abstract_class IEntityMapData
{
public:
	virtual bool ExtractValue(const char* keyName, char* Value) = 0;
	// find the nth keyName in the endata and change its value to specified one
	// where n == nKeyInstance
	virtual bool SetValue(const char* keyName, char* NewValue, int nKeyInstance = 0) = 0;
	virtual bool GetFirstKey(char* keyName, char* Value) = 0;
	virtual bool GetNextKey(char* keyName, char* Value) = 0;
	virtual const char* CurrentBufferPosition(void) = 0;
};

class IInterpolationContext {
public:
	virtual void EnableExtrapolation(bool state) = 0;

	virtual bool IsThereAContext() = 0;

	virtual bool IsExtrapolationAllowed() = 0;

	virtual void SetLastTimeStamp(float timestamp) = 0;

	virtual float GetLastTimeStamp() = 0;
};

abstract_class IInterpolatedVar
{
public:
	virtual		 ~IInterpolatedVar() {}

	//virtual void Setup(void* pValue, int type) = 0;
	virtual void SetInterpolationAmount(float seconds) = 0;

	// Returns true if the new value is different from the prior most recent value.
	virtual void NoteLastNetworkedValue(float networkTime) = 0;
	virtual bool NoteChanged(float currentTime, float changetime, bool bUpdateLastNetworkedValue) = 0;
	virtual void Reset(float currentTime) = 0;

	// Returns 1 if the value will always be the same if currentTime is always increasing.
	virtual int Interpolate(IInterpolationContext* pContext, float currentTime) = 0;

	virtual int& GetType() = 0;
	virtual void RestoreToLastNetworked() = 0;
	virtual void Copy(IInterpolatedVar* pSrc) = 0;

	virtual const char* GetDebugName() = 0;
	//virtual void SetDebugName(const char* pName) = 0;

	virtual void SetDebug(bool bDebug) = 0;
};

template< typename Type>
class ITypedInterpolatedVar : public IInterpolatedVar {
public:
	virtual void ClearHistory() = 0;
	virtual void AddToHead(float changeTime, const Type* values, bool bFlushNewer) = 0;
	virtual const Type& GetCurrent(int iArrayIndex = 0) const = 0;
	virtual int		GetHead() = 0;
	virtual bool	IsValidIndex(int i) = 0;
	virtual int		GetNext(int i) = 0;
	virtual Type*	GetHistoryValue(int index, float& changetime, int iArrayIndex = 0) = 0;
	virtual int GetMaxCount() const = 0;

};

// inherit from this interface to be able to call WatchPositionChanges
abstract_class IWatcherCallback
{
public:
	virtual ~IWatcherCallback() {}
};

abstract_class IPositionWatcher : public IWatcherCallback
{
public:
	virtual void NotifyPositionChanged(IHandleEntity * pEntity) = 0;
};

abstract_class IVPhysicsWatcher : public IWatcherCallback
{
public:
	virtual void NotifyVPhysicsStateChanged(IPhysicsObject * pPhysics, IHandleEntity* pEntity, bool bAwake) = 0;
};

class IWatcherList {
public:
	virtual void Init() = 0;
	virtual void AddToList(IHandleEntity* pWatcher) = 0;
	virtual void RemoveWatcher(IHandleEntity* pWatcher) = 0;
	virtual int GetCallbackObjects(IWatcherCallback** pList, int listMax) = 0;
};

// Implement this class and register with gEntList to receive entity create/delete notification
template< class T >
class IEntityListener
{
public:
	virtual void PreEntityRemove(T* pEntity) {};
	virtual void OnEntityCreated(T* pEntity) {};
	virtual void OnEntitySpawned(T* pEntity) {};
	virtual void OnEntityDeleted(T* pEntity) {};
	virtual void PostEntityRemove(int entnum) {};
};

class ISoundPatch
{
public:
	virtual ~ISoundPatch() {}
	virtual void	ChangePitch(float pitchTarget, float deltaTime) = 0;
	virtual void	ChangeVolume(float volumeTarget, float deltaTime) = 0;
	virtual void	FadeOut(float deltaTime, bool destroyOnFadeout) = 0;
	virtual float	GetPitch(void) = 0;
	virtual float	GetVolume(void) = 0;
	virtual string_t GetName() = 0;
	virtual string_t GetScriptName() = 0;
	// UNDONE: Don't call this, use the controller to shut down
	virtual void	Shutdown(void) = 0;
	virtual bool	Update(float time, float deltaTime) = 0;
	virtual void	Reset(void) = 0;
	virtual void	StartSound(float flStartTime = 0) = 0;
	virtual void	ResumeSound(void) = 0;
	virtual int		IsPlaying(void) = 0;
	virtual void	AddPlayerPost(IHandleEntity* pPlayer) = 0;
	virtual void	SetCloseCaptionDuration(float flDuration) = 0;
	virtual void	SetBaseFlags(int iFlags) = 0;
	// Returns the ent index
	virtual int		EntIndex() const = 0;
};

enum soundcommands_t
{
	SOUNDCTRL_CHANGE_VOLUME,
	SOUNDCTRL_CHANGE_PITCH,
	SOUNDCTRL_STOP,
	SOUNDCTRL_DESTROY,
};

//Envelope point
struct envelopePoint_t
{
	float	amplitudeMin, amplitudeMax;
	float	durationMin, durationMax;
};

//Envelope description
struct envelopeDescription_t
{
	envelopePoint_t* pPoints;
	int				nNumPoints;
};

#define SERVER_SOUNDENVELOPECONTROLLER_INTERFACE_VERSION	"ServerSoundEnvelopeController001"
#define CLIENT_SOUNDENVELOPECONTROLLER_INTERFACE_VERSION	"ClientSoundEnvelopeController001"

abstract_class ISoundEnvelopeController
{
public:
	virtual void		SystemReset(void) = 0;
	virtual void		SystemUpdate(void) = 0;
	virtual void		Play(ISoundPatch* pSound, float volume, float pitch, float flStartTime = 0) = 0;
	virtual void		CommandAdd(ISoundPatch* pSound, float executeDeltaTime, soundcommands_t command, float commandTime, float value) = 0;
	virtual void		CommandClear(ISoundPatch* pSound) = 0;
	virtual void		Shutdown(ISoundPatch* pSound) = 0;

	virtual ISoundPatch* SoundCreate(IRecipientFilter& filter, int nEntIndex, const char* pSoundName) = 0;
	virtual ISoundPatch* SoundCreate(IRecipientFilter& filter, int nEntIndex, int channel, const char* pSoundName,
							float attenuation) = 0;
	virtual ISoundPatch* SoundCreate(IRecipientFilter& filter, int nEntIndex, int channel, const char* pSoundName,
							soundlevel_t soundlevel) = 0;
	virtual ISoundPatch* SoundCreate(IRecipientFilter& filter, int nEntIndex, const EmitSound_t& es) = 0;
	virtual void		SoundDestroy(ISoundPatch*) = 0;
	virtual void		SoundChangePitch(ISoundPatch* pSound, float pitchTarget, float deltaTime) = 0;
	virtual void		SoundChangeVolume(ISoundPatch* pSound, float volumeTarget, float deltaTime) = 0;
	virtual void		SoundFadeOut(ISoundPatch* pSound, float deltaTime, bool destroyOnFadeout = false) = 0;
	virtual float		SoundGetPitch(ISoundPatch* pSound) = 0;
	virtual float		SoundGetVolume(ISoundPatch* pSound) = 0;

	virtual float		SoundPlayEnvelope(ISoundPatch* pSound, soundcommands_t soundCommand, envelopePoint_t* points, int numPoints) = 0;
	virtual float		SoundPlayEnvelope(ISoundPatch* pSound, soundcommands_t soundCommand, envelopeDescription_t* envelope) = 0;

	virtual void		CheckLoopingSoundsForPlayer(IHandleEntity* pPlayer) = 0;

	virtual string_t	SoundGetName(ISoundPatch* pSound) = 0;

	virtual void		SoundSetCloseCaptionDuration(ISoundPatch* pSound, float flDuration) = 0;
};

#endif // IHANDLEENTITY_H
