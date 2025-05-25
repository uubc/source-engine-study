//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for .
//
//===========================================================================//

#include "cbase.h"
#include "vcollide_parse.h"
#include "c_portal_player.h"
#include "c_basetempentity.h"
#include "takedamageinfo.h"
#include "in_buttons.h"
#include "iviewrender_beams.h"
#include "r_efx.h"
#include "dlight.h"
#include "PortalRender.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "tier1/KeyValues.h"
#include "ScreenSpaceEffects.h"
#include "portal_shareddefs.h"
#include "ivieweffects.h"		// for screenshake
#include "prop_portal_shared.h"
#include "ragdoll.h"

// NVNT for fov updates
#include "haptics/ihaptics.h"
#include "ivmodemanager.h"

// Don't alias here
#if defined( CPortal_Player )
#undef CPortal_Player	
#endif


#define DEATH_CC_LOOKUP_FILENAME "materials/correction/cc_death.raw"
#define DEATH_CC_FADE_SPEED 0.05f

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		// Create the effect.
		C_Portal_Player *pPlayer = dynamic_cast< C_Portal_Player* >( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->GetEngineObject()->IsDormant() )
		{
			pPlayer->DoAnimationEvent( (PlayerAnimEvent_t)m_iEvent.Get(), m_nData );
		}	
	}

public:
	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );

BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
RecvPropEHandle( RECVINFO( m_hPlayer ) ),
RecvPropInt( RECVINFO( m_iEvent ) ),
RecvPropInt( RECVINFO( m_nData ) )
END_RECV_TABLE()


//=================================================================================
//
// Ragdoll Entity
//
class C_PortalRagdoll : public C_ServerRagdoll
{
public:

	DECLARE_CLASS( C_PortalRagdoll, C_ServerRagdoll);
	DECLARE_CLIENTCLASS();

	C_PortalRagdoll();
	~C_PortalRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	int GetPlayerEntIndex() const;
	//IRagdoll* GetIRagdoll() const;

	virtual void SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );

private:

	C_PortalRagdoll( const C_PortalRagdoll & ) {}

	void Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity );
	void CreatePortalRagdoll();

private:

	EHANDLE	m_hPlayer;
	//CNetworkVector( m_vecRagdollVelocity );
	//CNetworkVector( m_vecRagdollOrigin );

};

IMPLEMENT_CLIENTCLASS_DT( C_PortalRagdoll, DT_PortalRagdoll, CPortalRagdoll )
//RecvPropVector( RECVINFO(m_vecRagdollOrigin) ),
RecvPropEHandle( RECVINFO( m_hPlayer ) ),
//RecvPropInt( RECVINFO( m_nModelIndex ) ),
//RecvPropInt( RECVINFO(m_nForceBone) ),
//RecvPropVector( RECVINFO(m_vecForce) ),
//RecvPropVector( RECVINFO( m_vecRagdollVelocity ) ),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_PortalRagdoll::C_PortalRagdoll()
{
	m_hPlayer = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_PortalRagdoll::~C_PortalRagdoll()
{
	( this );
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSourceEntity - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity )
{
	if ( !pSourceEntity )
		return;

	VarMapping_t *pSrc = pSourceEntity->GetEngineObject()->GetVarMapping();
	VarMapping_t *pDest = GetEngineObject()->GetVarMapping();

	// Find all the VarMapEntry_t's that represent the same variable.
	for ( int i = 0; i < pDest->m_Entries.Count(); i++ )
	{
		VarMapEntry_t *pDestEntry = &pDest->m_Entries[i];
		for ( int j=0; j < pSrc->m_Entries.Count(); j++ )
		{
			VarMapEntry_t *pSrcEntry = &pSrc->m_Entries[j];
			if ( !Q_strcmp( pSrcEntry->watcher->GetDebugName(), pDestEntry->watcher->GetDebugName() ) )
			{
				pDestEntry->watcher->Copy( pSrcEntry->watcher );
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Setup vertex weights for drawing
//-----------------------------------------------------------------------------
void C_PortalRagdoll::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	// While we're dying, we want to mimic the facial animation of the player. Once they're dead, we just stay as we are.
	if ( (m_hPlayer && m_hPlayer->IsAlive()) || !m_hPlayer )
	{
		BaseClass::SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
	else if ( m_hPlayer )
	{
		m_hPlayer->SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::CreatePortalRagdoll()
{
	// First, initialize all our data. If we have the player's entity on our client,
	// then we can make ourselves start out exactly where the player is.
	C_Portal_Player *pPlayer = dynamic_cast<C_Portal_Player*>( m_hPlayer.Get() );

	if ( pPlayer && !pPlayer->GetEngineObject()->IsDormant() )
	{
		// Move my current model instance to the ragdoll's so decals are preserved.
		pPlayer->GetEngineObject()->SnatchModelInstance( this->GetEngineObject());

		//VarMapping_t *varMap = GetEngineObject()->GetVarMapping();

		// This is the local player, so set them in a default
		// pose and slam their velocity, angles and origin
		GetEngineObject()->SetAbsOrigin( /* m_vecRagdollOrigin : */ pPlayer->GetRenderOrigin() );
		GetEngineObject()->SetAbsAngles( pPlayer->GetRenderAngles() );
		//GetEngineObject()->SetAbsVelocity( m_vecRagdollVelocity );

		// Hack! Find a neutral standing pose or use the idle.
		int iSeq = GetEngineObject()->LookupSequence( "ragdoll" );
		if ( iSeq == -1 )
		{
			Assert( false );
			iSeq = 0;
		}			
		GetEngineObject()->SetSequence( iSeq );
		GetEngineObject()->SetCycle( 0.0 );

		GetEngineObject()->Interp_Reset();

		GetEngineObject()->SetBody(pPlayer->GetEngineObject()->GetBody());
		GetEngineObject()->SetModelIndex(GetEngineObject()->GetModelIndex() );
		// Make us a ragdoll..
		GetEngineObject()->SetRenderFX(kRenderFxRagdoll);

		matrix3x4_t boneDelta0[MAXSTUDIOBONES];
		matrix3x4_t boneDelta1[MAXSTUDIOBONES];
		matrix3x4_t currentBones[MAXSTUDIOBONES];
		const float boneDt = 0.05f;

		pPlayer->GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );

		GetEngineObject()->InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );
		NoteRagdollCreationTick(this);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IRagdoll*
//-----------------------------------------------------------------------------
//IRagdoll* C_PortalRagdoll::GetIRagdoll() const
//{
//	return m_pRagdoll;
//}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		CreatePortalRagdoll();
	}
}


LINK_ENTITY_TO_CLASS( player, C_Portal_Player );

IMPLEMENT_CLIENTCLASS_DT(C_Portal_Player, DT_Portal_Player, CPortal_Player)
//RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
//RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),
RecvPropEHandle( RECVINFO( m_hRagdoll ) ),
RecvPropInt( RECVINFO( m_iSpawnInterpCounter ) ),
RecvPropInt( RECVINFO( m_iPlayerSoundType ) ),
RecvPropEHandle( RECVINFO( m_hSurroundingLiquidPortal ) ),
RecvPropBool( RECVINFO( m_bSuppressingCrosshair ) ),
END_RECV_TABLE()


BEGIN_PREDICTION_DATA( C_Portal_Player )
END_PREDICTION_DATA()

#define	_WALK_SPEED 150
#define	_NORM_SPEED 190
#define	_SPRINT_SPEED 320

static ConVar cl_playermodel( "cl_playermodel", "none", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE, "Default Player Model");

extern bool g_bUpsideDown;

//EHANDLE g_eKillTarget1;
//EHANDLE g_eKillTarget2;

void SpawnBlood (Vector vecSpot, const Vector &vecDir, int bloodColor, float flDamage);

C_Portal_Player::C_Portal_Player()
//: m_iv_angEyeAngles(gpGlobals->curtime, "C_Portal_Player::m_iv_angEyeAngles", &m_angEyeAngles, LATCH_SIMULATION_VAR)
{
	m_PlayerAnimState = CreatePortalPlayerAnimState( this );

	m_iIDEntIndex = 0;
	m_iSpawnInterpCounterCache = 0;
	m_flDeathCCWeight = 0.0f;

	m_hRagdoll.Set( NULL );
	m_flStartLookTime = 0.0f;



	//m_angEyeAngles.Init();

	m_blinkTimer.Invalidate();

	m_CCDeathHandle = INVALID_CLIENT_CCHANDLE;
}

bool C_Portal_Player::Init(int entnum, int iSerialNum) {
	bool ret = BaseClass::Init(entnum, iSerialNum);
	//GetEngineObject()->AddVar(&m_iv_angEyeAngles);//&m_angEyeAngles, , LATCH_SIMULATION_VAR
	GetEngineObject()->GetEntClientFlags() |= ENTCLIENTFLAG_DONTUSEIK;
	return ret;
}

C_Portal_Player::~C_Portal_Player( void )
{
	if ( m_PlayerAnimState )
	{
		m_PlayerAnimState->Release();
	}

	g_pColorCorrectionMgr->RemoveColorCorrection( m_CCDeathHandle );
}

int C_Portal_Player::GetIDTarget() const
{
	return m_iIDEntIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Update this client's target entity
//-----------------------------------------------------------------------------
void C_Portal_Player::UpdateIDTarget()
{
	if ( !IsLocalPlayer() )
		return;

	// Clear old target and find a new one
	m_iIDEntIndex = 0;

	// don't show IDs in chase spec mode
	if ( GetObserverMode() == OBS_MODE_CHASE || 
		GetObserverMode() == OBS_MODE_DEATHCAM )
		return;

	trace_t tr;
	Vector vecStart, vecEnd;
	VectorMA(g_pViewRender->MainViewOrigin(), 1500, g_pViewRender->MainViewForward(), vecEnd );
	VectorMA(g_pViewRender->MainViewOrigin(), 10, g_pViewRender->MainViewForward(), vecStart );
	UTIL_TraceLine(EntityList(), vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		C_BaseEntity *pEntity = (C_BaseEntity*)tr.m_pEnt;

		if ( pEntity && (pEntity != this) )
		{
			m_iIDEntIndex = pEntity->entindex();
		}
	}
}

void C_Portal_Player::TraceAttack( const ITakeDamageInfo&info, const Vector &vecDir, trace_t *ptr )
{
	Vector vecOrigin = ptr->endpos - vecDir * 4;

	float flDistance = 0.0f;

	if ( info.GetAttacker() )
	{
		flDistance = (ptr->endpos - info.GetAttacker()->GetEngineObject()->GetAbsOrigin()).Length();
	}

	if ( m_takedamage )
	{
		AddMultiDamage( info, this );

		int blood = BloodColor();

		if ( blood != DONT_BLEED )
		{
			SpawnBlood( vecOrigin, vecDir, blood, flDistance );// a little surface blood.
			TraceBleed( flDistance, vecDir, ptr, info.GetDamageType() );
		}
	}
}

void C_Portal_Player::Initialize( void )
{
	m_headYawPoseParam = GetEngineObject()->LookupPoseParameter(  "head_yaw" );
	GetEngineObject()->GetPoseParameterRange( m_headYawPoseParam, m_headYawMin, m_headYawMax );

	m_headPitchPoseParam = GetEngineObject()->LookupPoseParameter( "head_pitch" );
	GetEngineObject()->GetPoseParameterRange( m_headPitchPoseParam, m_headPitchMin, m_headPitchMax );

	IStudioHdr *hdr = GetEngineObject()->GetModelPtr();
	for ( int i = 0; i < hdr->GetNumPoseParameters() ; i++ )
	{
		GetEngineObject()->SetPoseParameter( hdr, i, 0.0 );
	}
}

IStudioHdr *C_Portal_Player::OnNewModel( void )
{
	IStudioHdr *hdr = BaseClass::OnNewModel();

	Initialize( );

	return hdr;
}

//-----------------------------------------------------------------------------
/**
* Orient head and eyes towards m_lookAt.
*/
void C_Portal_Player::UpdateLookAt( void )
{
	// head yaw
	if (m_headYawPoseParam < 0 || m_headPitchPoseParam < 0)
		return;

	// This is buggy with dt 0, just skip since there is no work to do.
	if ( gpGlobals->frametime <= 0.0f )
		return;

	// Player looks at themselves through portals. Pick the portal we're turned towards.
	const int iPortalCount = EntityList()->GetPortalCount();
	float *fPortalDot = (float *)stackalloc( sizeof( float ) * iPortalCount );
	float flLowDot = 1.0f;
	int iUsePortal = -1;

	// defaults if no portals are around
	Vector vPlayerForward;
	GetEngineObject()->GetVectors( &vPlayerForward, NULL, NULL );
	Vector vCurLookTarget = EyePosition();

	if ( !IsAlive() )
	{
		m_viewtarget = EyePosition() + vPlayerForward*10.0f;
		return;
	}

	bool bNewTarget = false;
	if ( UTIL_IntersectEntityExtentsWithPortal( this ) != NULL )
	{
		// player is in a portal
		vCurLookTarget = EyePosition() + vPlayerForward*10.0f;
	}
	else if (iPortalCount)
	{
		// Test through any active portals: This may be a shorter distance to the target
		for( int i = 0; i != iPortalCount; ++i )
		{
			IEnginePortalClient *pTempPortal = EntityList()->GetPortal(i);

			if( pTempPortal && pTempPortal->IsActivated() && pTempPortal->GetLinkedPortal())
			{
				Vector vEyeForward, vPortalForward;
				EyeVectors( &vEyeForward );
				pTempPortal->AsEngineObject()->GetVectors( &vPortalForward, NULL, NULL );
				fPortalDot[i] = vEyeForward.Dot( vPortalForward );
				if ( fPortalDot[i] < flLowDot )
				{
					flLowDot = fPortalDot[i];
					iUsePortal = i;
				}
			}
		}

		if ( iUsePortal >= 0 )
		{
			IEnginePortalClient* pPortal = EntityList()->GetPortal(iUsePortal);
			if ( pPortal )
			{
				vCurLookTarget = pPortal->MatrixThisToLinked()*vCurLookTarget;
				if ( vCurLookTarget != m_vLookAtTarget )
				{
					bNewTarget = true;
				}
			}
		}
	}
	else
	{
		// No other look targets, look straight ahead
		vCurLookTarget += vPlayerForward*10.0f;
	}

	// Figure out where we want to look in world space.
	QAngle desiredAngles;
	Vector to = vCurLookTarget - EyePosition();
	VectorAngles( to, desiredAngles );
	QAngle aheadAngles;
	VectorAngles( vCurLookTarget, aheadAngles );

	// Figure out where our body is facing in world space.
	QAngle bodyAngles( 0, 0, 0 );
	bodyAngles[YAW] = GetEngineObject()->GetLocalAngles()[YAW];

	m_flLastBodyYaw = bodyAngles[YAW];

	// Set the head's yaw.
	float desiredYaw = AngleNormalize( desiredAngles[YAW] - bodyAngles[YAW] );
	desiredYaw = clamp( desiredYaw, m_headYawMin, m_headYawMax );

	float desiredPitch = AngleNormalize( desiredAngles[PITCH] );
	desiredPitch = clamp( desiredPitch, m_headPitchMin, m_headPitchMax );

	if ( bNewTarget )
	{
		m_flStartLookTime = gpGlobals->curtime;
	}

	float dt = (gpGlobals->frametime);
	float flSpeed	= 1.0f - ExponentialDecay( 0.7f, 0.033f, dt );

	m_flCurrentHeadYaw = m_flCurrentHeadYaw + flSpeed * ( desiredYaw - m_flCurrentHeadYaw );
	m_flCurrentHeadYaw	= AngleNormalize( m_flCurrentHeadYaw );
	GetEngineObject()->SetPoseParameter( m_headYawPoseParam, m_flCurrentHeadYaw );

	m_flCurrentHeadPitch = m_flCurrentHeadPitch + flSpeed * ( desiredPitch - m_flCurrentHeadPitch );
	m_flCurrentHeadPitch = AngleNormalize( m_flCurrentHeadPitch );
	GetEngineObject()->SetPoseParameter( m_headPitchPoseParam, m_flCurrentHeadPitch );

	// This orients the eyes
	m_viewtarget = m_vLookAtTarget = vCurLookTarget;
}

void C_Portal_Player::ClientThink( void )
{
	//PortalEyeInterpolation.m_bNeedToUpdateEyePosition = true;
	BaseClass::ClientThink();

	Vector vForward;
	AngleVectors(GetEngineObject()->GetLocalAngles(), &vForward );

	// Allow sprinting
	HandleSpeedChanges();

	//FixTeleportationRoll();

	//QAngle vAbsAngles = EyeAngles();

	// Look at the thing that killed you
	//if ( !IsAlive() )
	//{
	//	C_BaseEntity *pEntity1 = g_eKillTarget1.Get();
	//	C_BaseEntity *pEntity2 = g_eKillTarget2.Get();

	//	if ( pEntity2 && pEntity1 )
	//	{
	//		//engine->GetViewAngles( vAbsAngles );

	//		Vector vLook = pEntity1->GetAbsOrigin() - pEntity2->GetAbsOrigin();
	//		VectorNormalize( vLook );

	//		QAngle qLook;
	//		VectorAngles( vLook, qLook );

	//		if ( qLook[PITCH] > 180.0f )
	//		{
	//			qLook[PITCH] -= 360.0f;
	//		}

	//		if ( vAbsAngles[YAW] < 0.0f )
	//		{
	//			vAbsAngles[YAW] += 360.0f;
	//		}

	//		if ( vAbsAngles[PITCH] < qLook[PITCH] )
	//		{
	//			vAbsAngles[PITCH] += gpGlobals->frametime * 120.0f;
	//			if ( vAbsAngles[PITCH] > qLook[PITCH] )
	//				vAbsAngles[PITCH] = qLook[PITCH];
	//		}
	//		else if ( vAbsAngles[PITCH] > qLook[PITCH] )
	//		{
	//			vAbsAngles[PITCH] -= gpGlobals->frametime * 120.0f;
	//			if ( vAbsAngles[PITCH] < qLook[PITCH] )
	//				vAbsAngles[PITCH] = qLook[PITCH];
	//		}

	//		if ( vAbsAngles[YAW] < qLook[YAW] )
	//		{
	//			vAbsAngles[YAW] += gpGlobals->frametime * 240.0f;
	//			if ( vAbsAngles[YAW] > qLook[YAW] )
	//				vAbsAngles[YAW] = qLook[YAW];
	//		}
	//		else if ( vAbsAngles[YAW] > qLook[YAW] )
	//		{
	//			vAbsAngles[YAW] -= gpGlobals->frametime * 240.0f;
	//			if ( vAbsAngles[YAW] < qLook[YAW] )
	//				vAbsAngles[YAW] = qLook[YAW];
	//		}

	//		if ( vAbsAngles[YAW] > 180.0f )
	//		{
	//			vAbsAngles[YAW] -= 360.0f;
	//		}

	//		engine->SetViewAngles( vAbsAngles );
	//	}
	//}

	// If dead, fade in death CC lookup
	if ( m_CCDeathHandle != INVALID_CLIENT_CCHANDLE )
	{
		if ( m_lifeState != LIFE_ALIVE )
		{
			if ( m_flDeathCCWeight < 1.0f )
			{
				m_flDeathCCWeight += DEATH_CC_FADE_SPEED;
				clamp( m_flDeathCCWeight, 0.0f, 1.0f );
			}
		}
		else 
		{
			m_flDeathCCWeight = 0.0f;
		}
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCDeathHandle, m_flDeathCCWeight );
	}

	UpdateIDTarget();
}



const QAngle& C_Portal_Player::GetRenderAngles()
{
	if (GetEngineObject()->IsRagdoll() )
	{
		return vec3_angle;
	}
	else
	{
		return BaseClass::GetRenderAngles();
	}
}

void C_Portal_Player::UpdateClientSideAnimation( void )
{
	UpdateLookAt();

	// Update the animation data. It does the local check here so this works when using
	// a third-person camera (and we don't have valid player angles).
	if ( this == C_Portal_Player::GetLocalPortalPlayer() )
		m_PlayerAnimState->Update( EyeAngles()[YAW], m_angEyeAngles[PITCH] );
	else
		m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );

	BaseClass::UpdateClientSideAnimation();
}

void C_Portal_Player::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	m_PlayerAnimState->DoAnimationEvent( event, nData );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_Portal_Player::DrawModel( int flags )
{
	if ( !GetEngineObject()->IsReadyToDraw() )
		return 0;

	if( IsLocalPlayer() )
	{
		if ( !C_BasePlayer::ShouldDrawThisPlayer() )
		{
			if ( !g_pViewRender->IsRenderingPortal() )
				return 0;

			if( (g_pViewRender->GetViewRecursionLevel() == 1) && (m_iForceNoDrawInPortalSurface != -1) ) //CPortalRender::s_iRenderingPortalView )
				return 0;
		}
	}

	return BaseClass::DrawModel(flags);
}



//-----------------------------------------------------------------------------
// Should this object receive shadows?
//-----------------------------------------------------------------------------
bool C_Portal_Player::ShouldReceiveProjectedTextures( int flags )
{
	Assert( flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );

	if (GetEngineObject()->IsEffectActive( EF_NODRAW ) )
		return false;

	if( flags & SHADOW_FLAGS_FLASHLIGHT )
	{
		return true;
	}

	return BaseClass::ShouldReceiveProjectedTextures( flags );
}

void C_Portal_Player::DoImpactEffect( trace_t &tr, int nDamageType )
{
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->DoImpactEffect( tr, nDamageType );
		return;
	}

	BaseClass::DoImpactEffect( tr, nDamageType );
}

void C_Portal_Player::PreThink( void )
{
	QAngle vTempAngles = GetEngineObject()->GetLocalAngles();

	if ( IsLocalPlayer() )
	{
		vTempAngles[PITCH] = EyeAngles()[PITCH];
	}
	else
	{
		vTempAngles[PITCH] = m_angEyeAngles[PITCH];
	}

	if ( vTempAngles[YAW] < 0.0f )
	{
		vTempAngles[YAW] += 360.0f;
	}

	GetEngineObject()->SetLocalAngles( vTempAngles );

	BaseClass::PreThink();

	HandleSpeedChanges();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_Portal_Player::AddEntity( void )
{
	BaseClass::AddEntity();

	QAngle vTempAngles = GetEngineObject()->GetLocalAngles();
	vTempAngles[PITCH] = m_angEyeAngles[PITCH];

	GetEngineObject()->SetLocalAngles( vTempAngles );

	// Zero out model pitch, blending takes care of all of it.
	GetEngineObject()->SetLocalAnglesDim( X_INDEX, 0 );

	if( this != (C_BasePlayer*)EntityList()->GetLocalPlayer() )
	{
		if (GetEngineObject()->IsEffectActive( EF_DIMLIGHT ) )
		{
			int iAttachment = GetEngineObject()->LookupAttachment( "anim_attachment_RH" );

			if ( iAttachment < 0 )
				return;

			Vector vecOrigin;
			QAngle eyeAngles = m_angEyeAngles;

			GetEngineObject()->GetAttachment( iAttachment, vecOrigin, eyeAngles );

			Vector vForward;
			AngleVectors( eyeAngles, &vForward );

			trace_t tr;
			UTIL_TraceLine(EntityList(), vecOrigin, vecOrigin + (vForward * 200), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		}
	}
}

ShadowType_t C_Portal_Player::ShadowCastType( void ) 
{
	// Drawing player shadows looks bad in first person when they get close to walls
	// It doesn't make sense to have shadows in the portal view, but not in the main view
	// So no shadows for the player
	return SHADOWS_NONE;
}

bool C_Portal_Player::ShouldDraw( void )
{
	if ( !IsAlive() )
		return false;

	//return true;

	//	if( GetTeamNumber() == TEAM_SPECTATOR )
	//		return false;

	if( IsLocalPlayer() && GetEngineObject()->IsRagdoll() )
		return true;

	if (GetEngineObject()->IsRagdoll() )
		return false;

	return true;

	return BaseClass::ShouldDraw();
}

const QAngle& C_Portal_Player::EyeAngles()
{
	if ( IsLocalPlayer() && g_pGameRules->GetKillCamMode() == OBS_MODE_NONE )
	{
		return BaseClass::EyeAngles();
	}
	else
	{
		//C_BaseEntity *pEntity1 = g_eKillTarget1.Get();
		//C_BaseEntity *pEntity2 = g_eKillTarget2.Get();

		//Vector vLook = Vector( 0.0f, 0.0f, 0.0f );

		//if ( pEntity2 )
		//{
		//	vLook = pEntity1->GetAbsOrigin() - pEntity2->GetAbsOrigin();
		//	VectorNormalize( vLook );
		//}
		//else if ( pEntity1 )
		//{
		//	return BaseClass::EyeAngles();
		//	//vLook =  - pEntity1->GetAbsOrigin();
		//}

		//if ( vLook != Vector( 0.0f, 0.0f, 0.0f ) )
		//{
		//	VectorAngles( vLook, m_angEyeAngles );
		//}

		return m_angEyeAngles;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IRagdoll*
//-----------------------------------------------------------------------------
const IEngineObjectClient* C_Portal_Player::GetRepresentativeRagdoll() const
{
	if ( m_hRagdoll.Get() )
	{
		C_PortalRagdoll *pRagdoll = static_cast<C_PortalRagdoll*>( m_hRagdoll.Get() );
		if ( !pRagdoll )
			return NULL;

		return pRagdoll->GetEngineObject();
	}
	else
	{
		return NULL;
	}
}

void C_Portal_Player::OnPreDataChanged( DataUpdateType_t type )
{
	m_hPreSurroundingLiquidPortal = m_hSurroundingLiquidPortal;
	BaseClass::OnPreDataChanged( type );
}

void C_Portal_Player::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if( m_hSurroundingLiquidPortal != m_hPreSurroundingLiquidPortal )
	{
		CLiquidPortal_InnerLiquidEffect *pLiquidEffect = (CLiquidPortal_InnerLiquidEffect *)g_pScreenSpaceEffects->GetScreenSpaceEffect( "LiquidPortal_InnerLiquid" );
		if( pLiquidEffect )
		{
			C_Func_LiquidPortal *pSurroundingPortal = m_hSurroundingLiquidPortal.Get();
			if( pSurroundingPortal != NULL )
			{
				C_Func_LiquidPortal *pOldSurroundingPortal = m_hPreSurroundingLiquidPortal.Get();
				if( pOldSurroundingPortal != pSurroundingPortal->m_hLinkedPortal.Get() )
				{
					pLiquidEffect->m_pImmersionPortal = pSurroundingPortal;
					pLiquidEffect->m_bFadeBackToReality = false;
				}
				else
				{
					pLiquidEffect->m_bFadeBackToReality = true;
					pLiquidEffect->m_fFadeBackTimeLeft = pLiquidEffect->s_fFadeBackEffectTime;
				}
			}
			else
			{
				pLiquidEffect->m_pImmersionPortal = NULL;
				pLiquidEffect->m_bFadeBackToReality = false;
			}
		}		
	}

	if ( type == DATA_UPDATE_CREATED )
	{
		// Load color correction lookup for the death effect
		m_CCDeathHandle = g_pColorCorrectionMgr->AddColorCorrection( DEATH_CC_LOOKUP_FILENAME );
		if ( m_CCDeathHandle != INVALID_CLIENT_CCHANDLE )
		{
			g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCDeathHandle, 0.0f );
		}

		//GetEngineObject()->SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	UpdateVisibility();
}



/*bool C_Portal_Player::ShouldInterpolate( void )
{
if( !IsInterpolationEnabled() )
return false;

return BaseClass::ShouldInterpolate();
}*/


void C_Portal_Player::PostDataUpdate( DataUpdateType_t updateType )
{
	// C_BaseEntity assumes we're networking the entity's angles, so pretend that it
	// networked the same value we already have.
	GetEngineObject()->SetNetworkAngles(GetEngineObject()->GetLocalAngles() );

	if ( m_iSpawnInterpCounter != m_iSpawnInterpCounterCache )
	{
		GetEngineObject()->MoveToLastReceivedPosition( true );
		GetEngineObject()->ResetLatched();
		m_iSpawnInterpCounterCache = m_iSpawnInterpCounter;
	}

	BaseClass::PostDataUpdate( updateType );
}

float C_Portal_Player::GetFOV( void )
{
	//Find our FOV with offset zoom value
	float flFOVOffset = C_BasePlayer::GetFOV() + GetZoom();

	// Clamp FOV in MP
	int min_fov = GetMinFOV();

	// Don't let it go too low
	flFOVOffset = MAX( min_fov, flFOVOffset );

	return flFOVOffset;
}

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector C_Portal_Player::GetAutoaimVector( float flDelta )
{
	// Never autoaim a predicted weapon (for now)
	Vector	forward;
	AngleVectors( EyeAngles() + m_Local.m_vecPunchAngle, &forward );
	return	forward;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we are allowed to sprint now.
//-----------------------------------------------------------------------------
bool C_Portal_Player::CanSprint( void )
{
	return ( (!m_Local.m_bDucked && !m_Local.m_bDucking) && (GetEngineObject()->GetWaterLevel() != 3) );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_Portal_Player::StartSprinting( void )
{
	//if( m_HL2Local.m_flSuitPower < 10 )
	//{
	//	// Don't sprint unless there's a reasonable
	//	// amount of suit power.
	//	CPASAttenuationFilter filter( this );
	//	filter.UsePredictionRules();
	//	EmitSound( filter, entindex(), "Player.SprintNoPower" );
	//	return;
	//}

	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();
	g_pSoundEmitterSystem->EmitSound( filter, entindex(), "Player.SprintStart" );

	SetMaxSpeed( _SPRINT_SPEED );
	m_fIsSprinting = true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_Portal_Player::StopSprinting( void )
{
	SetMaxSpeed( _NORM_SPEED );
	m_fIsSprinting = false;
}

void C_Portal_Player::HandleSpeedChanges( void )
{
	int buttonsChanged = m_afButtonPressed | m_afButtonReleased;

	if( buttonsChanged & IN_SPEED )
	{
		if ( !(m_afButtonPressed & IN_SPEED)  && IsSprinting() )
		{
			StopSprinting();
		}
		else if ( (m_afButtonPressed & IN_SPEED) && !IsSprinting() )
		{
			if ( CanSprint() )
			{
				StartSprinting();
			}
			else
			{
				// Reset key, so it will be activated post whatever is suppressing it.
				m_nButtons &= ~IN_SPEED;
			}
		}
	}
}

void C_Portal_Player::ItemPreFrame( void )
{
	if (GetEngineObject()->GetFlags() & FL_FROZEN )
		return;

	// Disallow shooting while zooming
	if ( m_nButtons & IN_ZOOM )
	{
		//FIXME: Held weapons like the grenade get sad when this happens
		m_nButtons &= ~(IN_ATTACK|IN_ATTACK2);
	}

	BaseClass::ItemPreFrame();

}

void C_Portal_Player::ItemPostFrame( void )
{
	if (GetEngineObject()->GetFlags() & FL_FROZEN )
		return;

	BaseClass::ItemPostFrame();
}

//C_BaseEntity *C_Portal_Player::BecomeRagdollOnClient()
//{
//	// Let the C_CSRagdoll entity do this.
//	// m_builtRagdoll = true;
//	return NULL;
//}



//Vector C_Portal_Player::EyePosition()
//{
//	return PortalEyeInterpolation.m_vEyePosition_Interpolated;  
//}



//void C_Portal_Player::CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov )
//{
//
//}

void C_Portal_Player::SetLocalViewAngles( const QAngle &viewAngles )
{
	// Nothing
	if ( engine->IsPlayingDemo() )
		return;
	BaseClass::SetLocalViewAngles( viewAngles );
}

void C_Portal_Player::SetViewAngles( const QAngle& ang )
{
	BaseClass::SetViewAngles( ang );

	if ( engine->IsPlayingDemo() )
	{
		pl.v_angle = ang;
	}
}



extern float g_fMaxViewModelLag;
void C_Portal_Player::CalcViewModelView( const Vector& eyeOrigin, const QAngle& eyeAngles)
{
	// HACK: Manually adjusting the eye position that view model looking up and down are similar
	// (solves view model "pop" on floor to floor transitions)
	Vector vInterpEyeOrigin = eyeOrigin;

	Vector vForward;
	Vector vRight;
	Vector vUp;
	AngleVectors( eyeAngles, &vForward, &vRight, &vUp );

	if ( vForward.z < 0.0f )
	{
		float fT = vForward.z * vForward.z;
		vInterpEyeOrigin += vRight * ( fT * 4.7f ) + vForward * ( fT * 5.0f ) + vUp * ( fT * 4.0f );
	}

	if ( UTIL_IntersectEntityExtentsWithPortal( this ) )
		g_fMaxViewModelLag = 0.0f;
	else
		g_fMaxViewModelLag = 1.5f;

	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = GetViewModel( i );
		if ( !vm )
			continue;

		vm->CalcViewModelView( this, vInterpEyeOrigin, eyeAngles );
	}
}

//bool LocalPlayerIsCloseToPortal( void )
//{
//	return C_Portal_Player::GetLocalPlayer()->IsCloseToPortal();
//}

