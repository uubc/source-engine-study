//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//


//#include "cbase.h"
#include "cdll_client_int.h"
//#include "c_baseentity.h"
#include "sharedInterface.h"
#include "particlemgr.h"
#include "particledraw.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"
#include "mempool.h"
#include "iclientmode.h"
#include "view_scene.h"
#include "tier0/vprof.h"
#include "engine/ivdebugoverlay.h"
#include "iviewrender.h"
#include "KeyValues.h"
#include "particles/particles.h"							// get new particle system access
#include "tier1/utlintrusivelist.h"
#include "particles_new.h"
#include "vstdlib/jobthread.h"
#include "filesystem.h"
#include "particle_parse.h"
#include "model_types.h"
#ifdef TF_CLIENT_DLL
#include "rtime.h"
#endif
#include "tier0/icommandline.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IParticleSystemQuery *g_pParticleSystemQuery;

//static int g_nParticlesDrawn;
// CCycleCount	g_ParticleTimer;

ConVar r_DrawParticles("r_drawparticles", "1", FCVAR_CHEAT, "Enable/disable particle rendering");
ConVar particle_simulateoverflow( "particle_simulateoverflow", "0", FCVAR_CHEAT, "Used for stress-testing particle systems. Randomly denies creation of particles." );
ConVar cl_particleeffect_aabb_buffer( "cl_particleeffect_aabb_buffer", "2", FCVAR_CHEAT, "Add this amount to a particle effect's bbox in the leaf system so if it's growing slowly, it won't have to be reinserted as often." );
ConVar cl_particle_show_bbox( "cl_particle_show_bbox", "0", FCVAR_CHEAT );
ConVar cl_particle_show_bbox_cost( "cl_particle_show_bbox_cost", "0", FCVAR_CHEAT, "Show # of particles: green->blue->red. Use a negative number to show ALL particles even cheap ones" );

// These reflect the convars so we don't parse the string every particle!
bool g_cl_particle_show_bbox = false;
int g_cl_particle_show_bbox_cost = 0;


static void StatsParticlesStart(int nClientIndex);
static void StatsParticlesStop(int nClientIndex);

static ConCommand cl_particle_stats_start( "cl_particle_stats_start", StatsParticlesStart, "Start or restart particle stats - also dumps to particle_stats.csv") ;
static ConCommand cl_particle_stats_stop( "cl_particle_stats_stop", StatsParticlesStop, "Stop particle stats, or snapshot this frame - also dumps to particle_stats.csv") ;
static ConVar cl_particle_stats_trigger_count( "cl_particle_stats_trigger_count", "0", 0, "Dump stats if the particle count exceeds this number." );





//-----------------------------------------------------------------------------
//
// Particle manager implementation
//
//-----------------------------------------------------------------------------


CParticleMgr *GetParticleMgr()
{
	static CParticleMgr s_ParticleMgr;
	return &s_ParticleMgr;
}

IParticleMgr* ParticleMgr()
{
	return GetParticleMgr();
}

//-----------------------------------------------------------------------------
// CParticleSubTextureGroup implementation.
//-----------------------------------------------------------------------------

CParticleSubTextureGroup::CParticleSubTextureGroup()
{
	m_pPageMaterial = NULL;
}


CParticleSubTextureGroup::~CParticleSubTextureGroup()
{
}


//-----------------------------------------------------------------------------
// CParticleSubTexture implementation.
//-----------------------------------------------------------------------------

CParticleSubTexture::CParticleSubTexture()
{
	m_tCoordMins[0] = m_tCoordMins[0] = 0;
	m_tCoordMaxs[0] = m_tCoordMaxs[0] = 1;
	m_pGroup = &m_DefaultGroup;
	m_pMaterial = NULL;

#ifdef _DEBUG
	m_szDebugName = NULL;
#endif
}


//-----------------------------------------------------------------------------
// CEffectMaterial.
//-----------------------------------------------------------------------------

CEffectMaterial::CEffectMaterial()
{
	m_Particles.m_pNext = m_Particles.m_pPrev = &m_Particles;
	m_pGroup = NULL;
}

					



//-----------------------------------------------------------------------------
// CParticleMgr
//-----------------------------------------------------------------------------
CParticleMgr::CParticleMgr()
{
	m_nToolParticleEffectId = 0;
	m_bUpdatingEffects = false;
	m_bRenderParticleEffects = true;
	m_pMaterialSystem = NULL;
	m_pThreadPool[0] = 0;
	m_pThreadPool[1] = 0;
	memset( &m_DirectionalLight, 0, sizeof( m_DirectionalLight ) );

	m_FrameCode = 1;

	m_DefaultInvalidSubTexture.m_pGroup = &m_DefaultInvalidSubTexture.m_DefaultGroup;
	m_DefaultInvalidSubTexture.m_pMaterial = NULL;
	m_DefaultInvalidSubTexture.m_tCoordMins[0] = m_DefaultInvalidSubTexture.m_tCoordMins[1] = 0;
	m_DefaultInvalidSubTexture.m_tCoordMaxs[0] = m_DefaultInvalidSubTexture.m_tCoordMaxs[1] = 1;
	
	m_nCurrentParticlesAllocated = 0;

	SetDefLessFunc( m_effectFactories );
}

CParticleMgr::~CParticleMgr()
{
	Term();
}


//-----------------------------------------------------------------------------
// Initialization and shutdown
//-----------------------------------------------------------------------------
bool CParticleMgr::Init(unsigned long count, IMaterialSystem *pMaterials)
{
	Term();

	m_bStatsRunning = false;
	m_nStatsFramesSinceLastAlert = 0;

	m_pMaterialSystem = pMaterials;

	// Initialize the particle system
	g_pParticleSystemMgr->Init( g_pParticleSystemQuery );
	// tell particle mgr to add the default simulation + rendering ops
	g_pParticleSystemMgr->AddBuiltinSimulationOperators();
	g_pParticleSystemMgr->AddBuiltinRenderingOperators();

	// Send true to load the sheets
	ParseParticleEffects( true, false );

#ifdef TF_CLIENT_DLL
	if ( IsX360() )
	{
		//m_pThreadPool[0] = CreateThreadPool();
		m_pThreadPool[1] = CreateThreadPool();

		ThreadPoolStartParams_t startParams;
		startParams.nThreads = 3;
		startParams.nStackSize = 128*1024;
		startParams.fDistribute = TRS_TRUE;
		startParams.bUseAffinityTable = true;    
		startParams.iAffinityTable[0] = XBOX_PROCESSOR_1;
		startParams.iAffinityTable[1] = XBOX_PROCESSOR_3;
		startParams.iAffinityTable[2] = XBOX_PROCESSOR_5;
		//m_pThreadPool[0]->Start( startParams );

		startParams.nThreads = 2;
		startParams.iAffinityTable[1] = CommandLine()->FindParm( "-swapcores" ) ? XBOX_PROCESSOR_5 : XBOX_PROCESSOR_3;
		m_pThreadPool[1]->Start( startParams );
	}
#endif

	return true;
}

void CParticleMgr::Term()
{
	// Free all the effects.
	intp iNext;
	for ( intp i = m_Effects.Head(); i != m_Effects.InvalidIndex(); i = iNext )
	{
		iNext = m_Effects.Next( i );
		m_Effects[i]->m_pSim->NotifyRemove();
	}
	m_Effects.Purge();
	m_NewEffects.Purge();

	for( int i = m_SubTextures.First(); i != m_SubTextures.InvalidIndex(); i = m_SubTextures.Next( i ) )
	{	
		IMaterial *pMaterial = m_SubTextures[i]->m_pMaterial;
		if ( pMaterial )
			pMaterial->Release();
	}
	m_SubTextures.PurgeAndDeleteElements();

	for( int i = m_SubTextureGroups.Count(); --i >= 0; )
	{	
		IMaterial *pMaterial = m_SubTextureGroups[i]->m_pPageMaterial;
		if ( pMaterial )
			pMaterial->Release();
	}
	m_SubTextureGroups.PurgeAndDeleteElements();

	g_pParticleSystemMgr->UncacheAllParticleSystems();
	if ( m_pMaterialSystem )
	{
		m_pMaterialSystem->UncacheUnusedMaterials();
	}
	m_pMaterialSystem = NULL;
	
	if ( m_pThreadPool[0] )
	{
		m_pThreadPool[0]->Stop();
		DestroyThreadPool( m_pThreadPool[0] );
		m_pThreadPool[0] = NULL;
	}
	if ( m_pThreadPool[1] )
	{
		m_pThreadPool[1]->Stop();
		DestroyThreadPool( m_pThreadPool[1] );
		m_pThreadPool[1] = NULL;
	}

	Assert( m_nCurrentParticlesAllocated == 0 );
}


void CParticleMgr::LevelInit()
{
	g_pParticleSystemMgr->SetLastSimulationTime( gpGlobals->curtime );
}


Particle *CParticleMgr::AllocParticle( int size )
{
	// Enforce max particle limit.
	if ( m_nCurrentParticlesAllocated >= MAX_TOTAL_PARTICLES )
		return NULL;
		
	Particle *pRet = (Particle *)malloc( size );
	if ( pRet )
		++m_nCurrentParticlesAllocated;

	return pRet;
}

void CParticleMgr::FreeParticle( Particle *pParticle )
{
	Assert( m_nCurrentParticlesAllocated > 0 );
	if ( pParticle )
		--m_nCurrentParticlesAllocated;
	
	free( pParticle );
}


//-----------------------------------------------------------------------------
// Should particle effects be rendered?
//-----------------------------------------------------------------------------
void CParticleMgr::RenderParticleSystems( bool bEnable )
{
	m_bRenderParticleEffects = bEnable;
}

bool CParticleMgr::ShouldRenderParticleSystems() const
{
	return m_bRenderParticleEffects;
}


//-----------------------------------------------------------------------------
// add a class that gets notified of entity events
//-----------------------------------------------------------------------------
void CParticleMgr::AddEffectListener( IClientParticleListener *pListener )
{
	int i = m_effectListeners.Find( pListener );
	if ( !m_effectListeners.IsValidIndex( i ) )
	{
		m_effectListeners.AddToTail( pListener );
	}
}

void CParticleMgr::RemoveEffectListener( IClientParticleListener *pListener )
{
	int i = m_effectListeners.Find( pListener );
	if ( m_effectListeners.IsValidIndex( i ) )
	{
		m_effectListeners.Remove( i );
	}
}



//-----------------------------------------------------------------------------
// registers effects classes, and create instances of these effects classes
//-----------------------------------------------------------------------------

void CParticleMgr::RegisterEffect( const char *pEffectType, CreateParticleEffectFN func )
{
#ifdef _DEBUG
	int i = m_effectFactories.Find( pEffectType );
	Assert( !m_effectFactories.IsValidIndex( i ) );
#endif

	m_effectFactories.Insert( pEffectType, func );
}

IParticleEffect *CParticleMgr::CreateEffect( const char *pEffectType )
{
	int i = m_effectFactories.Find( pEffectType );
	if ( !m_effectFactories.IsValidIndex( i ) )
	{
		Msg( "CParticleMgr::CreateEffect: factory not found for effect '%s'\n", pEffectType );
		return NULL;
	}

	CreateParticleEffectFN func = m_effectFactories[ i ];
	if ( func == NULL )
	{
		Msg( "CParticleMgr::CreateEffect: NULL factory for effect '%s'\n", pEffectType );
		return NULL;
	}

	return func();
}


//-----------------------------------------------------------------------------
// Adds and removes effects from our global list
//-----------------------------------------------------------------------------
void CParticleMgr::AddEffect( CNewParticleEffect *pEffect )
{
	m_NewEffects.AddToHead( pEffect );

#if !defined( PARTICLEPROTOTYPE_APP )
	ClientLeafSystem()->CreateRenderableHandle( pEffect );
#endif
	if ( pEffect->IsValid() && pEffect->m_pDef->IsViewModelEffect() )
	{
		ClientLeafSystem()->SetRenderGroup( pEffect->GetRenderHandle(), RENDER_GROUP_VIEW_MODEL_TRANSLUCENT );
	}
}


bool CParticleMgr::AddEffect( CParticleEffectBinding *pEffect, IParticleEffect *pSim )
{
#ifdef _DEBUG
	FOR_EACH_LL( m_Effects, i )
	{
		if( m_Effects[i]->m_pSim == pSim )
		{
			Assert( !"CParticleMgr::AddEffect: added same effect twice" );
			return false;
		}
	}
#endif

	pEffect->Init( this, pSim );

	// Add it to the leaf system.
#if !defined( PARTICLEPROTOTYPE_APP )
	ClientLeafSystem()->CreateRenderableHandle( pEffect );
#endif

	pEffect->m_ListIndex = m_Effects.AddToTail( pEffect );

	Assert( pEffect->m_ListIndex != 0xFFFF );

	// notify listeners
	int nListeners = m_effectListeners.Count();
	for ( int i = 0; i < nListeners; ++i )
	{
		m_effectListeners[ i ]->OnParticleEffectAdded( pSim );
	}

	return true;
}


void CParticleMgr::RemoveEffect( CParticleEffectBinding *pEffect )
{
	// This prevents certain recursive situations where a NotifyRemove
	// call can wind up triggering another one, usually in an effect's
	// destructor.
	if( pEffect->GetRemovalInProgressFlag() )
		return;
	pEffect->SetRemovalInProgressFlag();

	// Don't call RemoveEffect while inside an IParticleEffect's Update() function.
	// Return false from the Update function instead.
	Assert( !m_bUpdatingEffects );

	// notify listeners
	int nListeners = m_effectListeners.Count();
	for ( int i = 0; i < nListeners; ++i )
	{
		m_effectListeners[ i ]->OnParticleEffectRemoved( pEffect->m_pSim );
	}

	// Take it out of the leaf system.
	ClientLeafSystem()->RemoveRenderable( pEffect->m_hRenderHandle );

	int listIndex = pEffect->m_ListIndex;
	if ( pEffect->m_pSim )
	{
		pEffect->m_pSim->NotifyRemove();
		m_Effects.Remove( listIndex );
		
	}
	else
	{
		Assert( listIndex == 0xFFFF );
	}
}

void CParticleMgr::RemoveEffect( CNewParticleEffect *pEffect )
{
	// Don't call RemoveEffect while inside an IParticleEffect's Update() function.
	// Return false from the Update function instead.
	Assert( !m_bUpdatingEffects );

#if !defined( PARTICLEPROTOTYPE_APP )
	// Take it out of the leaf system.
	ClientLeafSystem()->RemoveRenderable( pEffect->m_hRenderHandle );
#endif

	m_NewEffects.RemoveNode( pEffect );
	pEffect->NotifyRemove();
}


void CParticleMgr::RemoveAllNewEffects()
{
	// Remove any of the new effects that were flagged to be removed.
	for( CNewParticleEffect *pNewEffect = m_NewEffects.m_pHead; pNewEffect;  )
	{
		CNewParticleEffect *pNextEffect = pNewEffect->m_pNext;
		// see it any entitiy has a particle prop pointing at this one. this loop through all
		// entities shouldn't be important perf-wise because it only happens on reload
		IClientEntityIterator iterator(EntityList());
		IClientEntity *pEnt;
		while ( (pEnt = iterator.Next()) != NULL )
		{
			if ( pEnt->ParticleProp() )
			{
				pEnt->ParticleProp()->OnParticleSystemDeleted( pNewEffect );
			}
		}		
		RemoveEffect( pNewEffect );
		pNewEffect = pNextEffect;
	}
}

void CParticleMgr::RemoveAllEffects()
{
	intp iNext;
	for ( intp i = m_Effects.Head(); i != m_Effects.InvalidIndex(); i = iNext )
	{
		iNext = m_Effects.Next( i );
		RemoveEffect( m_Effects[i] );
	}

	RemoveAllNewEffects();

	for( int i = m_SubTextures.First(); i != m_SubTextures.InvalidIndex(); i = m_SubTextures.Next( i ) )
	{	
		IMaterial *pMaterial = m_SubTextures[i]->m_pMaterial;
		if ( pMaterial )
			pMaterial->Release();

		m_SubTextures[i]->m_pMaterial = NULL;
	}
	//HACKHACK: commented out because we need to keep leaking handles until every piece of code that grabs one ditches it at level end
	//m_SubTextures.PurgeAndDeleteElements();

	for( int i = m_SubTextureGroups.Count(); --i >= 0; )
	{	
		IMaterial *pMaterial = m_SubTextureGroups[i]->m_pPageMaterial;
		if ( pMaterial )
			pMaterial->Release();

		m_SubTextureGroups[i]->m_pPageMaterial = NULL;
	}
	//HACKHACK: commented out because we need to keep leaking handles until every piece of code that grabs one ditches it at level end
	//m_SubTextureGroups.PurgeAndDeleteElements();
}



void CParticleMgr::IncrementFrameCode()
{
	VPROF( "CParticleMgr::IncrementFrameCode()" );

	++m_FrameCode;
	if ( m_FrameCode == 0 )
	{
		// Reset all the CParticleEffectBindings..
		FOR_EACH_LL( m_Effects, i )
		{
			m_Effects[i]->m_FrameCode = 0;
		}

		m_FrameCode = 1;
	}
	//!!new!!
}


//-----------------------------------------------------------------------------
// Main rendering loop
//-----------------------------------------------------------------------------
void CParticleMgr::Simulate( float flTimeDelta )
{
	//g_nParticlesDrawn = 0;

	if(!m_pMaterialSystem)
	{
		Assert(false);
		return;
	}

	// Update all the effects.
	UpdateAllEffects( flTimeDelta );
}

bool g_bMeasureParticlePerformance;
bool g_bDisplayParticlePerformance;

static int64 g_nNumParticlesSimulated;
static int64 g_nNumUSSpentSimulatingParticles;
static double g_flStartSimTime;

int GetParticlePerformance()
{
	if (! g_nNumUSSpentSimulatingParticles )
		return 0;
	return (1000*g_nNumParticlesSimulated) / g_nNumUSSpentSimulatingParticles;
}

void CParticleMgr::PostRender()
{
	VPROF("CParticleMgr::SimulateUndrawnEffects");

	// Simulate all effects that weren't drawn (if they have their 'always simulate' flag set).
	FOR_EACH_LL( m_Effects, i )
	{
		CParticleEffectBinding *pEffect = m_Effects[i];
		
		// Tell the effect if it was drawn or not.
		pEffect->SetWasDrawnPrevFrame( pEffect->WasDrawn() );

		// Now that we've rendered, clear this flag so it'll simulate next frame.
		pEffect->SetFlag( CParticleEffectBinding::FLAGS_FIRST_FRAME, false );	
	}
}


void CParticleMgr::DrawBeforeViewModelEffects()
{
	// NOTE (2012/11/27, TomF) - this whole system seems to be deprecated - nothing ever checks these flags, and CParticleMgr::DrawBeforeViewModelEffects is never called by anything!
	Assert ( !"Nothing ever calls CParticleMgr::DrawBeforeViewModelEffects any more!" );

	FOR_EACH_LL( m_Effects, i )
	{
		CParticleEffectBinding *pEffect = m_Effects[i];

		if ( pEffect->GetFlag( CParticleEffectBinding::FLAGS_DRAW_BEFORE_VIEW_MODEL ) )
		{
			Assert( !pEffect->WasDrawn() );
			pEffect->DrawModel( 1 );
		}
	}

}



void ResetParticlePerformanceCounters( void )
{
	g_nNumUSSpentSimulatingParticles = 0;
	g_nNumParticlesSimulated = 0;
}

void BeginSimulateParticles( void )
{
	g_flStartSimTime = Plat_FloatTime();
}


static ConVar r_particle_sim_spike_threshold_ms( "r_particle_sim_spike_threshold_ms", "5" );

void EndSimulateParticles( void )
{
	float flETime = Plat_FloatTime() - g_flStartSimTime;
	if ( g_bMeasureParticlePerformance )
	{
		g_nNumUSSpentSimulatingParticles += 1.0e6 * flETime;
	}
	g_pParticleSystemMgr->CommitProfileInformation( flETime > .001 * r_particle_sim_spike_threshold_ms.GetInt() );
}


static ConVar r_threaded_particles( "r_threaded_particles", "1" );

static float s_flThreadedPSystemTimeStep;

static void ProcessPSystem( CNewParticleEffect *&pNewEffect )
{
	// Enable FP exceptions here when FP_EXCEPTIONS_ENABLED is defined,
	// to help track down bad math.
	FPExceptionEnabler enableExceptions;

	// If this is a new effect, then update its bbox so it goes in the
	// right leaves (if it has particles).
	int bFirstUpdate = pNewEffect->GetNeedsBBoxUpdate();
	if ( bFirstUpdate )
	{
		// If the effect already disabled auto-updating of the bbox, then it should have
		// set the bbox by now and we can ignore this responsibility here.
		if ( !pNewEffect->GetAutoUpdateBBox() || pNewEffect->RecalculateBoundingBox() )
		{
			pNewEffect->SetNeedsBBoxUpdate( false );
		}
	}

	// This flag will get set to true if the effect is drawn through the leaf system.
	pNewEffect->SetDrawn( false );

	if ( pNewEffect->GetFirstFrameFlag() )
	{
		pNewEffect->Simulate( 0.0f );
		pNewEffect->SetFirstFrameFlag( false );
	}
	else if ( pNewEffect->ShouldSimulate() )
	{
		pNewEffect->Simulate( s_flThreadedPSystemTimeStep );
	}

	if ( pNewEffect->IsFinished() )
	{
		pNewEffect->SetRemoveFlag();
	}
}


int CParticleMgr::ComputeParticleDefScreenArea( int nInfoCount, RetireInfo_t *pInfo, float *pTotalArea, CParticleSystemDefinition* pDef, 
	const CViewSetup& view, const VMatrix &worldToPixels, float flFocalDist )
{
	int nCollection = 0;
	float flCullCost = pDef->GetCullFillCost();
	float flCullRadius = pDef->GetCullRadius();
	float flCullRadiusSqr = flCullRadius * flCullRadius;
	*pTotalArea = 0.0f;

#ifdef DBGFLAG_ASSERT
	float flMaxPixels = view.width * view.height;
#endif

	CParticleCollection *pCollection = pDef->FirstCollection();
	for ( ; pCollection; pCollection = pCollection->GetNextCollectionUsingSameDef() )
	{
		CNewParticleEffect *pEffect = static_cast< CNewParticleEffect* >( pCollection );
		if ( !pEffect->ShouldPerformCullCheck() )
			continue;

		// Don't count parents
		Assert( !pCollection->m_pParent );
		Assert( nCollection < nInfoCount && pDef == pCollection->m_pDef );

		pInfo[nCollection].m_flScreenArea = 0.0f;
		pInfo[nCollection].m_pCollection = pCollection;
		pInfo[nCollection].m_bFirstFrame = false;

		Vector vecCenter, vecScreenCenter, vecCenterCam;
		vecCenter = pCollection->GetControlPointAtCurrentTime( pDef->GetCullControlPoint() );

		Vector3DMultiplyPositionProjective( worldToPixels, vecCenter, vecScreenCenter );
		float lSqr = vecCenter.DistToSqr( view.origin );

		float flProjRadius = ( lSqr > flCullRadiusSqr ) ? 0.5f * flFocalDist * flCullRadius / sqrt( lSqr - flCullRadiusSqr ) : 1.0f;
		flProjRadius *= view.width;

		float flMinX = MAX( view.x, vecScreenCenter.x - flProjRadius );
		float flMaxX = MIN( view.x + view.width, vecScreenCenter.x + flProjRadius );

		float flMinY = MAX( view.y, vecScreenCenter.y - flProjRadius );
		float flMaxY = MIN( view.y + view.height, vecScreenCenter.y + flProjRadius );

		float flArea = ( flMaxX - flMinX ) * ( flMaxY - flMinY );
		Assert( flArea <= flMaxPixels );
		flArea *= flCullCost;
		*pTotalArea += flArea; 

		pInfo[nCollection].m_flScreenArea = flArea;
		pInfo[nCollection].m_pCollection = pCollection;
		pInfo[nCollection].m_bFirstFrame = pEffect->GetFirstFrameFlag();
		++nCollection;
	}

	return nCollection;
}

int CParticleMgr::RetireSort( const void *p1, const void *p2 ) 
{
	RetireInfo_t *pRetire1 = (RetireInfo_t*)p1;
	RetireInfo_t *pRetire2 = (RetireInfo_t*)p2;
	float flArea = pRetire1->m_flScreenArea - pRetire2->m_flScreenArea;
	if ( flArea == 0.0f )
		return 0;
	return ( flArea > 0 ) ? -1 : 1;
}

bool CParticleMgr::RetireParticleCollections( CParticleSystemDefinition* pDef, 
	int nCount, RetireInfo_t *pInfo, float flScreenArea, float flMaxTotalArea )
{
	bool bRetirementOccurred = false;

	// Don't cull out the particle system if there's only 1 and no replacement
	const char *pReplacementDef = pDef->GetCullReplacementDefinition();
	if ( ( !pReplacementDef || !pReplacementDef[0] ) && ( nCount <= 1 ) )
		return false;

	// Quicksort the retirement info
	qsort( pInfo, nCount, sizeof(RetireInfo_t), RetireSort );

	for ( int i = 0; i < nCount; ++i )
	{
		if ( flScreenArea <= flMaxTotalArea )
			break;

		// We can only replace stuff that's being emitted this frame
		if ( !pInfo[i].m_bFirstFrame )
			continue;

		CNewParticleEffect* pRetireEffect = static_cast< CNewParticleEffect* >( pInfo[i].m_pCollection );
		CNewParticleEffect* pNewEffect = pRetireEffect->ReplaceWith( pReplacementDef );
		if ( pNewEffect )
		{
			pNewEffect->Update( s_flThreadedPSystemTimeStep );
		}
		bRetirementOccurred = true;
		flScreenArea -= pInfo[i].m_flScreenArea;
	}

	return bRetirementOccurred;
}

// Next, see if there are new particle systems that need early retirement
static ConVar cl_particle_retire_cost( "cl_particle_retire_cost", "0", FCVAR_CHEAT );

bool CParticleMgr::EarlyRetireParticleSystems( int nCount, CNewParticleEffect **ppEffects )
{
	// NOTE: Doing a cheap and hacky estimate of worst-case fillrate
	const CViewSetup *pViewSetup = g_pViewRender->GetPlayerViewSetup();
	if ( pViewSetup->width == 0 || pViewSetup->height == 0 )
		return false;

	float flMaxScreenArea = cl_particle_retire_cost.GetFloat() * 1000.0f;
	if ( flMaxScreenArea == 0.0f )
		return false;

	int nDefCount = 0;
	CParticleSystemDefinition **ppDefs = (CParticleSystemDefinition**)stackalloc( nCount * sizeof(CParticleSystemDefinition*) );
	for ( int i = 0; i < nCount; ++i )
	{
		CParticleSystemDefinition *pDef = ppEffects[i]->m_pDef;

		// Skip stuff that doesn't have a cull radius set
		if ( pDef->GetCullRadius() == 0.0f )
			continue;

		// Only perform the cull check on creation
		if ( !ppEffects[i]->GetFirstFrameFlag() )
			continue;

		if ( pDef->HasRetirementBeenChecked( gpGlobals->framecount ) )
			continue;

		pDef->MarkRetirementCheck( gpGlobals->framecount );

		ppDefs[nDefCount++] = ppEffects[i]->m_pDef;
	}

	if ( nDefCount == 0 )
		return false;

	for ( int i = 0; i < nCount; ++i )
	{
		ppEffects[i]->MarkShouldPerformCullCheck( true );
	}

	Vector vecCameraForward;
	VMatrix worldToView, viewToProjection, worldToProjection, worldToScreen;
	render->GetMatricesForView( *pViewSetup, &worldToView, &viewToProjection, &worldToProjection, &worldToScreen );
	float flFocalDist = tan( DEG2RAD( pViewSetup->fov * 0.5f ) );

	bool bRetiredCollections = true;
	float flScreenArea;
	int nSize = nCount * sizeof(RetireInfo_t);
	RetireInfo_t *pInfo = (RetireInfo_t*)stackalloc( nSize );
	for ( int i = 0; i < nDefCount; ++i )
	{
		CParticleSystemDefinition* pDef = ppDefs[i];
		int nActualCount = ComputeParticleDefScreenArea( nCount, pInfo, &flScreenArea, pDef, *pViewSetup, worldToScreen, flFocalDist );
		if ( flScreenArea > flMaxScreenArea )
		{
			if ( RetireParticleCollections( pDef, nActualCount, pInfo, flScreenArea, flMaxScreenArea ) )
			{
				bRetiredCollections = true;
			}
		}
	}

	for ( int i = 0; i < nCount; ++i )
	{
		ppEffects[i]->MarkShouldPerformCullCheck( false );
	}
	return bRetiredCollections;
}

static ConVar particle_sim_alt_cores( "particle_sim_alt_cores", "2" );

void CParticleMgr::BuildParticleSimList( CUtlVector< CNewParticleEffect* > &list )
{
	float flNow = g_pParticleSystemMgr->GetLastSimulationTime();
	for( CNewParticleEffect *pNewEffect=m_NewEffects.m_pHead; pNewEffect;
		pNewEffect=pNewEffect->m_pNext )
	{
		if ( flNow >= pNewEffect->m_flNextSleepTime && pNewEffect->m_nActiveParticles > 0 )
			continue;
		if ( pNewEffect->GetRemoveFlag() )
			continue;
		if ( g_bMeasureParticlePerformance )
		{
			g_nNumParticlesSimulated += pNewEffect->m_nActiveParticles;
		}
		list.AddToTail( pNewEffect );
	}
}

static ConVar r_particle_timescale( "r_particle_timescale", "1.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

static int CountChildParticleSystems( CParticleCollection *p )
{
	int nCount = 1;
	for ( CParticleCollection *pChild = p->m_Children.m_pHead; pChild; pChild = pChild->m_pNext )
	{
		nCount += CountChildParticleSystems( pChild );
	}
	return nCount;
}

static int CountParticleSystemActiveParticles( CParticleCollection *p )
{
	int nCount = p->m_nActiveParticles;
	for ( CParticleCollection *pChild = p->m_Children.m_pHead; pChild; pChild = pChild->m_pNext )
	{
		nCount += CountParticleSystemActiveParticles( pChild );
	}
	return nCount;
}


void CParticleMgr::UpdateNewEffects( float flTimeDelta )
{
// #ifdef TF_CLIENT_DLL
// 	extern bool g_bDontMakeSkipToTimeTakeForever;
// 	g_bDontMakeSkipToTimeTakeForever = true;
// #endif
	flTimeDelta *= r_particle_timescale.GetFloat();
	VPROF_BUDGET( "CParticleMSG::UpdateNewEffects", "Particle Simulation" );

	g_pParticleSystemMgr->SetLastSimulationTime( gpGlobals->curtime );

	int nParticleActiveParticlesCount = 0;
	int nParticleStatsTriggerCount = cl_particle_stats_trigger_count.GetInt();

	BeginSimulateParticles();
	CUtlVector<CNewParticleEffect *> particlesToSimulate;
	BuildParticleSimList( particlesToSimulate );
	s_flThreadedPSystemTimeStep = flTimeDelta;

	int nCount = particlesToSimulate.Count();

	// first, run non-reentrant part to get CP updates from entities
	for( int i=0; i<nCount; i++ )
	{
		// this one can call into random entity code which may not be thread-safe
		particlesToSimulate[i]->Update( s_flThreadedPSystemTimeStep );
		if ( nParticleStatsTriggerCount > 0 )
		{
			nParticleActiveParticlesCount += CountParticleSystemActiveParticles( particlesToSimulate[i] );
		}
	}

	// See if there are new particle systems that need early retirement
	// This has to happen after the first update
	if ( EarlyRetireParticleSystems( nCount, particlesToSimulate.Base() ) )
	{
		particlesToSimulate.RemoveAll();
		BuildParticleSimList( particlesToSimulate );
		nCount = particlesToSimulate.Count();
	}

	if ( nCount )
	{
		EntityList()->UpdateDirtySpatialPartitionEntities();
		if ( !r_threaded_particles.GetBool() )
		{
			for( int i=0; i<nCount; i++)
			{
				ProcessPSystem( particlesToSimulate[i] );
			}
		}
		else
		{
			int nAltCore = IsX360() && particle_sim_alt_cores.GetInt();
			if ( !m_pThreadPool[1] || nAltCore == 0 )
			{
				ParallelProcess( "CParticleMgr::UpdateNewEffects", particlesToSimulate.Base(), nCount, ProcessPSystem );
			}
			else
			{
				if ( nAltCore > 2 )
				{
					nAltCore = 2;
				}
				CParallelProcessor<CNewParticleEffect*, CFuncJobItemProcessor<CNewParticleEffect*> > processor( "CParticleMgr::UpdateNewEffects" );
				processor.m_ItemProcessor.Init( ProcessPSystem, NULL, NULL );
				processor.Run( particlesToSimulate.Base(), nCount, INT_MAX, m_pThreadPool[nAltCore-1] );
			}
		}
	}

	// now, run non-reentrant part for updating changes
	for( int i=0; i<nCount; i++)
	{
		// this one can call into random entity code which may not be thread-safe
		particlesToSimulate[i]->DetectChanges();
	}

	EndSimulateParticles();

	if ( m_bStatsRunning )
	{
		StatsAccumulateActiveParticleSystems();
	}

	bool bFrameWarningNeeded = g_pParticleSystemMgr->Debug_FrameWarningNeededTestAndReset();

	m_nStatsFramesSinceLastAlert++;
	if ( ( nParticleStatsTriggerCount > 0 && ( nParticleActiveParticlesCount >= nParticleStatsTriggerCount ) ) || bFrameWarningNeeded )
	{
		if ( m_nStatsFramesSinceLastAlert >= ( 300 * 3 ) )		// 3 seconds at the clamp of 300 fps (or 15 secs at 60fps). Just stop it spamming too much.
		{
			m_nStatsFramesSinceLastAlert = 0;
			if ( m_bStatsRunning )
			{
				// Spew out the existing ones.
				StatsSpewResults();
			}
			// And turn the stats gathering on so we'll have more useful results if it does it again.
			m_bStatsRunning = true;

			// This single-frame capture doesn't work that well because the "actual drawn" numbers will just be zero. Ah well - better than nothing.
			StatsReset();
			StatsAccumulateActiveParticleSystems();
			StatsSpewResults();
			StatsReset();
		}
	}
}

void CParticleMgr::UpdateAllEffects( float flTimeDelta )
{
	// These reflect the convars so we don't parse the strings every particle.
	g_cl_particle_show_bbox = cl_particle_show_bbox.GetBool();
	g_cl_particle_show_bbox_cost = cl_particle_show_bbox_cost.GetInt();


	m_bUpdatingEffects = true;

	if( flTimeDelta > 0.1f )
		flTimeDelta = 0.1f;

	FOR_EACH_LL( m_Effects, iEffect )
	{
		CParticleEffectBinding *pEffect = m_Effects[iEffect];

		// Don't update this effect if it will be removed.
		if( pEffect->GetRemoveFlag() )
			continue;

		// If this is a new effect, then update its bbox so it goes in the
		// right leaves (if it has particles).
		int bFirstUpdate = pEffect->GetNeedsBBoxUpdate();
		if ( bFirstUpdate )
		{
			// If the effect already disabled auto-updating of the bbox, then it should have
			// set the bbox by now and we can ignore this responsibility here.
			if ( !pEffect->GetAutoUpdateBBox() || pEffect->RecalculateBoundingBox() )
			{
				pEffect->SetNeedsBBoxUpdate( false );
			}
		}

		// This flag will get set to true if the effect is drawn through the leaf system.
		pEffect->SetDrawn( false );

		// Update the effect.
		pEffect->m_pSim->Update( flTimeDelta );

		if ( pEffect->GetFirstFrameFlag() )
			pEffect->SetFirstFrameFlag( false );
		else
			pEffect->SimulateParticles( flTimeDelta );

		// Update its position in the leaf system if its bbox changed.
		pEffect->DetectChanges();
	}

	if ( g_bMeasureParticlePerformance )					// use fixed time step
	{
		for( float dt=0.0f; dt <= flTimeDelta ; dt+= 0.01f )
		{
			UpdateNewEffects( 0.01f );
		}
	}
	else
	{
		UpdateNewEffects( flTimeDelta );
	}

	m_bUpdatingEffects = false;

	// Remove any effects that were flagged to be removed.
	intp iNext;
	for ( intp i = m_Effects.Head(); i != m_Effects.InvalidIndex(); i=iNext )
	{
		iNext = m_Effects.Next( i );
		CParticleEffectBinding *pEffect = m_Effects[i];

		if( pEffect->GetRemoveFlag() )
		{
			RemoveEffect( pEffect );
		}
	}

	// Remove any of the new effects that were flagged to be removed.
	for( CNewParticleEffect *pNewEffect=m_NewEffects.m_pHead; pNewEffect;  )
	{
		CNewParticleEffect *pNextEffect = pNewEffect->m_pNext;
		if ( pNewEffect->GetRemoveFlag() )
		{
			RemoveEffect( pNewEffect );
		}

		pNewEffect = pNextEffect;
	}
}

CParticleSubTextureGroup* CParticleMgr::FindOrAddSubTextureGroup( IMaterial *pPageMaterial )
{
	for ( int i=0; i < m_SubTextureGroups.Count(); i++ )
	{
		if ( m_SubTextureGroups[i]->m_pPageMaterial == pPageMaterial )
			return m_SubTextureGroups[i];
	}

	CParticleSubTextureGroup *pGroup = new CParticleSubTextureGroup;
	m_SubTextureGroups.AddToTail( pGroup );
	pGroup->m_pPageMaterial = pPageMaterial;
	pPageMaterial->AddRef();

	return pGroup;
}


PMaterialHandle CParticleMgr::GetPMaterial( const char *pMaterialName )
{
	if( !m_pMaterialSystem )
	{
		Assert(false);
		return NULL;
	}

	int hMat = m_SubTextures.Find( pMaterialName );
	if ( hMat == m_SubTextures.InvalidIndex() )
	{
		IMaterial *pIMaterial = m_pMaterialSystem->FindMaterial( pMaterialName, TEXTURE_GROUP_PARTICLE );
		if ( pIMaterial )
		{
			pIMaterial->AddRef();

			CMatRenderContextPtr pRenderContext( m_pMaterialSystem );

			pRenderContext->Bind( pIMaterial, this );

			hMat = m_SubTextures.Insert( pMaterialName );
			CParticleSubTexture *pSubTexture = new CParticleSubTexture;
			m_SubTextures[hMat] = pSubTexture;
			pSubTexture->m_pMaterial = pIMaterial;

#ifdef _DEBUG
			int iNameLength = V_strlen( pMaterialName ) + 1;
			pSubTexture->m_szDebugName = new char [iNameLength];
			memcpy( pSubTexture->m_szDebugName, pMaterialName, iNameLength );
#endif

			// See if it's got a group name. If not, make a group with a special name.
			IMaterial *pPageMaterial = pIMaterial->GetMaterialPage();
			if ( pIMaterial->InMaterialPage() && pPageMaterial )
			{
				float flOffset[2], flScale[2];
				pIMaterial->GetMaterialOffset( flOffset );
				pIMaterial->GetMaterialScale( flScale );
				
				pSubTexture->m_tCoordMins[0] = (0*flScale[0] + flOffset[0]) * pPageMaterial->GetMappingWidth();
				pSubTexture->m_tCoordMaxs[0] = (1*flScale[0] + flOffset[0]) * pPageMaterial->GetMappingWidth();
				
				pSubTexture->m_tCoordMins[1] = (0*flScale[1] + flOffset[1]) * pPageMaterial->GetMappingHeight();
				pSubTexture->m_tCoordMaxs[1] = (1*flScale[1] + flOffset[1]) * pPageMaterial->GetMappingHeight();

				pSubTexture->m_pGroup = FindOrAddSubTextureGroup( pPageMaterial );
			}
			else
			{
				// Ok, this material isn't part of a group. Give it its own subtexture group.
				pSubTexture->m_pGroup = &pSubTexture->m_DefaultGroup;
				pSubTexture->m_DefaultGroup.m_pPageMaterial = pIMaterial;
				pPageMaterial = pIMaterial; // For tcoord scaling.
				
				pSubTexture->m_tCoordMins[0] = pSubTexture->m_tCoordMins[1] = 0;
				pSubTexture->m_tCoordMaxs[0] = pIMaterial->GetMappingWidth();
				pSubTexture->m_tCoordMaxs[1] = pIMaterial->GetMappingHeight();
			}

			// Rescale the texture coordinates.
			pSubTexture->m_tCoordMins[0] = (pSubTexture->m_tCoordMins[0] + 0.5f) / pPageMaterial->GetMappingWidth();
			pSubTexture->m_tCoordMins[1] = (pSubTexture->m_tCoordMins[1] + 0.5f) / pPageMaterial->GetMappingHeight();
			pSubTexture->m_tCoordMaxs[0] = (pSubTexture->m_tCoordMaxs[0] - 0.5f) / pPageMaterial->GetMappingWidth();
			pSubTexture->m_tCoordMaxs[1] = (pSubTexture->m_tCoordMaxs[1] - 0.5f) / pPageMaterial->GetMappingHeight();
		
			return pSubTexture;
		}
		else
		{
			return NULL;
		} 
	}
	else
	{
		RepairPMaterial( m_SubTextures[hMat] ); //HACKHACK: Remove this when we can stop leaking handles from level to level.

		return m_SubTextures[hMat];
	}
}


//HACKHACK: The old system would leak handles and materials until shutdown. The new system still needs to leak handles until every piece of code that grabs one ditches it at level end.
//This function takes a leaked handle from a previous level and reacquires necessary materials.
void CParticleMgr::RepairPMaterial( PMaterialHandle hMaterial )
{
	if( hMaterial->m_pMaterial != NULL )
		return;

	const char *pMaterialName = NULL;
	for( int i = m_SubTextures.First(); i != m_SubTextures.InvalidIndex(); i = m_SubTextures.Next( i ) )
	{
		if( m_SubTextures[i] == hMaterial )
		{
			pMaterialName = m_SubTextures.GetElementName( i );
			break;
		}
	}
	Assert( pMaterialName != NULL );

	IMaterial *pIMaterial = m_pMaterialSystem->FindMaterial( pMaterialName, TEXTURE_GROUP_PARTICLE );
	hMaterial->m_pMaterial = pIMaterial;
	if ( pIMaterial != NULL )
	{
		pIMaterial->AddRef();
		CMatRenderContextPtr pRenderContext( m_pMaterialSystem );
		pRenderContext->Bind( pIMaterial, this );

		IMaterial *pPageMaterial = pIMaterial->GetMaterialPage();
		if ( pIMaterial->InMaterialPage() && pPageMaterial )
		{
			if ( hMaterial->m_pGroup->m_pPageMaterial == NULL )
			{
				hMaterial->m_pGroup->m_pPageMaterial = pPageMaterial;
				pPageMaterial->AddRef();
			}
		}
		else
		{
			hMaterial->m_pGroup->m_pPageMaterial = pIMaterial;
		}
	}
}


IMaterial* CParticleMgr::PMaterialToIMaterial( PMaterialHandle hMaterial )
{
	if ( hMaterial )
	{
		RepairPMaterial( hMaterial ); //HACKHACK: Remove this when we can stop leaking handles from level to level.
		return hMaterial->m_pMaterial;
	}
	else
		return NULL;
}


void CParticleMgr::GetDirectionalLightInfo( CParticleLightInfo &info ) const
{
	info = m_DirectionalLight;
}


void CParticleMgr::SetDirectionalLightInfo( const CParticleLightInfo &info )
{
	m_DirectionalLight = info;
}

// ------------------------------------------------------------------------------------ //
// ------------------------------------------------------------------------------------ //
float Helper_GetTime()
{
#if defined( PARTICLEPROTOTYPE_APP )
	static bool bStarted = false;
	static CCycleCount startTimer;
	if( !bStarted )
	{
		bStarted = true;
		startTimer.Sample();
	}

	CCycleCount curCount;
	curCount.Sample();

	CCycleCount elapsed;
	CCycleCount::Sub( curCount, startTimer, elapsed );

	return (float)elapsed.GetSeconds();
#else
	return gpGlobals->curtime;
#endif
}


float Helper_RandomFloat( float minVal, float maxVal )
{
#if defined( PARTICLEPROTOTYPE_APP )
	return Lerp( (float)rand() / RAND_MAX, minVal, maxVal );
#else
	return random->RandomFloat( minVal, maxVal );
#endif
}


int Helper_RandomInt( int minVal, int maxVal )
{
#if defined( PARTICLEPROTOTYPE_APP )
	return minVal + (rand() * (maxVal - minVal)) / RAND_MAX;
#else
	return random->RandomInt( minVal, maxVal );
#endif
}


float Helper_GetFrameTime()
{
#if defined( PARTICLEPROTOTYPE_APP )
	extern float g_ParticleAppFrameTime;
	return g_ParticleAppFrameTime;
#else
	return gpGlobals->frametime;
#endif
}





// ------------------------------------------------------------------------------------ //
// Stats-gathering stuff.
// ------------------------------------------------------------------------------------ //

static void StatsParticlesStart(int nClientIndex)
{
#ifdef STAGING_ONLY
	CParticleMgr *pMgr = GetParticleMgr();
	if ( pMgr->m_bStatsRunning )
	{
		pMgr->StatsSpewResults();
	}
	pMgr->StatsReset();
	pMgr->m_bStatsRunning = true;
#endif
}

static void StatsParticlesStop(int nClientIndex)
{
#ifdef STAGING_ONLY
	CParticleMgr *pMgr = GetParticleMgr();
	if ( pMgr->m_bStatsRunning )
	{
		pMgr->StatsSpewResults();
		pMgr->StatsReset();
	}
	else
	{
		// Weren't running, so snapshot this frame.
		pMgr->StatsReset();
		pMgr->StatsAccumulateActiveParticleSystems();
		pMgr->StatsSpewResults();
		pMgr->StatsReset();
	}
	pMgr->m_bStatsRunning = false;
#endif
}


struct ParticleInfo_t
{
	ParticleInfo_t() : m_nCount(0), m_nChildCount(0), m_nTotalActiveParticles(0), m_nTotalDrawnParticles(0), m_nCountMax(0), m_nChildCountMax(0), m_nTotalActiveParticlesMax(0), m_nTotalDrawnParticlesMax(0), pDef(NULL) {}
	int m_nCount;
	int m_nChildCount;
	int m_nTotalActiveParticles;
	int m_nTotalDrawnParticles;

	// These are only used for the multi-frame stats.
	int m_nCountMax;
	int m_nChildCountMax;
	int m_nTotalActiveParticlesMax;
	int m_nTotalDrawnParticlesMax;

	CParticleSystemDefinition *pDef;
};

CUtlStringMap< ParticleInfo_t > ProfilingHistogram;
CUtlStringMap< ParticleInfo_t > SingleFrameHistogram;
int Profiling_nFrames;
int Profiling_nMaxParticles;


// These functions will be called by the particles as they're actually drawn. (TODO: thread safety?)
void CParticleMgr::StatsNewParticleEffectDrawn ( CNewParticleEffect *pParticles )
{
#ifdef STAGING_ONLY
	ParticleInfo_t *pParticleInfo = &(SingleFrameHistogram[ pParticles->GetName() ]);
	pParticleInfo->m_nTotalDrawnParticles += CountParticleSystemActiveParticles ( pParticles );
#endif
}

void CParticleMgr::StatsOldParticleEffectDrawn ( CParticleEffectBinding *pParticles )
{
#ifdef STAGING_ONLY
	ParticleInfo_t *pParticleInfo = &(SingleFrameHistogram[ pParticles->m_pSim->GetEffectName() ]);
	pParticleInfo->m_nTotalDrawnParticles += pParticles->m_nActiveParticles;
#endif
}

void CParticleMgr::StatsAccumulateActiveParticleSystems()
{
#ifdef STAGING_ONLY
	Profiling_nFrames++;
	Profiling_nMaxParticles = Max ( Profiling_nMaxParticles, g_pParticleSystemMgr->Debug_GetTotalParticleCount() );

	// Accumulate this frame's stats.

	// Count the new particle effects.
	for( CNewParticleEffect *pNewEffect=m_NewEffects.m_pHead; pNewEffect;
		pNewEffect=pNewEffect->m_pNext )
	{
		ParticleInfo_t *pParticleInfo = &(SingleFrameHistogram[ pNewEffect->GetName() ]);
		pParticleInfo->m_nCount++;
		pParticleInfo->m_nTotalActiveParticles += CountParticleSystemActiveParticles( pNewEffect );
		pParticleInfo->m_nChildCount += CountChildParticleSystems( pNewEffect );
		pParticleInfo->pDef = pNewEffect->m_pDef;
	}

	// Count the old types.
	FOR_EACH_LL( m_Effects, i )
	{
		CParticleEffectBinding *pParticleBinding = m_Effects[i];
		const char *pEffectName = pParticleBinding->m_pSim->GetEffectName();
		ParticleInfo_t *pParticleInfo = &(SingleFrameHistogram[ pEffectName ]);
		pParticleInfo->m_nCount++;
		pParticleInfo->m_nChildCount = 0;
		pParticleInfo->m_nTotalActiveParticles += pParticleBinding->m_nActiveParticles;
		pParticleInfo->pDef = NULL;		// These don't have this sort of def.
	}

	// Now accumulate/max then into the multi-frame stats.
	int nCount = SingleFrameHistogram.GetNumStrings();
	for ( int i = 0; i < nCount; ++i )
	{
		ParticleInfo_t *pSingleInfo = &(SingleFrameHistogram[i]);
		const char *pName = SingleFrameHistogram.String(i);

		ParticleInfo_t *pGlobalInfo = &(ProfilingHistogram[pName]);
		pGlobalInfo->m_nCount					+= pSingleInfo->m_nCount;
		pGlobalInfo->m_nChildCount				+= pSingleInfo->m_nChildCount;
		pGlobalInfo->m_nTotalActiveParticles	+= pSingleInfo->m_nTotalActiveParticles;
		pGlobalInfo->m_nTotalDrawnParticles		+= pSingleInfo->m_nTotalDrawnParticles;
		pGlobalInfo->m_nCountMax				= Max ( pGlobalInfo->m_nCountMax,					pSingleInfo->m_nCount );
		pGlobalInfo->m_nChildCountMax			= Max ( pGlobalInfo->m_nChildCountMax,				pSingleInfo->m_nChildCount );
		pGlobalInfo->m_nTotalActiveParticlesMax	= Max ( pGlobalInfo->m_nTotalActiveParticlesMax,	pSingleInfo->m_nTotalActiveParticles );
		pGlobalInfo->m_nTotalDrawnParticlesMax	= Max ( pGlobalInfo->m_nTotalDrawnParticlesMax,		pSingleInfo->m_nTotalDrawnParticles );
		pGlobalInfo->pDef = pSingleInfo->pDef;
	}

	SingleFrameHistogram.Clear();
#endif
}

void CParticleMgr::StatsReset()
{
#ifdef STAGING_ONLY
	ProfilingHistogram.Clear();
	Profiling_nFrames = 0;
	Profiling_nMaxParticles = 0;
#endif
}

void CParticleMgr::StatsSpewResults()
{
#ifdef STAGING_ONLY
#ifdef TF_CLIENT_DLL
	int nCount = ProfilingHistogram.GetNumStrings();

	Msg( "Active particle systems. Numbers are averages over %d frames. Max num particles %d.\n", Profiling_nFrames, Profiling_nMaxParticles );
	Msg( "Name\t\t\t\t\t\tSystems\t\tActive\t\tDrawn\tAv Children\t\tMaximums per frame\t\t\tMax draw dist\n" );
	for ( int i = 0; i < nCount; ++i )
	{
		ParticleInfo_t *pParticleInfo = &(ProfilingHistogram[i]);
		if ( pParticleInfo->m_nTotalActiveParticles > 0 )
		{
			Msg( "%38s\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\t%d\t%d\t%d\t\t%.1f\n",
				ProfilingHistogram.String(i),
				pParticleInfo->m_nCount / Profiling_nFrames,
				pParticleInfo->m_nTotalActiveParticles / Profiling_nFrames,
				pParticleInfo->m_nTotalDrawnParticles / Profiling_nFrames,
				pParticleInfo->m_nChildCount / pParticleInfo->m_nCount,
				pParticleInfo->m_nCountMax,
				pParticleInfo->m_nTotalActiveParticlesMax,
				pParticleInfo->m_nTotalDrawnParticlesMax,
				pParticleInfo->m_nChildCountMax / pParticleInfo->m_nCountMax,
				( pParticleInfo->pDef == NULL ) ? 0.0f : pParticleInfo->pDef->m_flMaxDrawDistance
				);
		}
	}

	CRTime CurrentTime;
	CurrentTime.SetToCurrentTime();

	// Also dump to CSV.
	FileHandle_t fh = g_pFullFileSystem->Open( "particle_stats.csv", "at" );	// at = append + text mode
	g_pFullFileSystem->FPrintf( fh, "\nNumframes,%d,Max particles,%d\n", Profiling_nFrames, Profiling_nMaxParticles );
	g_pFullFileSystem->FPrintf( fh, "Date,%d,%d,%d,Time,%d,%d,%d\n",
		CurrentTime.GetYear(),
		CurrentTime.GetMonth()+1,		// GetMonth() returns 0...11
		CurrentTime.GetDayOfMonth(),	// GetDay() returns 1...31
		CurrentTime.GetHour(),
		CurrentTime.GetMinute(),
		CurrentTime.GetSecond() );
	g_pFullFileSystem->FPrintf( fh, "Name, Systems, Particles active, Particles drawn, Av Children, Max systems, Max particles active, Max particles drawn, Max children, Max draw distance\n" );
	for ( int i = 0; i < nCount; ++i )
	{
		ParticleInfo_t *pParticleInfo = &(ProfilingHistogram[i]);
		if ( pParticleInfo->m_nTotalActiveParticles > 0 )
		{
			g_pFullFileSystem->FPrintf( fh, "%s,%d,%d,%d,%d,%d,%d,%d,%d,%.1f\n",
				ProfilingHistogram.String(i),
				pParticleInfo->m_nCount / Profiling_nFrames,
				pParticleInfo->m_nTotalActiveParticles / Profiling_nFrames,
				pParticleInfo->m_nTotalDrawnParticles / Profiling_nFrames,
				pParticleInfo->m_nChildCount / pParticleInfo->m_nCount,
				pParticleInfo->m_nCountMax,
				pParticleInfo->m_nTotalActiveParticlesMax,
				pParticleInfo->m_nTotalDrawnParticlesMax,
				pParticleInfo->m_nChildCountMax / pParticleInfo->m_nCountMax,
				( pParticleInfo->pDef == NULL ) ? 0.0f : pParticleInfo->pDef->m_flMaxDrawDistance
				);
		}
	}
	g_pFullFileSystem->Close( fh );
#endif
#endif
}




