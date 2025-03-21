//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "mathlib/mathlib.h"
#include "util_shared.h"
#include "model_types.h"
#include "convar.h"
#include "IEffects.h"
#include "vphysics/object_hash.h"
#include "mathlib/IceKey.H"
#include "checksum_crc.h"
#ifdef TF_CLIENT_DLL
#include "cdll_util.h"
#endif
#include "particle_parse.h"
#include "KeyValues.h"
#include "time.h"
#ifdef CLIENT_DLL
	#include "c_te_effect_dispatch.h"
#else
	#include "te_effect_dispatch.h"
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar g_Language;
























































//-----------------------------------------------------------------------------
// Sweeps against a particular model, using collision rules 
//-----------------------------------------------------------------------------
void UTIL_TraceModel( const Vector &vecStart, const Vector &vecEnd, const Vector &hullMin, 
					  const Vector &hullMax, CBaseEntity *pentModel, int collisionGroup, trace_t *ptr )
{
	// Cull it....
	if ( pentModel && pentModel->ShouldCollide( collisionGroup, MASK_ALL ) )
	{
		Ray_t ray;
		ray.Init( vecStart, vecEnd, hullMin, hullMax );
		enginetrace->ClipRayToEntity( ray, MASK_ALL, pentModel, ptr ); 
	}
	else
	{
		memset( ptr, 0, sizeof(trace_t) );
		ptr->fraction = 1.0f;
	}
}

void UTIL_Portal_NDebugOverlay(const Vector& ptPortalCenter, const QAngle& qPortalAngles, int r, int g, int b, int a, bool noDepthTest, float duration)
{
#ifndef CLIENT_DLL
	Vector pvTri1[3], pvTri2[3];

	UTIL_Portal_Triangles(ptPortalCenter, qPortalAngles, pvTri1, pvTri2);
	IServerEntity* player = EntityList()->GetLocalPlayer();
	if (!player)
		return;
	{
		Vector to1 = pvTri1[0] - player->GetEngineObject()->GetAbsOrigin();
		Vector to2 = pvTri1[1] - player->GetEngineObject()->GetAbsOrigin();
		Vector to3 = pvTri1[2] - player->GetEngineObject()->GetAbsOrigin();

		if ((to1.LengthSqr() > 90000000) &&
			(to2.LengthSqr() > 90000000) &&
			(to3.LengthSqr() > 90000000))
		{
			return;
		}

		// Clip triangles that are behind the client 
		Vector clientForward;
		player->EyeVectors(&clientForward);

		float  dot1 = DotProduct(clientForward, to1);
		float  dot2 = DotProduct(clientForward, to2);
		float  dot3 = DotProduct(clientForward, to3);

		if (dot1 < 0 && dot2 < 0 && dot3 < 0)
			return;

		if (debugoverlay)
		{
			debugoverlay->AddTriangleOverlay(pvTri1[0], pvTri1[1], pvTri1[2], r, g, b, a, noDepthTest, duration);
		}
	}
	{
		Vector to1 = pvTri2[0] - player->GetEngineObject()->GetAbsOrigin();
		Vector to2 = pvTri2[1] - player->GetEngineObject()->GetAbsOrigin();
		Vector to3 = pvTri2[2] - player->GetEngineObject()->GetAbsOrigin();

		if ((to1.LengthSqr() > 90000000) &&
			(to2.LengthSqr() > 90000000) &&
			(to3.LengthSqr() > 90000000))
		{
			return;
		}

		// Clip triangles that are behind the client 
		Vector clientForward;
		player->EyeVectors(&clientForward);

		float  dot1 = DotProduct(clientForward, to1);
		float  dot2 = DotProduct(clientForward, to2);
		float  dot3 = DotProduct(clientForward, to3);

		if (dot1 < 0 && dot2 < 0 && dot3 < 0)
			return;

		if (debugoverlay)
		{
			debugoverlay->AddTriangleOverlay(pvTri2[0], pvTri2[1], pvTri2[2], r, g, b, a, noDepthTest, duration);
		}
	}
#endif //#ifndef CLIENT_DLL
}

void UTIL_Portal_NDebugOverlay(const IEnginePortal* pPortal, int r, int g, int b, int a, bool noDepthTest, float duration)
{
#ifndef CLIENT_DLL
	UTIL_Portal_NDebugOverlay(pPortal->AsEngineObject()->GetAbsOrigin(), pPortal->AsEngineObject()->GetAbsAngles(), r, g, b, a, noDepthTest, duration);
#endif //#ifndef CLIENT_DLL
}

void UTIL_Portal_Trace_Filter(CTraceFilterSimpleClassnameList* traceFilterPortalShot)
{
	traceFilterPortalShot->AddClassnameToIgnore("prop_physics");
	traceFilterPortalShot->AddClassnameToIgnore("func_physbox");
	traceFilterPortalShot->AddClassnameToIgnore("npc_portal_turret_floor");
	traceFilterPortalShot->AddClassnameToIgnore("prop_energy_ball");
	traceFilterPortalShot->AddClassnameToIgnore("npc_security_camera");
	traceFilterPortalShot->AddClassnameToIgnore("player");
	traceFilterPortalShot->AddClassnameToIgnore("simple_physics_prop");
	traceFilterPortalShot->AddClassnameToIgnore("simple_physics_brush");
	traceFilterPortalShot->AddClassnameToIgnore("prop_ragdoll");
	traceFilterPortalShot->AddClassnameToIgnore("prop_glados_core");
	traceFilterPortalShot->AddClassnameToIgnore("updateitem2");
}

void UTIL_ClipTraceToPlayers( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, ITraceFilter *filter, trace_t *tr )
{
	trace_t playerTrace;
	Ray_t ray;
	float smallestFraction = tr->fraction;
	const float maxRange = 60.0f;

	ray.Init( vecAbsStart, vecAbsEnd );

	for ( int k = 1; k <= gpGlobals->maxClients; ++k )
	{
		CBasePlayer *player = ToBasePlayer(EntityList()->GetPlayerByIndex( k ));

		if ( !player || !player->IsAlive() )
			continue;

#ifdef CLIENT_DLL
		if ( player->IsDormant() )
			continue;
#endif // CLIENT_DLL

		if ( filter && filter->ShouldHitEntity( player, mask ) == false )
			continue;

		float range = DistanceToRay( player->WorldSpaceCenter(), vecAbsStart, vecAbsEnd );
		if ( range < 0.0f || range > maxRange )
			continue;

		enginetrace->ClipRayToEntity( ray, mask|CONTENTS_HITBOX, player, &playerTrace );
		if ( playerTrace.fraction < smallestFraction )
		{
			// we shortened the ray - save off the trace
			*tr = playerTrace;
			smallestFraction = playerTrace.fraction;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make a tracer using a particle effect
//-----------------------------------------------------------------------------
void UTIL_ParticleTracer( const char *pszTracerEffectName, const Vector &vecStart, const Vector &vecEnd, 
				 int iEntIndex, int iAttachment, bool bWhiz )
{
	int iParticleIndex = GetParticleSystemIndex( pszTracerEffectName );
	UTIL_Tracer( vecStart, vecEnd, iEntIndex, iAttachment, 0, bWhiz, "ParticleTracer", iParticleIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Make a tracer effect using the old, non-particle system, tracer effects.
//-----------------------------------------------------------------------------
void UTIL_Tracer( const Vector &vecStart, const Vector &vecEnd, int iEntIndex, 
				 int iAttachment, float flVelocity, bool bWhiz, const char *pCustomTracerName, int iParticleID )
{
	CEffectData data;
	data.m_vStart = vecStart;
	data.m_vOrigin = vecEnd;
	data.m_hEntity = EntityList()->GetBaseEntity( iEntIndex );
	data.m_flScale = flVelocity;
	data.m_nHitBox = iParticleID;

	// Flags
	if ( bWhiz )
	{
		data.m_fFlags |= TRACER_FLAG_WHIZ;
	}

	if ( iAttachment != TRACER_DONT_USE_ATTACHMENT )
	{
		data.m_fFlags |= TRACER_FLAG_USEATTACHMENT;
		data.m_nAttachmentIndex = iAttachment;
	}

	// Fire it off
	if ( pCustomTracerName )
	{
		g_pEffects->DispatchEffect( pCustomTracerName, data );
	}
	else
	{
		g_pEffects->DispatchEffect( "Tracer", data );
	}
}


void UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( color == DONT_BLEED || amount == 0 )
		return;

	if ( g_Language.GetInt() == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// scale up blood effect in multiplayer for better visibility
		amount *= 5;
	}

	if ( amount > 255 )
		amount = 255;

	if (color == BLOOD_COLOR_MECH)
	{
		g_pEffects->Sparks(origin);
		if (random->RandomFloat(0, 2) >= 1)
		{
			UTIL_Smoke(origin, random->RandomInt(10, 15), 10);
		}
	}
	else
	{
		// Normal blood impact
		UTIL_BloodImpact( origin, direction, color, amount );
	}
}	

//-----------------------------------------------------------------------------
// Purpose: Returns low violence settings
//-----------------------------------------------------------------------------
static ConVar	violence_hblood( "violence_hblood","1", 0, "Draw human blood" );
static ConVar	violence_hgibs( "violence_hgibs","1", 0, "Show human gib entities" );
static ConVar	violence_ablood( "violence_ablood","1", 0, "Draw alien blood" );
static ConVar	violence_agibs( "violence_agibs","1", 0, "Show alien gib entities" );

bool UTIL_IsLowViolence( void )
{
	// These convars are no longer necessary -- the engine is the final arbiter of
	// violence settings -- but they're here for legacy support and for testing low
	// violence when the engine is in normal violence mode.
	if ( !violence_hblood.GetBool() || !violence_ablood.GetBool() || !violence_hgibs.GetBool() || !violence_agibs.GetBool() )
		return true;

#ifdef TF_CLIENT_DLL
	// Use low violence if the local player has an item that allows them to see it (Pyro Goggles)
	if ( IsLocalPlayerUsingVisionFilterFlags( TF_VISION_FILTER_PYRO ) )
	{
		return true;
	}
#endif

	return engine->IsLowViolence();
}

bool UTIL_ShouldShowBlood( int color )
{
	if ( color != DONT_BLEED )
	{
		if ( color == BLOOD_COLOR_RED )
		{
			return violence_hblood.GetBool();
		}
		else
		{
			return violence_ablood.GetBool();
		}
	}
	return false;
}


//------------------------------------------------------------------------------
// Purpose : Use trace to pass a specific decal type to the entity being decaled
// Input   :
// Output  :
//------------------------------------------------------------------------------
void UTIL_DecalTrace( trace_t *pTrace, char const *decalName )
{
	if (pTrace->fraction == 1.0)
		return;

	CBaseEntity *pEntity = (CBaseEntity*)pTrace->m_pEnt;
	pEntity->DecalTrace( pTrace, decalName );
}


void UTIL_BloodDecalTrace( trace_t *pTrace, int bloodColor )
{
	if ( UTIL_ShouldShowBlood( bloodColor ) )
	{
		if ( bloodColor == BLOOD_COLOR_RED )
		{
			UTIL_DecalTrace( pTrace, "Blood" );
		}
		else
		{
			UTIL_DecalTrace( pTrace, "YellowBlood" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pos - 
//			&dir - 
//			color - 
//			amount - 
//-----------------------------------------------------------------------------
void UTIL_BloodImpact( const Vector &pos, const Vector &dir, int color, int amount )
{
	CEffectData	data;

	data.m_vOrigin = pos;
	data.m_vNormal = dir;
	data.m_flScale = (float)amount;
	data.m_nColor = (unsigned char)color;

	g_pEffects->DispatchEffect( "bloodimpact", data );
}

bool UTIL_IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax )
{
	Vector vHalfDims = ( vMax - vMin ) * 0.5f;
	Vector vCenter = vMin + vHalfDims;

	trace_t trace;
	UTIL_TraceHull(EntityList(), vCenter, vCenter, -vHalfDims, vHalfDims, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &trace );

	bool bClear = ( trace.fraction == 1 && trace.allsolid != 1 && (trace.startsolid != 1) );
	return bClear;
}

void UTIL_StringToFloatArray( float *pVector, int count, const char *pString )
{
	datamap_t::UTIL_StringToFloatArray(pVector, count, pString);
}

void UTIL_StringToVector( float *pVector, const char *pString )
{
	datamap_t::UTIL_StringToVector(pVector, pString);
}

void UTIL_StringToIntArray( int *pVector, int count, const char *pString )
{
	datamap_t::UTIL_StringToIntArray(pVector, count, pString);
}

void UTIL_StringToColor32( color32 *color, const char *pString )
{
	datamap_t::UTIL_StringToColor32(color, pString);
}

#ifndef _XBOX
void UTIL_DecodeICE( unsigned char * buffer, int size, const unsigned char *key)
{
	if ( !key )
		return;

	IceKey ice( 0 ); // level 0 = 64bit key
	ice.set( key ); // set key

	int blockSize = ice.blockSize();

	unsigned char *temp = (unsigned char *)_alloca( PAD_NUMBER( size, blockSize ) );
	unsigned char *p1 = buffer;
	unsigned char *p2 = temp;
				
	// encrypt data in 8 byte blocks
	int bytesLeft = size;
	while ( bytesLeft >= blockSize )
	{
		ice.decrypt( p1, p2 );
		bytesLeft -= blockSize;
		p1+=blockSize;
		p2+=blockSize;
	}

	// copy encrypted data back to original buffer
	Q_memcpy( buffer, temp, size-bytesLeft );
}
#endif

// work-around since client header doesn't like inlined gpGlobals->curtime
float IntervalTimer::Now( void ) const
{
	return gpGlobals->curtime;
}

// work-around since client header doesn't like inlined gpGlobals->curtime
float CountdownTimer::Now( void ) const
{
	return gpGlobals->curtime;
}

char* ReadAndAllocStringValue( KeyValues *pSub, const char *pName, const char *pFilename )
{
	const char *pValue = pSub->GetString( pName, NULL );
	if ( !pValue )
	{
		if ( pFilename )
		{
			DevWarning( "Can't get key value	'%s' from file '%s'.\n", pName, pFilename );
		}
		return "";
	}

	int len = Q_strlen( pValue ) + 1;
	char *pAlloced = new char[ len ];
	Assert( pAlloced );
	Q_strncpy( pAlloced, pValue, len );
	return pAlloced;
}

int UTIL_StringFieldToInt( const char *szValue, const char **pValueStrings, int iNumStrings )
{
	if ( !szValue || !szValue[0] )
		return -1;

	for ( int i = 0; i < iNumStrings; i++ )
	{
		if ( FStrEq(szValue, pValueStrings[i]) )
			return i;
	}

	Assert(0);
	return -1;
}


int find_day_of_week( struct tm& found_day, int day_of_week, int step )
{
	return 0;
}

void PhysRecheckObjectPair(IPhysicsObject* pObject0, IPhysicsObject* pObject1)
{
	if (!pObject0->IsStatic())
	{
		pObject0->RecheckCollisionFilter();
	}
	if (!pObject1->IsStatic())
	{
		pObject1->RecheckCollisionFilter();
	}
}

// disables collisions between entities (each entity may contain multiple objects)
void PhysDisableObjectCollisions(IPhysicsObject* pObject0, IPhysicsObject* pObject1)
{
	if (!pObject0 || !pObject1)
		return;

	EntityList()->PhysGetEntityCollisionHash()->AddObjectPair(pObject0, pObject1);
	PhysRecheckObjectPair(pObject0, pObject1);
}

// disables collisions between entities (each entity may contain multiple objects)
void PhysDisableEntityCollisions(IPhysicsObject* pObject0, IPhysicsObject* pObject1)
{
	if (!pObject0 || !pObject1)
		return;

	EntityList()->PhysGetEntityCollisionHash()->AddObjectPair(pObject0->GetGameData(), pObject1->GetGameData());
	PhysRecheckObjectPair(pObject0, pObject1);
}

void PhysDisableEntityCollisions(IHandleEntity* pEntity0, IHandleEntity* pEntity1)
{
	if (!pEntity0 || !pEntity1)
		return;

	EntityList()->PhysGetEntityCollisionHash()->AddObjectPair(pEntity0, pEntity1);
#ifndef CLIENT_DLL
	pEntity0->GetEngineObject()->CollisionRulesChanged();
	pEntity1->GetEngineObject()->CollisionRulesChanged();
#endif
}

void PhysEnableObjectCollisions(IPhysicsObject* pObject0, IPhysicsObject* pObject1)
{
	if (!pObject0 || !pObject1)
		return;

	EntityList()->PhysGetEntityCollisionHash()->RemoveObjectPair(pObject0, pObject1);
	PhysRecheckObjectPair(pObject0, pObject1);
}

void PhysEnableEntityCollisions(IPhysicsObject* pObject0, IPhysicsObject* pObject1)
{
	if (!pObject0 || !pObject1)
		return;

	EntityList()->PhysGetEntityCollisionHash()->RemoveObjectPair(pObject0->GetGameData(), pObject1->GetGameData());
	PhysRecheckObjectPair(pObject0, pObject1);
}

void PhysEnableEntityCollisions(IHandleEntity* pEntity0, IHandleEntity* pEntity1)
{
	if (!pEntity0 || !pEntity1)
		return;

	EntityList()->PhysGetEntityCollisionHash()->RemoveObjectPair(pEntity0, pEntity1);
#ifndef CLIENT_DLL
	pEntity0->GetEngineObject()->CollisionRulesChanged();
	pEntity1->GetEngineObject()->CollisionRulesChanged();
#endif
}

bool PhysEntityCollisionsAreDisabled(IHandleEntity* pEntity0, IHandleEntity* pEntity1)
{
	return EntityList()->PhysGetEntityCollisionHash()->IsObjectPairInHash(pEntity0, pEntity1);
}



extern ConVar hl2_episodic;

CRagdollLowViolenceManager g_RagdollLVManager;

void CRagdollLowViolenceManager::SetLowViolence(const char* pMapName)
{
	// set the value using the engine's low violence settings
	m_bLowViolence = UTIL_IsLowViolence();

#if !defined( CLIENT_DLL )
	// the server doesn't worry about low violence during multiplayer games
	if (g_pGameRules && g_pGameRules->IsMultiplayer())
	{
		m_bLowViolence = false;
	}
#endif

	// Turn the low violence ragdoll stuff off if we're in the HL2 Citadel maps because
	// the player has the super gravity gun and fading ragdolls will break things.
	if (hl2_episodic.GetBool())
	{
		if (Q_stricmp(pMapName, "ep1_citadel_02") == 0 ||
			Q_stricmp(pMapName, "ep1_citadel_02b") == 0 ||
			Q_stricmp(pMapName, "ep1_citadel_03") == 0)
		{
			m_bLowViolence = false;
		}
	}
	else
	{
		if (Q_stricmp(pMapName, "d3_citadel_03") == 0 ||
			Q_stricmp(pMapName, "d3_citadel_04") == 0 ||
			Q_stricmp(pMapName, "d3_citadel_05") == 0 ||
			Q_stricmp(pMapName, "d3_breen_01") == 0)
		{
			m_bLowViolence = false;
		}
	}
}