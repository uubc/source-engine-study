//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

//
// This module implements the particle manager for the client DLL.
// In a nutshell, to create your own effect, implement the ParticleEffect 
// interface and call CParticleMgr::AddEffect to add your effect. Then you can 
// add particles and simulate and render them.

/*

Particle manager documentation
-----------------------------------------------------------------------------

All particle effects are managed by a class called CParticleMgr. It tracks 
the list of particles, manages their materials, sorts the particles, and
has callbacks to render them.

Conceptually, CParticleMgr is NOT part of VEngine's entity system. It does
not care about entities, only particle effects. Usually, the two are implemented
together, but you should be aware the CParticleMgr talks to you through its
own interfaces and does not talk to entities. Thus, it is possible to have
particle effects that are not entities.

To make a particle effect, you need two things: 

1. An implementation of the IParticleEffect interface. This is how CParticleMgr 
   talks to you for things like rendering and updating your effect.

2. A (member) variable of type CParticleEffectBinding. This allows CParticleMgr to 
   store its internal data associated with your effect.

Once you have those two things, you call CParticleMgr::AddEffect and pass them
both in. You will then get updates through IParticleEffect::Update, and you will
be asked to render your particles with IParticleEffect::SimulateAndRender.

When you want to remove the effect, call CParticleEffectBinding::SetRemoveFlag(), which
tells CParticleMgr to remove the effect next chance it gets.

Example class:

	class CMyEffect : public IParticleEffect
	{
	public:
		// Call this to start the effect by adding it to the particle manager.
		void			Start()
		{
			ParticleMgr()->AddEffect( &m_ParticleEffect, this );
		}

		// implementation of IParticleEffect functions go here...

	public:
		CParticleEffectBinding	m_ParticleEffect;
	};



How the particle effects are integrated with the entity system
-----------------------------------------------------------------------------

There are two helper classes that you can use to create particles for your
entities. Each one is useful under different conditions.

1. CSimpleEmitter is a class that does some of the dirty work of using particles.
   If you want, you can just instantiate one of these with CSimpleEmitter::Create
   and call its AddParticle functions to add particles. When you are done and 
   want to 'free' it, call its Release function rather than deleting it, and it
   will wait until all of its particles have gone away before removing itself
   (so you don't have to write code to wait for all of the particles to go away).

   In most cases, it is the easiest and most clear to use CSimpleEmitter or
   derive a class from it, then use that class from inside an entity that wants
   to make particles.

   CSimpleEmitter and derived classes handle adding themselves to the particle
   manager, tracking how many particles in the effect are active, and 
   rendering the particles.

   CSimpleEmitter has code to simulate and render particles in a generic fashion,
   but if you derive a class from it, you can override some of its behavior
   with virtuals like UpdateAlpha, UpdateScale, UpdateColor, etc..

   Example code:
		CSimpleEmitter *pEmitter = CSimpleEmitter::Create();
		
		CEffectMaterialHandle hMaterial = pEmitter->GetCEffectMaterial( "mymaterial" );
		
		for( int i=0; i < 100; i++ )
			pEmitter->AddParticle( hMaterial, RandomVector(0,10), 4 );

		pEmitter->Release();

2. Some older effects derive from C_BaseParticleEffect and implement an entity 
   and a particle system at the same time. This gets nasty and is not encouraged anymore.

*/


#ifndef PARTICLEMGR_H
#define PARTICLEMGR_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "mathlib/vector.h"
#include "mathlib/vmatrix.h"
#include "mathlib/mathlib.h"
#include "iclientrenderable.h"
#include "clientleafsystem.h"
#include "tier0/fasttimer.h"
#include "utllinkedlist.h"
#include "utldict.h"
#ifdef WIN32
#include <typeinfo>
#else
#include <typeinfo>
#endif
#include "tier1/utlintrusivelist.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

class IParticleEffect;
class IClientParticleListener;
struct Particle;
class ParticleDraw;
class CMeshBuilder;
class CUtlMemoryPool;
class CEffectMaterial;
class CParticleSimulateIterator;
class CParticleRenderIterator;
class IThreadPool;
class CParticleSystemDefinition;
class CParticleMgr;
class CNewParticleEffect;
class CParticleCollection;



// Various stats, disabled
// extern int			g_nParticlesDrawn;
// extern CCycleCount	g_ParticleTimer;


class CParticleSubTexture;
class CParticleSubTextureGroup;


//-----------------------------------------------------------------------------
// The basic particle description; all particles need to inherit from this.
//-----------------------------------------------------------------------------




//-----------------------------------------------------------------------------
// This is the CParticleMgr's reference to a material in the material system.
// Particles are sorted by material.
//-----------------------------------------------------------------------------






//-----------------------------------------------------------------------------
// interface IParticleEffect:
//
// This is the interface that particles effects must implement. The effect is 
// responsible for starting itself and calling CParticleMgr::AddEffect, then it 
// will get the callbacks it needs to simulate and render the particles.
//-----------------------------------------------------------------------------




class CParticleMgr : public IParticleMgr
{
	friend class CParticleEffectBinding;
	friend class CParticleCollection;

public:

	CParticleMgr();
	virtual			~CParticleMgr();

	// Call at init time to preallocate the bucket of particles.
	bool			Init(unsigned long nPreallocatedParticles, IMaterialSystem *pMaterial);

	// Shutdown - free everything.
	void			Term();

	void			LevelInit();

	void			RegisterEffect( const char *pEffectType, CreateParticleEffectFN func );
	IParticleEffect	*CreateEffect( const char *pEffectType );

	// Add and remove effects from the active list.
	// Note: once you call AddEffect, CParticleEffectBinding will automatically call
	//       RemoveEffect in its destructor.
	// Note: it's much safer to call CParticleEffectBinding::SetRemoveFlag instead of
	//       CParticleMgr::RemoveEffect.
	bool			AddEffect( CParticleEffectBinding *pEffect, IParticleEffect *pSim );
	void			RemoveEffect( CParticleEffectBinding *pEffect );

	void			AddEffect( CNewParticleEffect *pEffect );
	void			RemoveEffect( CNewParticleEffect *pEffect );

	// Called at level shutdown to free all the lingering particle effects (usually
	// CParticleEffect-derived effects that can linger with noone holding onto them).
	void			RemoveAllEffects();

	// This should be called at the start of the frame.
	void			IncrementFrameCode();
	unsigned short	GetFrameCode() {return m_FrameCode;}
	
	// This updates all the particle effects and inserts them into the leaves.
	void			Simulate( float fTimeDelta );

	// This just marks effects that were drawn so during their next simulation they can know
	// if they were drawn in the previous frame.
	void			PostRender();

	// Draw the effects marked with SetDrawBeforeViewModel.
	void			DrawBeforeViewModelEffects();

	// Returns the modelview matrix
	VMatrix&		GetModelView();
	void			SetModelView(const VMatrix& mModelView) {
		m_mModelView = mModelView;
	}

	Particle		*AllocParticle( int size );
	void			FreeParticle( Particle * );

	PMaterialHandle	GetPMaterial( const char *pMaterialName );
	IMaterial*		PMaterialToIMaterial( PMaterialHandle hMaterial );

	//HACKHACK: quick fix that compensates for the fact that this system was designed to never release materials EVER.
	void RepairPMaterial( PMaterialHandle hMaterial );

	// Particles drawn with the ParticleSphere material will use this info.
	// This should be set in IParticleEffect.
	void GetDirectionalLightInfo( CParticleLightInfo &info ) const;
	void SetDirectionalLightInfo( const CParticleLightInfo &info );

	// add a class that gets notified of entity events
	void AddEffectListener( IClientParticleListener *pListener );
	void RemoveEffectListener( IClientParticleListener *pListener );

	// Tool effect ids
	int AllocateToolParticleEffectId();

	// Remove all new effects
	void RemoveAllNewEffects();

	// Should particle effects be rendered?
	void RenderParticleSystems( bool bEnable );
	bool ShouldRenderParticleSystems() const;

	bool IsStatsRunning() { return m_bStatsRunning; }

	// Quick profiling (counts only, not clock cycles).
	bool		m_bStatsRunning;
	int			m_nStatsFramesSinceLastAlert;

	void StatsAccumulateActiveParticleSystems();
	void StatsReset();
	void StatsSpewResults();
	void StatsNewParticleEffectDrawn ( CNewParticleEffect *pParticles );
	void StatsOldParticleEffectDrawn ( CParticleEffectBinding *pParticles );

	IMaterialSystem* GetMaterialSystem() {
		return m_pMaterialSystem;
	}

	CParticleSubTexture* GetDefaultInvalidSubTexture() {
		return &m_DefaultInvalidSubTexture;
	}
private:
	struct RetireInfo_t
	{
		CParticleCollection *m_pCollection;
		float m_flScreenArea;
		bool m_bFirstFrame;
	};

	// Call Update() on all the effects.
	void UpdateAllEffects( float flTimeDelta );

	void UpdateNewEffects( float flTimeDelta );				// update new particle effects

	CParticleSubTextureGroup* FindOrAddSubTextureGroup( IMaterial *pPageMaterial );

	int ComputeParticleDefScreenArea( int nInfoCount, RetireInfo_t *pInfo, float *pTotalArea, CParticleSystemDefinition* pDef, 
		const CViewSetup& view, const VMatrix &worldToPixels, float flFocalDist );

	bool RetireParticleCollections( CParticleSystemDefinition* pDef, int nCount, RetireInfo_t *pInfo, float flScreenArea, float flMaxTotalArea );
	void BuildParticleSimList( CUtlVector< CNewParticleEffect* > &list );
	bool EarlyRetireParticleSystems( int nCount, CNewParticleEffect **ppEffects );
	static int RetireSort( const void *p1, const void *p2 ); 

private:

	int m_nCurrentParticlesAllocated;

	// Directional lighting info.
	CParticleLightInfo m_DirectionalLight;

	// Frame code, used to prevent CParticleEffects from simulating multiple times per frame.
	// Their DrawModel can be called multiple times per frame because of water reflections,
	// but we only want to simulate the particles once.
	unsigned short					m_FrameCode;

	bool							m_bUpdatingEffects;
	bool							m_bRenderParticleEffects;

	// All the active effects.
	CUtlLinkedList<CParticleEffectBinding*, unsigned short>		m_Effects;

	// all the active effects using the new particle interface
	CUtlIntrusiveDList< CNewParticleEffect > m_NewEffects;

	
	CUtlVector< IClientParticleListener *> m_effectListeners;

	IMaterialSystem					*m_pMaterialSystem;

	// Store the concatenated modelview matrix
	VMatrix							m_mModelView;
	
	CUtlVector<CParticleSubTextureGroup*>				m_SubTextureGroups;	// lookup by group name
	CUtlDict<CParticleSubTexture*,unsigned short>		m_SubTextures;		// lookup by material name
	CParticleSubTexture m_DefaultInvalidSubTexture; // Used when they specify an invalid material name.

	CUtlMap< const char*, CreateParticleEffectFN > m_effectFactories;

	int m_nToolParticleEffectId;

	IThreadPool *m_pThreadPool[2];
};

inline int CParticleMgr::AllocateToolParticleEffectId()
{
	return m_nToolParticleEffectId++;
}

// Implement this class and register with CParticleMgr to receive particle effect add/remove notification
class IClientParticleListener
{
public:
	virtual void OnParticleEffectAdded( IParticleEffect *pEffect ) = 0;
	virtual void OnParticleEffectRemoved( IParticleEffect *pEffect ) = 0;
};

// ------------------------------------------------------------------------ //
// CParticleMgr inlines
// ------------------------------------------------------------------------ //

inline VMatrix& CParticleMgr::GetModelView()
{
	return m_mModelView;
}







// ------------------------------------------------------------------------ //
// GLOBALS
// ------------------------------------------------------------------------ //

CParticleMgr* GetParticleMgr();

// ------------------------------------------------------------------------ //
// Transform a particle.
// ------------------------------------------------------------------------ //




#include "particle_iterators.h"


#endif


