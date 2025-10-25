//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "c_baseentity.h"
#include "iprediction.h"
#include "model_types.h"
#include "iviewrender_beams.h"
#include "dlight.h"
#include "iviewrender.h"
#include "iefx.h"
#include "c_team.h"
#include "clientmode.h"
#include "usercmd.h"
#include "engine/IEngineSound.h"
#include "engine/IEngineTrace.h"
#include "engine/ivmodelinfo.h"
#include "tier0/vprof.h"
#include "fx_line.h"
#include "interface.h"
#include "materialsystem/imaterialsystem.h"
#include "soundinfo.h"
#include "mathlib/vmatrix.h"
#include "isaverestore.h"
#include "interval.h"
#include "engine/ivdebugoverlay.h"
#include "c_ai_basenpc.h"
#include "apparent_velocity_helper.h"
#include "c_baseanimatingoverlay.h"
#include "tier1/KeyValues.h"
#include "hltvcamera.h"
#include "datacache/imdlcache.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "decals.h"
//#include "cdll_bounded_cvars.h"
#include "inetchannelinfo.h"
#include "proto_version.h"
#include "predictioncopy.h"
#include "tier0/icommandline.h"
#include "ragdoll.h"
#include "bone_setup.h"
#include "gamestringpool.h"
#include "c_rope.h"
#include "studio_stats.h"
#include "eventlist.h"
#include "cl_animevent.h"
#include "c_te_legacytempents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifdef INTERPOLATEDVAR_PARANOID_MEASUREMENT
	int g_nInterpolatedVarsChanged = 0;
	bool g_bRestoreInterpolatedVarValues = false;
#endif

static int  g_nThreadModeTicks = 0;
// If an NPC is moving faster than this, he should play the running footstep sound
const float RUN_SPEED_ESTIMATE_SQR = 150.0f * 150.0f;

static ConVar  cl_interp_npcs( "cl_interp_npcs", "0.0", FCVAR_USERINFO, "Interpolate NPC positions starting this many seconds in past (or cl_interp, if greater)" );  
ConVar  r_drawmodeldecals( "r_drawmodeldecals", "1" );
static ConVar  r_drawrenderboxes( "r_drawrenderboxes", "0", FCVAR_CHEAT );  
static void VCollideWireframe_ChangeCallback(IConVar* pConVar, char const* pOldString, float flOldValue)
{
	for (IClientEntity* pEntity = EntityList()->FirstBaseEntity(); pEntity; pEntity = EntityList()->NextBaseEntity(pEntity))
	{
		pEntity->UpdateVisibility();
	}
}

ConVar vcollide_wireframe("vcollide_wireframe", "0", FCVAR_CHEAT, "Render physics collision models in wireframe", VCollideWireframe_ChangeCallback);

// Should these be somewhere else?
#define PITCH 0

void RecvProxy_ToolRecording( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	if ( !ToolsEnabled() )
		return;

	CBaseEntity *pEnt = (CBaseEntity *)pStruct;
	pEnt->GetEngineObject()->SetToolRecording( pData->m_Value.m_Int != 0 );
}

// Expose it to the engine.
IMPLEMENT_CLIENTCLASS(C_BaseEntity, DT_BaseEntity, CBaseEntity);



//static void RecvProxy_Solid( const CRecvProxyData *pData, void *pStruct, void *pOut )
//{
//	((C_BaseEntity*)pStruct)->SetSolid( (SolidType_t)pData->m_Value.m_Int );
//}

//static void RecvProxy_SolidFlags( const CRecvProxyData *pData, void *pStruct, void *pOut )
//{
//	((C_BaseEntity*)pStruct)->SetSolidFlags( pData->m_Value.m_Int );
//}

//#ifndef NO_ENTITY_PREDICTION
//BEGIN_RECV_TABLE_NOBASE( C_BaseEntity, DT_PredictableId )
//	RecvPropPredictableId( RECVINFO( m_PredictableID ) ),
//	RecvPropInt( RECVINFO( m_bIsPlayerSimulated ) ),
//END_RECV_TABLE()
//#endif


BEGIN_RECV_TABLE_NOBASE(C_BaseEntity, DT_BaseEntity)

	RecvPropInt(RECVINFO(m_iTeamNum)),
	RecvPropFloat(RECVINFO(m_flShadowCastDistance)),


	//RecvPropDataTable( RECVINFO_DT( m_Collision ), 0, &REFERENCE_RECV_TABLE(DT_CollisionProperty) ),
	
	RecvPropInt( RECVINFO ( m_iTextureFrameIndex ) ),
//#if !defined( NO_ENTITY_PREDICTION )
//	RecvPropDataTable( "predictable_id", 0, 0, &REFERENCE_RECV_TABLE( DT_PredictableId ) ),
//#endif



#ifdef TF_CLIENT_DLL
	RecvPropArray3( RECVINFO_ARRAY(m_nModelIndexOverrides),	RecvPropInt( RECVINFO(m_nModelIndexOverrides[0]) ) ),
#endif
	RecvPropEHandle(RECVINFO(m_hLightingOrigin)),
	RecvPropEHandle(RECVINFO(m_hLightingOriginRelative)),
	RecvPropFloat(RECVINFO(m_fadeMinDist)),
	RecvPropFloat(RECVINFO(m_fadeMaxDist)),
	RecvPropFloat(RECVINFO(m_flFadeScale)),
END_RECV_TABLE()


BEGIN_PREDICTION_DATA_NO_BASE( C_BaseEntity )

	// These have a special proxy to handle send/receive
	//DEFINE_PRED_TYPEDESCRIPTION( m_Collision, CCollisionProperty ),

	//DEFINE_PRED_FIELD( m_MoveType, FIELD_CHARACTER, FTYPEDESC_INSENDTABLE ),
	//DEFINE_PRED_FIELD( m_MoveCollide, FIELD_CHARACTER, FTYPEDESC_INSENDTABLE ),

	//DEFINE_FIELD( m_vecAbsVelocity, FIELD_VECTOR ),
	//DEFINE_PRED_FIELD_TOL( m_vecVelocity, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.5f ),
//	DEFINE_PRED_FIELD( m_fEffects, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	//DEFINE_PRED_FIELD( m_nRenderMode, FIELD_CHARACTER, FTYPEDESC_INSENDTABLE ),
	//DEFINE_PRED_FIELD( m_nRenderFX, FIELD_CHARACTER, FTYPEDESC_INSENDTABLE ),
//	DEFINE_PRED_FIELD( m_flAnimTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
//	DEFINE_PRED_FIELD( m_flSimulationTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	//DEFINE_PRED_FIELD( m_fFlags, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_vecViewOffset, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.25f ),
//	DEFINE_PRED_FIELD( m_nModelIndex, FIELD_SHORT, FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX ),
	//DEFINE_PRED_FIELD( m_flFriction, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iTeamNum, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	//DEFINE_PRED_FIELD( m_hOwnerEntity, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),

//	DEFINE_FIELD( m_nSimulationTick, FIELD_INTEGER ),

//	DEFINE_PRED_FIELD( m_pMoveParent, FIELD_EHANDLE ),
//	DEFINE_PRED_FIELD( m_pMoveChild, FIELD_EHANDLE ),
//	DEFINE_PRED_FIELD( m_pMovePeer, FIELD_EHANDLE ),
//	DEFINE_PRED_FIELD( m_pMovePrevPeer, FIELD_EHANDLE ),


	//DEFINE_FIELD( m_vecAbsOrigin, FIELD_VECTOR ),
	//DEFINE_FIELD( m_angAbsRotation, FIELD_VECTOR ),
	//DEFINE_FIELD( m_vecOrigin, FIELD_VECTOR ),
	//DEFINE_FIELD( m_angRotation, FIELD_VECTOR ),

//	DEFINE_FIELD( m_hGroundEntity, FIELD_EHANDLE ),
//	DEFINE_FIELD( m_nWaterLevel, FIELD_CHARACTER ),
//	DEFINE_FIELD( m_nWaterType, FIELD_CHARACTER ),
//	DEFINE_FIELD( m_vecAngVelocity, FIELD_VECTOR ),
//	DEFINE_FIELD( m_vecAbsAngVelocity, FIELD_VECTOR ),


//	DEFINE_FIELD( model, FIELD_INTEGER ), // writing pointer literally
//	DEFINE_FIELD( index, FIELD_INTEGER ),
//	DEFINE_FIELD( m_ClientHandle, FIELD_SHORT ),
//	DEFINE_FIELD( m_Partition, FIELD_SHORT ),
//	DEFINE_FIELD( m_hRender, FIELD_SHORT ),
//	DEFINE_FIELD( m_bDormant, FIELD_BOOLEAN ),
//	DEFINE_FIELD( current_position, FIELD_INTEGER ),
//	DEFINE_FIELD( m_flLastMessageTime, FIELD_FLOAT ),
//	DEFINE_FIELD( m_vecBaseVelocity, FIELD_VECTOR ),
	//DEFINE_FIELD( m_flGravity, FIELD_FLOAT ),
//	DEFINE_FIELD( m_ModelInstance, FIELD_SHORT ),
	//DEFINE_FIELD( m_flProxyRandomValue, FIELD_FLOAT ),

//	DEFINE_FIELD( m_PredictableID, FIELD_INTEGER ),
//	DEFINE_FIELD( m_pPredictionContext, FIELD_POINTER ),
	// Stuff specific to rendering and therefore not to be copied back and forth
	// DEFINE_PRED_FIELD( m_clrRender, color32, FTYPEDESC_INSENDTABLE  ),
	// DEFINE_FIELD( m_bReadyToDraw, FIELD_BOOLEAN ),
	// DEFINE_FIELD( anim, CLatchedAnim ),
	// DEFINE_FIELD( mouth, CMouthInfo ),
	// DEFINE_FIELD( GetAbsOrigin(), FIELD_VECTOR ),
	// DEFINE_FIELD( GetAbsAngles(), FIELD_VECTOR ),
	// DEFINE_FIELD( m_nNumAttachments, FIELD_SHORT ),
	// DEFINE_FIELD( m_pAttachmentAngles, FIELD_VECTOR ),
	// DEFINE_FIELD( m_pAttachmentOrigin, FIELD_VECTOR ),
	// DEFINE_FIELD( m_listentry, CSerialEntity ),
	// DEFINE_FIELD( m_ShadowHandle, ClientShadowHandle_t ),
	// DEFINE_FIELD( m_hThink, ClientThinkHandle_t ),
	// Definitely private and not copied around
	// DEFINE_FIELD( m_bPredictable, FIELD_BOOLEAN ),
	// DEFINE_FIELD( m_CollisionGroup, FIELD_INTEGER ),
	// DEFINE_FIELD( m_DataChangeEventRef, FIELD_INTEGER ),
#if !defined( CLIENT_DLL )
	// DEFINE_FIELD( m_bPredictionEligible, FIELD_BOOLEAN ),
#endif
END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Helper functions.
//-----------------------------------------------------------------------------

void SpewInterpolatedVar( CInterpolatedVar< Vector > *pVar )
{
	Msg( "--------------------------------------------------\n" );
	int i = pVar->GetHead();
	CApparentVelocity<Vector> apparent;
	float prevtime = 0.0f;
	while ( 1 )
	{
		float changetime;
		Vector *pVal = pVar->GetHistoryValue( i, changetime );
		if ( !pVal )
			break;

		float vel = apparent.AddSample( changetime, *pVal );
		Msg( "%6.6f: (%.2f %.2f %.2f), vel: %.2f [dt %.1f]\n", changetime, VectorExpand( *pVal ), vel, prevtime == 0.0f ? 0.0f : 1000.0f * ( changetime - prevtime ) );
		i = pVar->GetNext( i );
		prevtime = changetime;
	}
	Msg( "--------------------------------------------------\n" );
}

void SpewInterpolatedVar( CInterpolatedVar< Vector > *pVar, float flNow, float flInterpAmount, bool bSpewAllEntries = true )
{
	float target = flNow - flInterpAmount;

	Msg( "--------------------------------------------------\n" );
	int i = pVar->GetHead();
	CApparentVelocity<Vector> apparent;
	float newtime = 999999.0f;
	Vector newVec( 0, 0, 0 );
	bool bSpew = true;

	while ( 1 )
	{
		float changetime;
		Vector *pVal = pVar->GetHistoryValue( i, changetime );
		if ( !pVal )
			break;

		if ( bSpew && target >= changetime )
		{
			Vector o;
			pVar->DebugInterpolate(NULL, &o, flNow );
			bool bInterp = newtime != 999999.0f;
			float frac = 0.0f;
			char desc[ 32 ];

			if ( bInterp )
			{
				frac = ( target - changetime ) / ( newtime - changetime );
				Q_snprintf( desc, sizeof( desc ), "interpolated [%.2f]", frac );
			}
			else
			{
				bSpew = true;
				int savei = i;
				i = pVar->GetNext( i );
				float oldtertime = 0.0f;
				pVar->GetHistoryValue( i, oldtertime );

				if ( changetime != oldtertime )
				{
					frac = ( target - changetime ) / ( changetime - oldtertime );
				}

				Q_snprintf( desc, sizeof( desc ), "extrapolated [%.2f]", frac );
				i = savei;
			}

			if ( bSpew )
			{
				Msg( "  > %6.6f: (%.2f %.2f %.2f) %s for %.1f msec\n", 
					target, 
					VectorExpand( o ), 
					desc,
					1000.0f * ( target - changetime ) );
				bSpew = false;
			}
		}

		float vel = apparent.AddSample( changetime, *pVal );
		if ( bSpewAllEntries )
		{
			Msg( "    %6.6f: (%.2f %.2f %.2f), vel: %.2f [dt %.1f]\n", changetime, VectorExpand( *pVal ), vel, newtime == 999999.0f ? 0.0f : 1000.0f * ( newtime - changetime ) );
		}
		i = pVar->GetNext( i );
		newtime = changetime;
		newVec = *pVal;
	}
	Msg( "--------------------------------------------------\n" );
}
void SpewInterpolatedVar( CInterpolatedVar< float > *pVar )
{
	Msg( "--------------------------------------------------\n" );
	int i = pVar->GetHead();
	CApparentVelocity<float> apparent;
	while ( 1 )
	{
		float changetime;
		float *pVal = pVar->GetHistoryValue( i, changetime );
		if ( !pVal )
			break;

		float vel = apparent.AddSample( changetime, *pVal );
		Msg( "%6.6f: (%.2f), vel: %.2f\n", changetime, *pVal, vel );
		i = pVar->GetNext( i );
	}
	Msg( "--------------------------------------------------\n" );
}

template<class T>
void GetInterpolatedVarTimeRange( CInterpolatedVar<T> *pVar, float &flMin, float &flMax )
{
	flMin = 1e23;
	flMax = -1e23;

	int i = pVar->GetHead();
	CApparentVelocity<Vector> apparent;
	while ( 1 )
	{
		float changetime;
		if ( !pVar->GetHistoryValue( i, changetime ) )
			return;

		flMin = MIN( flMin, changetime );
		flMax = MAX( flMax, changetime );
		i = pVar->GetNext( i );
	}
}


int	C_BaseEntity::GetTextureFrameIndex( void )
{
	return m_iTextureFrameIndex;
}

void C_BaseEntity::SetTextureFrameIndex( int iIndex )
{
	m_iTextureFrameIndex = iIndex;
}


//-----------------------------------------------------------------------------
// Functions.
//-----------------------------------------------------------------------------
C_BaseEntity::C_BaseEntity()
{
	m_bEnableRenderingClipPlane = false;

	m_nRenderFXBlend = 255;

	//SetPredictionEligible( false );


	//GetEngineObject()->Init(this);
#ifdef _DEBUG
	m_vecViewOffset.Init();
#endif

	m_fBBoxVisFlags = 0;
	m_iEyeAttachment = 0;
	m_nEventSequence = -1;
	m_nPrevResetEventsParity = -1;
//#if !defined( NO_ENTITY_PREDICTION )
//	m_pPredictionContext = NULL;
//#endif
	//NOTE: not virtual! we are in the constructor!
	C_BaseEntity::Clear();

#ifdef TF_CLIENT_DLL
	m_bValidatedOwner = false;
	m_bDeemedInvalid = false;
	m_bWasDeemedInvalid = false;
#endif

	ParticleProp()->Init( this );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
C_BaseEntity::~C_BaseEntity()
{
	TermRopes();
}

IEngineObjectClient* C_BaseEntity::GetEngineObject() {
	return EntityList()->GetEngineObject(entindex());
}

const IEngineObjectClient* C_BaseEntity::GetEngineObject() const {
	return EntityList()->GetEngineObject(entindex());
}

IEngineWorldClient* C_BaseEntity::GetEngineWorld() {
	return GetEngineObject()->AsEngineWorld();
}

const IEngineWorldClient* C_BaseEntity::GetEngineWorld() const {
	return GetEngineObject()->AsEngineWorld();
}

IEnginePlayerClient* C_BaseEntity::GetEnginePlayer() {
	return GetEngineObject()->AsEnginePlayer();
}

const IEnginePlayerClient* C_BaseEntity::GetEnginePlayer() const {
	return GetEngineObject()->AsEnginePlayer();
}

IEnginePortalClient* C_BaseEntity::GetEnginePortal()
{
	return GetEngineObject()->AsEnginePortal();
}

const IEnginePortalClient* C_BaseEntity::GetEnginePortal() const
{
	return GetEngineObject()->AsEnginePortal();
}

IEngineVehicleClient* C_BaseEntity::GetEngineVehicle()
{
	return GetEngineObject()->AsEngineVehicle();
}

const IEngineVehicleClient* C_BaseEntity::GetEngineVehicle() const
{
	return GetEngineObject()->AsEngineVehicle();
}

IEngineRopeClient* C_BaseEntity::GetEngineRope()
{
	return GetEngineObject()->AsEngineRope();
}

const IEngineRopeClient* C_BaseEntity::GetEngineRope() const
{
	return GetEngineObject()->AsEngineRope();
}

IEngineGhostClient* C_BaseEntity::GetEngineGhost()
{
	return GetEngineObject()->AsEngineGhost();
}

const IEngineGhostClient* C_BaseEntity::GetEngineGhost() const
{
	return GetEngineObject()->AsEngineGhost();
}

void C_BaseEntity::Clear( void )
{
	//m_RefEHandle.Term();

	//index = -1;
	if (entindex() >= 0) {
		GetEngineObject()->SetLocalOrigin(vec3_origin);
		GetEngineObject()->SetLocalAngles(vec3_angle);
		GetEngineObject()->Clear();
	}
	m_vecViewOffset.Init();

	m_ShadowDirUseOtherEntity = NULL;


#if defined(SIXENSE)
	m_vecEyeOffset.Init();
	m_EyeAngleOffset.Init();
#endif

	// Remove prediction context if it exists
//#if !defined( NO_ENTITY_PREDICTION )
//	delete m_pPredictionContext;
//	m_pPredictionContext = NULL;
//#endif
	// Do not enable this on all entities. It forces bone setup for entities that
	// don't need it.
	//AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
	if (entindex() >= 0) {
		UpdateVisibility();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::Spawn( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::Activate()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::SpawnClientEntity( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Attach to entity
// Input  : *pEnt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::Init( int entnum, int iSerialNum )
{
	Assert( entnum >= 0 && entnum < NUM_ENT_ENTRIES );

	//index = entnum;

	if (entnum >= 0) {
		//EntityList()->AddNetworkableEntity(this, entnum, iSerialNum);//GetIClientUnknown()
	}

	return true;
}

void C_BaseEntity::AfterInit() {
	GetEngineObject()->Interp_SetupMappings();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseEntity::InitializeAsClientEntity( const char *pszModelName, RenderGroup_t renderGroup )
{
	int nModelIndex;

	if ( pszModelName != NULL )
	{
		nModelIndex = modelinfo->GetModelIndex( pszModelName );
		
		if ( nModelIndex == -1 )
		{
			// Model could not be found
			Assert( !"Model could not be found, index is -1" );
			return false;
		}
	}
	else
	{
		nModelIndex = -1;
	}

	GetEngineObject()->Interp_SetupMappings();

	return InitializeAsClientEntityByIndex( nModelIndex, renderGroup );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseEntity::InitializeAsClientEntityByIndex( int iIndex, RenderGroup_t renderGroup )
{
	//index = -1;

	// Setup model data.
	SetModelByIndex( iIndex );

	// Add the client entity to the master entity list.
	//EntityList()->AddNonNetworkableEntity( this );//GetIClientUnknown()
	Assert(GetRefEHandle() != EntityList()->InvalidHandle() );

	// Add the client entity to the renderable "leaf system." (Renderable)
	GetEngineObject()->AddToLeafSystem( renderGroup );

	// Add the client entity to the spatial partition. (Collidable)
	//GetEngineObject()->CreatePartitionHandle();

	SpawnClientEntity();

	return true;
}

//void C_BaseEntity::SetRefEHandle( const CBaseHandle &handle )
//{
//	m_RefEHandle = handle;
//}


//const CBaseHandle& C_BaseEntity::GetRefEHandle() const
//{
//	return m_RefEHandle;
//}

//-----------------------------------------------------------------------------
// Purpose: Free beams and destroy object
//-----------------------------------------------------------------------------
void C_BaseEntity::Release()
{
	EntityList()->DestroyEntity(this);
}

void C_BaseEntity::SetRemovalFlag( bool bRemove ) 
{ 
	if (bRemove) 
		GetEngineObject()->AddEFlags(EFL_KILLME);
	else 
		GetEngineObject()->RemoveEFlags(EFL_KILLME);
}


bool C_BaseEntity::VPhysicsIsFlesh( void )
{
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = GetEngineObject()->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
	for ( int i = 0; i < count; i++ )
	{
		int material = pList[i]->GetMaterialIndex();
		const surfacedata_t *pSurfaceData = EntityList()->PhysGetProps()->GetSurfaceData( material );
		// Is flesh ?, don't allow pickup
		if ( pSurfaceData->game.material == CHAR_TEX_ANTLION || pSurfaceData->game.material == CHAR_TEX_FLESH || pSurfaceData->game.material == CHAR_TEX_BLOODYFLESH || pSurfaceData->game.material == CHAR_TEX_ALIENFLESH )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Returns the health fraction
//-----------------------------------------------------------------------------
float C_BaseEntity::HealthFraction() const
{
	if (GetMaxHealth() == 0)
		return 1.0f;

	float flFraction = (float)GetHealth() / (float)GetMaxHealth();
	flFraction = clamp( flFraction, 0.0f, 1.0f );
	return flFraction;
}




void C_BaseEntity::UpdateVisibility()
{
#ifdef TF_CLIENT_DLL
	// TF prevents drawing of any entity attached to players that aren't items in the inventory of the player.
	// This is to prevent servers creating fake cosmetic items and attaching them to players.
	if ( !engine->IsPlayingDemo() )
	{
		static bool bIsStaging = ( engine->GetAppID() == 810 );
		if ( !m_bValidatedOwner )
		{
			bool bRetry = false;

			// Check it the first time we call update visibility (Source TV doesn't bother doing validation)
			m_bDeemedInvalid = engine->IsHLTV() ? false : !ValidateEntityAttachedToPlayer( bRetry );
			m_bValidatedOwner = !bRetry;
		}

		if ( m_bDeemedInvalid )
		{
			if ( bIsStaging )
			{
				if ( !m_bWasDeemedInvalid )
				{
					m_PreviousRenderMode = GetEngineObject()->GetRenderMode();
					m_PreviousRenderColor = GetEngineObject()->GetRenderColor();
					m_bWasDeemedInvalid = true;
				}

				GetEngineObject()->SetRenderMode( kRenderTransColor );
				GetEngineObject()->SetRenderColor( 255, 0, 0, 200 );

			}
			else
			{
				GetEngineObject()->RemoveFromLeafSystem();
				return;
			}
		}
		else if ( m_bWasDeemedInvalid )
		{
			if ( bIsStaging )
			{
				// We need to fix up the rendering.
				GetEngineObject()->SetRenderMode( m_PreviousRenderMode );
				GetEngineObject()->SetRenderColor( m_PreviousRenderColor.r, m_PreviousRenderColor.g, m_PreviousRenderColor.b, m_PreviousRenderColor.a );
			}

			m_bWasDeemedInvalid = false;
		}
	}
#endif

	if ( ShouldDraw() && !GetEngineObject()->IsDormant() && ( !ToolsEnabled() || GetEngineObject()->IsEnabledInToolView() ) )
	{
		// add/update leafsystem
		GetEngineObject()->AddToLeafSystem();
	}
	else
	{
		// remove from leaf system
		GetEngineObject()->RemoveFromLeafSystem();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether object should render.
//-----------------------------------------------------------------------------
bool C_BaseEntity::ShouldDraw()
{
// Only test this in tf2
#if defined( INVASION_CLIENT_DLL )
	// Let the client mode (like commander mode) reject drawing entities.
	if (g_pGameRules && !g_pGameRules->ShouldDrawEntity(this) )
		return false;
#endif

	// Some rendermodes prevent rendering
	if (GetEngineObject()->GetRenderMode() == kRenderNone)
		return false;

	return (GetEngineObject()->GetModel() != 0) && !GetEngineObject()->IsEffectActive(EF_NODRAW) && (entindex() != 0);
}

bool C_BaseEntity::TestCollision( const Ray_t& ray, unsigned int mask, trace_t& trace )
{
	if (IsBaseAnimating()) {
		MDLCACHE_CRITICAL_SECTION();
		if (ray.m_IsRay && GetEngineObject()->IsSolidFlagSet(FSOLID_CUSTOMRAYTEST))
		{
			if (!TestHitboxes(ray, mask, trace))
				return true;

			return trace.DidHit();
		}

		if (!ray.m_IsRay && GetEngineObject()->IsSolidFlagSet(FSOLID_CUSTOMBOXTEST))
		{
			if (!TestHitboxes(ray, mask, trace))
				return true;

			return true;
		}

		// We shouldn't get here.
		Assert(0);
		return false;
	}
	else {
		return false;
	}
}

bool C_BaseEntity::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	if (IsBaseAnimating()) {
		IStudioHdr* pStudioHdr = GetEngineObject()->GetModelPtr();
		if (!pStudioHdr)
			return false;

		mstudiohitboxset_t* set = pStudioHdr->pHitboxSet(GetEngineObject()->GetHitboxSet());
		if (!set || !set->numhitboxes)
			return false;

		// Use vcollide for box traces.
		if (!ray.m_IsRay)
			return false;

		// This *has* to be true for the existing code to function correctly.
		Assert(ray.m_StartOffset == vec3_origin);

		const matrix3x4_t* hitboxbones[MAXSTUDIOBONES];
		GetEngineObject()->GetHitboxBoneTransforms(hitboxbones);

		if (TraceToStudio(EntityList()->PhysGetProps(), ray, pStudioHdr, set, hitboxbones, fContentsMask, GetRenderOrigin(), GetEngineObject()->GetModelScale(), tr))
		{
			mstudiobbox_t* pbox = set->pHitbox(tr.hitbox);
			mstudiobone_t* pBone = pStudioHdr->pBone(pbox->bone);
			tr.surface.name = "**studio**";
			tr.surface.flags = SURF_HITBOX;
			tr.surface.surfaceProps = EntityList()->PhysGetProps()->GetSurfaceIndex(pBone->pszSurfaceProp());
			if (GetEngineObject()->IsRagdoll())
			{
				IPhysicsObject* pReplace = GetEngineObject()->GetElement(tr.physicsbone);
				if (pReplace)
				{
					GetEngineObject()->VPhysicsSetObject(NULL);
					GetEngineObject()->VPhysicsSetObject(pReplace);
				}
			}
		}

		return true;
	}
	else {
		return false;
	}
}

bool NPC_IsImportantNPC(C_BaseEntity* pAnimating)
{
	C_AI_BaseNPC* pBaseNPC = dynamic_cast <C_AI_BaseNPC*> (pAnimating);

	if (pBaseNPC == NULL)
		return false;

	return pBaseNPC->ImportantRagdoll();
}

C_BaseEntity* C_BaseEntity::BecomeRagdollOnClient()
{
	if (!IsBaseAnimating()) {
		return NULL;
	}

	GetEngineObject()->MoveToLastReceivedPosition(true);
	GetEngineObject()->GetAbsOrigin();

	C_ClientRagdoll* pRagdoll = CreateRagdollCopy();
	if (!pRagdoll)
	{
		return NULL;
	}

	TermRopes();
	const model_t* model = GetEngineObject()->GetModel();
	const char* pModelName = modelinfo->GetModelName(model);

	if (pRagdoll->InitializeAsClientEntity(pModelName, RENDER_GROUP_OPAQUE_ENTITY) == false)
	{
		EntityList()->DestroyEntity(pRagdoll);// ->Release();
		return NULL;
	}

	// move my current model instance to the ragdoll's so decals are preserved.
	GetEngineObject()->SnatchModelInstance(pRagdoll->GetEngineObject());

	// We need to take these from the entity
	pRagdoll->GetEngineObject()->SetAbsOrigin(GetEngineObject()->GetAbsOrigin());
	pRagdoll->GetEngineObject()->SetAbsAngles(GetEngineObject()->GetAbsAngles());

	pRagdoll->IgniteRagdoll(this);
	pRagdoll->TransferDissolveFrom(this);
	pRagdoll->InitModelEffects();

	if (GetEngineObject()->IsEffectActive(EF_NOSHADOW))
	{
		pRagdoll->GetEngineObject()->AddEffects(EF_NOSHADOW);
	}
	pRagdoll->GetEngineObject()->SetRenderFX(kRenderFxRagdoll);
	pRagdoll->GetEngineObject()->SetRenderMode(GetEngineObject()->GetRenderMode());
	pRagdoll->GetEngineObject()->SetRenderColor(GetEngineObject()->GetRenderColor().r, GetEngineObject()->GetRenderColor().g, GetEngineObject()->GetRenderColor().b, GetEngineObject()->GetRenderColor().a);

	pRagdoll->GetEngineObject()->SetBody(GetEngineObject()->GetBody());
	pRagdoll->GetEngineObject()->SetSkin(GetEngineObject()->GetSkin());
	pRagdoll->GetEngineObject()->SetVecForce(GetEngineObject()->GetVecForce());
	pRagdoll->GetEngineObject()->SetForceBone(GetEngineObject()->GetForceBone());
	pRagdoll->GetEngineObject()->SetNextClientThink(CLIENT_THINK_ALWAYS);

	pRagdoll->GetEngineObject()->SetModelName(AllocPooledString(pModelName));
	pRagdoll->GetEngineObject()->SetModelScale(GetEngineObject()->GetModelScale());
	matrix3x4_t boneDelta0[MAXSTUDIOBONES];
	matrix3x4_t boneDelta1[MAXSTUDIOBONES];
	matrix3x4_t currentBones[MAXSTUDIOBONES];
	const float boneDt = 0.1f;
	GetRagdollInitBoneArrays(boneDelta0, boneDelta1, currentBones, boneDt);
	pRagdoll->GetEngineObject()->InitAsClientRagdoll(boneDelta0, boneDelta1, currentBones, boneDt);
	NoteRagdollCreationTick(this);
	if (AddRagdollToFadeQueue() == true)
	{
		pRagdoll->m_bImportant = NPC_IsImportantNPC(this);
		EntityList()->MoveToTopOfLRU(pRagdoll, pRagdoll->m_bImportant);
		pRagdoll->m_bFadeOut = true;
	}

	GetEngineObject()->AddEffects(EF_NODRAW);
	GetEngineObject()->SetBuiltRagdoll(true);
	return pRagdoll;
}

C_ClientRagdoll* C_BaseEntity::CreateRagdollCopy()
{
	//Adrian: We now create a separate entity that becomes this entity's ragdoll.
	//That way the server side version of this entity can go away. 
	//Plus we can hook save/restore code to these ragdolls so they don't fall on restore anymore.
	C_ClientRagdoll* pRagdoll = (C_ClientRagdoll*)EntityList()->CreateEntityByName("C_ClientRagdoll");//false
	return pRagdoll;
}

void C_BaseEntity::ForceSetupBonesAtTime(matrix3x4_t* pBonesOut, float flTime)
{
	// blow the cached prev bones
	GetEngineObject()->InvalidateBoneCache();

	// reset root position to flTime
	Interpolate(NULL, flTime);

	// Setup bone state at the given time
	GetEngineObject()->SetupBones(pBonesOut, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, flTime);
}

void C_BaseEntity::GetRagdollInitBoneArrays(matrix3x4_t* pDeltaBones0, matrix3x4_t* pDeltaBones1, matrix3x4_t* pCurrentBones, float boneDt)
{
	ForceSetupBonesAtTime(pDeltaBones0, gpGlobals->curtime - boneDt);
	ForceSetupBonesAtTime(pDeltaBones1, gpGlobals->curtime);
	float ragdollCreateTime = EntityList()->PhysGetSyncCreateTime();
	if (ragdollCreateTime != gpGlobals->curtime)
	{
		// The next simulation frame begins before the end of this frame
		// so initialize the ragdoll at that time so that it will reach the current
		// position at curtime.  Otherwise the ragdoll will simulate forward from curtime
		// and pop into the future a bit at this point of transition
		ForceSetupBonesAtTime(pCurrentBones, ragdollCreateTime);
	}
	else
	{
		memcpy(pCurrentBones, GetEngineObject()->GetBoneArray(), sizeof(matrix3x4_t) * GetEngineObject()->GetBoneCount());
	}
}

//=========================================================
// StudioFrameAdvance - advance the animation frame up some interval (default 0.1) into the future
//=========================================================
void C_BaseEntity::StudioFrameAdvance()
{
	if (GetEngineObject()->IsUsingClientSideAnimation())
		return;

	IStudioHdr* hdr = GetEngineObject()->GetModelPtr();
	if (!hdr)
		return;

#ifdef DEBUG
	bool watch = dbganimmodel.GetString()[0] && V_stristr(hdr->pszName(), dbganimmodel.GetString());
#else
	bool watch = false; // Q_strstr( hdr->name, "rifle" ) ? true : false;
#endif

	//if (!anim.prevanimtime)
	//{
		//anim.prevanimtime = m_flAnimTime = gpGlobals->curtime;
	//}

	// How long since last animtime
	float flInterval = GetAnimTimeInterval();

	if (flInterval <= 0.001)
	{
		// Msg("%s : %s : %5.3f (skip)\n", STRING(pev->classname), GetSequenceName( GetSequence() ), GetCycle() );
		return;
	}

	GetEngineObject()->UpdateModelScale();

	//anim.prevanimtime = m_flAnimTime;
	float cycleAdvance = flInterval * GetEngineObject()->GetSequenceCycleRate(hdr, GetEngineObject()->GetSequence()) * GetEngineObject()->GetPlaybackRate();
	float flNewCycle = GetEngineObject()->GetCycle() + cycleAdvance;
	GetEngineObject()->SetAnimTime(gpGlobals->curtime);

	if (watch)
	{
		Msg("%s %6.3f : %6.3f (%.3f)\n", GetClassname(), gpGlobals->curtime, GetEngineObject()->GetAnimTime(), flInterval);
	}

	if (flNewCycle < 0.0f || flNewCycle >= 1.0f)
	{
		if (GetEngineObject()->IsSequenceLooping(hdr, GetEngineObject()->GetSequence()))
		{
			flNewCycle -= (int)(flNewCycle);
		}
		else
		{
			flNewCycle = (flNewCycle < 0.0f) ? 0.0f : 1.0f;
		}

		GetEngineObject()->SetSequenceFinished(true);	// just in case it wasn't caught in GetEvents
	}

	GetEngineObject()->SetCycle(flNewCycle);

	GetEngineObject()->SetGroundSpeed(GetEngineObject()->GetSequenceGroundSpeed(hdr, GetEngineObject()->GetSequence()) * GetEngineObject()->GetModelScale());

#if 0
	// I didn't have a test case for this, but it seems like the right thing to do.  Check multi-player!

	// Msg("%s : %s : %5.1f\n", GetClassname(), GetSequenceName( GetSequence() ), GetCycle() );
	GetEngineObject()->InvalidatePhysicsRecursive(ANIMATION_CHANGED);
#endif

	if (watch)
	{
		Msg("%s : %s : %5.1f\n", GetClassname(), GetEngineObject()->GetSequenceName(GetEngineObject()->GetSequence()), GetEngineObject()->GetCycle());
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flInterval - 
// Output : float
//-----------------------------------------------------------------------------
float C_BaseEntity::FrameAdvance(float flInterval)
{
	IStudioHdr* hdr = GetEngineObject()->GetModelPtr();
	if (!hdr)
		return 0.0f;

#ifdef DEBUG
	bool bWatch = dbganimmodel.GetString()[0] && V_stristr(hdr->pszName(), dbganimmodel.GetString());
#else
	bool bWatch = false; // Q_strstr( hdr->name, "medkit_large" ) ? true : false;
#endif

	float curtime = gpGlobals->curtime;

	if (flInterval == 0.0f)
	{
		flInterval = (curtime - GetEngineObject()->GetAnimTime());
		if (flInterval <= 0.001f)
		{
			return 0.0f;
		}
	}

	if (!GetEngineObject()->GetAnimTime())
	{
		flInterval = 0.0f;
	}

	float cyclerate = GetEngineObject()->GetSequenceCycleRate(hdr, GetEngineObject()->GetSequence());
	float addcycle = flInterval * cyclerate * GetEngineObject()->GetPlaybackRate();

	if (GetServerIntendedCycle() != -1.0f)
	{
		// The server would like us to ease in a correction so that we will animate the same on the client and server.
		// So we will actually advance the average of what we would have done and what the server wants.
		float serverCycle = GetServerIntendedCycle();
		float serverAdvance = serverCycle - GetEngineObject()->GetCycle();
		bool adjustOkay = serverAdvance > 0.0f;// only want to go forward. backing up looks really jarring, even when slight
		if (serverAdvance < -0.8f)
		{
			// Oh wait, it was just a wraparound from .9 to .1.
			serverAdvance += 1;
			adjustOkay = true;
		}

		if (adjustOkay)
		{
			float originalAdvance = addcycle;
			addcycle = (serverAdvance + addcycle) / 2;

			const float MAX_CYCLE_ADJUSTMENT = 0.1f;
			addcycle = MIN(MAX_CYCLE_ADJUSTMENT, addcycle);// Don't do too big of a jump; it's too jarring as well.

			DevMsg(2, "(%d): Cycle latch used to correct %.2f in to %.2f instead of %.2f.\n",
				entindex(), GetEngineObject()->GetCycle(), GetEngineObject()->GetCycle() + addcycle, GetEngineObject()->GetCycle() + originalAdvance);
		}

		SetServerIntendedCycle(-1.0f); // Only use a correction once, it isn't valid any time but right now.
	}

	float flNewCycle = GetEngineObject()->GetCycle() + addcycle;
	GetEngineObject()->SetAnimTime(curtime);

	if (bWatch)
	{
		Msg("%i CLIENT Time: %6.3f : (Interval %f) : cycle %f rate %f add %f\n",
			gpGlobals->tickcount, gpGlobals->curtime, flInterval, flNewCycle, cyclerate, addcycle);
	}

	if ((flNewCycle < 0.0f) || (flNewCycle >= 1.0f))
	{
		if (GetEngineObject()->IsSequenceLooping(hdr, GetEngineObject()->GetSequence()))
		{
			flNewCycle -= (int)(flNewCycle);
		}
		else
		{
			flNewCycle = (flNewCycle < 0.0f) ? 0.0f : 1.0f;
		}
		GetEngineObject()->SetSequenceFinished(true);
	}

	GetEngineObject()->SetCycle(flNewCycle);

	return flInterval;
}

unsigned int C_BaseEntity::ComputeClientSideAnimationFlags()
{
	return FCLIENTANIM_SEQUENCE_CYCLE;
}

void C_BaseEntity::UpdateClientSideAnimation()
{
	if (!IsBaseAnimating()) {
		return;
	}
	// Update client side animation
	if (GetEngineObject()->IsUsingClientSideAnimation())
	{
		//Assert( m_ClientSideAnimationListHandle != INVALID_CLIENTSIDEANIMATION_LIST_HANDLE );
		if (GetEngineObject()->GetSequence() != -1)
		{
			// latch old values
			GetEngineObject()->OnLatchInterpolatedVariables(LATCH_ANIMATION_VAR);
			// move frame forward
			FrameAdvance(0.0f); // 0 means to use the time we last advanced instead of a constant
		}
	}
	else
	{
		//Assert( m_ClientSideAnimationListHandle == INVALID_CLIENTSIDEANIMATION_LIST_HANDLE );
	}
}

extern ConVar muzzleflash_light;

void C_BaseEntity::ProcessMuzzleFlashEvent()
{
	// If we have an attachment, then stick a light on it.
	if (muzzleflash_light.GetBool())
	{
		//FIXME: We should really use a named attachment for this
		if (GetEngineObject()->GetAttachmentCount() > 0)
		{
			Vector vAttachment;
			QAngle dummyAngles;
			GetEngineObject()->GetAttachment(1, vAttachment, dummyAngles);

			// Make an elight
			dlight_t* el = effects->CL_AllocElight(LIGHT_INDEX_MUZZLEFLASH + entindex());
			el->origin = vAttachment;
			el->radius = random->RandomInt(32, 64);
			el->decay = el->radius / 0.05f;
			el->die = gpGlobals->curtime + 0.05f;
			el->color.r = 255;
			el->color.g = 192;
			el->color.b = 64;
			el->color.exponent = 5;
		}
	}
}

void C_BaseEntity::InitModelEffects(void)
{
	m_bInitModelEffects = true;
	TermRopes();
}

void C_BaseEntity::TermRopes()
{
	FOR_EACH_LL(m_Ropes, i)
		EntityList()->DestroyEntity(m_Ropes[i]);// ->Release();

	m_Ropes.Purge();
}

bool C_BaseEntity::IsMenuModel() const
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Load the model's keyvalues section and create effects listed inside it
//-----------------------------------------------------------------------------
void C_BaseEntity::DelayedInitModelEffects(void)
{
	m_bInitModelEffects = false;

	// Parse the keyvalues and see if they want to make ropes on this model.
	KeyValues* modelKeyValues = new KeyValues("");
	if (modelKeyValues->LoadFromBuffer(modelinfo->GetModelName(GetEngineObject()->GetModel()), modelinfo->GetModelKeyValueText(GetEngineObject()->GetModel())))
	{
		// Do we have a cables section?
		KeyValues* pkvAllCables = modelKeyValues->FindKey("Cables");
		if (pkvAllCables)
		{
			// Start grabbing the sounds and slotting them in
			for (KeyValues* pSingleCable = pkvAllCables->GetFirstSubKey(); pSingleCable; pSingleCable = pSingleCable->GetNextKey())
			{
				C_RopeKeyframe* pRope = C_RopeKeyframe::CreateFromKeyValues(this, pSingleCable);
				m_Ropes.AddToTail(pRope);
			}
		}

		if (!m_bNoModelParticles)
		{
			// Do we have a particles section?
			KeyValues* pkvAllParticleEffects = modelKeyValues->FindKey("Particles");
			if (pkvAllParticleEffects)
			{
				// Start grabbing the sounds and slotting them in
				for (KeyValues* pSingleEffect = pkvAllParticleEffects->GetFirstSubKey(); pSingleEffect; pSingleEffect = pSingleEffect->GetNextKey())
				{
					const char* pszParticleEffect = pSingleEffect->GetString("name", "");
					const char* pszAttachment = pSingleEffect->GetString("attachment_point", "");
					const char* pszAttachType = pSingleEffect->GetString("attachment_type", "");

					// Convert attach type
					int iAttachType = GetAttachTypeFromString(pszAttachType);
					if (iAttachType == -1)
					{
						Warning("Invalid attach type specified for particle effect in model '%s' keyvalues section. Trying to spawn effect '%s' with attach type of '%s'\n", GetEngineObject()->GetModelName(), pszParticleEffect, pszAttachType);
						return;
					}

					// Convert attachment point
					int iAttachment = atoi(pszAttachment);
					// See if we can find any attachment points matching the name
					if (pszAttachment[0] != '0' && iAttachment == 0)
					{
						iAttachment = GetEngineObject()->LookupAttachment(pszAttachment);
						if (iAttachment <= 0)
						{
							Warning("Failed to find attachment point specified for particle effect in model '%s' keyvalues section. Trying to spawn effect '%s' on attachment named '%s'\n", GetEngineObject()->GetModelName(), pszParticleEffect, pszAttachment);
							return;
						}
					}
#ifdef TF_CLIENT_DLL
					// Halloween Hack for Sentry Rockets
					if (!V_strcmp("sentry_rocket", pszParticleEffect))
					{
						// Halloween Spell Effect Check
						int iHalloweenSpell = 0;
						// if the owner is a Sentry, Check its owner
						CBaseObject* pSentry = dynamic_cast<CBaseObject*>(GetOwnerEntity());
						if (pSentry)
						{
							CALL_ATTRIB_HOOK_INT_ON_OTHER(pSentry->GetOwner(), iHalloweenSpell, halloween_pumpkin_explosions);
						}
						else
						{
							CALL_ATTRIB_HOOK_INT_ON_OTHER(GetOwnerEntity(), iHalloweenSpell, halloween_pumpkin_explosions);
						}

						if (iHalloweenSpell > 0)
						{
							pszParticleEffect = "halloween_rockettrail";
						}
					}
#endif
					// Spawn the particle effect
					ParticleProp()->Create(pszParticleEffect, (ParticleAttachment_t)iAttachType, iAttachment);
				}
			}
		}
	}

	modelKeyValues->deleteThis();
}

//-----------------------------------------------------------------------------
// Used when the collision prop is told to ask game code for the world-space surrounding box
//-----------------------------------------------------------------------------
void C_BaseEntity::ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	// This should only be called if you're using USE_GAME_CODE on the server
	// and you forgot to implement the client-side version of this method.
	Assert(0);
}

void C_BaseEntity::RefreshCollisionBounds(void)
{
	GetEngineObject()->RefreshScaledCollisionBounds();
}

//-----------------------------------------------------------------------------
// Purpose: Derived classes will have to write their own message cracking routines!!!
// Input  : length - 
//			*data - 
//-----------------------------------------------------------------------------
void C_BaseEntity::ReceiveMessage( int classID, bf_read &msg )
{
	// BaseEntity doesn't have a base class we could relay this message to
	Assert( classID == GetClientClass()->m_ClassID );
	
	int messageType = msg.ReadByte();
	switch( messageType )
	{
		case BASEENTITY_MSG_REMOVE_DECALS:	RemoveAllDecals();
											break;
	}
}


//void* C_BaseEntity::GetDataTableBasePtr()
//{
//	return this;
//}


//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t C_BaseEntity::ShadowCastType()
{
	if (IsBaseAnimating()) {
		IStudioHdr* pStudioHdr = GetEngineObject()->GetModelPtr();
		if (!pStudioHdr || !pStudioHdr->SequencesAvailable())
			return SHADOWS_NONE;

		if (GetEngineObject()->IsEffectActive(EF_NODRAW | EF_NOSHADOW))
			return SHADOWS_NONE;

		if (pStudioHdr->GetNumSeq() == 0)
			return SHADOWS_RENDER_TO_TEXTURE;

		if (!GetEngineObject()->IsRagdoll())
		{
			// If we have pose parameters, always update
			if (pStudioHdr->GetNumPoseParameters() > 0)
				return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;

			// If we have bone controllers, always update
			if (pStudioHdr->numbonecontrollers() > 0)
				return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;

			// If we use IK, always update
			if (pStudioHdr->numikchains() > 0)
				return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;
		}

		// FIXME: Do something to check to see how many frames the current animation has
		// If we do this, we have to be able to handle the case of changing ShadowCastTypes
		// at the moment, they are assumed to be constant.
		return SHADOWS_RENDER_TO_TEXTURE;
	}
	else {
		if (GetEngineObject()->IsEffectActive(EF_NODRAW | EF_NOSHADOW))
			return SHADOWS_NONE;

		int modelType = modelinfo->GetModelType(GetEngineObject()->GetModel());
		return (modelType == mod_studio) ? SHADOWS_RENDER_TO_TEXTURE : SHADOWS_NONE;
	}
}


//-----------------------------------------------------------------------------
// Per-entity shadow cast distance + direction
//-----------------------------------------------------------------------------
bool C_BaseEntity::GetShadowCastDistance( float *pDistance, ShadowType_t shadowType ) const			
{ 
	if ( m_flShadowCastDistance != 0.0f )
	{
		*pDistance = m_flShadowCastDistance; 
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseEntity *C_BaseEntity::GetShadowUseOtherEntity( void ) const
{
	return m_ShadowDirUseOtherEntity;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetShadowUseOtherEntity( C_BaseEntity *pEntity )
{
	m_ShadowDirUseOtherEntity = pEntity;
}



//-----------------------------------------------------------------------------
// Purpose: Return a per-entity shadow cast direction
//-----------------------------------------------------------------------------
bool C_BaseEntity::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const			
{ 
	if ( m_ShadowDirUseOtherEntity )
		return m_ShadowDirUseOtherEntity->GetShadowCastDirection( pDirection, shadowType );

	return false;
}


//-----------------------------------------------------------------------------
// Should this object receive shadows?
//-----------------------------------------------------------------------------
bool C_BaseEntity::ShouldReceiveProjectedTextures( int flags )
{
	Assert( flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );

	if (GetEngineObject()->IsEffectActive( EF_NODRAW ) )
		 return false;

	if( flags & SHADOW_FLAGS_FLASHLIGHT )
	{
		if (GetEngineObject()->GetRenderMode() > kRenderNormal && GetEngineObject()->GetRenderColor().a == 0 )
			 return false;

		return true;
	}

	Assert( flags & SHADOW_FLAGS_SHADOW );

	if (GetEngineObject()->IsEffectActive( EF_NORECEIVESHADOW ) )
		 return false;

	if (modelinfo->GetModelType(GetEngineObject()->GetModel()) == mod_studio)
		return false;

	return true;
}
	
//-----------------------------------------------------------------------------
// Purpose: Returns index into entities list for this entity
// Output : Index
//-----------------------------------------------------------------------------
//int	C_BaseEntity::entindex( void ) const
//{
//	return index;
//}

int C_BaseEntity::GetSoundSourceIndex() const
{
#ifdef _DEBUG
	if ( entindex() != -1 )
	{
		Assert(entindex() == GetRefEHandle().GetEntryIndex() );
	}
#endif
	return GetRefEHandle().GetEntryIndex();
}

//-----------------------------------------------------------------------------
// Get render origin and angles
//-----------------------------------------------------------------------------
const Vector& C_BaseEntity::GetRenderOrigin( void )
{
	if (IsBaseAnimating()) {
		if (GetEngineObject()->IsRagdoll())
		{
			return GetEngineObject()->GetRagdollOrigin();
		}
		else
		{
			return GetEngineObject()->GetAbsOrigin();
		}
	}
	return GetEngineObject()->GetAbsOrigin();
}

const QAngle& C_BaseEntity::GetRenderAngles( void )
{
	if (IsBaseAnimating()) {
		if (GetEngineObject()->IsRagdoll())
		{
			return vec3_angle;

		}
		else
		{
			return GetEngineObject()->GetAbsAngles();
		}
	}
	return GetEngineObject()->GetAbsAngles();
}



IPVSNotify* C_BaseEntity::GetPVSNotifyInterface()
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : theMins - 
//			theMaxs - 
//-----------------------------------------------------------------------------
void C_BaseEntity::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if (IsBaseAnimating()) {
		if (GetEngineObject()->IsRagdoll())
		{
			GetEngineObject()->GetRagdollBounds(theMins, theMaxs);
		}
		else if (GetEngineObject()->GetModel())
		{
			IStudioHdr* pStudioHdr = GetEngineObject()->GetModelPtr();
			if (!pStudioHdr || !pStudioHdr->SequencesAvailable() || GetEngineObject()->GetSequence() == -1)
			{
				theMins = vec3_origin;
				theMaxs = vec3_origin;
				return;
			}
			if (!VectorCompare(vec3_origin, pStudioHdr->view_bbmin()) || !VectorCompare(vec3_origin, pStudioHdr->view_bbmax()))
			{
				// clipping bounding box
				VectorCopy(pStudioHdr->view_bbmin(), theMins);
				VectorCopy(pStudioHdr->view_bbmax(), theMaxs);
			}
			else
			{
				// movement bounding box
				VectorCopy(pStudioHdr->hull_min(), theMins);
				VectorCopy(pStudioHdr->hull_max(), theMaxs);
			}

			mstudioseqdesc_t& seqdesc = pStudioHdr->pSeqdesc(GetEngineObject()->GetSequence());
			VectorMin(seqdesc.bbmin, theMins, theMins);
			VectorMax(seqdesc.bbmax, theMaxs, theMaxs);
		}
		else
		{
			theMins = vec3_origin;
			theMaxs = vec3_origin;
		}

		// Scale this up depending on if our model is currently scaling
		const float flScale = GetEngineObject()->GetModelScale();
		theMaxs *= flScale;
		theMins *= flScale;
	}
	else {
		int nModelType = modelinfo->GetModelType(GetEngineObject()->GetModel());
		if (nModelType == mod_studio || nModelType == mod_brush)
		{
			modelinfo->GetModelRenderBounds(GetEngineObject()->GetModel(), theMins, theMaxs);
		}
		else
		{
			// By default, we'll just snack on the collision bounds, transform
			// them into entity-space, and call it a day.
			if (GetRenderAngles() == GetEngineObject()->GetCollisionAngles())
			{
				theMins = GetEngineObject()->OBBMins();
				theMaxs = GetEngineObject()->OBBMaxs();
			}
			else
			{
				Assert(GetEngineObject()->GetCollisionAngles() == vec3_angle);
				if (GetEngineObject()->IsPointSized())
				{
					//theMins = GetEngineObject()->GetCollisionOrigin();
					//theMaxs	= theMins;
					theMins = theMaxs = vec3_origin;
				}
				else
				{
					// NOTE: This shouldn't happen! Or at least, I haven't run
					// into a valid case where it should yet.
	//				Assert(0);
					IRotateAABB(GetEngineObject()->EntityToWorldTransform(), GetEngineObject()->OBBMins(), GetEngineObject()->OBBMaxs(), theMins, theMaxs);
				}
			}
		}
	}
}

// Figure out a world space bounding box that encloses the entity's local render bounds in world space.
inline void CalcRenderableWorldSpaceAABB(
	IClientRenderable* pRenderable,
	Vector& absMins,
	Vector& absMaxs)
{
	pRenderable->GetRenderBoundsWorldspace(absMins, absMaxs);
}

// This gets an AABB for the renderable, but it doesn't cause a parent's bones to be setup.
// This is used for placement in the leaves, but the more expensive version is used for culling.
void CalcRenderableWorldSpaceAABB_Fast(IClientRenderable* pRenderable, Vector& absMin, Vector& absMax)
{
	IClientEntity* pEnt = pRenderable->GetIClientUnknown()->GetBaseEntity();
	if (pEnt && pEnt->GetEngineObject()->IsFollowingEntity())
	{
		IEngineObjectClient* pParent = pEnt->GetEngineObject()->GetMoveParent();
		Assert(pParent);

		// Get the parent's abs space world bounds.
		CalcRenderableWorldSpaceAABB_Fast(pParent, absMin, absMax);

		// Add the maximum of our local render bounds. This is making the assumption that we can be at any
		// point and at any angle within the parent's world space bounds.
		Vector vAddMins, vAddMaxs;
		pEnt->GetRenderBounds(vAddMins, vAddMaxs);
		// if our origin is actually farther away than that, expand again
		float radius = pEnt->GetEngineObject()->GetLocalOrigin().Length();

		float flBloatSize = MAX(vAddMins.Length(), vAddMaxs.Length());
		flBloatSize = MAX(flBloatSize, radius);
		absMin -= Vector(flBloatSize, flBloatSize, flBloatSize);
		absMax += Vector(flBloatSize, flBloatSize, flBloatSize);
	}
	else
	{
		// Start out with our own render bounds. Since we don't have a parent, this won't incur any nasty 
		CalcRenderableWorldSpaceAABB(pRenderable, absMin, absMax);
	}
}

//-----------------------------------------------------------------------------
// Helper functions.
//-----------------------------------------------------------------------------
void DefaultRenderBoundsWorldspace(IClientRenderable* pRenderable, Vector& absMins, Vector& absMaxs)
{
	// Tracker 37433:  This fixes a bug where if the stunstick is being wielded by a combine soldier, the fact that the stick was
	//  attached to the soldier's hand would move it such that it would get frustum culled near the edge of the screen.
	IClientEntity* pEnt = pRenderable->GetIClientUnknown()->GetBaseEntity();
	if (pEnt && pEnt->GetEngineObject()->IsFollowingEntity())
	{
		IEngineObjectClient* pParent = pEnt->GetEngineObject()->GetFollowedEntity();
		if (pParent)
		{
			// Get the parent's abs space world bounds.
			CalcRenderableWorldSpaceAABB_Fast(pParent, absMins, absMaxs);

			// Add the maximum of our local render bounds. This is making the assumption that we can be at any
			// point and at any angle within the parent's world space bounds.
			Vector vAddMins, vAddMaxs;
			pEnt->GetRenderBounds(vAddMins, vAddMaxs);
			// if our origin is actually farther away than that, expand again
			float radius = pEnt->GetEngineObject()->GetLocalOrigin().Length();

			float flBloatSize = MAX(vAddMins.Length(), vAddMaxs.Length());
			flBloatSize = MAX(flBloatSize, radius);
			absMins -= Vector(flBloatSize, flBloatSize, flBloatSize);
			absMaxs += Vector(flBloatSize, flBloatSize, flBloatSize);
			return;
		}
	}

	Vector mins, maxs;
	pRenderable->GetRenderBounds(mins, maxs);

	// FIXME: Should I just use a sphere here?
	// Another option is to pass the OBB down the tree; makes for a better fit
	// Generate a world-aligned AABB
	const QAngle& angles = pRenderable->GetRenderAngles();
	const Vector& origin = pRenderable->GetRenderOrigin();
	if (angles == vec3_angle)
	{
		VectorAdd(mins, origin, absMins);
		VectorAdd(maxs, origin, absMaxs);
	}
	else
	{
		matrix3x4_t	boxToWorld;
		AngleMatrix(angles, origin, boxToWorld);
		TransformAABB(boxToWorld, mins, maxs, absMins, absMaxs);
	}
	Assert(absMins.IsValid() && absMaxs.IsValid());
}

void C_BaseEntity::GetRenderBoundsWorldspace( Vector& mins, Vector& maxs )
{
	DefaultRenderBoundsWorldspace( this->GetEngineObject(), mins, maxs );
}


void C_BaseEntity::GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType )
{
	GetEngineObject()->GetEntClientFlags() |= ENTCLIENTFLAG_GETTINGSHADOWRENDERBOUNDS;
	GetRenderBounds( mins, maxs );
	GetEngineObject()->GetEntClientFlags() &= ~ENTCLIENTFLAG_GETTINGSHADOWRENDERBOUNDS;
}

bool C_BaseEntity::IsTwoPass( void )
{
	return modelinfo->IsTranslucentTwoPass(GetEngineObject()->GetModel() );
}

bool C_BaseEntity::UsesPowerOfTwoFrameBufferTexture()
{
	if (IsBaseAnimating()) {
		return modelinfo->IsUsingFBTexture(GetEngineObject()->GetModel(), GetEngineObject()->GetSkin(), GetEngineObject()->GetBody(), GetClientRenderable());
	}
	return false;
}

bool C_BaseEntity::UsesFullFrameBufferTexture()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get pointer to CMouthInfo data
// Output : CMouthInfo
//-----------------------------------------------------------------------------
//CMouthInfo *C_BaseEntity::GetMouth( void )
//{
//	return NULL;
//}


//-----------------------------------------------------------------------------
// Purpose: Retrieve sound spatialization info for the specified sound on this entity
// Input  : info - 
// Output : Return false to indicate sound is not audible
//-----------------------------------------------------------------------------
bool C_BaseEntity::GetSoundSpatialization( SpatializationInfo_t& info )
{
	// World is always audible
	if ( entindex() == 0 )
	{
		return true;
	}

	if (IsBaseAnimating()) {
		{
			AutoAllowBoneAccess boneaccess(true, false);
			// Out of PVS
			if (GetEngineObject()->IsDormant())
			{
				return false;
			}

			// pModel might be NULL, but modelinfo can handle that
			const model_t* pModel = GetEngineObject()->GetModel();

			if (info.pflRadius)
			{
				*info.pflRadius = modelinfo->GetModelRadius(pModel);
			}

			if (info.pOrigin)
			{
				*info.pOrigin = GetEngineObject()->GetAbsOrigin();

				// move origin to middle of brush
				if (modelinfo->GetModelType(pModel) == mod_brush)
				{
					Vector mins, maxs, center;

					modelinfo->GetModelBounds(pModel, mins, maxs);
					VectorAdd(mins, maxs, center);
					VectorScale(center, 0.5f, center);

					(*info.pOrigin) += center;
				}
			}

			if (info.pAngles)
			{
				VectorCopy(GetEngineObject()->GetAbsAngles(), *info.pAngles);
			}

			//if (!BaseClass::GetSoundSpatialization(info))
			//	return false;
		}

		// move sound origin to center if npc has IK
		if (info.pOrigin && IsNPC() && GetEngineObject()->GetIk())
		{
			*info.pOrigin = GetEngineObject()->GetAbsOrigin();

			Vector mins, maxs, center;

			modelinfo->GetModelBounds(GetEngineObject()->GetModel(), mins, maxs);
			VectorAdd(mins, maxs, center);
			VectorScale(center, 0.5f, center);

			(*info.pOrigin) += center;
		}
		return true;
	}
	else {
		// Out of PVS
		if (GetEngineObject()->IsDormant())
		{
			return false;
		}

		// pModel might be NULL, but modelinfo can handle that
		const model_t* pModel = GetEngineObject()->GetModel();

		if (info.pflRadius)
		{
			*info.pflRadius = modelinfo->GetModelRadius(pModel);
		}

		if (info.pOrigin)
		{
			*info.pOrigin = GetEngineObject()->GetAbsOrigin();

			// move origin to middle of brush
			if (modelinfo->GetModelType(pModel) == mod_brush)
			{
				Vector mins, maxs, center;

				modelinfo->GetModelBounds(pModel, mins, maxs);
				VectorAdd(mins, maxs, center);
				VectorScale(center, 0.5f, center);

				(*info.pOrigin) += center;
			}
		}

		if (info.pAngles)
		{
			VectorCopy(GetEngineObject()->GetAbsAngles(), *info.pAngles);
		}

		return true;
	}
}

//-----------------------------------------------------------------------------
// Returns the attachment in local space
//-----------------------------------------------------------------------------
bool C_BaseEntity::GetAttachmentLocal(int iAttachment, matrix3x4_t& attachmentToLocal)
{
	matrix3x4_t attachmentToWorld;
	if (!GetEngineObject()->GetAttachment(iAttachment, attachmentToWorld))
		return false;

	matrix3x4_t worldToEntity;
	MatrixInvert(GetEngineObject()->EntityToWorldTransform(), worldToEntity);
	ConcatTransforms(worldToEntity, attachmentToWorld, attachmentToLocal);
	return true;
}

bool C_BaseEntity::GetAttachmentLocal(int iAttachment, Vector& origin, QAngle& angles)
{
	matrix3x4_t attachmentToEntity;

	if (GetAttachmentLocal(iAttachment, attachmentToEntity))
	{
		origin.Init(attachmentToEntity[0][3], attachmentToEntity[1][3], attachmentToEntity[2][3]);
		MatrixAngles(attachmentToEntity, angles);
		return true;
	}
	return false;
}

bool C_BaseEntity::GetAttachmentLocal(int iAttachment, Vector& origin)
{
	matrix3x4_t attachmentToEntity;

	if (GetAttachmentLocal(iAttachment, attachmentToEntity))
	{
		MatrixPosition(attachmentToEntity, origin);
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get attachment point by index
// Input  : number - which point
// Output : float * - the attachment point
//-----------------------------------------------------------------------------
//bool C_BaseEntity::GetAttachment( int number, Vector &origin, QAngle &angles )
//{
//	origin = GetEngineObject()->GetAbsOrigin();
//	angles = GetEngineObject()->GetAbsAngles();
//	return true;
//}

//bool C_BaseEntity::GetAttachment( int number, Vector &origin )
//{
//	origin = GetEngineObject()->GetAbsOrigin();
//	return true;
//}

//bool C_BaseEntity::GetAttachment( int number, matrix3x4_t &matrix )
//{
//	MatrixCopy(GetEngineObject()->EntityToWorldTransform(), matrix );
//	return true;
//}

//bool C_BaseEntity::GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel )
//{
//	originVel = GetEngineObject()->GetAbsVelocity();
//	angleVel.Init();
//	return true;
//}


//-----------------------------------------------------------------------------
// Purpose: Get this entity's rendering clip plane if one is defined
// Output : float * - The clip plane to use, or NULL if no clip plane is defined
//-----------------------------------------------------------------------------
float *C_BaseEntity::GetRenderClipPlane( void )
{
	if( m_bEnableRenderingClipPlane )
		return m_fRenderingClipPlane;
	else
		return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BaseEntity::DrawBrushModel( bool bDrawingTranslucency, int nFlags, bool bTwoPass )
{
	VPROF_BUDGET( "C_BaseEntity::DrawBrushModel", VPROF_BUDGETGROUP_BRUSHMODEL_RENDERING );
	// Identity brushes are drawn in view->DrawWorld as an optimization
	Assert ( modelinfo->GetModelType(GetEngineObject()->GetModel()) == mod_brush );

	ERenderDepthMode DepthMode = DEPTH_MODE_NORMAL;
	if ( ( nFlags & STUDIO_SSAODEPTHTEXTURE ) != 0 )
	{
		DepthMode = DEPTH_MODE_SSA0;
	}
	else if ( ( nFlags & STUDIO_SHADOWDEPTHTEXTURE ) != 0 )
	{
		DepthMode = DEPTH_MODE_SHADOW;
	}

	if ( DepthMode != DEPTH_MODE_NORMAL )
	{
		render->DrawBrushModelShadowDepth( this, (model_t *)GetEngineObject()->GetModel(), GetEngineObject()->GetAbsOrigin(), GetEngineObject()->GetAbsAngles(), DepthMode );
	}
	else
	{
		DrawBrushModelMode_t mode = DBM_DRAW_ALL;
		if ( bTwoPass )
		{
			mode = bDrawingTranslucency ? DBM_DRAW_TRANSLUCENT_ONLY : DBM_DRAW_OPAQUE_ONLY;
		}
		render->DrawBrushModelEx( this, (model_t *)GetEngineObject()->GetModel(), GetEngineObject()->GetAbsOrigin(), GetEngineObject()->GetAbsAngles(), mode);
	}

	return 1;
}

ConVar r_drawothermodels("r_drawothermodels", "1", FCVAR_CHEAT, "0=Off, 1=Normal, 2=Wireframe");

//-----------------------------------------------------------------------------
// Purpose: Draws the object
// Input  : flags - 
//-----------------------------------------------------------------------------
int C_BaseEntity::DrawModel( int flags )
{
	if (IsBaseAnimating()) {
		VPROF_BUDGET("C_BaseAnimating::DrawModel", VPROF_BUDGETGROUP_MODEL_RENDERING);
		if (!GetEngineObject()->IsReadyToDraw())
			return 0;

		int drawn = 0;

#ifdef TF_CLIENT_DLL
		ValidateModelIndex();
#endif

		if (r_drawothermodels.GetInt())
		{
			MDLCACHE_CRITICAL_SECTION();

			int extraFlags = 0;
			if (r_drawothermodels.GetInt() == 2)
			{
				extraFlags |= STUDIO_WIREFRAME;
			}

			if (flags & STUDIO_SHADOWDEPTHTEXTURE)
			{
				extraFlags |= STUDIO_SHADOWDEPTHTEXTURE;
			}

			if (flags & STUDIO_SSAODEPTHTEXTURE)
			{
				extraFlags |= STUDIO_SSAODEPTHTEXTURE;
			}

			if ((flags & (STUDIO_SSAODEPTHTEXTURE | STUDIO_SHADOWDEPTHTEXTURE)) == 0 &&
				g_pStudioStatsEntity != NULL && g_pStudioStatsEntity == GetClientRenderable())
			{
				extraFlags |= STUDIO_GENERATE_STATS;
			}

			// Necessary for lighting blending
			GetEngineObject()->CreateModelInstance();

			if (!GetEngineObject()->IsFollowingEntity())
			{
				drawn = InternalDrawModel(flags | extraFlags);
			}
			else
			{
				// this doesn't draw unless master entity is visible and it's a studio model!!!
				C_BaseEntity* follow = (C_BaseEntity*)GetEngineObject()->FindFollowedEntity()->GetOuter();
				if (follow)
				{
					// recompute master entity bone structure
					int baseDrawn = follow->DrawModel(0);

					// draw entity
					// FIXME: Currently only draws if aiment is drawn.  
					// BUGBUG: Fixup bbox and do a separate cull for follow object
					if (baseDrawn)
					{
						drawn = InternalDrawModel(STUDIO_RENDER | extraFlags);
					}
				}
			}
		}

		// If we're visualizing our bboxes, draw them
		DrawBBoxVisualizations();

		return drawn;
	}
	else {
		if (!GetEngineObject()->IsReadyToDraw())
			return 0;

		int drawn = 0;
		if (!GetEngineObject()->GetModel())
		{
			return drawn;
		}

		int modelType = modelinfo->GetModelType(GetEngineObject()->GetModel());
		switch (modelType)
		{
		case mod_brush:
			drawn = DrawBrushModel(flags & STUDIO_TRANSPARENCY ? true : false, flags, (flags & STUDIO_TWOPASS) ? true : false);
			break;
		case mod_studio:
			// All studio models must be derived from C_BaseAnimating.  Issue warning.
			Warning("ERROR:  Can't draw studio model %s because %s is not derived from C_BaseAnimating\n",
				modelinfo->GetModelName(GetEngineObject()->GetModel()), GetClientClass()->m_pNetworkName ? GetClientClass()->m_pNetworkName : "unknown");
			break;
		case mod_sprite:
			//drawn = DrawSprite();
			Warning("ERROR:  Sprite model's not supported any more except in legacy temp ents\n");
			break;
		default:
			break;
		}

		// If we're visualizing our bboxes, draw them
		DrawBBoxVisualizations();

		return drawn;
	}
}

void C_BaseEntity::DoInternalDrawModel(ClientModelRenderInfo_t* pInfo, DrawModelState_t* pState, matrix3x4_t* pBoneToWorldArray)
{
	if (pState)
	{
		modelrender->DrawModelExecute(*pState, *pInfo, pBoneToWorldArray);
	}

	if (vcollide_wireframe.GetBool())
	{
		if (GetEngineObject()->IsRagdoll())
		{
			GetEngineObject()->DrawWireframe();
		}
		else if (GetEngineObject()->IsSolid() && GetEngineObject()->GetSolid() == SOLID_VPHYSICS)
		{
			vcollide_t* pCollide = modelinfo->GetVCollide(GetEngineObject()->GetModelIndex());
			if (pCollide && pCollide->solidCount == 1)
			{
				static color32 debugColor = { 0,255,255,0 };
				matrix3x4_t matrix;
				AngleMatrix(GetEngineObject()->GetAbsAngles(), GetEngineObject()->GetAbsOrigin(), matrix);
				engine->DebugDrawPhysCollide(pCollide->solids[0], NULL, matrix, debugColor);
				if (GetEngineObject()->VPhysicsGetObject())
				{
					static color32 debugColorPhys = { 255,0,0,0 };
					matrix3x4_t matrix;
					GetEngineObject()->VPhysicsGetObject()->GetPositionMatrix(&matrix);
					engine->DebugDrawPhysCollide(pCollide->solids[0], NULL, matrix, debugColorPhys);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws the object
// Input  : flags - 
//-----------------------------------------------------------------------------
int C_BaseEntity::InternalDrawModel(int flags)
{
	VPROF("C_BaseAnimating::InternalDrawModel");

	if (!GetEngineObject()->GetModel())
		return 0;

	// This should never happen, but if the server class hierarchy has bmodel entities derived from CBaseAnimating or does a
	//  SetModel with the wrong type of model, this could occur.
	if (modelinfo->GetModelType(GetEngineObject()->GetModel()) != mod_studio)
	{
		return DrawModel(flags);
	}

	// Make sure hdr is valid for drawing
	if (!GetEngineObject()->GetModelPtr())
		return 0;

	//UpdateBoneAttachments( );

	if (GetEngineObject()->IsEffectActive(EF_ITEM_BLINK))
	{
		flags |= STUDIO_ITEM_BLINK;
	}

	ClientModelRenderInfo_t info;
	ClientModelRenderInfo_t* pInfo;

	pInfo = &info;

	pInfo->flags = flags;
	pInfo->pRenderable = this->GetEngineObject();
	pInfo->instance = GetEngineObject()->GetModelInstance();
	pInfo->entity_index = entindex();
	pInfo->pModel = GetEngineObject()->GetModel();
	pInfo->origin = GetRenderOrigin();
	pInfo->angles = GetRenderAngles();
	pInfo->skin = GetEngineObject()->GetSkin();
	pInfo->body = GetEngineObject()->GetBody();
	pInfo->hitboxset = GetEngineObject()->GetHitboxSet();

	if (!OnInternalDrawModel(pInfo))
	{
		return 0;
	}

	Assert(!pInfo->pModelToWorld);
	if (!pInfo->pModelToWorld)
	{
		pInfo->pModelToWorld = &pInfo->modelToWorld;

		// Turns the origin + angles into a matrix
		AngleMatrix(pInfo->angles, pInfo->origin, pInfo->modelToWorld);
	}

	DrawModelState_t state;
	matrix3x4_t* pBoneToWorld = NULL;
	bool bMarkAsDrawn = modelrender->DrawModelSetup(*pInfo, &state, NULL, &pBoneToWorld);

	// Scale the base transform if we don't have a bone hierarchy
	if (GetEngineObject()->IsModelScaled())
	{
		IStudioHdr* pHdr = GetEngineObject()->GetModelPtr();
		if (pHdr && pBoneToWorld && pHdr->numbones() == 1)
		{
			// Scale the bone to world at this point
			const float flScale = GetEngineObject()->GetModelScale();
			VectorScale((*pBoneToWorld)[0], flScale, (*pBoneToWorld)[0]);
			VectorScale((*pBoneToWorld)[1], flScale, (*pBoneToWorld)[1]);
			VectorScale((*pBoneToWorld)[2], flScale, (*pBoneToWorld)[2]);
		}
	}

	DoInternalDrawModel(pInfo, (bMarkAsDrawn && (pInfo->flags & STUDIO_RENDER)) ? &state : NULL, pBoneToWorld);

	OnPostInternalDrawModel(pInfo);

	return bMarkAsDrawn;
}

//-----------------------------------------------------------------------------
// Relative lighting entity
//-----------------------------------------------------------------------------
class C_InfoLightingRelative : public C_BaseEntity
{
public:
	DECLARE_CLASS(C_InfoLightingRelative, C_BaseEntity);
	DECLARE_CLIENTCLASS();

	void GetLightingOffset(matrix3x4_t& offset);

private:
	EHANDLE			m_hLightingLandmark;
};

IMPLEMENT_CLIENTCLASS_DT(C_InfoLightingRelative, DT_InfoLightingRelative, CInfoLightingRelative)
RecvPropEHandle(RECVINFO(m_hLightingLandmark)),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Relative lighting entity
//-----------------------------------------------------------------------------
void C_InfoLightingRelative::GetLightingOffset(matrix3x4_t& offset)
{
	if (m_hLightingLandmark.Get())
	{
		matrix3x4_t matWorldToLandmark;
		MatrixInvert(m_hLightingLandmark->GetEngineObject()->EntityToWorldTransform(), matWorldToLandmark);
		ConcatTransforms(GetEngineObject()->EntityToWorldTransform(), matWorldToLandmark, offset);
	}
	else
	{
		SetIdentityMatrix(offset);
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool C_BaseEntity::OnPostInternalDrawModel(ClientModelRenderInfo_t* pInfo)
{
	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool C_BaseEntity::OnInternalDrawModel(ClientModelRenderInfo_t* pInfo)
{
	if (m_hLightingOriginRelative.Get())
	{
		C_InfoLightingRelative* pInfoLighting = assert_cast<C_InfoLightingRelative*>(m_hLightingOriginRelative.Get());
		pInfoLighting->GetLightingOffset(pInfo->lightingOffset);
		pInfo->pLightingOffset = &pInfo->lightingOffset;
	}
	if (m_hLightingOrigin)
	{
		pInfo->pLightingOrigin = &(m_hLightingOrigin->GetEngineObject()->GetAbsOrigin());
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Setup vertex weights for drawing
//-----------------------------------------------------------------------------
void C_BaseEntity::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
}


//-----------------------------------------------------------------------------
// Purpose: Process any local client-side animation events
//-----------------------------------------------------------------------------
void C_BaseEntity::DoAnimationEvents(IStudioHdr* pStudioHdr)
{
	if (!pStudioHdr)
		return;

#ifdef DEBUG
	bool watch = dbganimmodel.GetString()[0] && V_stristr(pStudioHdr->pszName(), dbganimmodel.GetString());
#else
	bool watch = false; // Q_strstr( hdr->name, "rifle" ) ? true : false;
#endif

	//Adrian: eh? This should never happen.
	if (GetEngineObject()->GetSequence() == -1)
		return;

	// build root animation
	float flEventCycle = GetEngineObject()->GetCycle();

	// If we're invisible, don't draw the muzzle flash
	bool bIsInvisible = !IsVisible() && !IsViewModel() && !IsMenuModel();

	if (bIsInvisible && !clienttools->IsInRecordingMode())
		return;

	// add in muzzleflash effect
	if (GetEngineObject()->ShouldMuzzleFlash())
	{
		GetEngineObject()->DisableMuzzleFlash();

		ProcessMuzzleFlashEvent();
	}

	// If we're invisible, don't process animation events.
	if (bIsInvisible)
		return;

	// If we don't have any sequences, don't do anything
	int nStudioNumSeq = pStudioHdr->GetNumSeq();
	if (nStudioNumSeq < 1)
	{
		Warning("%s[%d]: no sequences?\n", GetDebugName(), entindex());
		Assert(nStudioNumSeq >= 1);
		return;
	}

	int nSeqNum = GetEngineObject()->GetSequence();
	if (nSeqNum >= nStudioNumSeq)
	{
		// This can happen e.g. while reloading Heavy's shotgun, switch to the minigun.
		Warning("%s[%d]: Playing sequence %d but there's only %d in total?\n", GetDebugName(), entindex(), nSeqNum, nStudioNumSeq);
		return;
	}

	mstudioseqdesc_t& seqdesc = pStudioHdr->pSeqdesc(nSeqNum);

	if (seqdesc.numevents == 0)
		return;

	// Forces anim event indices to get set and returns pEvent(0);
	mstudioevent_t* pevent = mdlcache->GetEventIndexForSequence(seqdesc);

	if (watch)
	{
		Msg("%i cycle %f\n", gpGlobals->tickcount, GetEngineObject()->GetCycle());
	}

	bool resetEvents = GetEngineObject()->GetResetEventsParity() != m_nPrevResetEventsParity;
	m_nPrevResetEventsParity = GetEngineObject()->GetResetEventsParity();

	if (m_nEventSequence != GetEngineObject()->GetSequence() || resetEvents)
	{
		if (watch)
		{
			Msg("new seq: %i - old seq: %i - reset: %s - m_flCycle %f - Model Name: %s - (time %.3f)\n",
				GetEngineObject()->GetSequence(), m_nEventSequence,
				resetEvents ? "true" : "false",
				GetEngineObject()->GetCycle(), pStudioHdr->pszName(),
				gpGlobals->curtime);
		}

		m_nEventSequence = GetEngineObject()->GetSequence();
		flEventCycle = 0.0f;
		m_flPrevEventCycle = -0.01; // back up to get 0'th frame animations
	}

	// stalled?
	if (flEventCycle == m_flPrevEventCycle)
		return;

	if (watch)
	{
		Msg("%i (seq %d cycle %.3f ) evcycle %.3f prevevcycle %.3f (time %.3f)\n",
			gpGlobals->tickcount,
			GetEngineObject()->GetSequence(),
			GetEngineObject()->GetCycle(),
			flEventCycle,
			m_flPrevEventCycle,
			gpGlobals->curtime);
	}

	// check for looping
	BOOL bLooped = false;
	if (flEventCycle <= m_flPrevEventCycle)
	{
		if (m_flPrevEventCycle - flEventCycle > 0.5)
		{
			bLooped = true;
		}
		else
		{
			// things have backed up, which is bad since it'll probably result in a hitch in the animation playback
			// but, don't play events again for the same time slice
			return;
		}
	}

	// This makes sure events that occur at the end of a sequence occur are
	// sent before events that occur at the beginning of a sequence.
	if (bLooped)
	{
		for (int i = 0; i < (int)seqdesc.numevents; i++)
		{
			// ignore all non-client-side events

			if (pevent[i].type & AE_TYPE_NEWEVENTSYSTEM)
			{
				if (!(pevent[i].type & AE_TYPE_CLIENT))
					continue;
			}
			else if (pevent[i].event < 5000) //Adrian - Support the old event system
				continue;

			if (pevent[i].cycle <= m_flPrevEventCycle)
				continue;

			if (watch)
			{
				Msg("%i FE %i Looped cycle %f, prev %f ev %f (time %.3f)\n",
					gpGlobals->tickcount,
					pevent[i].event,
					pevent[i].cycle,
					m_flPrevEventCycle,
					flEventCycle,
					gpGlobals->curtime);
			}


			FireEvent(GetEngineObject()->GetAbsOrigin(), GetEngineObject()->GetAbsAngles(), pevent[i].event, pevent[i].pszOptions());
		}

		// Necessary to get the next loop working
		m_flPrevEventCycle = -0.01;
	}

	for (int i = 0; i < (int)seqdesc.numevents; i++)
	{
		if (pevent[i].type & AE_TYPE_NEWEVENTSYSTEM)
		{
			if (!(pevent[i].type & AE_TYPE_CLIENT))
				continue;
		}
		else if (pevent[i].event < 5000) //Adrian - Support the old event system
			continue;

		if ((pevent[i].cycle > m_flPrevEventCycle && pevent[i].cycle <= flEventCycle))
		{
			if (watch)
			{
				Msg("%i (seq: %d) FE %i Normal cycle %f, prev %f ev %f (time %.3f)\n",
					gpGlobals->tickcount,
					GetEngineObject()->GetSequence(),
					pevent[i].event,
					pevent[i].cycle,
					m_flPrevEventCycle,
					flEventCycle,
					gpGlobals->curtime);
			}

			FireEvent(GetEngineObject()->GetAbsOrigin(), GetEngineObject()->GetAbsAngles(), pevent[i].event, pevent[i].pszOptions());
		}
	}

	m_flPrevEventCycle = flEventCycle;
}

void MaterialFootstepSound(C_BaseEntity* pEnt, bool bLeftFoot, float flVolume)
{
	trace_t tr;
	Vector traceStart;
	QAngle angles;

	int attachment;

	//!!!PERF - These string lookups here aren't the swiftest, but
	// this doesn't get called very frequently unless a lot of NPCs
	// are using this code.
	if (bLeftFoot)
	{
		attachment = pEnt->GetEngineObject()->LookupAttachment("LeftFoot");
	}
	else
	{
		attachment = pEnt->GetEngineObject()->LookupAttachment("RightFoot");
	}

	if (attachment == -1)
	{
		// Exit if this NPC doesn't have the proper attachments.
		return;
	}

	pEnt->GetEngineObject()->GetAttachment(attachment, traceStart, angles);

	UTIL_TraceLine(EntityList(), traceStart, traceStart - Vector(0, 0, 48.0f), MASK_SHOT_HULL, pEnt, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction < 1.0 && tr.m_pEnt)
	{
		surfacedata_t* psurf = EntityList()->PhysGetProps()->GetSurfaceData(tr.surface.surfaceProps);
		if (psurf)
		{
			EmitSound_t params;
			if (bLeftFoot)
			{
				params.m_pSoundName = EntityList()->PhysGetProps()->GetString(psurf->sounds.stepleft);
			}
			else
			{
				params.m_pSoundName = EntityList()->PhysGetProps()->GetString(psurf->sounds.stepright);
			}

			CPASAttenuationFilter filter(pEnt, params.m_pSoundName);

			params.m_bWarnOnDirectWaveReference = true;
			params.m_flVolume = flVolume;

			g_pSoundEmitterSystem->EmitSound(filter, pEnt->entindex(), params);//pEnt->
		}
	}
}

void C_BaseEntity::FireEvent(const Vector& origin, const QAngle& angles, int event, const char* options)
{
	Vector attachOrigin;
	QAngle attachAngles;

	switch (event)
	{
	case AE_CL_CREATE_PARTICLE_EFFECT:
	{
		int iAttachment = -1;
		int iAttachType = PATTACH_ABSORIGIN_FOLLOW;
		char token[256];
		char szParticleEffect[256];

		// Get the particle effect name
		const char* p = options;
		p = nexttoken(token, p, ' ');
		if (token)
		{
			const char* mtoken = ModifyEventParticles(token);
			if (!mtoken || mtoken[0] == '\0')
				return;
			Q_strncpy(szParticleEffect, mtoken, sizeof(szParticleEffect));
		}

		// Get the attachment type
		p = nexttoken(token, p, ' ');
		if (token)
		{
			iAttachType = GetAttachTypeFromString(token);
			if (iAttachType == -1)
			{
				Warning("Invalid attach type specified for particle effect anim event. Trying to spawn effect '%s' with attach type of '%s'\n", szParticleEffect, token);
				return;
			}
		}

		// Get the attachment point index
		p = nexttoken(token, p, ' ');
		if (token)
		{
			iAttachment = atoi(token);

			// See if we can find any attachment points matching the name
			if (token[0] != '0' && iAttachment == 0)
			{
				iAttachment = GetEngineObject()->LookupAttachment(token);
				if (iAttachment <= 0)
				{
					Warning("Failed to find attachment point specified for particle effect anim event. Trying to spawn effect '%s' on attachment named '%s'\n", szParticleEffect, token);
					return;
				}
			}
		}

		// Spawn the particle effect
		ParticleProp()->Create(szParticleEffect, (ParticleAttachment_t)iAttachType, iAttachment);
	}
	break;

	case AE_CL_PLAYSOUND:
	{
		CLocalPlayerFilter filter;

		if (GetEngineObject()->GetAttachmentCount() > 0)
		{
			GetEngineObject()->GetAttachment(1, attachOrigin, attachAngles);
			g_pSoundEmitterSystem->EmitSound(filter, GetSoundSourceIndex(), options, &attachOrigin);
		}
		else
		{
			g_pSoundEmitterSystem->EmitSound(filter, GetSoundSourceIndex(), options, &GetEngineObject()->GetAbsOrigin());
		}
	}
	break;
	case AE_CL_STOPSOUND:
	{
		g_pSoundEmitterSystem->StopSound(GetSoundSourceIndex(), options);
	}
	break;

	case CL_EVENT_FOOTSTEP_LEFT:
	{
#ifndef HL2MP
		char pSoundName[256];
		if (!options || !options[0])
		{
			options = "NPC_CombineS";
		}

		Vector vel;
		GetEngineObject()->EstimateAbsVelocity(vel);

		// If he's moving fast enough, play the run sound
		if (vel.Length2DSqr() > RUN_SPEED_ESTIMATE_SQR)
		{
			Q_snprintf(pSoundName, 256, "%s.RunFootstepLeft", options);
		}
		else
		{
			Q_snprintf(pSoundName, 256, "%s.FootstepLeft", options);
		}

		const char* soundname = pSoundName;
		CPASAttenuationFilter filter(this, soundname);

		EmitSound_t params;
		params.m_pSoundName = soundname;
		params.m_flSoundTime = 0.0f;
		params.m_pflSoundDuration = NULL;
		params.m_bWarnOnDirectWaveReference = true;
		g_pSoundEmitterSystem->EmitSound(filter, this->entindex(), params);
#endif
	}
	break;

	case CL_EVENT_FOOTSTEP_RIGHT:
	{
#ifndef HL2MP
		char pSoundName[256];
		if (!options || !options[0])
		{
			options = "NPC_CombineS";
		}

		Vector vel;
		GetEngineObject()->EstimateAbsVelocity(vel);
		// If he's moving fast enough, play the run sound
		if (vel.Length2DSqr() > RUN_SPEED_ESTIMATE_SQR)
		{
			Q_snprintf(pSoundName, 256, "%s.RunFootstepRight", options);
		}
		else
		{
			Q_snprintf(pSoundName, 256, "%s.FootstepRight", options);
		}
		const char* soundname = pSoundName;
		CPASAttenuationFilter filter(this, soundname);

		EmitSound_t params;
		params.m_pSoundName = soundname;
		params.m_flSoundTime = 0.0f;
		params.m_pflSoundDuration = NULL;
		params.m_bWarnOnDirectWaveReference = true;
		g_pSoundEmitterSystem->EmitSound(filter, this->entindex(), params);
#endif
	}
	break;

	case CL_EVENT_MFOOTSTEP_LEFT:
	{
		MaterialFootstepSound(this, true, VOL_NORM * 0.5f);
	}
	break;

	case CL_EVENT_MFOOTSTEP_RIGHT:
	{
		MaterialFootstepSound(this, false, VOL_NORM * 0.5f);
	}
	break;

	case CL_EVENT_MFOOTSTEP_LEFT_LOUD:
	{
		MaterialFootstepSound(this, true, VOL_NORM);
	}
	break;

	case CL_EVENT_MFOOTSTEP_RIGHT_LOUD:
	{
		MaterialFootstepSound(this, false, VOL_NORM);
	}
	break;

	// Eject brass
	case CL_EVENT_EJECTBRASS1:
		if (GetEngineObject()->GetAttachmentCount() > 0)
		{
			if (g_pViewRender->MainViewOrigin().DistToSqr(GetEngineObject()->GetAbsOrigin()) < (256 * 256))
			{
				Vector attachOrigin;
				QAngle attachAngles;

				if (GetEngineObject()->GetAttachment(2, attachOrigin, attachAngles))
				{
					tempents->EjectBrass(attachOrigin, attachAngles, GetEngineObject()->GetAbsAngles(), atoi(options));
				}
			}
		}
		break;

	case AE_MUZZLEFLASH:
	{
		// Send out the effect for a player
		DispatchMuzzleEffect(options, true);
		break;
	}

	case AE_NPC_MUZZLEFLASH:
	{
		// Send out the effect for an NPC
		DispatchMuzzleEffect(options, false);
		break;
	}

	// OBSOLETE EVENTS. REPLACED BY NEWER SYSTEMS.
	// See below in FireObsoleteEvent() for comments on what to use instead.
	case AE_CLIENT_EFFECT_ATTACH:
	case CL_EVENT_DISPATCHEFFECT0:
	case CL_EVENT_DISPATCHEFFECT1:
	case CL_EVENT_DISPATCHEFFECT2:
	case CL_EVENT_DISPATCHEFFECT3:
	case CL_EVENT_DISPATCHEFFECT4:
	case CL_EVENT_DISPATCHEFFECT5:
	case CL_EVENT_DISPATCHEFFECT6:
	case CL_EVENT_DISPATCHEFFECT7:
	case CL_EVENT_DISPATCHEFFECT8:
	case CL_EVENT_DISPATCHEFFECT9:
	case CL_EVENT_MUZZLEFLASH0:
	case CL_EVENT_MUZZLEFLASH1:
	case CL_EVENT_MUZZLEFLASH2:
	case CL_EVENT_MUZZLEFLASH3:
	case CL_EVENT_NPC_MUZZLEFLASH0:
	case CL_EVENT_NPC_MUZZLEFLASH1:
	case CL_EVENT_NPC_MUZZLEFLASH2:
	case CL_EVENT_NPC_MUZZLEFLASH3:
	case CL_EVENT_SPARK0:
	case CL_EVENT_SOUND:
		FireObsoleteEvent(origin, angles, event, options);
		break;

	case AE_CL_ENABLE_BODYGROUP:
	{
		int index = GetEngineObject()->FindBodygroupByName(options);
		if (index >= 0)
		{
			GetEngineObject()->SetBodygroup(index, 1);
		}
	}
	break;

	case AE_CL_DISABLE_BODYGROUP:
	{
		int index = GetEngineObject()->FindBodygroupByName(options);
		if (index >= 0)
		{
			GetEngineObject()->SetBodygroup(index, 0);
		}
	}
	break;

	case AE_CL_BODYGROUP_SET_VALUE:
	{
		char szBodygroupName[256];
		int value = 0;

		char token[256];

		const char* p = options;

		// Bodygroup Name
		p = nexttoken(token, p, ' ');
		if (token)
		{
			Q_strncpy(szBodygroupName, token, sizeof(szBodygroupName));
		}

		// Get the desired value
		p = nexttoken(token, p, ' ');
		if (token)
		{
			value = atoi(token);
		}

		int index = GetEngineObject()->FindBodygroupByName(szBodygroupName);
		if (index >= 0)
		{
			GetEngineObject()->SetBodygroup(index, value);
		}
	}
	break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: These events are all obsolete events, left here to support old games.
//			Their systems have all been replaced with better ones.
//-----------------------------------------------------------------------------
void C_BaseEntity::FireObsoleteEvent(const Vector& origin, const QAngle& angles, int event, const char* options)
{
	Vector attachOrigin;
	QAngle attachAngles;

	switch (event)
	{
		// Obsolete. Use the AE_CL_CREATE_PARTICLE_EFFECT event instead, which uses the artist driven particle system & editor.
	case AE_CLIENT_EFFECT_ATTACH:
	{
		int iAttachment = -1;
		int iParam = 0;
		char token[128];
		char effectFunc[128];

		const char* p = options;

		p = nexttoken(token, p, ' ');

		if (token)
		{
			Q_strncpy(effectFunc, token, sizeof(effectFunc));
		}

		p = nexttoken(token, p, ' ');

		if (token)
		{
			iAttachment = atoi(token);
		}

		p = nexttoken(token, p, ' ');

		if (token)
		{
			iParam = atoi(token);
		}

		if (iAttachment != -1 && GetEngineObject()->GetAttachmentCount() >= iAttachment)
		{
			GetEngineObject()->GetAttachment(iAttachment, attachOrigin, attachAngles);

			// Fill out the generic data
			CEffectData data;
			data.m_vOrigin = attachOrigin;
			data.m_vAngles = attachAngles;
			AngleVectors(attachAngles, &data.m_vNormal);
			data.m_hEntity = this;
			data.m_nAttachmentIndex = iAttachment + 1;
			data.m_fFlags = iParam;

			g_pEffects->DispatchEffect(effectFunc, data);
		}
	}
	break;

	// Obsolete. Use the AE_CL_CREATE_PARTICLE_EFFECT event instead, which uses the artist driven particle system & editor.
	case CL_EVENT_DISPATCHEFFECT0:
	case CL_EVENT_DISPATCHEFFECT1:
	case CL_EVENT_DISPATCHEFFECT2:
	case CL_EVENT_DISPATCHEFFECT3:
	case CL_EVENT_DISPATCHEFFECT4:
	case CL_EVENT_DISPATCHEFFECT5:
	case CL_EVENT_DISPATCHEFFECT6:
	case CL_EVENT_DISPATCHEFFECT7:
	case CL_EVENT_DISPATCHEFFECT8:
	case CL_EVENT_DISPATCHEFFECT9:
	{
		int iAttachment = -1;

		// First person muzzle flashes
		switch (event)
		{
		case CL_EVENT_DISPATCHEFFECT0:
			iAttachment = 0;
			break;

		case CL_EVENT_DISPATCHEFFECT1:
			iAttachment = 1;
			break;

		case CL_EVENT_DISPATCHEFFECT2:
			iAttachment = 2;
			break;

		case CL_EVENT_DISPATCHEFFECT3:
			iAttachment = 3;
			break;

		case CL_EVENT_DISPATCHEFFECT4:
			iAttachment = 4;
			break;

		case CL_EVENT_DISPATCHEFFECT5:
			iAttachment = 5;
			break;

		case CL_EVENT_DISPATCHEFFECT6:
			iAttachment = 6;
			break;

		case CL_EVENT_DISPATCHEFFECT7:
			iAttachment = 7;
			break;

		case CL_EVENT_DISPATCHEFFECT8:
			iAttachment = 8;
			break;

		case CL_EVENT_DISPATCHEFFECT9:
			iAttachment = 9;
			break;
		}

		if (iAttachment != -1 && GetEngineObject()->GetAttachmentCount() > iAttachment)
		{
			GetEngineObject()->GetAttachment(iAttachment + 1, attachOrigin, attachAngles);

			// Fill out the generic data
			CEffectData data;
			data.m_vOrigin = attachOrigin;
			data.m_vAngles = attachAngles;
			AngleVectors(attachAngles, &data.m_vNormal);
			data.m_hEntity = this;
			data.m_nAttachmentIndex = iAttachment + 1;

			g_pEffects->DispatchEffect(options, data);
		}
	}
	break;

	// Obsolete. Use the AE_MUZZLEFLASH / AE_NPC_MUZZLEFLASH events instead.
	case CL_EVENT_MUZZLEFLASH0:
	case CL_EVENT_MUZZLEFLASH1:
	case CL_EVENT_MUZZLEFLASH2:
	case CL_EVENT_MUZZLEFLASH3:
	case CL_EVENT_NPC_MUZZLEFLASH0:
	case CL_EVENT_NPC_MUZZLEFLASH1:
	case CL_EVENT_NPC_MUZZLEFLASH2:
	case CL_EVENT_NPC_MUZZLEFLASH3:
	{
		int iAttachment = -1;
		bool bFirstPerson = true;

		// First person muzzle flashes
		switch (event)
		{
		case CL_EVENT_MUZZLEFLASH0:
			iAttachment = 0;
			break;

		case CL_EVENT_MUZZLEFLASH1:
			iAttachment = 1;
			break;

		case CL_EVENT_MUZZLEFLASH2:
			iAttachment = 2;
			break;

		case CL_EVENT_MUZZLEFLASH3:
			iAttachment = 3;
			break;

			// Third person muzzle flashes
		case CL_EVENT_NPC_MUZZLEFLASH0:
			iAttachment = 0;
			bFirstPerson = false;
			break;

		case CL_EVENT_NPC_MUZZLEFLASH1:
			iAttachment = 1;
			bFirstPerson = false;
			break;

		case CL_EVENT_NPC_MUZZLEFLASH2:
			iAttachment = 2;
			bFirstPerson = false;
			break;

		case CL_EVENT_NPC_MUZZLEFLASH3:
			iAttachment = 3;
			bFirstPerson = false;
			break;
		}

		if (iAttachment != -1 && GetEngineObject()->GetAttachmentCount() > iAttachment)
		{
			GetEngineObject()->GetAttachment(iAttachment + 1, attachOrigin, attachAngles);
			int entId = render->GetViewEntity();
			C_BaseEntity* hEntity = (C_BaseEntity*)EntityList()->GetBaseEntity(entId);
			tempents->MuzzleFlash(attachOrigin, attachAngles, atoi(options), hEntity, bFirstPerson);
		}
	}
	break;

	// Obsolete: Use the AE_CL_CREATE_PARTICLE_EFFECT event instead, which uses the artist driven particle system & editor.
	case CL_EVENT_SPARK0:
	{
		Vector vecForward;
		GetEngineObject()->GetAttachment(1, attachOrigin, attachAngles);
		AngleVectors(attachAngles, &vecForward);
		g_pEffects->Sparks(attachOrigin, atoi(options), 1, &vecForward);
	}
	break;

	// Obsolete: Use the AE_CL_PLAYSOUND event instead, which doesn't rely on a magic number in the .qc
	case CL_EVENT_SOUND:
	{
		CLocalPlayerFilter filter;

		if (GetEngineObject()->GetAttachmentCount() > 0)
		{
			GetEngineObject()->GetAttachment(1, attachOrigin, attachAngles);
			g_pSoundEmitterSystem->EmitSound(filter, GetSoundSourceIndex(), options, &attachOrigin);
		}
		else
		{
			g_pSoundEmitterSystem->EmitSound(filter, GetSoundSourceIndex(), options);
		}
	}
	break;

	default:
		break;
	}
}

bool C_BaseEntity::DispatchMuzzleEffect(const char* options, bool isFirstPerson)
{
	const char* p = options;
	char		token[128];
	int			weaponType = 0;

	// Get the first parameter
	p = nexttoken(token, p, ' ');

	// Find the weapon type
	if (token)
	{
		//TODO: Parse the type from a list instead
		if (Q_stricmp(token, "COMBINE") == 0)
		{
			weaponType = MUZZLEFLASH_COMBINE;
		}
		else if (Q_stricmp(token, "SMG1") == 0)
		{
			weaponType = MUZZLEFLASH_SMG1;
		}
		else if (Q_stricmp(token, "PISTOL") == 0)
		{
			weaponType = MUZZLEFLASH_PISTOL;
		}
		else if (Q_stricmp(token, "SHOTGUN") == 0)
		{
			weaponType = MUZZLEFLASH_SHOTGUN;
		}
		else if (Q_stricmp(token, "357") == 0)
		{
			weaponType = MUZZLEFLASH_357;
		}
		else if (Q_stricmp(token, "RPG") == 0)
		{
			weaponType = MUZZLEFLASH_RPG;
		}
		else
		{
			//NOTENOTE: This means you specified an invalid muzzleflash type, check your spelling?
			Assert(0);
		}
	}
	else
	{
		//NOTENOTE: This means that there wasn't a proper parameter passed into the animevent
		Assert(0);
		return false;
	}

	// Get the second parameter
	p = nexttoken(token, p, ' ');

	int	attachmentIndex = -1;

	// Find the attachment name
	if (token)
	{
		attachmentIndex = GetEngineObject()->LookupAttachment(token);

		// Found an invalid attachment
		if (attachmentIndex <= 0)
		{
			//NOTENOTE: This means that the attachment you're trying to use is invalid
			Assert(0);
			return false;
		}
	}
	else
	{
		//NOTENOTE: This means that there wasn't a proper parameter passed into the animevent
		Assert(0);
		return false;
	}

	// Send it out
	tempents->MuzzleFlash(weaponType, this, attachmentIndex, isFirstPerson);

	return true;
}

void C_BaseEntity::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	// Init should have been called before we get in here.
	Assert(GetEngineObject()->GetPartitionHandle() != PARTITION_INVALID_HANDLE );
	if ( entindex() < 0 )
		return;
	
	switch( state )
	{
	case SHOULDTRANSMIT_START:
		{
			// We've just been sent by the server. Become active.
			GetEngineObject()->SetDormant( false );
			
			GetEngineObject()->UpdatePartitionListEntry();

//#if !defined( NO_ENTITY_PREDICTION )
//			// Note that predictables get a chance to hook up to their server counterparts here
//			if ( m_PredictableID.IsActive() )
//			{
//				// Find corresponding client side predicted entity and remove it from predictables
//				m_PredictableID.SetAcknowledged( true );
//
//				C_BaseEntity *otherEntity = FindPreviouslyCreatedEntity( m_PredictableID );
//				if ( otherEntity )
//				{
//					Assert( otherEntity->IsClientCreated() );
//					Assert( otherEntity->m_PredictableID.IsActive() );
//					Assert( EntityList()->IsHandleValid( otherEntity->GetRefEHandle() ) );
//
//					otherEntity->m_PredictableID.SetAcknowledged( true );
//
//					if ( OnPredictedEntityRemove( false, otherEntity ) )
//					{
//						// Mark it for delete after receive all network data
//						DestroyEntity(otherEntity);// ->Release();
//					}
//				}
//			}
//#endif
		}
		break;

	case SHOULDTRANSMIT_END:
		{
			// Clear out links if we're out of the picture...
			GetEngineObject()->UnlinkFromHierarchy();

			// We're no longer being sent by the server. Become dormant.
			GetEngineObject()->SetDormant( true );
			
			// remove the entity from the KD tree so we won't collide against it
			partition->Remove( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS, GetEngineObject()->GetPartitionHandle() );
		
		}
		break;

	default:
		Assert( 0 );
		break;
	}

	if (IsBaseAnimating()) {
		if (state == SHOULDTRANSMIT_START)
		{
			// If he's been firing a bunch, then he comes back into the PVS, his muzzle flash
			// will show up even if he isn't firing now.
			GetEngineObject()->DisableMuzzleFlash();

			m_nPrevResetEventsParity = GetEngineObject()->GetResetEventsParity();
			m_nEventSequence = GetEngineObject()->GetSequence();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Entity is about to be decoded from the network stream
// Input  : bnewentity - is this a new entity this update?
//-----------------------------------------------------------------------------
void C_BaseEntity::PreDataUpdate( DataUpdateType_t updateType )
{
	VPROF( "C_BaseEntity::PreDataUpdate" );

#if 0 // Yahn suggesting commenting this out as a fix to demo recording not working
	// If the entity moves itself every FRAME on the server but doesn't update animtime,
	// then use the current server time as the time for interpolation.
	if ( IsSelfAnimating() )
	{
		m_flAnimTime = engine->GetLastTimeStamp();
	}
#endif

}

//-----------------------------------------------------------------------------
// Update move-parent if needed. For SourceTV.
//-----------------------------------------------------------------------------
void C_BaseEntity::HierarchyUpdateMoveParent()
{
	if (GetEngineObject()->GetNetworkMoveParent() && GetEngineObject()->GetNetworkMoveParent() == GetEngineObject()->GetMoveParent())
		return;

	GetEngineObject()->HierarchySetParent(GetEngineObject()->GetNetworkMoveParent());
}


//-----------------------------------------------------------------------------
// Purpose: Make sure that the correct model is referenced for this entity
//-----------------------------------------------------------------------------
void C_BaseEntity::ValidateModelIndex( void )
{
#ifdef TF_CLIENT_DLL
	if ( m_nModelIndexOverrides[VISION_MODE_NONE] > 0 ) 
	{
		if ( IsLocalPlayerUsingVisionFilterFlags( TF_VISION_FILTER_HALLOWEEN ) )
		{
			if ( m_nModelIndexOverrides[VISION_MODE_HALLOWEEN] > 0 )
			{
				SetModelByIndex( m_nModelIndexOverrides[VISION_MODE_HALLOWEEN] );
				return;
			}
		}
		
		if ( IsLocalPlayerUsingVisionFilterFlags( TF_VISION_FILTER_PYRO ) )
		{
			if ( m_nModelIndexOverrides[VISION_MODE_PYRO] > 0 )
			{
				SetModelByIndex( m_nModelIndexOverrides[VISION_MODE_PYRO] );
				return;
			}
		}

		if ( IsLocalPlayerUsingVisionFilterFlags( TF_VISION_FILTER_ROME ) )
		{
			if ( m_nModelIndexOverrides[VISION_MODE_ROME] > 0 )
			{
				SetModelByIndex( m_nModelIndexOverrides[VISION_MODE_ROME] );
				return;
			}
		}

		SetModelByIndex( m_nModelIndexOverrides[VISION_MODE_NONE] );		

		return;
	}
#endif

	SetModelByIndex( GetEngineObject()->GetModelIndex() );
}

//-----------------------------------------------------------------------------
// Purpose: Entity data has been parsed and unpacked.  Now do any necessary decoding, munging
// Input  : bnewentity - was this entity new in this update packet?
//-----------------------------------------------------------------------------
void C_BaseEntity::PostDataUpdate( DataUpdateType_t updateType )
{
	MDLCACHE_CRITICAL_SECTION();

	PREDICTION_TRACKVALUECHANGESCOPE_ENTITY( this, "postdataupdate" );

	// It's possible that a new entity will need to be forceably added to the 
	//   player simulation list.  If so, do this here
//#if !defined( NO_ENTITY_PREDICTION )
//	C_BasePlayer *local = (C_BasePlayer*)EntityList()->GetLocalPlayer();
//	if ( IsPlayerSimulated() &&
//		( NULL != local ) && 
//		( local == m_hOwnerEntity ) )
//	{
//		// Make sure player is driving simulation (field is only ever sent to local player)
//		SetPlayerSimulated( local );
//	}
//#endif

	GetEngineObject()->UpdatePartitionListEntry();
	
}

//bool C_BaseEntity::IsSelfAnimating()
//{
//	return true;
//}


//-----------------------------------------------------------------------------
// Sets the model... 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetModelByIndex( int nModelIndex )
{
	GetEngineObject()->SetModelIndex( nModelIndex );
}


//-----------------------------------------------------------------------------
// Set model... (NOTE: Should only be used by client-only entities
//-----------------------------------------------------------------------------
bool C_BaseEntity::SetModel( const char *pModelName )
{
	if ( pModelName )
	{
		int nModelIndex = modelinfo->GetModelIndex( pModelName );
		SetModelByIndex( nModelIndex );
		return ( nModelIndex != -1 );
	}
	else
	{
		SetModelByIndex( -1 );
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Default interpolation for entities
// Output : true means entity should be drawn, false means probably not
//-----------------------------------------------------------------------------
bool C_BaseEntity::Interpolate(IInterpolationContext* pContext, float currentTime )
{
	VPROF( "C_BaseEntity::Interpolate" );
	if (IsBaseAnimating()) 
	{
		// ragdolls don't need interpolation
		if (GetEngineObject()->RagdollBoneCount())
			return true;

		VPROF("C_BaseAnimating::Interpolate");

		Vector oldOrigin;
		QAngle oldAngles;
		Vector oldVel;
		float flOldCycle = GetEngineObject()->GetCycle();
		int nChangeFlags = 0;



		int bNoMoreChanges;
		int retVal = GetEngineObject()->BaseInterpolatePart1(pContext, currentTime, oldOrigin, oldAngles, oldVel, bNoMoreChanges);
		if (retVal == INTERPOLATE_STOP)
		{
			if (bNoMoreChanges)
				GetEngineObject()->RemoveFromInterpolationList();
			return true;
		}


		// Did cycle change?
		if (GetEngineObject()->GetCycle() != flOldCycle)
			nChangeFlags |= ANIMATION_CHANGED;

		if (bNoMoreChanges)
			GetEngineObject()->RemoveFromInterpolationList();

		GetEngineObject()->BaseInterpolatePart2(oldOrigin, oldAngles, oldVel, nChangeFlags);
		return true;
	}
	else 
	{
		Vector oldOrigin;
		QAngle oldAngles;
		Vector oldVel;

		int bNoMoreChanges;
		int retVal = GetEngineObject()->BaseInterpolatePart1(pContext, currentTime, oldOrigin, oldAngles, oldVel, bNoMoreChanges);

		// If all the Interpolate() calls returned that their values aren't going to
		// change anymore, then get us out of the interpolation list.
		if (bNoMoreChanges)
			GetEngineObject()->RemoveFromInterpolationList();

		if (retVal == INTERPOLATE_STOP)
			return true;

		int nChangeFlags = 0;
		GetEngineObject()->BaseInterpolatePart2(oldOrigin, oldAngles, oldVel, nChangeFlags);

		return true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: See if we should force reset our sequence on a new model
//-----------------------------------------------------------------------------
bool C_BaseEntity::ShouldResetSequenceOnNewModel(void)
{
	return (GetEngineObject()->GetReceivedSequence() == false);
}

IStudioHdr *C_BaseEntity::OnNewModel()
{
	if (IsBaseAnimating()) {

		//if ( m_bDynamicModelPending )
		//{
		//	modelinfo->UnregisterModelLoadCallback( -1, this );
		//	m_bDynamicModelPending = false;
		//}

		//m_AutoRefModelIndex.Clear();

		if (!GetEngineObject()->GetModel() || modelinfo->GetModelType(GetEngineObject()->GetModel()) != mod_studio)
			return NULL;

		// Reference (and thus start loading) dynamic model
		//int nNewIndex = m_nModelIndex;
		//if ( modelinfo->GetModel( nNewIndex ) != GetModel() )
		//{
		//	// XXX what's authoritative? the model pointer or the model index? what a mess.
		//	nNewIndex = modelinfo->GetModelIndex( modelinfo->GetModelName( GetModel() ) );
		//	Assert( nNewIndex < 0 || modelinfo->GetModel( nNewIndex ) == GetModel() );
		//	if ( nNewIndex < 0 )
		//		nNewIndex = m_nModelIndex;
		//}

		//m_AutoRefModelIndex = nNewIndex;
		//if ( IsDynamicModelIndex( nNewIndex ) && modelinfo->IsDynamicModelLoading( nNewIndex ) )
		//{
		//	m_bDynamicModelPending = true;
		//	modelinfo->RegisterModelLoadCallback( nNewIndex, this );
		//}

		//if ( IsDynamicModelLoading() )
		//{
		//	// Called while dynamic model still loading -> new model, clear deferred state
		//	m_bResetSequenceInfoOnLoad = false;
		//	return NULL;
		//}

		IStudioHdr* hdr = GetEngineObject()->GetModelPtr();
		if (hdr == NULL)
			return NULL;

		InitModelEffects();

		// lookup generic eye attachment, if exists
		m_iEyeAttachment = GetEngineObject()->LookupAttachment("eyes");

		// If we didn't have a model before, then we might need to go in the interpolation list now.
		if (ShouldInterpolate())
			GetEngineObject()->AddToInterpolationList();

		// objects with attachment points need to be queryable even if they're not solid
		if (hdr->GetNumAttachments() != 0)
		{
			GetEngineObject()->AddEFlags(EFL_USE_PARTITION_WHEN_NOT_SOLID);
		}


		// Most entities clear out their sequences when they change models on the server, but 
		// not all entities network down their m_nSequence (like multiplayer game player entities), 
		// so we may need to clear it out here. Force a SetSequence call no matter what, though.
		int forceSequence = ShouldResetSequenceOnNewModel() ? 0 : GetEngineObject()->GetSequence();

		if (GetEngineObject()->GetSequence() >= hdr->GetNumSeq())
		{
			forceSequence = 0;
		}

		GetEngineObject()->SetSequence(-1);
		GetEngineObject()->SetSequence(forceSequence);

		//if ( m_bResetSequenceInfoOnLoad )
		//{
		//	m_bResetSequenceInfoOnLoad = false;
		//	ResetSequenceInfo();
		//}

		GetEngineObject()->UpdateRelevantInterpolatedVars();

		return hdr;
	}
	else {
#ifdef TF_CLIENT_DLL
		m_bValidatedOwner = false;
#endif

		return NULL;
	}
}

void C_BaseEntity::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	return;
}

// Above this velocity and we'll assume a warp/teleport
#define MAX_INTERPOLATE_VELOCITY 4000.0f
#define MAX_INTERPOLATE_VELOCITY_PLAYER 1250.0f


//-----------------------------------------------------------------------------
// Purpose: Is this a submodel of the world ( model name starts with * )?
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsSubModel( void )
{
	if (GetEngineObject()->GetModel() &&
		modelinfo->GetModelType(GetEngineObject()->GetModel()) == mod_brush &&
		modelinfo->GetModelName(GetEngineObject()->GetModel())[0] == '*' )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Create entity lighting effects
//-----------------------------------------------------------------------------
void C_BaseEntity::CreateLightEffects( void )
{
	dlight_t *dl;

	// Is this for player flashlights only, if so move to linkplayers?
	if (entindex() == render->GetViewEntity() )
		return;

	if (GetEngineObject()->IsEffectActive(EF_BRIGHTLIGHT))
	{
		dl = effects->CL_AllocDlight (entindex());
		dl->origin = GetEngineObject()->GetAbsOrigin();
		dl->origin[2] += 16;
		dl->color.r = dl->color.g = dl->color.b = 250;
		dl->radius = random->RandomFloat(400,431);
		dl->die = gpGlobals->curtime + 0.001;
	}
	if (GetEngineObject()->IsEffectActive(EF_DIMLIGHT))
	{			
		dl = effects->CL_AllocDlight (entindex());
		dl->origin = GetEngineObject()->GetAbsOrigin();
		dl->color.r = dl->color.g = dl->color.b = 100;
		dl->radius = random->RandomFloat(200,231);
		dl->die = gpGlobals->curtime + 0.001;
	}
}

bool C_BaseEntity::ShouldInterpolate()
{
	if ( render->GetViewEntity() == entindex())
		return true;

	if (entindex() == 0 || !GetEngineObject()->GetModel() )
		return false;

	// always interpolate if visible
	if ( IsVisible() )
		return true;

	// if any movement child needs interpolation, we have to interpolate too
	IEngineObjectClient *pChild = GetEngineObject()->FirstMoveChild();
	while( pChild )
	{
		if ( pChild->GetOuter()->ShouldInterpolate() )	
			return true;

		pChild = pChild->NextMovePeer();
	}

	// don't interpolate
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Add entity to visibile entities list
//-----------------------------------------------------------------------------
void C_BaseEntity::AddEntity( void )
{
	// Don't ever add the world, it's drawn separately
	if (entindex() == 0 )
		return;

	if (IsBaseAnimating()) {
		// Server says don't interpolate this frame, so set previous info to new info.
		if (GetEngineObject()->IsNoInterpolationFrame())
		{
			GetEngineObject()->ResetLatched();
		}
	}

	// Create flashlight effects, etc.
	CreateLightEffects();
}


//-----------------------------------------------------------------------------
// Returns the aiment render origin + angles
//-----------------------------------------------------------------------------
void C_BaseEntity::GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pOrigin, QAngle *pAngles )
{
	// Should be overridden for things that attach to attchment points
	if (IsBaseAnimating()) 
	{
		IEngineObjectClient* pMoveParent;
		if (GetEngineObject()->IsEffectActive(EF_BONEMERGE) && GetEngineObject()->IsEffectActive(EF_BONEMERGE_FASTCULL) && (pMoveParent = GetEngineObject()->GetMoveParent()) != NULL)
		{
			// Doing this saves a lot of CPU.
			*pOrigin = pMoveParent->GetOuter()->WorldSpaceCenter();
			*pAngles = pMoveParent->GetOuter()->GetRenderAngles();
		}
		else
		{
			if (!GetEngineObject()->GetAimEntOrigin(pOrigin, pAngles)) 
			{
				// Slam origin to the origin of the entity we are attached to...
				*pOrigin = ((C_BaseEntity*)pAttachedTo)->GetEngineObject()->GetAbsOrigin();
				*pAngles = ((C_BaseEntity*)pAttachedTo)->GetEngineObject()->GetAbsAngles();
			}
		}
	}
	else 
	{
		// Slam origin to the origin of the entity we are attached to...
		*pOrigin = ((C_BaseEntity*)pAttachedTo)->GetEngineObject()->GetAbsOrigin();
		*pAngles = ((C_BaseEntity*)pAttachedTo)->GetEngineObject()->GetAbsAngles();
	}	
}

//-----------------------------------------------------------------------------
// Default implementation for GetTextureAnimationStartTime
//-----------------------------------------------------------------------------
float C_BaseEntity::GetTextureAnimationStartTime()
{
	return GetEngineObject()->GetSpawnTime();
}

//-----------------------------------------------------------------------------
// Default implementation, indicates that a texture animation has wrapped
//-----------------------------------------------------------------------------
void C_BaseEntity::TextureAnimationWrapped()
{
}


void C_BaseEntity::ClientThink()
{
}

void C_BaseEntity::Simulate()
{
	if (IsBaseAnimating()) {
		if (m_bInitModelEffects)
		{
			DelayedInitModelEffects();
		}

		if (gpGlobals->frametime != 0.0f)
		{
			DoAnimationEvents(GetEngineObject()->GetModelPtr());
		}
	}
	GetEngineObject()->Simulate();
	AddEntity();	// Legacy support. Once-per-frame stuff should go in Simulate().
	if (IsBaseAnimating()) {
		if (GetEngineObject()->IsNoInterpolationFrame())
		{
			GetEngineObject()->ResetLatched();
		}
	}
}

float C_BaseEntity::GetAnimTimeInterval(void) const
{
#define MAX_ANIMTIME_INTERVAL 0.2f

	float flInterval = MIN(gpGlobals->curtime - GetEngineObject()->GetAnimTime(), MAX_ANIMTIME_INTERVAL);
	return flInterval;
}

// (static function)
//void C_BaseEntity::AddVisibleEntities()
//{
//#if !defined( NO_ENTITY_PREDICTION )
//	VPROF_BUDGET( "C_BaseEntity::AddVisibleEntities", VPROF_BUDGETGROUP_WORLD_RENDERING );
//
//	// Let non-dormant client created predictables get added, too
//	int c = predictables->GetPredictableCount();
//	for ( int i = 0 ; i < c ; i++ )
//	{
//		C_BaseEntity *pEnt = predictables->GetPredictable( i );
//		if ( !pEnt )
//			continue;
//
//		if ( !pEnt->IsClientCreated() )
//			continue;
//
//		// Only draw until it's ack'd since that means a real entity has arrived
//		if ( pEnt->m_PredictableID.GetAcknowledged() )
//			continue;
//
//		// Don't draw if dormant
//		if ( pEnt->IsDormantPredictable() )
//			continue;
//
//		pEnt->UpdateVisibility();	
//	}
//#endif
//}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void C_BaseEntity::OnPreDataChanged( DataUpdateType_t type )
{

}

void C_BaseEntity::OnDataChanged( DataUpdateType_t type )
{
	// Set up shadows; do it here so that objects can change shadowcasting state
	GetEngineObject()->CreateShadow();

	if ( type == DATA_UPDATE_CREATED )
	{
		UpdateVisibility();
	}
}




//-----------------------------------------------------------------------------
// Purpose: This routine modulates renderamt according to m_nRenderFX's value
//  This is a client side effect and will not be in-sync on machines across a
//  network game.
// Input  : origin - 
//			alpha - 
// Output : int
//-----------------------------------------------------------------------------
void C_BaseEntity::ComputeFxBlend( void )
{
	// Don't recompute if we've already computed this frame
	if ( m_nFXComputeFrame == gpGlobals->framecount )
		return;

	MDLCACHE_CRITICAL_SECTION();
	int blend=0;
	float offset;

	offset = ((int)entindex()) * 363.0;// Use ent index to de-sync these fx

	switch(GetEngineObject()->GetRenderFX() )
	{
	case kRenderFxPulseSlowWide:
		blend = GetEngineObject()->GetRenderColor().a + 0x40 * sin( gpGlobals->curtime * 2 + offset );
		break;
		
	case kRenderFxPulseFastWide:
		blend = GetEngineObject()->GetRenderColor().a + 0x40 * sin( gpGlobals->curtime * 8 + offset );
		break;
	
	case kRenderFxPulseFastWider:
		blend = ( 0xff * fabs(sin( gpGlobals->curtime * 12 + offset ) ) );
		break;

	case kRenderFxPulseSlow:
		blend = GetEngineObject()->GetRenderColor().a + 0x10 * sin( gpGlobals->curtime * 2 + offset );
		break;
		
	case kRenderFxPulseFast:
		blend = GetEngineObject()->GetRenderColor().a + 0x10 * sin( gpGlobals->curtime * 8 + offset );
		break;
		
	// JAY: HACK for now -- not time based
	case kRenderFxFadeSlow:			
		if (GetEngineObject()->GetRenderColor().a > 0 )
		{
			GetEngineObject()->SetRenderColorA(GetEngineObject()->GetRenderColor().a - 1 );
		}
		else
		{
			GetEngineObject()->SetRenderColorA( 0 );
		}
		blend = GetEngineObject()->GetRenderColor().a;
		break;
		
	case kRenderFxFadeFast:
		if (GetEngineObject()->GetRenderColor().a > 3 )
		{
			GetEngineObject()->SetRenderColorA(GetEngineObject()->GetRenderColor().a - 4 );
		}
		else
		{
			GetEngineObject()->SetRenderColorA( 0 );
		}
		blend = GetEngineObject()->GetRenderColor().a;
		break;
		
	case kRenderFxSolidSlow:
		if (GetEngineObject()->GetRenderColor().a < 255 )
		{
			GetEngineObject()->SetRenderColorA(GetEngineObject()->GetRenderColor().a + 1 );
		}
		else
		{
			GetEngineObject()->SetRenderColorA( 255 );
		}
		blend = GetEngineObject()->GetRenderColor().a;
		break;
		
	case kRenderFxSolidFast:
		if (GetEngineObject()->GetRenderColor().a < 252 )
		{
			GetEngineObject()->SetRenderColorA(GetEngineObject()->GetRenderColor().a + 4 );
		}
		else
		{
			GetEngineObject()->SetRenderColorA( 255 );
		}
		blend = GetEngineObject()->GetRenderColor().a;
		break;
		
	case kRenderFxStrobeSlow:
		blend = 20 * sin( gpGlobals->curtime * 4 + offset );
		if ( blend < 0 )
		{
			blend = 0;
		}
		else
		{
			blend = GetEngineObject()->GetRenderColor().a;
		}
		break;
		
	case kRenderFxStrobeFast:
		blend = 20 * sin( gpGlobals->curtime * 16 + offset );
		if ( blend < 0 )
		{
			blend = 0;
		}
		else
		{
			blend = GetEngineObject()->GetRenderColor().a;
		}
		break;
		
	case kRenderFxStrobeFaster:
		blend = 20 * sin( gpGlobals->curtime * 36 + offset );
		if ( blend < 0 )
		{
			blend = 0;
		}
		else
		{
			blend = GetEngineObject()->GetRenderColor().a;
		}
		break;
		
	case kRenderFxFlickerSlow:
		blend = 20 * (sin( gpGlobals->curtime * 2 ) + sin( gpGlobals->curtime * 17 + offset ));
		if ( blend < 0 )
		{
			blend = 0;
		}
		else
		{
			blend = GetEngineObject()->GetRenderColor().a;
		}
		break;
		
	case kRenderFxFlickerFast:
		blend = 20 * (sin( gpGlobals->curtime * 16 ) + sin( gpGlobals->curtime * 23 + offset ));
		if ( blend < 0 )
		{
			blend = 0;
		}
		else
		{
			blend = GetEngineObject()->GetRenderColor().a;
		}
		break;
		
	case kRenderFxHologram:
	case kRenderFxDistort:
		{
			Vector	tmp;
			float	dist;
			
			VectorCopy(GetEngineObject()->GetAbsOrigin(), tmp );
			VectorSubtract( tmp, g_pViewRender->CurrentViewOrigin(), tmp );
			dist = DotProduct( tmp, g_pViewRender->CurrentViewForward() );
			
			// Turn off distance fade
			if (GetEngineObject()->GetRenderFX() == kRenderFxDistort)
			{
				dist = 1;
			}
			if ( dist <= 0 )
			{
				blend = 0;
			}
			else 
			{
				GetEngineObject()->SetRenderColorA( 180 );
				if ( dist <= 100 )
					blend = GetEngineObject()->GetRenderColor().a;
				else
					blend = (int) ((1.0 - (dist - 100) * (1.0 / 400.0)) * GetEngineObject()->GetRenderColor().a);
				blend += random->RandomInt(-32,31);
			}
		}
		break;
	
	case kRenderFxNone:
	case kRenderFxClampMinScale:
	default:
		if (GetEngineObject()->GetRenderMode() == kRenderNormal)
			blend = 255;
		else
			blend = GetEngineObject()->GetRenderColor().a;
		break;	
		
	}

	blend = clamp( blend, 0, 255 );

	// Look for client-side fades
	unsigned char nFadeAlpha = GetClientSideFade();
	if ( nFadeAlpha != 255 )
	{
		float flBlend = blend / 255.0f;
		float flFade = nFadeAlpha / 255.0f;
		blend = (int)( flBlend * flFade * 255.0f + 0.5f );
		blend = clamp( blend, 0, 255 );
	}

	m_nRenderFXBlend = blend;
	m_nFXComputeFrame = gpGlobals->framecount;

	// Update the render group
	if (GetEngineObject()->GetRenderHandle() != INVALID_CLIENT_RENDER_HANDLE )
	{
		ClientLeafSystem()->SetRenderGroup(GetEngineObject()->GetRenderHandle(), GetRenderGroup() );
	}

	// Tell our shadow
	if (GetEngineObject()->GetShadowHandle() != CLIENTSHADOW_INVALID_HANDLE )
	{
		g_pClientShadowMgr->SetFalloffBias(GetEngineObject()->GetShadowHandle(), (255 - m_nRenderFXBlend));
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BaseEntity::GetFxBlend( void )
{
	Assert( m_nFXComputeFrame == gpGlobals->framecount );
	return m_nRenderFXBlend;
}

//-----------------------------------------------------------------------------
// Determine the color modulation amount
//-----------------------------------------------------------------------------

void C_BaseEntity::GetColorModulation( float* color )
{
	color[0] = GetEngineObject()->GetRenderColor().r / 255.0f;
	color[1] = GetEngineObject()->GetRenderColor().g / 255.0f;
	color[2] = GetEngineObject()->GetRenderColor().b / 255.0f;
}


//-----------------------------------------------------------------------------
// Returns true if we should add this to the collision list
//-----------------------------------------------------------------------------
CollideType_t C_BaseEntity::GetCollideType( void )
{
	if (IsBaseAnimating()) {
		if (GetEngineObject()->IsRagdoll())
			return ENTITY_SHOULD_RESPOND;

		//return BaseClass::GetCollideType();
	}
	
	if (!GetEngineObject()->GetModelIndex() || !GetEngineObject()->GetModel())
		return ENTITY_SHOULD_NOT_COLLIDE;

	if (!GetEngineObject()->IsSolid())
		return ENTITY_SHOULD_NOT_COLLIDE;

	// If the model is a bsp or studio (i.e. it can collide with the player
	if ((modelinfo->GetModelType(GetEngineObject()->GetModel()) != mod_brush) && (modelinfo->GetModelType(GetEngineObject()->GetModel()) != mod_studio))
		return ENTITY_SHOULD_NOT_COLLIDE;

	// Don't get stuck on point sized entities ( world doesn't count )
	if (GetEngineObject()->GetModelIndex() != 1)
	{
		if (GetEngineObject()->IsPointSized())
			return ENTITY_SHOULD_NOT_COLLIDE;
	}

	return ENTITY_SHOULD_COLLIDE;
}


//-----------------------------------------------------------------------------
// Is this a brush model?
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsBrushModel() const
{
	int modelType = modelinfo->GetModelType(GetEngineObject()->GetModel());
	return (modelType == mod_brush);
}


//-----------------------------------------------------------------------------
// This method works when we've got a studio model
//-----------------------------------------------------------------------------
void C_BaseEntity::AddStudioDecal( const Ray_t& ray, int hitbox, int decalIndex, 
								  bool doTrace, trace_t& tr, int maxLODToDecal )
{
	if (doTrace)
	{
		enginetrace->ClipRayToEntity( ray, MASK_SHOT, this, &tr );

		// Trace the ray against the entity
		if (tr.fraction == 1.0f)
			return;

		// Set the trace index appropriately...
		tr.m_pEnt = this;
	}

	// Exit out after doing the trace so any other effects that want to happen can happen.
	if ( !r_drawmodeldecals.GetBool() )
		return;

	// Found the point, now lets apply the decals
	GetEngineObject()->CreateModelInstance();

	// FIXME: Pass in decal up?
	Vector up(0, 0, 1);

	if (doTrace && (GetEngineObject()->GetSolid() == SOLID_VPHYSICS) && !tr.startsolid && !tr.allsolid)
	{
		// Choose a more accurate normal direction
		// Also, since we have more accurate info, we can avoid pokethru
		Vector temp;
		VectorSubtract( tr.endpos, tr.plane.normal, temp );
		Ray_t betterRay;
		betterRay.Init( tr.endpos, temp );
		modelrender->AddDecal(GetEngineObject()->GetModelInstance(), betterRay, up, decalIndex, GetEngineObject()->GetBody(), true, maxLODToDecal);
	}
	else
	{
		modelrender->AddDecal(GetEngineObject()->GetModelInstance(), ray, up, decalIndex, GetEngineObject()->GetBody(), false, maxLODToDecal);
	}
}

//-----------------------------------------------------------------------------
void C_BaseEntity::AddColoredStudioDecal( const Ray_t& ray, int hitbox, int decalIndex, 
	bool doTrace, trace_t& tr, Color cColor, int maxLODToDecal )
{
	if (doTrace)
	{
		enginetrace->ClipRayToEntity( ray, MASK_SHOT, this, &tr );

		// Trace the ray against the entity
		if (tr.fraction == 1.0f)
			return;

		// Set the trace index appropriately...
		tr.m_pEnt = this;
	}

	// Exit out after doing the trace so any other effects that want to happen can happen.
	if ( !r_drawmodeldecals.GetBool() )
		return;

	// Found the point, now lets apply the decals
	GetEngineObject()->CreateModelInstance();

	// FIXME: Pass in decal up?
	Vector up(0, 0, 1);

	if (doTrace && (GetEngineObject()->GetSolid() == SOLID_VPHYSICS) && !tr.startsolid && !tr.allsolid)
	{
		// Choose a more accurate normal direction
		// Also, since we have more accurate info, we can avoid pokethru
		Vector temp;
		VectorSubtract( tr.endpos, tr.plane.normal, temp );
		Ray_t betterRay;
		betterRay.Init( tr.endpos, temp );
		modelrender->AddColoredDecal(GetEngineObject()->GetModelInstance(), betterRay, up, decalIndex, GetEngineObject()->GetBody(), cColor, true, maxLODToDecal);
	}
	else
	{
		modelrender->AddColoredDecal(GetEngineObject()->GetModelInstance(), ray, up, decalIndex, GetEngineObject()->GetBody(), cColor, false, maxLODToDecal);
	}
}


//-----------------------------------------------------------------------------
// This method works when we've got a brush model
//-----------------------------------------------------------------------------
void C_BaseEntity::AddBrushModelDecal( const Ray_t& ray, const Vector& decalCenter, 
									  int decalIndex, bool doTrace, trace_t& tr )
{
	if ( doTrace )
	{
		enginetrace->ClipRayToEntity( ray, MASK_SHOT, this, &tr );
		if ( tr.fraction == 1.0f )
			return;
	}

	effects->DecalShoot( decalIndex, entindex(),
		GetEngineObject()->GetModel(), GetEngineObject()->GetAbsOrigin(), GetEngineObject()->GetAbsAngles(), decalCenter, 0, 0 );
}


//-----------------------------------------------------------------------------
// A method to apply a decal to an entity
//-----------------------------------------------------------------------------
void C_BaseEntity::AddDecal( const Vector& rayStart, const Vector& rayEnd,
		const Vector& decalCenter, int hitbox, int decalIndex, bool doTrace, trace_t& tr, int maxLODToDecal )
{
	Ray_t ray;
	ray.Init( rayStart, rayEnd );

	// FIXME: Better bloat?
	// Bloat a little bit so we get the intersection
	ray.m_Delta *= 1.1f;

	int modelType = modelinfo->GetModelType(GetEngineObject()->GetModel());
	switch ( modelType )
	{
	case mod_studio:
		AddStudioDecal( ray, hitbox, decalIndex, doTrace, tr, maxLODToDecal );
		break;

	case mod_brush:
		AddBrushModelDecal( ray, decalCenter, decalIndex, doTrace, tr );
		break;

	default:
		// By default, no collision
		tr.fraction = 1.0f;
		break;
	}
}

//-----------------------------------------------------------------------------
void C_BaseEntity::AddColoredDecal( const Vector& rayStart, const Vector& rayEnd,
	const Vector& decalCenter, int hitbox, int decalIndex, bool doTrace, trace_t& tr, Color cColor, int maxLODToDecal )
{
	Ray_t ray;
	ray.Init( rayStart, rayEnd );
	
	// FIXME: Better bloat?
	// Bloat a little bit so we get the intersection
	ray.m_Delta *= 1.1f;

	int modelType = modelinfo->GetModelType(GetEngineObject()->GetModel());
	if ( doTrace )
	{
		enginetrace->ClipRayToEntity( ray, MASK_SHOT, this, &tr );
		switch ( modelType )
		{
		case mod_studio:
			tr.m_pEnt = this;
			break;
		case mod_brush:
			if ( tr.fraction == 1.0f )
				return;		// Explicitly end
		default:
			// By default, no collision
			tr.fraction = 1.0f;
			break;
		}
	}

	switch ( modelType )
	{
	case mod_studio:
		AddColoredStudioDecal( ray, hitbox, decalIndex, doTrace, tr, cColor, maxLODToDecal );
		break;

	case mod_brush:
		{
			color32 cColor32 = { (uint8)cColor.r(), (uint8)cColor.g(), (uint8)cColor.b(), (uint8)cColor.a() };
			effects->DecalColorShoot( decalIndex, entindex(), GetEngineObject()->GetModel(), GetEngineObject()->GetAbsOrigin(), GetEngineObject()->GetAbsAngles(), decalCenter, 0, 0, cColor32 );
		}
		break;

	default:
		// By default, no collision
		tr.fraction = 1.0f;
		break;
	}
}

//-----------------------------------------------------------------------------
// A method to remove all decals from an entity
//-----------------------------------------------------------------------------
void C_BaseEntity::RemoveAllDecals( void )
{
	// For now, we only handle removing decals from studiomodels
	if ( modelinfo->GetModelType(GetEngineObject()->GetModel()) == mod_studio )
	{
		GetEngineObject()->CreateModelInstance();
		modelrender->RemoveAllDecals(GetEngineObject()->GetModelInstance() );
	}
}



#include "tier0/memdbgoff.h"

//-----------------------------------------------------------------------------
// C_BaseEntity new/delete
// All fields in the object are all initialized to 0.
//-----------------------------------------------------------------------------
void *C_BaseEntity::operator new( size_t stAllocateBlock )
{
	Assert( stAllocateBlock != 0 );	
	MEM_ALLOC_CREDIT();
	void *pMem = MemAlloc_Alloc( stAllocateBlock );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}

void *C_BaseEntity::operator new[]( size_t stAllocateBlock )
{
	Assert( stAllocateBlock != 0 );				
	MEM_ALLOC_CREDIT();
	void *pMem = MemAlloc_Alloc( stAllocateBlock );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}

void *C_BaseEntity::operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	Assert( stAllocateBlock != 0 );	
	void *pMem = MemAlloc_Alloc( stAllocateBlock, pFileName, nLine );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}

void *C_BaseEntity::operator new[]( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	Assert( stAllocateBlock != 0 );				
	void *pMem = MemAlloc_Alloc( stAllocateBlock, pFileName, nLine );
	memset( pMem, 0, stAllocateBlock );
	return pMem;												
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMem - 
//-----------------------------------------------------------------------------
void C_BaseEntity::operator delete( void *pMem )
{
	// get the engine to free the memory
	MemAlloc_Free( pMem );
}

#include "tier0/memdbgon.h"

//========================================================================================
// TEAM HANDLING
//========================================================================================
C_Team *C_BaseEntity::GetTeam( void )
{
	return GetGlobalTeam( m_iTeamNum );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseEntity::GetTeamNumber( void ) const
{
	return m_iTeamNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_BaseEntity::GetRenderTeamNumber( void )
{
	return GetTeamNumber();
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if these entities are both in at least one team together
//-----------------------------------------------------------------------------
bool C_BaseEntity::InSameTeam( C_BaseEntity *pEntity )
{
	if ( !pEntity )
		return false;

	return ( pEntity->GetTeam() == GetTeam() );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the entity's on the same team as the local player
//-----------------------------------------------------------------------------
bool C_BaseEntity::InLocalTeam( void )
{
	return ( GetTeam() == GetLocalTeam() );
}


//-----------------------------------------------------------------------------
// Purpose: Flags this entity as being inside or outside of this client's PVS
//			on the server.
//			NOTE: this is meaningless for client-side only entities.
// Input  : inside_pvs - 
//-----------------------------------------------------------------------------
void C_BaseEntity::AfterSetDormant( bool bOldDormant )
{
	Assert( IsNetworkable() );
	// Kill drawing if we became dormant.
	UpdateVisibility();
	ParticleProp()->OwnerSetDormantTo(GetEngineObject()->IsDormant() );
}



//-----------------------------------------------------------------------------
// Purpose: Tells the entity that it's about to be destroyed due to the client receiving
// an uncompressed update that's caused it to destroy all entities & recreate them.
//-----------------------------------------------------------------------------
void C_BaseEntity::SetDestroyedOnRecreateEntities( void )
{
	// Robin: We need to destroy all our particle systems immediately, because 
	// we're about to be recreated, and their owner EHANDLEs will match up to 
	// the new entity, but it won't know anything about them.
	ParticleProp()->StopEmissionAndDestroyImmediately();
}



/*
void C_BaseEntity::SetAbsAngularVelocity( const QAngle &vecAbsAngVelocity )
{
	// The abs velocity won't be dirty since we're setting it here
	InvalidatePhysicsRecursive( EFL_DIRTY_ABSANGVELOCITY );
	m_iEFlags &= ~EFL_DIRTY_ABSANGVELOCITY;

	m_vecAbsAngVelocity = vecAbsAngVelocity;

	C_BaseEntity *pMoveParent = GetMoveParent();
	if (!pMoveParent)
	{
		m_vecAngVelocity = vecAbsAngVelocity;
		return;
	}

	// First subtract out the parent's abs velocity to get a relative
	// angular velocity measured in world space
	QAngle relAngVelocity;
	relAngVelocity = vecAbsAngVelocity - pMoveParent->GetAbsAngularVelocity();

	matrix3x4_t entityToWorld;
	AngleMatrix( relAngVelocity, entityToWorld );

	// Moveparent case: transform the abs angular vel into local space
	matrix3x4_t worldToParent, localMatrix;
	MatrixInvert( pMoveParent->EntityToWorldTransform(), worldToParent );
	ConcatTransforms( worldToParent, entityToWorld, localMatrix );
	MatrixAngles( localMatrix, m_vecAngVelocity );
}
*/

//-----------------------------------------------------------------------------
// Sets the local position from a transform
//-----------------------------------------------------------------------------
void C_BaseEntity::SetLocalTransform( const matrix3x4_t &localTransform )
{
	Vector vecLocalOrigin;
	QAngle vecLocalAngles;
	MatrixGetColumn( localTransform, 3, vecLocalOrigin );
	MatrixAngles( localTransform, vecLocalAngles );
	GetEngineObject()->SetLocalOrigin( vecLocalOrigin );
	GetEngineObject()->SetLocalAngles( vecLocalAngles );
}


//-----------------------------------------------------------------------------
// FIXME: REMOVE!!!
//-----------------------------------------------------------------------------
void C_BaseEntity::MoveToAimEnt( )
{
	Vector vecAimEntOrigin;
	QAngle vecAimEntAngles;
	GetAimEntOrigin((GetEngineObject()->GetMoveParent()?GetEngineObject()->GetMoveParent()->GetOuter():NULL), &vecAimEntOrigin, &vecAimEntAngles);
	GetEngineObject()->SetAbsOrigin( vecAimEntOrigin );
	GetEngineObject()->SetAbsAngles( vecAimEntAngles );
}


void C_BaseEntity::BoneMergeFastCullBloat( Vector &localMins, Vector &localMaxs, const Vector &thisEntityMins, const Vector &thisEntityMaxs ) const
{
	// By default, we bloat the bbox for fastcull ents by the maximum length it could hang out of the parent bbox,
	// it one corner were touching the edge of the parent's box, and the whole diagonal stretched out.
	float flExpand = (thisEntityMaxs - thisEntityMins).Length();

	localMins.x -= flExpand;
	localMins.y -= flExpand;
	localMins.z -= flExpand;

	localMaxs.x += flExpand;
	localMaxs.y += flExpand;
	localMaxs.z += flExpand;
}




/*
void C_BaseEntity::CalcAbsoluteAngularVelocity()
{
	if ((m_iEFlags & EFL_DIRTY_ABSANGVELOCITY ) == 0)
		return;

	m_iEFlags &= ~EFL_DIRTY_ABSANGVELOCITY;

	CBaseEntity *pMoveParent = GetMoveParent();
	if ( !pMoveParent )
	{
		m_vecAbsAngVelocity = m_vecAngVelocity;
		return;
	}

	matrix3x4_t angVelToParent, angVelToWorld;
	AngleMatrix( m_vecAngVelocity, angVelToParent );
	ConcatTransforms( pMoveParent->EntityToWorldTransform(), angVelToParent, angVelToWorld );
	MatrixAngles( angVelToWorld, m_vecAbsAngVelocity );

	// Now add in the parent abs angular velocity
	m_vecAbsAngVelocity += pMoveParent->GetAbsAngularVelocity();
}
*/

//-----------------------------------------------------------------------------
// Purpose: Just look up index
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseEntity::PrecacheModel( const char *name )
{
	return modelinfo->GetModelIndex( name );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
//bool C_BaseEntity::GetPredictionEligible( void ) const
//{
//#if !defined( NO_ENTITY_PREDICTION )
//	return m_bPredictionEligible;
//#else
//	return false;
//#endif
//}


C_BaseEntity* C_BaseEntity::Instance( CBaseHandle hEnt )
{
	return (C_BaseEntity*)EntityList()->GetBaseEntityFromHandle( hEnt );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iEnt - 
// Output : C_BaseEntity
//-----------------------------------------------------------------------------
C_BaseEntity *C_BaseEntity::Instance( int iEnt )
{
	return (C_BaseEntity*)EntityList()->GetBaseEntity( iEnt );
}

#ifdef WIN32
#pragma warning( push )
#include <typeinfo>
#pragma warning( pop )
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *C_BaseEntity::GetClassname( void ) const
{
	static char outstr[ 256 ];
	outstr[ 0 ] = 0;
	bool gotname = false;
#ifndef NO_ENTITY_PREDICTION
	if ( GetPredDescMap() )
	{
		const char *mapname =  EntityList()->GetMapClassName( GetPredDescMap()->dataClassName );
		if ( mapname && mapname[ 0 ] ) 
		{
			Q_strncpy( outstr, mapname, sizeof( outstr ) );
			gotname = true;
		}
	}
#endif

	if ( !gotname )
	{
		Q_strncpy( outstr, typeid( *this ).name(), sizeof( outstr ) );
	}

	return outstr;
}

const char *C_BaseEntity::GetDebugName( void ) const
{
	return GetClassname();
}

//-----------------------------------------------------------------------------
// Purpose: Creates an entity by string name, but does not spawn it
// Input  : *className - 
// Output : C_BaseEntity
//-----------------------------------------------------------------------------
//C_BaseEntity *CreateEntityByName( const char *className )
//{
//	C_BaseEntity *ent = GetClassMap().CreateEntity( className );
//	if ( ent )
//	{
//		return ent;
//	}
//
//	Warning( "Can't find factory for entity: %s\n", className );
//	return NULL;
//}

#ifdef _DEBUG
CON_COMMAND( cl_sizeof, "Determines the size of the specified client class." )
{
	if ( args.ArgC() != 2 )
	{
		Msg( "cl_sizeof <gameclassname>\n" );
		return;
	}

	int size = EntityList()->GetEntitySize( args[ 1 ] );

	Msg( "%s is %i bytes\n", args[ 1 ], size );
}
#endif

CON_COMMAND_F( dlight_debug, "Creates a dlight in front of the player", FCVAR_CHEAT )
{
	dlight_t *el = effects->CL_AllocDlight( 1 );
	C_BasePlayer *player = (C_BasePlayer*)EntityList()->GetLocalPlayer();
	if ( !player )
		return;
	Vector start = player->EyePosition();
	Vector forward;
	player->EyeVectors( &forward );
	Vector end = start + forward * MAX_TRACE_LENGTH;
	trace_t tr;
	UTIL_TraceLine(EntityList(), start, end, MASK_SHOT_HULL & (~CONTENTS_GRATE), player, COLLISION_GROUP_NONE, &tr );
	el->origin = tr.endpos - forward * 12.0f;
	el->radius = 200; 
	el->decay = el->radius / 5.0f;
	el->die = gpGlobals->curtime + 5.0f;
	el->color.r = 255;
	el->color.g = 192;
	el->color.b = 64;
	el->color.exponent = 5;

}
//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
//bool C_BaseEntity::IsClientCreated( void ) const
//{
//#ifndef NO_ENTITY_PREDICTION
//	if ( m_pPredictionContext != NULL )
//	{
//		// For now can't be both
//		Assert( !GetPredictable() );
//		return true;
//	}
//#endif
//	return false;
//}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *classname - 
//			*module - 
//			line - 
// Output : C_BaseEntity
//-----------------------------------------------------------------------------
//C_BaseEntity *C_BaseEntity::CreatePredictedEntityByName( const char *classname, const char *module, int line, bool persist /*= false */ )
//{
//#if !defined( NO_ENTITY_PREDICTION )
//	C_BasePlayer *player = C_BaseEntity::GetPredictionPlayer();
//
//	Assert( player );
//	Assert( player->m_pCurrentCommand );
//	Assert( prediction->InPrediction() );
//
//	C_BaseEntity *ent = NULL;
//
//	// What's my birthday (should match server)
//	int command_number	= player->m_pCurrentCommand->command_number;
//	// Who's my daddy?
//	int player_index	= player->entindex() - 1;
//
//	// Create id/context
//	CPredictableId testId;
//	testId.Init( player_index, command_number, classname, module, line );
//
//	// If repredicting, should be able to find the entity in the previously created list
//	if ( !prediction->IsFirstTimePredicted() )
//	{
//		// Only find previous instance if entity was created with persist set
//		if ( persist )
//		{
//			ent = FindPreviouslyCreatedEntity( testId );
//			if ( ent )
//			{
//				return ent;
//			}
//		}
//
//		return NULL;
//	}
//
//	// Try to create it
//	ent = EntityList()->CreateEntityByName( classname );
//	if ( !ent )
//	{
//		return NULL;
//	}
//
//	// It's predictable
//	ent->SetPredictionEligible( true );
//
//	// Set up "shared" id number
//	ent->m_PredictableID.SetRaw( testId.GetRaw() );
//
//	// Get a context (mostly for debugging purposes)
//	PredictionContext *context			= new PredictionContext;
//	context->m_bActive					= true;
//	context->m_nCreationCommandNumber	= command_number;
//	context->m_nCreationLineNumber		= line;
//	context->m_pszCreationModule		= module;
//
//	// Attach to entity
//	ent->m_pPredictionContext = context;
//
//	// Add to client entity list
//	//EntityList()->AddNonNetworkableEntity( ent );
//
//	//  and predictables
//	g_Predictables.AddToPredictableList( ent->GetRefEHandle() );
//
//	// Duhhhh..., but might as well be safe
//	Assert( !ent->GetPredictable() );
//	Assert( ent->IsClientCreated() );
//
//	// Add the client entity to the spatial partition. (Collidable)
//	ent->GetEngineObject()->CreatePartitionHandle();
//
//	// CLIENT ONLY FOR NOW!!!
//	ent->index = -1;
//
//	if ( AddDataChangeEvent( ent, DATA_UPDATE_CREATED, &ent->m_DataChangeEventRef ) )
//	{
//		ent->OnPreDataChanged( DATA_UPDATE_CREATED );
//	}
//
//	ent->Interp_UpdateInterpolationAmounts( ent->GetVarMapping() );
//	
//	return ent;
//#else
//	return NULL;
//#endif
//}

//-----------------------------------------------------------------------------
// Purpose: Called each packet that the entity is created on and finally gets called after the next packet
//  that doesn't have a create message for the "parent" entity so that the predicted version
//  can be removed.  Return true to delete entity right away.
//-----------------------------------------------------------------------------
//bool C_BaseEntity::OnPredictedEntityRemove( bool isbeingremoved, C_BaseEntity *predicted )
//{
//#if !defined( NO_ENTITY_PREDICTION )
//	// Nothing right now, but in theory you could look at the error in origins and set
//	//  up something to smooth out the error
//	PredictionContext *ctx = predicted->m_pPredictionContext;
//	Assert( ctx );
//	if ( ctx )
//	{
//		// Create backlink to actual entity
//		ctx->m_hServerEntity = this;
//
//		/*
//		Msg( "OnPredictedEntity%s:  %s created %s(%i) instance(%i)\n",
//			isbeingremoved ? "Remove" : "Acknowledge",
//			predicted->GetClassname(),
//			ctx->m_pszCreationModule,
//			ctx->m_nCreationLineNumber,
//			predicted->m_PredictableID.GetInstanceNumber() );
//		*/
//	}
//
//	// If it comes through with an ID, it should be eligible
//	SetPredictionEligible( true );
//
//	// Start predicting simulation forward from here
//	CheckInitPredictable( "OnPredictedEntityRemove" );
//
//	// Always mark it dormant since we are the "real" entity now
//	predicted->SetDormantPredictable( true );
//
//	InvalidatePhysicsRecursive( POSITION_CHANGED | ANGLES_CHANGED | VELOCITY_CHANGED );
//
//	// By default, signal that it should be deleted right away
//	// If a derived class implements this method, it might chain to here but return
//	// false if it wants to keep the dormant predictable around until the chain of
//	//  DATA_UPDATE_CREATED messages passes
//#endif
//	return true;
//}



//-----------------------------------------------------------------------------
// Purpose: Put the entity in the specified team
//-----------------------------------------------------------------------------
void C_BaseEntity::ChangeTeam( int iTeamNum )
{
	m_iTeamNum = iTeamNum;
}

//-----------------------------------------------------------------------------
// Purpose: Nothing yet, could eventually supercede Term()
//-----------------------------------------------------------------------------
void C_BaseEntity::UpdateOnRemove( void )
{
	// Are we in the partition?
	//GetEngineObject()->DestroyPartitionHandle();

	// If Client side only entity index will be -1
	if (entindex() != -1)
	{
		beams->KillDeadBeams(this);
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : canpredict - 
//-----------------------------------------------------------------------------
//void C_BaseEntity::SetPredictionEligible( bool canpredict )
//{
//#if !defined( NO_ENTITY_PREDICTION )
//	m_bPredictionEligible = canpredict;
//#endif
//}


//-----------------------------------------------------------------------------
// Purpose: Returns a value that scales all damage done by this entity.
//-----------------------------------------------------------------------------
float C_BaseEntity::GetAttackDamageScale(IHandleEntity* pVictim)
{
	float flScale = 1;
// Not hooked up to prediction yet
#if 0
	FOR_EACH_LL( m_DamageModifiers, i )
	{
		if ( !m_DamageModifiers[i]->IsDamageDoneToMe() )
		{
			flScale *= m_DamageModifiers[i]->GetModifier();
		}
	}
#endif
	return flScale;
}

//-----------------------------------------------------------------------------
// Purpose: Special effects
// Input  : transform - 
//-----------------------------------------------------------------------------
void C_BaseEntity::ApplyBoneMatrixTransform(matrix3x4_t& transform)
{
	if (IsBaseAnimating()) 
	{
		switch (GetEngineObject()->GetRenderFX())
		{
		case kRenderFxDistort:
		case kRenderFxHologram:
			if (RandomInt(0, 49) == 0)
			{
				int axis = RandomInt(0, 1);
				if (axis == 1) // Choose between x & z
					axis = 2;
				VectorScale(transform[axis], RandomFloat(1, 1.484), transform[axis]);
			}
			else if (RandomInt(0, 49) == 0)
			{
				float offset;
				int axis = RandomInt(0, 1);
				if (axis == 1) // Choose between x & z
					axis = 2;
				offset = RandomFloat(-10, 10);
				transform[RandomInt(0, 2)][3] += offset;
			}
			break;
		case kRenderFxExplode:
		{
			float scale;

			scale = 1.0 + (gpGlobals->curtime - GetEngineObject()->GetAnimTime()) * 10.0;
			if (scale > 2)	// Don't blow up more than 200%
				scale = 2;
			transform[0][1] *= scale;
			transform[1][1] *= scale;
			transform[2][1] *= scale;
		}
		break;
		default:
			break;

		}

		if (GetEngineObject()->IsModelScaled())
		{
			// The bone transform is in worldspace, so to scale this, we need to translate it back
			float scale = GetEngineObject()->GetModelScale();

			Vector pos;
			MatrixGetColumn(transform, 3, pos);
			pos -= GetRenderOrigin();
			pos *= scale;
			pos += GetRenderOrigin();
			MatrixSetColumn(pos, 3, transform);

			VectorScale(transform[0], scale, transform[0]);
			VectorScale(transform[1], scale, transform[1]);
			VectorScale(transform[2], scale, transform[2]);
		}
	}
}

class CTraceFilterSkipNPCsAndPlayers : public CTraceFilterSimple
{
public:
	CTraceFilterSkipNPCsAndPlayers(const IHandleEntity* passentity, int collisionGroup)
		: CTraceFilterSimple(passentity, collisionGroup)
	{
	}

	virtual bool ShouldHitEntity(IHandleEntity* pServerEntity, int contentsMask)
	{
		if (CTraceFilterSimple::ShouldHitEntity(pServerEntity, contentsMask))
		{
			C_BaseEntity* pEntity = EntityFromEntityHandle(pServerEntity);
			if (!pEntity)
				return true;

			if (pEntity->IsNPC() || pEntity->IsPlayer())
				return false;

			return true;
		}
		return false;
	}
};

void C_BaseEntity::CalculateIKLocks(float currentTime)
{
	if (!IsBaseAnimating()) {
		Error("aaa");
	}

	if (!GetEngineObject()->GetIk())
		return;

	int targetCount = GetEngineObject()->GetIk()->m_target.Count();
	if (targetCount == 0)
		return;

	// In TF, we might be attaching a player's view to a walking model that's using IK. If we are, it can
	// get in here during the view setup code, and it's not normally supposed to be able to access the spatial
	// partition that early in the rendering loop. So we allow access right here for that special case.
	SpatialPartitionListMask_t curSuppressed = partition->GetSuppressedLists();
	partition->SuppressLists(PARTITION_ALL_CLIENT_EDICTS, false);
	EntityList()->PushEnableAbsRecomputations(false);

	Ray_t ray;
	CTraceFilterSkipNPCsAndPlayers traceFilter(this, GetEngineObject()->GetCollisionGroup());

	// FIXME: trace based on gravity or trace based on angles?
	Vector up;
	AngleVectors(GetRenderAngles(), NULL, NULL, &up);

	// FIXME: check number of slots?
	float minHeight = FLT_MAX;
	float maxHeight = -FLT_MAX;

	for (int i = 0; i < targetCount; i++)
	{
		trace_t trace;
		CIKTarget* pTarget = &GetEngineObject()->GetIk()->m_target[i];

		if (!pTarget->IsActive())
			continue;

		switch (pTarget->type)
		{
		case IK_GROUND:
		{
			Vector estGround;
			Vector p1, p2;

			// adjust ground to original ground position
			estGround = (pTarget->est.pos - GetRenderOrigin());
			estGround = estGround - (estGround * up) * up;
			estGround = GetEngineObject()->GetAbsOrigin() + estGround + pTarget->est.floor * up;

			VectorMA(estGround, pTarget->est.height, up, p1);
			VectorMA(estGround, -pTarget->est.height, up, p2);

			float r = MAX(pTarget->est.radius, 1);

			// don't IK to other characters
			ray.Init(p1, p2, Vector(-r, -r, 0), Vector(r, r, r * 2));
			enginetrace->TraceRay(ray, PhysicsSolidMaskForEntity(), &traceFilter, &trace);

			if (trace.m_pEnt != NULL && ((C_BaseEntity*)trace.m_pEnt)->GetEngineObject()->GetMoveType() == MOVETYPE_PUSH)
			{
				pTarget->SetOwner(((C_BaseEntity*)trace.m_pEnt)->entindex(), ((C_BaseEntity*)trace.m_pEnt)->GetEngineObject()->GetAbsOrigin(), ((C_BaseEntity*)trace.m_pEnt)->GetEngineObject()->GetAbsAngles());
			}
			else
			{
				pTarget->ClearOwner();
			}

			if (trace.startsolid)
			{
				// trace from back towards hip
				Vector tmp = estGround - pTarget->trace.closest;
				tmp.NormalizeInPlace();
				ray.Init(estGround - tmp * pTarget->est.height, estGround, Vector(-r, -r, 0), Vector(r, r, 1));

				// debugoverlay->AddLineOverlay( ray.m_Start, ray.m_Start + ray.m_Delta, 255, 0, 0, 0, 0 );

				enginetrace->TraceRay(ray, MASK_SOLID, &traceFilter, &trace);

				if (!trace.startsolid)
				{
					p1 = trace.endpos;
					VectorMA(p1, -pTarget->est.height, up, p2);
					ray.Init(p1, p2, Vector(-r, -r, 0), Vector(r, r, 1));

					enginetrace->TraceRay(ray, MASK_SOLID, &traceFilter, &trace);
				}

				// debugoverlay->AddLineOverlay( ray.m_Start, ray.m_Start + ray.m_Delta, 0, 255, 0, 0, 0 );
			}


			if (!trace.startsolid)
			{
				if (trace.DidHitWorld())
				{
					// clamp normal to 33 degrees
					const float limit = 0.832;
					float dot = DotProduct(trace.plane.normal, up);
					if (dot < limit)
					{
						Assert(dot >= 0);
						// subtract out up component
						Vector diff = trace.plane.normal - up * dot;
						// scale remainder such that it and the up vector are a unit vector
						float d = sqrt((1 - limit * limit) / DotProduct(diff, diff));
						trace.plane.normal = up * limit + d * diff;
					}
					// FIXME: this is wrong with respect to contact position and actual ankle offset
					pTarget->SetPosWithNormalOffset(trace.endpos, trace.plane.normal);
					pTarget->SetNormal(trace.plane.normal);
					pTarget->SetOnWorld(true);

					// only do this on forward tracking or commited IK ground rules
					if (pTarget->est.release < 0.1)
					{
						// keep track of ground height
						float offset = DotProduct(pTarget->est.pos, up);
						if (minHeight > offset)
							minHeight = offset;

						if (maxHeight < offset)
							maxHeight = offset;
					}
					// FIXME: if we don't drop legs, running down hills looks horrible
					/*
					if (DotProduct( pTarget->est.pos, up ) < DotProduct( estGround, up ))
					{
						pTarget->est.pos = estGround;
					}
					*/
				}
				else if (trace.DidHitNonWorldEntity())
				{
					pTarget->SetPos(trace.endpos);
					pTarget->SetAngles(GetRenderAngles());

					// only do this on forward tracking or commited IK ground rules
					if (pTarget->est.release < 0.1)
					{
						float offset = DotProduct(pTarget->est.pos, up);
						if (minHeight > offset)
							minHeight = offset;

						if (maxHeight < offset)
							maxHeight = offset;
					}
					// FIXME: if we don't drop legs, running down hills looks horrible
					/*
					if (DotProduct( pTarget->est.pos, up ) < DotProduct( estGround, up ))
					{
						pTarget->est.pos = estGround;
					}
					*/
				}
				else
				{
					pTarget->IKFailed();
				}
			}
			else
			{
				if (!trace.DidHitWorld())
				{
					pTarget->IKFailed();
				}
				else
				{
					pTarget->SetPos(trace.endpos);
					pTarget->SetAngles(GetRenderAngles());
					pTarget->SetOnWorld(true);
				}
			}

			/*
			debugoverlay->AddTextOverlay( p1, i, 0, "%d %.1f %.1f %.1f ", i,
				pTarget->latched.deltaPos.x, pTarget->latched.deltaPos.y, pTarget->latched.deltaPos.z );
			debugoverlay->AddBoxOverlay( pTarget->est.pos, Vector( -r, -r, -1 ), Vector( r, r, 1), QAngle( 0, 0, 0 ), 255, 0, 0, 0, 0 );
			*/
			// debugoverlay->AddBoxOverlay( pTarget->latched.pos, Vector( -2, -2, 2 ), Vector( 2, 2, 6), QAngle( 0, 0, 0 ), 0, 255, 0, 0, 0 );
		}
		break;

		case IK_ATTACHMENT:
		{
			C_BaseEntity* pEntity = NULL;
			float flDist = pTarget->est.radius;

			// FIXME: make entity finding sticky!
			// FIXME: what should the radius check be?
			for (CEntitySphereQuery sphere(pTarget->est.pos, 64); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity())
			{
				//C_BaseAnimating *pAnim = pEntity->GetBaseAnimating( );
				if (!pEntity->GetEngineObject()->GetModelPtr())
					continue;

				int iAttachment = pEntity->GetEngineObject()->LookupAttachment(pTarget->offset.pAttachmentName);
				if (iAttachment <= 0)
					continue;

				Vector origin;
				QAngle angles;
				pEntity->GetEngineObject()->GetAttachment(iAttachment, origin, angles);

				// debugoverlay->AddBoxOverlay( origin, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ), QAngle( 0, 0, 0 ), 255, 0, 0, 0, 0 );

				float d = (pTarget->est.pos - origin).Length();

				if (d >= flDist)
					continue;

				flDist = d;
				pTarget->SetPos(origin);
				pTarget->SetAngles(angles);
				// debugoverlay->AddBoxOverlay( pTarget->est.pos, Vector( -pTarget->est.radius, -pTarget->est.radius, -pTarget->est.radius ), Vector( pTarget->est.radius, pTarget->est.radius, pTarget->est.radius), QAngle( 0, 0, 0 ), 0, 255, 0, 0, 0 );
			}

			if (flDist >= pTarget->est.radius)
			{
				// debugoverlay->AddBoxOverlay( pTarget->est.pos, Vector( -pTarget->est.radius, -pTarget->est.radius, -pTarget->est.radius ), Vector( pTarget->est.radius, pTarget->est.radius, pTarget->est.radius), QAngle( 0, 0, 0 ), 0, 0, 255, 0, 0 );
				// no solution, disable ik rule
				pTarget->IKFailed();
			}
		}
		break;
		}
	}

#if defined( HL2_CLIENT_DLL )
	if (minHeight < FLT_MAX)
	{
		EntityList()->GetWorld()->AddIKGroundContactInfo(entindex(), minHeight, maxHeight);
	}
#endif

	EntityList()->PopEnableAbsRecomputations();
	partition->SuppressLists(curSuppressed, true);
}

//#if !defined( NO_ENTITY_PREDICTION )
////-----------------------------------------------------------------------------
//// Purpose: 
//// Output : Returns true on success, false on failure.
////-----------------------------------------------------------------------------
//bool C_BaseEntity::IsDormantPredictable( void ) const
//{
//	return m_bDormantPredictable;
//}
//#endif
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dormant - 
//-----------------------------------------------------------------------------
//void C_BaseEntity::SetDormantPredictable( bool dormant )
//{
//#if !defined( NO_ENTITY_PREDICTION )
//	Assert( IsClientCreated() );
//
//	m_bDormantPredictable = true;
//	m_nIncomingPacketEntityBecameDormant = prediction->GetIncomingPacketNumber();
//
//// Do we need to do the following kinds of things?
//#if 0
//	// Remove from collisions
//	SetSolid( SOLID_NOT );
//	// Don't render
//	AddEffects( EF_NODRAW );
//#endif
//#endif
//}

//-----------------------------------------------------------------------------
// Purpose: Used to determine when a dorman client predictable can be safely deleted
//  Note that it can be deleted earlier than this by OnPredictedEntityRemove returning true
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
//bool C_BaseEntity::BecameDormantThisPacket( void ) const
//{
//#if !defined( NO_ENTITY_PREDICTION )
//	Assert( IsDormantPredictable() );
//
//	if ( m_nIncomingPacketEntityBecameDormant != prediction->GetIncomingPacketNumber() )
//		return false;
//
//	return true;
//#else
//	return false;
//#endif
//}





// Convenient way to delay removing oneself
void C_BaseEntity::SUB_Remove( void )
{
	if (m_iHealth > 0)
	{
		// this situation can screw up NPCs who can't tell their entity pointers are invalid.
		m_iHealth = 0;
		DevWarning( 2, "SUB_Remove called on entity with health > 0\n");
	}

	Release( );
}

CBaseEntity *FindEntityInFrontOfLocalPlayer()
{
	C_BasePlayer *pPlayer = (C_BasePlayer*)EntityList()->GetLocalPlayer();
	if ( pPlayer )
	{
		// Get the entity under my crosshair
		trace_t tr;
		Vector forward;
		pPlayer->EyeVectors( &forward );
		UTIL_TraceLine(EntityList(), pPlayer->EyePosition(), pPlayer->EyePosition() + forward * MAX_COORD_RANGE,	MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction != 1.0 && tr.DidHitNonWorldEntity() )
		{
			return (C_BaseEntity*)tr.m_pEnt;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Debug command to wipe the decals off an entity
//-----------------------------------------------------------------------------
static void RemoveDecals_f( void )
{
	CBaseEntity *pHit = FindEntityInFrontOfLocalPlayer();
	if ( pHit )
	{
		pHit->RemoveAllDecals();
	}
}

static ConCommand cl_removedecals( "cl_removedecals", RemoveDecals_f, "Remove the decals from the entity under the crosshair.", FCVAR_CHEAT );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::ToggleBBoxVisualization( int fVisFlags )
{
	if ( m_fBBoxVisFlags & fVisFlags )
	{
		m_fBBoxVisFlags &= ~fVisFlags;
	}
	else
	{
		m_fBBoxVisFlags |= fVisFlags;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void ToggleBBoxVisualization( int fVisFlags, const CCommand &args )
{
	CBaseEntity *pHit;

	int iEntity = -1;
	if ( args.ArgC() >= 2 )
	{
		iEntity = atoi( args[ 1 ] );
	}

	if ( iEntity == -1 )
	{
		pHit = FindEntityInFrontOfLocalPlayer();
	}
	else
	{
		pHit = (C_BaseEntity*)EntityList()->GetBaseEntity( iEntity );
	}

	if ( pHit )
	{
		pHit->ToggleBBoxVisualization( fVisFlags );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Command to toggle visualizations of bboxes on the client
//-----------------------------------------------------------------------------
CON_COMMAND_F( cl_ent_bbox, "Displays the client's bounding box for the entity under the crosshair.", FCVAR_CHEAT )
{
	ToggleBBoxVisualization( CBaseEntity::VISUALIZE_COLLISION_BOUNDS, args );
}


//-----------------------------------------------------------------------------
// Purpose: Command to toggle visualizations of bboxes on the client
//-----------------------------------------------------------------------------
CON_COMMAND_F( cl_ent_absbox, "Displays the client's absbox for the entity under the crosshair.", FCVAR_CHEAT )
{
	ToggleBBoxVisualization( CBaseEntity::VISUALIZE_SURROUNDING_BOUNDS, args );
}


//-----------------------------------------------------------------------------
// Purpose: Command to toggle visualizations of bboxes on the client
//-----------------------------------------------------------------------------
CON_COMMAND_F( cl_ent_rbox, "Displays the client's render box for the entity under the crosshair.", FCVAR_CHEAT )
{
	ToggleBBoxVisualization( CBaseEntity::VISUALIZE_RENDER_BOUNDS, args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::DrawBBoxVisualizations( void )
{
	if ( m_fBBoxVisFlags & VISUALIZE_COLLISION_BOUNDS )
	{
		debugoverlay->AddBoxOverlay(GetEngineObject()->GetCollisionOrigin(), GetEngineObject()->OBBMins(),
			GetEngineObject()->OBBMaxs(), GetEngineObject()->GetCollisionAngles(), 190, 190, 0, 0, 0.01 );
	}

	if ( m_fBBoxVisFlags & VISUALIZE_SURROUNDING_BOUNDS )
	{
		Vector vecSurroundMins, vecSurroundMaxs;
		GetEngineObject()->WorldSpaceSurroundingBounds( &vecSurroundMins, &vecSurroundMaxs );
		debugoverlay->AddBoxOverlay( vec3_origin, vecSurroundMins,
			vecSurroundMaxs, vec3_angle, 0, 255, 255, 0, 0.01 );
	}

	if ( m_fBBoxVisFlags & VISUALIZE_RENDER_BOUNDS || r_drawrenderboxes.GetInt() )
	{
		Vector vecRenderMins, vecRenderMaxs;
		GetRenderBounds( vecRenderMins, vecRenderMaxs );
		debugoverlay->AddBoxOverlay( GetRenderOrigin(), vecRenderMins, vecRenderMaxs,
			GetRenderAngles(), 255, 0, 255, 0, 0.01 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get rendermode
// Output : int - the render mode
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsTransparent(void)
{
	bool modelIsTransparent = modelinfo->IsTranslucent(GetEngineObject()->GetModel());
	return modelIsTransparent || (GetEngineObject()->GetRenderMode() != kRenderNormal);
}

bool C_BaseEntity::IgnoresZBuffer(void) const
{
	return GetEngineObject()->GetRenderMode() == kRenderGlow || GetEngineObject()->GetRenderMode() == kRenderWorldGlow;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : RenderGroup_t
//-----------------------------------------------------------------------------
RenderGroup_t C_BaseEntity::GetRenderGroup()
{
	// Don't sort things that don't need rendering
	if (GetEngineObject()->GetRenderMode() == kRenderNone)
		return RENDER_GROUP_OPAQUE_ENTITY;

	// When an entity has a material proxy, we have to recompute
	// translucency here because the proxy may have changed it.
	if (modelinfo->ModelHasMaterialProxy(GetEngineObject()->GetModel() ))
	{
		modelinfo->RecomputeTranslucency( const_cast<model_t*>(GetEngineObject()->GetModel()), GetEngineObject()->GetSkin(), GetEngineObject()->GetBody(), GetClientRenderable() );
	}

	// NOTE: Bypassing the GetFXBlend protection logic because we want this to
	// be able to be called from AddToLeafSystem.
	int nTempComputeFrame = m_nFXComputeFrame;
	m_nFXComputeFrame = gpGlobals->framecount;

	int nFXBlend = GetFxBlend();

	m_nFXComputeFrame = nTempComputeFrame;

	// Don't need to sort invisible stuff
	if ( nFXBlend == 0 )
		return RENDER_GROUP_OPAQUE_ENTITY;

		// Figure out its RenderGroup.
	int modelType = modelinfo->GetModelType(GetEngineObject()->GetModel());
	RenderGroup_t renderGroup = (modelType == mod_brush) ? RENDER_GROUP_OPAQUE_BRUSH : RENDER_GROUP_OPAQUE_ENTITY;
	if ( ( nFXBlend != 255 ) || IsTransparent() )
	{
		if (GetEngineObject()->GetRenderMode() != kRenderEnvironmental )
		{
			renderGroup = RENDER_GROUP_TRANSLUCENT_ENTITY;
		}
		else
		{
			renderGroup = RENDER_GROUP_OTHER;
		}
	}

	if ( ( renderGroup == RENDER_GROUP_TRANSLUCENT_ENTITY ) &&
		 ( modelinfo->IsTranslucentTwoPass(GetEngineObject()->GetModel()) ) )
	{
		renderGroup = RENDER_GROUP_TWOPASS;
	}

	return renderGroup;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the fade scale of the entity in question
// Output : unsigned char - 0 - 255 alpha value
//-----------------------------------------------------------------------------
unsigned char C_BaseEntity::GetClientSideFade(void)
{
	return UTIL_ComputeEntityFade(this, m_fadeMinDist, m_fadeMaxDist, m_flFadeScale);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseEntity::SetFadeMinMax(float fademin, float fademax)
{
	m_fadeMinDist = fademin;
	m_fadeMaxDist = fademax;
}

void C_BaseEntity::OnPostRestoreData()
{
	if (GetEngineObject()->GetMoveParent() )
	{
		GetEngineObject()->AddToAimEntsList();
	}

	// If our model index has changed, then make sure it's reflected in our model pointer.
	// (Mostly superseded by new modelindex delta check in RestoreData, but I'm leaving it
	// because it might be band-aiding any other missed calls to SetModelByIndex --henryg)
	if (GetEngineObject()->GetModel() != modelinfo->GetModel(GetEngineObject()->GetModelIndex() ) )
	{
		MDLCACHE_CRITICAL_SECTION();
		SetModelByIndex(GetEngineObject()->GetModelIndex() );
	}

	if (IsBaseAnimating()) {
		IStudioHdr* pHdr = GetEngineObject()->GetModelPtr();
		if (pHdr && GetEngineObject()->GetSequence() >= pHdr->GetNumSeq())
		{
			// Don't let a network update give us an invalid sequence
			GetEngineObject()->SetSequence(0);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fixme, this needs a better solution
// Input  : flags - 
// Output : float
//-----------------------------------------------------------------------------

static float AdjustInterpolationAmount( C_BaseEntity *pEntity, float baseInterpolation )
{
	if ( cl_interp_npcs.GetFloat() > 0 )
	{
		const float minNPCInterpolationTime = cl_interp_npcs.GetFloat();
		const float minNPCInterpolation = TICK_INTERVAL * ( TIME_TO_TICKS( minNPCInterpolationTime ) + 1 );

		if ( minNPCInterpolation > baseInterpolation )
		{
			while ( pEntity )
			{
				if ( pEntity->IsNPC() )
					return minNPCInterpolation;

				pEntity = pEntity->GetEngineObject()->GetMoveParent() ? (C_BaseEntity*)pEntity->GetEngineObject()->GetMoveParent()->GetOuter() : NULL;
			}
		}
	}

	return baseInterpolation;
}

float GetClientInterpAmount()
{
	static const ConVar* pUpdateRate = g_pCVar->FindVar("cl_updaterate");
	if (pUpdateRate)
	{
		ConVarRef cl_interp("cl_interp");
		ConVarRef cl_interp_ratio("cl_interp_ratio");
		// #define FIXME_INTERP_RATIO
		return MAX(cl_interp.GetFloat(), cl_interp_ratio.GetFloat() / pUpdateRate->GetFloat());
	}
	else
	{
		if (!CommandLine()->FindParm("-hushasserts"))
		{
			AssertMsgOnce(false, "GetInterpolationAmount: can't get cl_updaterate cvar.");
		}

		return 0.1;
	}
}

//-------------------------------------
float C_BaseEntity::GetInterpolationAmount( int flags )
{
	// If single player server is "skipping ticks" everything needs to interpolate for a bit longer
	int serverTickMultiple = 1;
	if ( EntityList()->IsSimulatingOnAlternateTicks() )
	{
		serverTickMultiple = 2;
	}

	if (GetEngineObject()->GetPredictable() /*|| IsClientCreated()*/)
	{
		return TICK_INTERVAL * serverTickMultiple;
	}

	// Always fully interpolate during multi-player or during demo playback, if the recorded
	// demo was recorded locally.
	const bool bPlayingDemo = engine->IsPlayingDemo();
	const bool bPlayingMultiplayer = !bPlayingDemo && ( gpGlobals->maxClients > 1 );
	const bool bPlayingNonLocallyRecordedDemo = bPlayingDemo && !engine->IsPlayingDemoALocallyRecordedDemo();
	if ( bPlayingMultiplayer || bPlayingNonLocallyRecordedDemo )
	{
		return AdjustInterpolationAmount( this, TICKS_TO_TIME( TIME_TO_TICKS( GetClientInterpAmount() ) + serverTickMultiple ) );
	}

	int expandedServerTickMultiple = serverTickMultiple;
	if ( IsEngineThreaded() )
	{
		expandedServerTickMultiple += g_nThreadModeTicks;
	}

	if ( GetEngineObject()->IsAnimatedEveryTick() && GetEngineObject()->IsSimulatedEveryTick() )
	{
		return TICK_INTERVAL * expandedServerTickMultiple;
	}

	if ( ( flags & LATCH_ANIMATION_VAR ) && GetEngineObject()->IsAnimatedEveryTick() )
	{
		return TICK_INTERVAL * expandedServerTickMultiple;
	}
	if ( ( flags & LATCH_SIMULATION_VAR ) && GetEngineObject()->IsSimulatedEveryTick() )
	{
		return TICK_INTERVAL * expandedServerTickMultiple;
	}

	return AdjustInterpolationAmount( this, TICKS_TO_TIME( TIME_TO_TICKS( GetClientInterpAmount() ) + serverTickMultiple ) );
}

//-----------------------------------------------------------------------------
// Simply here for game shared 
//-----------------------------------------------------------------------------
bool C_BaseEntity::IsFloating()
{
	// NOTE: This is only here because it's called by game shared.
	// The server uses it to lower falling impact damage
	return false;
}


BEGIN_DATADESC_NO_BASE( C_BaseEntity )
	//DEFINE_FIELD( m_ModelName, FIELD_STRING ),
	//DEFINE_CUSTOM_FIELD_INVALID( m_vecAbsOrigin, engineObjectFuncs),
	//DEFINE_CUSTOM_FIELD_INVALID( m_angAbsRotation, engineObjectFuncs),
	//DEFINE_FIELD( m_fFlags, FIELD_INTEGER ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseEntity::ShouldSavePhysics()
{
	return false;
}

//-----------------------------------------------------------------------------
// handler to do stuff before you are saved
//-----------------------------------------------------------------------------
void C_BaseEntity::OnSave()
{

}


//-----------------------------------------------------------------------------
// handler to do stuff after you are restored
//-----------------------------------------------------------------------------
void C_BaseEntity::OnRestore()
{	
	GetEngineObject()->UpdatePartitionListEntry();
	GetEngineObject()->UpdatePartition();

	UpdateVisibility();
}

//-----------------------------------------------------------------------------
// Purpose: Recursively saves all the classes in an object, in reverse order (top down)
// Output : int 0 on failure, 1 on success
//-----------------------------------------------------------------------------
//int C_BaseEntity::SaveDataDescBlock( ISave &save, datamap_t *dmap )
//{
//	int nResult = save.WriteAll( this, dmap );
//	return nResult;
//}

//-----------------------------------------------------------------------------
// Purpose: Recursively restores all the classes in an object, in reverse order (top down)
// Output : int 0 on failure, 1 on success
//-----------------------------------------------------------------------------
//int C_BaseEntity::RestoreDataDescBlock( IRestore &restore, datamap_t *dmap )
//{
//	return restore.ReadAll( this, dmap );
//}

//-----------------------------------------------------------------------------
// capabilities
//-----------------------------------------------------------------------------
int C_BaseEntity::ObjectCaps( void ) 
{
	return 0; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : C_AI_BaseNPC
//-----------------------------------------------------------------------------
C_AI_BaseNPC *C_BaseEntity::MyNPCPointer( void )
{
	if ( IsNPC() ) 
	{
		return assert_cast<C_AI_BaseNPC *>(this);
	}

	return NULL;
}




void C_BaseEntity::GetToolRecordingState( KeyValues *msg )
{
	Assert( ToolsEnabled() );
	if ( !ToolsEnabled() )
		return;

	VPROF_BUDGET( "C_BaseEntity::GetToolRecordingState", VPROF_BUDGETGROUP_TOOLS );

	IEngineObjectClient *pOwner = GetEngineObject()->GetOwnerEntity();

	static BaseEntityRecordingState_t state;
	state.m_flTime = gpGlobals->curtime;
	state.m_pModelName = modelinfo->GetModelName(GetEngineObject()->GetModel() );
	state.m_nOwner = pOwner ? pOwner->entindex() : -1;
	state.m_nEffects = GetEngineObject()->GetEffects();
	state.m_bVisible = ShouldDraw() && !GetEngineObject()->IsDormant();
	state.m_bRecordFinalVisibleSample = false;
	state.m_vecRenderOrigin = GetRenderOrigin();
	state.m_vecRenderAngles = GetRenderAngles();

	// use EF_NOINTERP if the owner or a hierarchical parent has NO_INTERP
	if ( pOwner && pOwner->IsNoInterpolationFrame() )
	{
		state.m_nEffects |= EF_NOINTERP;
	}
	IEngineObjectClient *pParent = GetEngineObject()->GetMoveParent();
	while ( pParent )
	{
		if ( pParent->IsNoInterpolationFrame() )
		{
			state.m_nEffects |= EF_NOINTERP;
			break;
		}

		pParent = pParent->GetMoveParent();
	}

	msg->SetPtr( "baseentity", &state );
}

void C_BaseEntity::CleanupToolRecordingState( KeyValues *msg )
{
}

void C_BaseEntity::RecordToolMessage()
{
	Assert( GetEngineObject()->IsToolRecording() );
	if ( !GetEngineObject()->IsToolRecording() )
		return;

	if (GetEngineObject()->HasRecordedThisFrame() )
		return;

	KeyValues *msg = new KeyValues( "entity_state" );

	// Post a message back to all IToolSystems
	GetToolRecordingState( msg );
	Assert( (int)GetEngineObject()->GetToolHandle() != 0 );
	ToolFramework_PostToolMessage(GetEngineObject()->GetToolHandle(), msg );
	CleanupToolRecordingState( msg );

	msg->deleteThis();

	GetEngineObject()->SetLastRecordedFrame(gpGlobals->framecount);
}

#ifdef TF_CLIENT_DLL
bool C_BaseEntity::ValidateEntityAttachedToPlayer( bool &bShouldRetry )
{
	bShouldRetry = false;
	C_BaseEntity *pParent = GetRootMoveParent();
	if ( pParent == this )
		return true;

	// Some wearables parent to the view model
	C_BasePlayer *pPlayer = ToBasePlayer( pParent );
	if ( pPlayer && pPlayer->GetViewModel() == this )
	{
		return true;
	}

	// always allow the briefcase model
	const char *pszModel = modelinfo->GetModelName( GetModel() );
	if ( pszModel && pszModel[0] )
	{
		if ( FStrEq( pszModel, "models/flag/briefcase.mdl" ) )
			return true;

		if ( FStrEq( pszModel, "models/props_doomsday/australium_container.mdl" ) )
			return true;

		// Temp for MVM testing
		if ( FStrEq( pszModel, "models/buildables/sapper_placement_sentry1.mdl" ) )
			return true;

		if ( FStrEq( pszModel, "models/props_td/atom_bomb.mdl" ) )
			return true;

		if ( FStrEq( pszModel, "models/props_lakeside_event/bomb_temp_hat.mdl" ) )
			return true;
	}

	// Any entity that's not an item parented to a player is invalid.
	// This prevents them creating some other entity to pretend to be a cosmetic item.
	return !pParent->IsPlayer();
}
#endif // TF_CLIENT_DLL




void C_BaseEntity::CheckCLInterpChanged()
{
	float flCurValue_Interp = GetClientInterpAmount();
	static float flLastValue_Interp = flCurValue_Interp;

	float flCurValue_InterpNPCs = cl_interp_npcs.GetFloat();
	static float flLastValue_InterpNPCs = flCurValue_InterpNPCs;
	
	if ( flLastValue_Interp != flCurValue_Interp || 
		 flLastValue_InterpNPCs != flCurValue_InterpNPCs  )
	{
		flLastValue_Interp = flCurValue_Interp;
		flLastValue_InterpNPCs = flCurValue_InterpNPCs;
	
		// Tell all the existing entities to update their interpolation amounts to account for the change.
		C_BaseEntityIterator iterator;
		C_BaseEntity *pEnt;
		while ( (pEnt = iterator.Next()) != NULL )
		{
			pEnt->GetEngineObject()->Interp_UpdateInterpolationAmounts();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *ent - 
//-----------------------------------------------------------------------------
bool C_BaseEntity::HasNPCsOnIt(void)
{
	clientgroundlink_t* link;
	clientgroundlink_t* root = (clientgroundlink_t*)GetEngineObject()->GetDataObject(GROUNDLINK);
	if (root)
	{
		for (link = root->nextLink; link != root; link = link->nextLink)
		{
			if (EntityList()->GetClientEntityFromHandle(link->entity) && ((C_BaseEntity*)EntityList()->GetClientEntityFromHandle(link->entity))->MyNPCPointer())
				return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Clientside bone follower class. Used just to visualize them.
//			Bone followers WON'T be sent to the client if VISUALIZE_FOLLOWERS is
//			undefined in the server's physics_bone_followers.cpp
//-----------------------------------------------------------------------------
class C_BoneFollower : public C_BaseEntity
{
	DECLARE_CLASS(C_BoneFollower, C_BaseEntity);
	DECLARE_CLIENTCLASS();
public:
	C_BoneFollower(void)
	{
	}

	bool	ShouldDraw(void);
	int		DrawModel(int flags);
	bool	TestCollision(const Ray_t& ray, unsigned int mask, trace_t& trace);

private:
	int m_modelIndex;
	int m_solidIndex;
};

IMPLEMENT_CLIENTCLASS_DT(C_BoneFollower, DT_BoneFollower, CBoneFollower)
	RecvPropInt(RECVINFO(m_modelIndex)),
	RecvPropInt(RECVINFO(m_solidIndex)),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Returns whether object should render.
//-----------------------------------------------------------------------------
bool C_BoneFollower::ShouldDraw(void)
{
	return (vcollide_wireframe.GetBool());  //MOTODO
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BoneFollower::DrawModel(int flags)
{
	vcollide_t* pCollide = modelinfo->GetVCollide(m_modelIndex);
	if (pCollide)
	{
		static color32 debugColor = { 0,255,255,0 };
		matrix3x4_t matrix;
		AngleMatrix(GetEngineObject()->GetAbsAngles(), GetEngineObject()->GetAbsOrigin(), matrix);
		engine->DebugDrawPhysCollide(pCollide->solids[m_solidIndex], NULL, matrix, debugColor);
	}
	return 1;
}

bool C_BoneFollower::TestCollision(const Ray_t& ray, unsigned int mask, trace_t& trace)
{
	vcollide_t* pCollide = modelinfo->GetVCollide(m_modelIndex);
	Assert(pCollide && pCollide->solidCount > m_solidIndex);

	EntityList()->PhysGetCollision()->TraceBox(ray, pCollide->solids[m_solidIndex], GetEngineObject()->GetAbsOrigin(), GetEngineObject()->GetAbsAngles(), &trace);

	if (trace.fraction >= 1)
		return false;

	// return owner as trace hit
	trace.m_pEnt = GetEngineObject()->GetOwnerEntity() ? GetEngineObject()->GetOwnerEntity()->GetClientEntity() : NULL;
	trace.hitgroup = 0;//m_hitGroup;
	trace.physicsbone = 0;//m_physicsBone; // UNDONE: Get physics bone index & hitgroup
	return trace.DidHit();
}

//------------------------------------------------------------------------------
void CC_CL_Find_Ent( const CCommand& args )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Format: cl_find_ent <substring>\n" );
		return;
	}

	int iCount = 0;
	const char *pszSubString = args[1];
	Msg("Searching for client entities with classname containing substring: '%s'\n", pszSubString );

	C_BaseEntity *ent = NULL;
	while ( (ent = (C_BaseEntity*)EntityList()->NextBaseEntity(ent)) != NULL )
	{
		const char *pszClassname = ent->GetClassname();

		bool bMatches = false;
		if ( pszClassname && pszClassname[0] )
		{
			if ( Q_stristr( pszClassname, pszSubString ) )
			{
				bMatches = true;
			}
		}

		if ( bMatches )
		{
			iCount++;
			Msg("   '%s' (entindex %d) %s \n", pszClassname ? pszClassname : "[NO NAME]", ent->entindex(), ent->GetEngineObject()->IsDormant() ? "(DORMANT)" : "" );
		}
	}

	Msg("Found %d matches.\n", iCount);
}
static ConCommand cl_find_ent("cl_find_ent", CC_CL_Find_Ent, "Find and list all client entities with classnames that contain the specified substring.\nFormat: cl_find_ent <substring>\n", FCVAR_CHEAT);

//------------------------------------------------------------------------------
void CC_CL_Find_Ent_Index( const CCommand& args )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Format: cl_find_ent_index <index>\n" );
		return;
	}

	int iIndex = atoi(args[1]);
	C_BaseEntity *ent = (C_BaseEntity*)EntityList()->GetBaseEntity( iIndex );
	if ( ent )
	{
		const char *pszClassname = ent->GetClassname();
		Msg("   '%s' (entindex %d) %s \n", pszClassname ? pszClassname : "[NO NAME]", iIndex, ent->GetEngineObject()->IsDormant() ? "(DORMANT)" : "" );
	}
	else
	{
		Msg("Found no entity at %d.\n", iIndex);
	}
}
static ConCommand cl_find_ent_index("cl_find_ent_index", CC_CL_Find_Ent_Index, "Display data for clientside entity matching specified index.\nFormat: cl_find_ent_index <index>\n", FCVAR_CHEAT);
