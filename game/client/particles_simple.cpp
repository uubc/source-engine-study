//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "particles_simple.h"
#include "env_wind_shared.h"
#include "KeyValues.h"
#include "toolframework_client.h"
#include "toolframework/itoolframework.h"
#include "vstdlib/IKeyValuesSystem.h"
#include "particle_iterators.h"
#include "clientleafsystem.h"
#include "engine/ivdebugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar r_DrawParticles;
extern ConVar cl_particleeffect_aabb_buffer;
extern ConVar particle_simulateoverflow;
extern bool g_cl_particle_show_bbox;
extern int g_cl_particle_show_bbox_cost;

#define BUCKET_SORT_EVERY_N		8			// It does a bucket sort for each material approximately every N times.
#define BBOX_UPDATE_EVERY_N		8			// It does a full bbox update (checks all particles instead of every eighth one).
#define PARTICLE_SIZE	96


// Used for debugging to make sure all particle effects get freed when we exit.
CUtlLinkedList<CParticleEffect*,int> g_ParticleEffects;
class CEffectChecker
{
public:
	~CEffectChecker()
	{
		Assert( g_ParticleEffects.Count() == 0 );
	}
} g_EffectChecker;


//-----------------------------------------------------------------------------
// CParticleEffectBinding.
//-----------------------------------------------------------------------------
CParticleEffectBinding::CParticleEffectBinding()
{
	m_pParticleMgr = NULL;
	m_pSim = NULL;

	m_LocalSpaceTransform.Identity();
	m_bLocalSpaceTransformIdentity = true;

	m_Flags = 0;
	SetAutoUpdateBBox(true);
	SetFirstFrameFlag(true);
	SetNeedsBBoxUpdate(true);
	SetAlwaysSimulate(true);
	SetEffectCameraSpace(true);
	SetDrawThruLeafSystem(true);
	SetAutoApplyLocalTransform(true);

	// default bbox
	m_Min.Init(-50, -50, -50);
	m_Max.Init(50, 50, 50);

	m_LastMin = m_Min;
	m_LastMax = m_Max;

	SetParticleCullRadius(0.0f);
	m_nActiveParticles = 0;

	m_FrameCode = 0;
	m_ListIndex = 0xFFFF;

	m_UpdateBBoxCounter = 0;

	memset(m_EffectMaterialHash, 0, sizeof(m_EffectMaterialHash));
}


CParticleEffectBinding::~CParticleEffectBinding()
{
	if (m_pParticleMgr)
		m_pParticleMgr->RemoveEffect(this);

	Term();
}

// The is the max size of the particles for use in bounding	computation
void CParticleEffectBinding::SetParticleCullRadius(float flMaxParticleRadius)
{
	if (m_flParticleCullRadius != flMaxParticleRadius)
	{
		m_flParticleCullRadius = flMaxParticleRadius;

		if (m_hRenderHandle != INVALID_CLIENT_RENDER_HANDLE)
		{
			ClientLeafSystem()->RenderableChanged(m_hRenderHandle);
		}
	}
}

const Vector& CParticleEffectBinding::GetRenderOrigin(void)
{
	return m_pSim->GetSortOrigin();
}


const QAngle& CParticleEffectBinding::GetRenderAngles(void)
{
	return vec3_angle;
}

const matrix3x4_t& CParticleEffectBinding::RenderableToWorldTransform()
{
	static matrix3x4_t mat;
	SetIdentityMatrix(mat);
	PositionMatrix(GetRenderOrigin(), mat);
	return mat;
}

void CParticleEffectBinding::GetRenderBounds(Vector& mins, Vector& maxs)
{
	const Vector& vSortOrigin = m_pSim->GetSortOrigin();

	// Convert to local space (around the sort origin).
	mins = m_Min - vSortOrigin;
	mins.x -= m_flParticleCullRadius; mins.y -= m_flParticleCullRadius; mins.z -= m_flParticleCullRadius;
	maxs = m_Max - vSortOrigin;
	maxs.x += m_flParticleCullRadius; maxs.y += m_flParticleCullRadius; maxs.z += m_flParticleCullRadius;
}

bool CParticleEffectBinding::ShouldDraw(void)
{
	return GetFlag(FLAGS_DRAW_THRU_LEAF_SYSTEM) != 0;
}


bool CParticleEffectBinding::IsTransparent(void)
{
	return true;
}


inline void CParticleEffectBinding::StartDrawMaterialParticles(
	CEffectMaterial* pMaterial,
	float flTimeDelta,
	IMesh*& pMesh,
	CMeshBuilder& builder,
	ParticleDraw& particleDraw,
	bool bWireframe)
{
	CMatRenderContextPtr pRenderContext(m_pParticleMgr->GetMaterialSystem());

	// Setup the ParticleDraw and bind the material.
	if (bWireframe)
	{
		IMaterial* pMaterial = m_pParticleMgr->GetMaterialSystem()->FindMaterial("debug/debugparticlewireframe", TEXTURE_GROUP_OTHER);
		pRenderContext->Bind(pMaterial, NULL);
	}
	else
	{
		pRenderContext->Bind(pMaterial->m_pGroup->m_pPageMaterial, m_pParticleMgr);
	}

	pMesh = pRenderContext->GetDynamicMesh(true);

	builder.Begin(pMesh, MATERIAL_QUADS, NUM_PARTICLES_PER_BATCH * 4);
	particleDraw.Init(&builder, pMaterial->m_pGroup->m_pPageMaterial, flTimeDelta);
}


void CParticleEffectBinding::BBoxCalcStart(Vector& bbMin, Vector& bbMax)
{
	if (!GetAutoUpdateBBox())
		return;

	// We're going to fully recompute the bbox.
	bbMin.Init(FLT_MAX, FLT_MAX, FLT_MAX);
	bbMax.Init(-FLT_MAX, -FLT_MAX, -FLT_MAX);
}


void CParticleEffectBinding::BBoxCalcEnd(bool bboxSet, Vector& bbMin, Vector& bbMax)
{
	if (!GetAutoUpdateBBox())
		return;

	// Get the bbox into world space.
	Vector bbMinWorld, bbMaxWorld;
	if (m_bLocalSpaceTransformIdentity)
	{
		bbMinWorld = bbMin;
		bbMaxWorld = bbMax;
	}
	else
	{
		TransformAABB(m_LocalSpaceTransform.As3x4(), bbMin, bbMax, bbMinWorld, bbMaxWorld);
	}

	// If there were ANY particles in the system, then we've got a valid bbox here. Otherwise,
	// we don't have anything, so leave m_Min and m_Max at the sort origin.
	if (bboxSet)
	{
		m_Min = bbMinWorld;
		m_Max = bbMaxWorld;
	}
	else
	{
		m_Min = m_Max = m_pSim->GetSortOrigin();
	}
}


int CParticleEffectBinding::DrawModel(int flags)
{
	VPROF_BUDGET("CParticleEffectBinding::DrawModel", VPROF_BUDGETGROUP_PARTICLE_RENDERING);
#ifndef PARTICLEPROTOTYPE_APP
	if (!r_DrawParticles.GetInt())
		return 0;
#endif

	Assert(flags != 0);

	// If we're in commander mode and it's trying to draw the effect,
	// exit out. If the effect has FLAGS_ALWAYSSIMULATE set, then it'll come back
	// in here and simulate at the end of the frame.

	// NOTE: We do not check ParticleMgr()->ShouldRenderParticleSystems()
	// here as a sort of hack: the SFM currently plays back Tempents, which create
	// old-style particle systems back during playback, which means we want
	// them to display always
	if (!EntityList()->GetWorld()->ShouldDrawParticles())
		return 0;

	//Avoid drawing particles while building depth textures. Perf win.
	//At the very least, we absolutely should not do refraction updates below. So if this gets removed, be sure to wrap the refract/screen texture updates.
	if (flags & (STUDIO_SHADOWDEPTHTEXTURE | STUDIO_SSAODEPTHTEXTURE))
	{
		return 0;
	}

	SetDrawn(true);

	// Don't do anything if there are no particles.
	if (!m_nActiveParticles)
		return 1;

	// Reset the transformation matrix to identity.
	VMatrix mTempModel, mTempView;
	RenderStart(mTempModel, mTempView);

	bool bBucketSort = random->RandomInt(0, BUCKET_SORT_EVERY_N) == 0;

	// Set frametime to zero if we've already rendered this frame.
	float flFrameTime = 0;
	if (m_FrameCode != m_pParticleMgr->GetFrameCode())
	{
		m_FrameCode = m_pParticleMgr->GetFrameCode();
		flFrameTime = Helper_GetFrameTime();
	}

	// For each material, render...
	// This does an incremental bubble sort. It only does one pass every frame, and it will shuffle 
	// unsorted particles one step towards where they should be.
	bool bWireframe = false;
	FOR_EACH_LL(m_Materials, iMaterial)
	{
		CEffectMaterial* pMaterial = m_Materials[iMaterial];

		if (pMaterial->m_pGroup->m_pPageMaterial && pMaterial->m_pGroup->m_pPageMaterial->NeedsPowerOfTwoFrameBufferTexture())
		{
			g_pViewRender->UpdateRefractTexture();
		}

		if (pMaterial->m_pGroup->m_pPageMaterial && pMaterial->m_pGroup->m_pPageMaterial->NeedsFullFrameBufferTexture())
		{
			g_pViewRender->UpdateScreenEffectTexture();
		}

		DrawMaterialParticles(
			bBucketSort,
			pMaterial,
			flFrameTime,
			bWireframe);
	}

	if (ShouldDrawInWireFrameMode())
	{
		bWireframe = true;
		FOR_EACH_LL(m_Materials, iDrawMaterial)
		{
			CEffectMaterial* pMaterial = m_Materials[iDrawMaterial];

			DrawMaterialParticles(
				bBucketSort,
				pMaterial,
				flFrameTime,
				bWireframe);
		}
	}


	if (!IsRetail())
	{
		IParticleMgr* pMgr = ParticleMgr();
		if (pMgr->IsStatsRunning())
		{
			pMgr->StatsOldParticleEffectDrawn(this);
		}

		if (g_cl_particle_show_bbox || (g_cl_particle_show_bbox_cost != 0))
		{
			int nParticlesShowBboxCost = g_cl_particle_show_bbox_cost;
			bool bShowCheapSystems = false;
			if (nParticlesShowBboxCost < 0)
			{
				nParticlesShowBboxCost = -nParticlesShowBboxCost;
				bShowCheapSystems = true;
			}

			Vector center = (m_Min + m_Max) / 2;
			Vector mins = m_Min - center;
			Vector maxs = m_Max - center;

			int r, g, b;
			bool bDraw = true;
			if (nParticlesShowBboxCost > 0)
			{
				float fAmount = (float)m_nActiveParticles / (float)nParticlesShowBboxCost;
				if (fAmount < 0.5f)
				{
					if (bShowCheapSystems)
					{
						r = 0;
						g = 255;
						b = 0;
					}
					else
					{
						// Prevent the screen getting spammed with low-count particles which aren't that expensive.
						bDraw = false;
						r = 0;
						g = 0;
						b = 0;
					}
				}
				else if (fAmount < 1.0f)
				{
					// green 0.5-1.0 blue
					int nBlend = (int)(512.0f * (fAmount - 0.5f));
					nBlend = MIN(255, MAX(0, nBlend));
					r = 0;
					g = 255 - nBlend;
					b = nBlend;
				}
				else if (fAmount < 2.0f)
				{
					// blue 1.0-2.0 red
					int nBlend = (int)(256.0f * (fAmount - 1.0f));
					nBlend = MIN(255, MAX(0, nBlend));
					r = nBlend;
					g = 0;
					b = 255 - nBlend;
				}
				else
				{
					r = 255;
					g = 0;
					b = 0;
				}
			}
			else
			{
				if (m_Flags & FLAGS_AUTOUPDATEBBOX)
				{
					// red is bad, the bbox update is costly
					r = 255;
					g = 0;
					b = 0;
				}
				else
				{
					// green, this effect presents less cpu load 
					r = 0;
					g = 255;
					b = 0;
				}
			}

			if (bDraw)
			{
				debugoverlay->AddBoxOverlay(center, mins, maxs, QAngle(0, 0, 0), r, g, b, 16, 0);
				debugoverlay->AddTextOverlayRGB(center, 0, 0, r, g, b, 64, "%s:(%d)", m_pSim->GetEffectName(), m_nActiveParticles);
			}
		}
	}

	RenderEnd(mTempModel, mTempView);
	return 1;
}


PMaterialHandle CParticleEffectBinding::FindOrAddMaterial(const char* pMaterialName)
{
	if (!m_pParticleMgr)
	{
		return NULL;
	}

	return m_pParticleMgr->GetPMaterial(pMaterialName);
}


Particle* CParticleEffectBinding::AddParticle(int sizeInBytes, PMaterialHandle hMaterial)
{
	m_pParticleMgr->RepairPMaterial(hMaterial); //HACKHACK: Remove this when we can stop leaking handles from level to level.

	// We've currently clamped the particle size to PARTICLE_SIZE,
	// we may need to change this algorithm if we get particles with
	// widely varying size
	if (sizeInBytes > PARTICLE_SIZE)
	{
		Assert(sizeInBytes <= PARTICLE_SIZE);
		return NULL;
	}

	// This is for testing - simulate it running out of memory.
	if (particle_simulateoverflow.GetInt())
	{
		if (rand() % 10 <= 6)
			return NULL;
	}

	// Allocate the puppy. We are actually allocating space for the
	// internals + the actual data
	Particle* pParticle = m_pParticleMgr->AllocParticle(PARTICLE_SIZE);
	if (!pParticle)
		return NULL;

	// Link it in
	CEffectMaterial* pEffectMat = GetEffectMaterial(hMaterial);
	InsertParticleAfter(pParticle, &pEffectMat->m_Particles);

	if (hMaterial)
		pParticle->m_pSubTexture = hMaterial;
	else
		pParticle->m_pSubTexture = m_pParticleMgr->GetDefaultInvalidSubTexture();

	++m_nActiveParticles;
	return pParticle;
}

void CParticleEffectBinding::SetBBox(const Vector& bbMin, const Vector& bbMax, bool bDisableAutoUpdate)
{
	m_Min = bbMin;
	m_Max = bbMax;

	if (bDisableAutoUpdate)
		SetAutoUpdateBBox(false);
}

void CParticleEffectBinding::GetWorldspaceBounds(Vector* pMins, Vector* pMaxs)
{
	*pMins = m_Min;
	*pMaxs = m_Max;
}

void CParticleEffectBinding::SetLocalSpaceTransform(const matrix3x4_t& transform)
{
	m_LocalSpaceTransform.CopyFrom3x4(transform);
	if (m_LocalSpaceTransform.IsIdentity())
	{
		m_bLocalSpaceTransformIdentity = true;
	}
	else
	{
		m_bLocalSpaceTransformIdentity = false;
	}
}

bool CParticleEffectBinding::EnlargeBBoxToContain(const Vector& pt)
{
	if (m_nActiveParticles == 0)
	{
		m_Min = m_Max = pt;
		return true;
	}

	bool bHasChanged = false;

	// check min bounds
	if (pt.x < m_Min.x)
	{
		m_Min.x = pt.x; bHasChanged = true;
	}

	if (pt.y < m_Min.y)
	{
		m_Min.y = pt.y; bHasChanged = true;
	}

	if (pt.z < m_Min.z)
	{
		m_Min.z = pt.z; bHasChanged = true;
	}

	// check max bounds
	if (pt.x > m_Max.x)
	{
		m_Max.x = pt.x; bHasChanged = true;
	}

	if (pt.y > m_Max.y)
	{
		m_Max.y = pt.y; bHasChanged = true;
	}

	if (pt.z > m_Max.z)
	{
		m_Max.z = pt.z; bHasChanged = true;
	}

	return bHasChanged;
}


void CParticleEffectBinding::DetectChanges()
{
	// if we have no render handle, return
	if (m_hRenderHandle == INVALID_CLIENT_RENDER_HANDLE)
		return;

	float flBuffer = cl_particleeffect_aabb_buffer.GetFloat();
	float flExtraBuffer = flBuffer * 1.3f;

	// if nothing changed, return
	if (m_Min.x < m_LastMin.x ||
		m_Min.y < m_LastMin.y ||
		m_Min.z < m_LastMin.z ||

		m_Min.x >(m_LastMin.x + flExtraBuffer) ||
		m_Min.y >(m_LastMin.y + flExtraBuffer) ||
		m_Min.z >(m_LastMin.z + flExtraBuffer) ||

		m_Max.x > m_LastMax.x ||
		m_Max.y > m_LastMax.y ||
		m_Max.z > m_LastMax.z ||

		m_Max.x < (m_LastMax.x - flExtraBuffer) ||
		m_Max.y < (m_LastMax.y - flExtraBuffer) ||
		m_Max.z < (m_LastMax.z - flExtraBuffer)
		)
	{
		// call leafsystem to updated this guy
		ClientLeafSystem()->RenderableChanged(m_hRenderHandle);

		// remember last parameters
		// Add some padding in here so we don't reinsert it into the leaf system if it just changes a tiny amount.
		m_LastMin = m_Min - Vector(flBuffer, flBuffer, flBuffer);
		m_LastMax = m_Max + Vector(flBuffer, flBuffer, flBuffer);
	}
}


void CParticleEffectBinding::GrowBBoxFromParticlePositions(CEffectMaterial* pMaterial, bool& bboxSet, Vector& bbMin, Vector& bbMax)
{
	// If its bbox is manually set, don't bother updating it here.
	if (!GetAutoUpdateBBox())
		return;

	for (Particle* pCur = pMaterial->m_Particles.m_pNext; pCur != &pMaterial->m_Particles; pCur = pCur->m_pNext)
	{
		// Update bounding box 
		VectorMin(bbMin, pCur->m_Pos, bbMin);
		VectorMax(bbMax, pCur->m_Pos, bbMax);
		bboxSet = true;
	}
}


//-----------------------------------------------------------------------------
// Simulate particles
//-----------------------------------------------------------------------------
void CParticleEffectBinding::SimulateParticles(float flTimeDelta)
{
	if (!m_pSim->ShouldSimulate())
		return;

	if (GetFlag(FLAGS_NEW_PARTICLE_SYSTEM))
	{
		CParticleSimulateIterator simulateIterator;
		simulateIterator.m_pEffectBinding = this;
		simulateIterator.m_pMaterial = NULL; //pMaterial;
		simulateIterator.m_flTimeDelta = flTimeDelta;
		m_pSim->SimulateParticles(&simulateIterator);
	}
	else
	{
		Vector bbMin(0, 0, 0), bbMax(0, 0, 0);
		bool bboxSet = false;

		// slow the expensive update operation for particle systems that use auto-update-bbox
		// auto update the bbox after N frames then randomly 1/N or after 2*N frames 
		bool bFullBBoxUpdate = false;
		++m_UpdateBBoxCounter;
		if ((m_UpdateBBoxCounter >= BBOX_UPDATE_EVERY_N && random->RandomInt(0, BBOX_UPDATE_EVERY_N) == 0) ||
			(m_UpdateBBoxCounter >= 2 * BBOX_UPDATE_EVERY_N))
		{
			bFullBBoxUpdate = true;

			// reset watchdog
			m_UpdateBBoxCounter = 0;
		}

		if (bFullBBoxUpdate)
		{
			BBoxCalcStart(bbMin, bbMax);
		}
		FOR_EACH_LL(m_Materials, i)
		{
			CEffectMaterial* pMaterial = m_Materials[i];

			CParticleSimulateIterator simulateIterator;

			simulateIterator.m_pEffectBinding = this;
			simulateIterator.m_pMaterial = pMaterial;
			simulateIterator.m_flTimeDelta = flTimeDelta;

			m_pSim->SimulateParticles(&simulateIterator);

			// Update the bbox.
			if (bFullBBoxUpdate)
			{
				GrowBBoxFromParticlePositions(pMaterial, bboxSet, bbMin, bbMax);
			}
		}
		if (bFullBBoxUpdate)
		{
			BBoxCalcEnd(bboxSet, bbMin, bbMax);
		}
	}
}


void CParticleEffectBinding::SetDrawThruLeafSystem(int bDraw)
{
	// NOTE (2012/11/27, TomF) - this whole system seems to be deprecated - nothing ever checks these flags, and CParticleMgr::DrawBeforeViewModelEffects is never called by anything!

	if (bDraw)
	{
		// If SetDrawBeforeViewModel was called, then they shouldn't be telling it to draw through
		// the leaf system too.
		Assert(!(m_Flags & FLAGS_DRAW_BEFORE_VIEW_MODEL));
	}

	SetFlag(FLAGS_DRAW_THRU_LEAF_SYSTEM, bDraw);
}


void CParticleEffectBinding::SetDrawBeforeViewModel(int bDraw)
{
	// NOTE (2012/11/27, TomF) - this whole system seems to be deprecated - nothing ever checks these flags, and CParticleMgr::DrawBeforeViewModelEffects is never called by anything!

	// Don't draw through the leaf system if they want it to specifically draw before the view model.
	if (bDraw)
		m_Flags &= ~FLAGS_DRAW_THRU_LEAF_SYSTEM;

	SetFlag(FLAGS_DRAW_BEFORE_VIEW_MODEL, bDraw);
}


int CParticleEffectBinding::GetNumActiveParticles()
{
	return m_nActiveParticles;
}

// Build a list of all active particles
int CParticleEffectBinding::GetActiveParticleList(int nCount, Particle** ppParticleList)
{
	int nCurrCount = 0;

	FOR_EACH_LL(m_Materials, i)
	{
		CEffectMaterial* pMaterial = m_Materials[i];
		Particle* pParticle = pMaterial->m_Particles.m_pNext;
		for (; pParticle != &pMaterial->m_Particles; pParticle = pParticle->m_pNext)
		{
			ppParticleList[nCurrCount] = pParticle;
			if (++nCurrCount == nCount)
				return nCurrCount;
		}
	}

	return nCurrCount;
}


int CParticleEffectBinding::DrawMaterialParticles(
	bool bBucketSort,
	CEffectMaterial* pMaterial,
	float flTimeDelta,
	bool bWireframe
)
{
	// Setup everything.
	CMeshBuilder builder;
	ParticleDraw particleDraw;
	IMesh* pMesh = NULL;
	StartDrawMaterialParticles(pMaterial, flTimeDelta, pMesh, builder, particleDraw, bWireframe);

	if (m_nActiveParticles > MAX_TOTAL_PARTICLES)
		Error("CParticleEffectBinding::DrawMaterialParticles: too many particles (%d should be less than %d)", m_nActiveParticles, MAX_TOTAL_PARTICLES);

	// Simluate and render all the particles.
	CParticleRenderIterator renderIterator;

	renderIterator.m_pEffectBinding = this;
	renderIterator.m_pMaterial = pMaterial;
	renderIterator.m_pParticleDraw = &particleDraw;
	renderIterator.m_pMeshBuilder = &builder;
	renderIterator.m_pMesh = pMesh;
	renderIterator.m_bBucketSort = bBucketSort;

	m_pSim->RenderParticles(&renderIterator);
	//g_nParticlesDrawn += m_nActiveParticles;

	if (bBucketSort)
	{
		DoBucketSort(pMaterial, renderIterator.m_zCoords, renderIterator.m_nZCoords, renderIterator.m_MinZ, renderIterator.m_MaxZ);
	}

	// Flush out any remaining particles.
	builder.End(false, true);

	return m_nActiveParticles;
}


void CParticleEffectBinding::RenderStart(VMatrix& tempModel, VMatrix& tempView)
{
	if (IsEffectCameraSpace())
	{
		CMatRenderContextPtr pRenderContext(m_pParticleMgr->GetMaterialSystem());

		// Store matrices off so we can restore them in RenderEnd().
		pRenderContext->GetVMatrix(MATERIAL_VIEW, &tempView);
		pRenderContext->GetVMatrix(MATERIAL_MODEL, &tempModel);

		// We're gonna assume the model matrix was identity and blow it off
		// This means that the particle positions are all specified in world space
		// which makes bounding box computations faster. 
		m_pParticleMgr->SetModelView(tempView);

		// Force the user clip planes to use the old view matrix
		pRenderContext->EnableUserClipTransformOverride(true);
		pRenderContext->UserClipTransform(tempView);

		// The particle renderers want to do things in camera space
		pRenderContext->MatrixMode(MATERIAL_MODEL);
		pRenderContext->LoadIdentity();

		pRenderContext->MatrixMode(MATERIAL_VIEW);
		pRenderContext->LoadIdentity();
	}
	else
	{
		m_pParticleMgr->GetModelView().Identity();
	}

	// Add their local space transform if they have one and they want it applied.
	if (GetAutoApplyLocalTransform() && !m_bLocalSpaceTransformIdentity)
	{
		m_pParticleMgr->SetModelView(m_pParticleMgr->GetModelView() * m_LocalSpaceTransform);
	}

	// Let the particle effect do any per-frame setup/processing here
	m_pSim->StartRender(m_pParticleMgr->GetModelView());
}


void CParticleEffectBinding::RenderEnd(VMatrix& tempModel, VMatrix& tempView)
{
	if (IsEffectCameraSpace())
	{
		CMatRenderContextPtr pRenderContext(m_pParticleMgr->GetMaterialSystem());

		// Make user clip planes work normally
		pRenderContext->EnableUserClipTransformOverride(false);

		// Reset the model matrix.
		pRenderContext->MatrixMode(MATERIAL_MODEL);
		pRenderContext->LoadVMatrix(tempModel);

		// Reset the view matrix.
		pRenderContext->MatrixMode(MATERIAL_VIEW);
		pRenderContext->LoadVMatrix(tempView);
	}
}


void CParticleEffectBinding::DoBucketSort(CEffectMaterial* pMaterial, float* zCoords, int nZCoords, float minZ, float maxZ)
{
	// Do an O(N) bucket sort. This helps the sort when there are lots of particles.
#define NUM_BUCKETS	32
	Particle buckets[NUM_BUCKETS];
	for (int iBucket = 0; iBucket < NUM_BUCKETS; iBucket++)
	{
		buckets[iBucket].m_pPrev = buckets[iBucket].m_pNext = &buckets[iBucket];
	}

	// Sort into buckets.
	int iCurParticle = 0;
	Particle* pNext, * pCur;
	for (pCur = pMaterial->m_Particles.m_pNext; pCur != &pMaterial->m_Particles; pCur = pNext)
	{
		pNext = pCur->m_pNext;
		if (iCurParticle >= nZCoords)
			break;

		// Remove it..
		UnlinkParticle(pCur);

		// Add it to the appropriate bucket.
		float flPercent;
		if (maxZ == minZ)
			flPercent = 0;
		else
			flPercent = (zCoords[iCurParticle] - minZ) / (maxZ - minZ);

		int iAddBucket = (int)(flPercent * (NUM_BUCKETS - 0.0001f));
		iAddBucket = NUM_BUCKETS - iAddBucket - 1;
		Assert(iAddBucket >= 0 && iAddBucket < NUM_BUCKETS);

		InsertParticleAfter(pCur, &buckets[iAddBucket]);

		++iCurParticle;
	}

	// Put the buckets back into the main list.
	for (int iReAddBucket = 0; iReAddBucket < NUM_BUCKETS; iReAddBucket++)
	{
		Particle* pListHead = &buckets[iReAddBucket];
		for (pCur = pListHead->m_pNext; pCur != pListHead; pCur = pNext)
		{
			pNext = pCur->m_pNext;
			InsertParticleAfter(pCur, &pMaterial->m_Particles);
			--iCurParticle;
		}
	}

	Assert(iCurParticle == 0);
}


void CParticleEffectBinding::Init(IParticleMgr* pMgr, IParticleEffect* pSim)
{
	// Must Term before reinitializing.
	Assert(!m_pSim && !m_pParticleMgr);

	m_pSim = pSim;
	m_pParticleMgr = pMgr;
}


void CParticleEffectBinding::Term()
{
	if (!m_pParticleMgr)
		return;

	// Free materials.
	FOR_EACH_LL(m_Materials, iMaterial)
	{
		CEffectMaterial* pMaterial = m_Materials[iMaterial];

		// Remove all particles tied to this effect.
		Particle* pNext = NULL;
		for (Particle* pCur = pMaterial->m_Particles.m_pNext; pCur != &pMaterial->m_Particles; pCur = pNext)
		{
			pNext = pCur->m_pNext;

			RemoveParticle(pCur);
		}

		delete pMaterial;
	}
	m_Materials.Purge();

	memset(m_EffectMaterialHash, 0, sizeof(m_EffectMaterialHash));
}


void CParticleEffectBinding::RemoveParticle(Particle* pParticle)
{
	UnlinkParticle(pParticle);

	// Important that this is updated BEFORE NotifyDestroyParticle is called.
	--m_nActiveParticles;
	Assert(m_nActiveParticles >= 0);

	// Let the effect do any necessary cleanup
	m_pSim->NotifyDestroyParticle(pParticle);

	// Remove it from the list of particles and deallocate
	m_pParticleMgr->FreeParticle(pParticle);
}


bool CParticleEffectBinding::RecalculateBoundingBox()
{
	if (m_nActiveParticles == 0)
	{
		m_Max = m_Min = m_pSim->GetSortOrigin();
		return false;
	}

	Vector bbMin(1e28, 1e28, 1e28);
	Vector bbMax(-1e28, -1e28, -1e28);

	FOR_EACH_LL(m_Materials, iMaterial)
	{
		CEffectMaterial* pMaterial = m_Materials[iMaterial];

		for (Particle* pCur = pMaterial->m_Particles.m_pNext; pCur != &pMaterial->m_Particles; pCur = pCur->m_pNext)
		{
			VectorMin(bbMin, pCur->m_Pos, bbMin);
			VectorMax(bbMax, pCur->m_Pos, bbMax);
		}
	}

	// Get the bbox into world space.
	if (m_bLocalSpaceTransformIdentity)
	{
		m_Min = bbMin;
		m_Max = bbMax;
	}
	else
	{
		TransformAABB(m_LocalSpaceTransform.As3x4(), bbMin, bbMax, m_Min, m_Max);
	}

	return true;
}


CEffectMaterial* CParticleEffectBinding::GetEffectMaterial(CParticleSubTexture* pSubTexture)
{
	// Hash the IMaterial pointer.
	unsigned int index = (((uintp)pSubTexture->m_pGroup) >> 6) % EFFECT_MATERIAL_HASH_SIZE;
	for (CEffectMaterial* pCur = m_EffectMaterialHash[index]; pCur; pCur = pCur->m_pHashedNext)
	{
		if (pCur->m_pGroup == pSubTexture->m_pGroup)
			return pCur;
	}

	CEffectMaterial* pEffectMat = new CEffectMaterial;
	pEffectMat->m_pGroup = pSubTexture->m_pGroup;
	pEffectMat->m_pHashedNext = m_EffectMaterialHash[index];
	m_EffectMaterialHash[index] = pEffectMat;

	m_Materials.AddToTail(pEffectMat);
	return pEffectMat;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CParticleEffect::CParticleEffect( const char *pName )
{
	m_pDebugName = pName;
	m_vSortOrigin.Init();
	m_Flags = FLAG_ALLOCATED;
	m_nToolParticleEffectId = TOOLPARTICLESYSTEMID_INVALID;
	m_RefCount = 0;
	m_bSimulate = true;
	ParticleMgr()->AddEffect( &m_ParticleEffect, this );
#if defined( _DEBUG )
	g_ParticleEffects.AddToTail( this );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CParticleEffect::~CParticleEffect( void )
{
#if defined( _DEBUG )
	int index = g_ParticleEffects.Find( this );
	Assert( g_ParticleEffects.IsValidIndex(index) );
	g_ParticleEffects.Remove( index );
#endif
	// HACKHACK: Prevent re-entering the destructor, clear m_Flags.
	// For some reason we'll get a callback into NotifyRemove() after being deleted!
	// Investigate dangling pointer
	m_Flags = 0;

#if !defined( _XBOX )
	if ( ( m_nToolParticleEffectId != TOOLPARTICLESYSTEMID_INVALID ) && clienttools->IsInRecordingMode() )
	{
		KeyValues *msg = new KeyValues( "OldParticleSystem_Destroy" );
		msg->SetInt( "id", m_nToolParticleEffectId );
		msg->SetFloat( "time", gpGlobals->curtime );
		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
		m_nToolParticleEffectId = TOOLPARTICLESYSTEMID_INVALID; 
	}
#endif
}


void CParticleEffect::SetDynamicallyAllocated( bool bDynamic )
{
	if( bDynamic )
		m_Flags |= FLAG_ALLOCATED;
	else
		m_Flags &= ~FLAG_ALLOCATED;
}


int CParticleEffect::IsReleased()
{
	return m_RefCount == 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleEffect::AddRef()
{
	++m_RefCount;
}


void CParticleEffect::Release()
{
	Assert( m_RefCount > 0 );
	--m_RefCount;

	// If all the particles are already gone, delete ourselves now.
	// If there are still particles, wait for the last NotifyDestroyParticle.
	if ( m_RefCount == 0 )
	{
		if ( m_Flags & FLAG_ALLOCATED )
		{
			if ( m_ParticleEffect.GetNumActiveParticles() == 0 )
			{
				m_ParticleEffect.SetRemoveFlag();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vSortOrigin - 
//-----------------------------------------------------------------------------
const Vector &CParticleEffect::GetSortOrigin()
{
	Assert(m_vSortOrigin.IsValid());
	return m_vSortOrigin;
}

const char *CParticleEffect::GetEffectName()
{
	return m_pDebugName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pParticle - 
//-----------------------------------------------------------------------------
void CParticleEffect::NotifyDestroyParticle( Particle* pParticle )
{
	// Go away if we're released and there are no more particles.
	if( m_ParticleEffect.GetNumActiveParticles() == 0 && IsReleased() && (m_Flags & FLAG_ALLOCATED) && !(m_Flags & FLAG_DONT_REMOVE) )
	{
		m_ParticleEffect.SetRemoveFlag();
	}
}


void CParticleEffect::Update( float flTimeDelta )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleEffect::NotifyRemove()
{
	if( m_Flags & FLAG_ALLOCATED )
	{
		Assert( IsReleased() );
		delete this;
	}
}


void CParticleEffect::SetSortOrigin( const Vector &vSortOrigin )
{
	if ( GetBinding().GetAutoUpdateBBox() )
	{
		if ( m_ParticleEffect.EnlargeBBoxToContain( vSortOrigin ) )
		{
			m_vSortOrigin = vSortOrigin;
		}
	}
	else
	{
		// If not auto-updating bbox, don't change the bbox, just set the sort origin.
		m_vSortOrigin = vSortOrigin;
	}
}

void CParticleEffect::SetParticleCullRadius( float radius )
{
	m_ParticleEffect.SetParticleCullRadius( radius );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : PMaterialHandle
//-----------------------------------------------------------------------------
PMaterialHandle CParticleEffect::GetPMaterial(const char *name)
{
	return m_ParticleEffect.FindOrAddMaterial(name);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : particleSize - 
//			material - 
// Output : SimpleParticle
//-----------------------------------------------------------------------------
Particle *CParticleEffect::AddParticle( unsigned int particleSize, PMaterialHandle material, const Vector &origin )
{
	// If you get here, then you must call SetSortOrigin before adding particles.
	Assert( m_vSortOrigin.IsValid() );

	Particle *pParticle = (Particle *) m_ParticleEffect.AddParticle( particleSize, material );

	if( pParticle == NULL )
		return NULL;

	pParticle->m_Pos = origin;
	return pParticle;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------

REGISTER_EFFECT_USING_CREATE( CSimpleEmitter );

CSimpleEmitter::CSimpleEmitter( const char *pDebugName ) : CParticleEffect( pDebugName )
{
	m_flNearClipMin	= 16.0f;
	m_flNearClipMax	= 64.0f;
}


CSimpleEmitter::~CSimpleEmitter()
{
}

CSmartPtr<CSimpleEmitter> CSimpleEmitter::Create( const char *pDebugName )
{
	CSimpleEmitter *pRet = new CSimpleEmitter( pDebugName );
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: Set the internal near clip range for this particle system
// Input  : nearClipMin - beginning of clip range
//			nearClipMax - end of clip range
//-----------------------------------------------------------------------------
void CSimpleEmitter::SetNearClip( float nearClipMin, float nearClipMax )
{ 
	m_flNearClipMin = nearClipMin;
	m_flNearClipMax = nearClipMax;
}


SimpleParticle*	CSimpleEmitter::AddSimpleParticle( 
	PMaterialHandle hMaterial, 
	const Vector &vOrigin,
	float flDieTime,
	unsigned char uchSize )
{
	SimpleParticle *pRet = (SimpleParticle*)AddParticle( sizeof( SimpleParticle ), hMaterial, vOrigin );
	if ( pRet )
	{
		pRet->m_Pos = vOrigin;
		pRet->m_vecVelocity.Init();
		pRet->m_flRoll = 0;
		pRet->m_flRollDelta = 0;
		pRet->m_flLifetime = 0;
		pRet->m_flDieTime = flDieTime;
		pRet->m_uchColor[0] = pRet->m_uchColor[1] = pRet->m_uchColor[2] = 0;
		pRet->m_uchStartAlpha = pRet->m_uchEndAlpha = 255;
		pRet->m_uchStartSize = pRet->m_uchEndSize = uchSize;
		pRet->m_iFlags = 0;
	}

	return pRet;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float CSimpleEmitter::UpdateAlpha( const SimpleParticle *pParticle )
{
	return (pParticle->m_uchStartAlpha/255.0f) + ( (float)(pParticle->m_uchEndAlpha/255.0f) - (float)(pParticle->m_uchStartAlpha/255.0f) ) * (pParticle->m_flLifetime / pParticle->m_flDieTime);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float CSimpleEmitter::UpdateScale( const SimpleParticle *pParticle )
{
	return	(float)pParticle->m_uchStartSize + ( (float)pParticle->m_uchEndSize - (float)pParticle->m_uchStartSize ) * (pParticle->m_flLifetime / pParticle->m_flDieTime);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
// Output : Vector
//-----------------------------------------------------------------------------

#define WIND_ACCEL 50

void CSimpleEmitter::UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
{
	if (pParticle->m_iFlags & SIMPLE_PARTICLE_FLAG_WINDBLOWN)
	{
		Vector vecWind;
		GetWindspeedAtTime( gpGlobals->curtime, vecWind );

		for ( int i = 0 ; i < 2 ; i++ )
		{
			if ( pParticle->m_vecVelocity[i] < vecWind[i] )
			{
				pParticle->m_vecVelocity[i] += ( timeDelta * WIND_ACCEL );

				// clamp
				if ( pParticle->m_vecVelocity[i] > vecWind[i] )
					pParticle->m_vecVelocity[i] = vecWind[i];
			}
			else if (pParticle->m_vecVelocity[i] > vecWind[i] )
			{
				pParticle->m_vecVelocity[i] -= ( timeDelta * WIND_ACCEL );

				// clamp.
				if ( pParticle->m_vecVelocity[i] < vecWind[i] )
					pParticle->m_vecVelocity[i] = vecWind[i];
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float CSimpleEmitter::UpdateRoll( SimpleParticle *pParticle, float timeDelta )
{
	pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;

	return pParticle->m_flRoll;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
//-----------------------------------------------------------------------------
Vector CSimpleEmitter::UpdateColor( const SimpleParticle *pParticle )
{
	static Vector	cColor;

	cColor[0] = pParticle->m_uchColor[0] / 255.0f;
	cColor[1] = pParticle->m_uchColor[1] / 255.0f;
	cColor[2] = pParticle->m_uchColor[2] / 255.0f;

	return cColor;
}

void CSimpleEmitter::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	float timeDelta = pIterator->GetTimeDelta();

	SimpleParticle *pParticle = (SimpleParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		//Update velocity
		UpdateVelocity( pParticle, timeDelta );
		pParticle->m_Pos += pParticle->m_vecVelocity * timeDelta;

		//Should this particle die?
		pParticle->m_flLifetime += timeDelta;
		UpdateRoll( pParticle, timeDelta );

		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
			pIterator->RemoveParticle( pParticle );

		pParticle = (SimpleParticle*)pIterator->GetNext();
	}
}

void CSimpleEmitter::RenderParticles( CParticleRenderIterator *pIterator )
{
	const SimpleParticle *pParticle = (const SimpleParticle *)pIterator->GetFirst();
	while ( pParticle )
	{
		//Render
		Vector	tPos;

		TransformParticle( ParticleMgr()->GetModelView(), pParticle->m_Pos, tPos );
		float sortKey = (int) tPos.z;

		//Render it
		RenderParticle_ColorSizeAngle(
			pIterator->GetParticleDraw(),
			tPos,
			UpdateColor( pParticle ),
			UpdateAlpha( pParticle ) * GetAlphaDistanceFade( tPos, m_flNearClipMin, m_flNearClipMax ),
			UpdateScale( pParticle ),
			pParticle->m_flRoll
			);

		pParticle = (const SimpleParticle *)pIterator->GetNext( sortKey );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CSimpleEmitter::SetDrawBeforeViewModel( bool state )
{
	m_ParticleEffect.SetDrawBeforeViewModel( state );
}

//==================================================
// Particle Library
//==================================================

CEmberEffect::CEmberEffect( const char *pDebugName ) : CSimpleEmitter( pDebugName )
{
}


CSmartPtr<CEmberEffect> CEmberEffect::Create( const char *pDebugName )
{
	CEmberEffect *pRet = new CEmberEffect( pDebugName );
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}


void CEmberEffect::UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
{
	float	speed = VectorNormalize( pParticle->m_vecVelocity );
	Vector	offset;

	speed -= ( 12.0f * timeDelta );

	offset.Random( -0.125f, 0.125f );

	pParticle->m_vecVelocity += offset;
	VectorNormalize( pParticle->m_vecVelocity );

	pParticle->m_vecVelocity *= speed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
//-----------------------------------------------------------------------------
Vector CEmberEffect::UpdateColor( const SimpleParticle *pParticle )
{
	Vector	color;
	float	ramp = 1.0f - ( pParticle->m_flLifetime / pParticle->m_flDieTime );

	color[0] = ( (float) pParticle->m_uchColor[0] * ramp ) / 255.0f;
	color[1] = ( (float) pParticle->m_uchColor[1] * ramp ) / 255.0f;
	color[2] = ( (float) pParticle->m_uchColor[2] * ramp ) / 255.0f;

	return color;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
// Output : float
//-----------------------------------------------------------------------------
CFireSmokeEffect::CFireSmokeEffect( const char *pDebugName ) : CSimpleEmitter( pDebugName )
{
}


CSmartPtr<CFireSmokeEffect> CFireSmokeEffect::Create( const char *pDebugName )
{
	CFireSmokeEffect *pRet = new CFireSmokeEffect( pDebugName );
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}


float CFireSmokeEffect::UpdateAlpha( const SimpleParticle *pParticle )
{
	return ( ((float)pParticle->m_uchStartAlpha/255.0f) * sin( M_PI * (pParticle->m_flLifetime / pParticle->m_flDieTime) ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
//-----------------------------------------------------------------------------
void CFireSmokeEffect::UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
// Output : Vector
//-----------------------------------------------------------------------------
CFireParticle::CFireParticle( const char *pDebugName ) : CSimpleEmitter( pDebugName )
{
}


CSmartPtr<CFireParticle> CFireParticle::Create( const char *pDebugName )
{
	CFireParticle *pRet = new CFireParticle( pDebugName );
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}


Vector CFireParticle::UpdateColor( const SimpleParticle *pParticle )
{
	for ( int i = 0; i < 3; i++ )
	{
		//FIXME: This is frame dependant... but I don't want to store off start/end colors yet
		//pParticle->m_uchColor[i] = MAX( 0, pParticle->m_uchColor[i]-2 );
	}

	return CSimpleEmitter::UpdateColor( pParticle );
}
