//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef PARTICLES_SIMPLE_H
#define PARTICLES_SIMPLE_H
#ifdef _WIN32
#pragma once
#endif

#include "networkvar.h"
//#include "particlemgr.h"
#include "particlesphererenderer.h"
#include "smartptr.h"

// In order to create a particle effect, you must have one of these around and
// implement IParticleEffect. Pass them both into CParticleMgr::AddEffect and you
// are good to go.
class CParticleEffectBinding : public CDefaultClientRenderable
{
	friend class CParticleMgr;
	friend class CParticleSimulateIterator;
	friend class CNewParticleEffect;

public:
	CParticleEffectBinding();
	~CParticleEffectBinding();


	// Helper functions to setup, add particles, etc..
public:

	// Simulate all the particles.
	void			SimulateParticles(float flTimeDelta);

	// Use this to specify materials when adding particles. 
	// Returns the index of the material it found or added.
	// Returns INVALID_MATERIAL_HANDLE if it couldn't find or add a material.
	PMaterialHandle	FindOrAddMaterial(const char* pMaterialName);

	// Allocate particles. The Particle manager will automagically
	// deallocate them when the IParticleEffect SimulateAndRender() method 
	// returns false. The first argument is the size of the particle
	// structure in bytes
	Particle* AddParticle(int sizeInBytes, PMaterialHandle pMaterial);

	// This is an optional call you can make if you want to manually manage the effect's
	// bounding box. Normally, the bounding box is managed automatically, but in certain
	// cases it is more efficient to set it manually.
	//
	// Note: this is a WORLD SPACE bounding box, even if you've used SetLocalSpaceTransform.
	//
	// After you make this call, the particle manager will no longer update the bounding
	// box automatically if bDisableAutoUpdate is true.
	void			SetBBox(const Vector& bbMin, const Vector& bbMax, bool bDisableAutoUpdate = true);
	// gets a copy of the current bbox mins/maxs in worldspace
	void			GetWorldspaceBounds(Vector* pMins, Vector* pMaxs);

	// This tells the particle manager that your particles are transformed by the specified matrix.
	// That way, it can transform the bbox defined by Particle::m_Pos into world space correctly.
	//
	// It also sets up the matrix returned by CParticleMgr::GetModelView() to include this matrix, so you
	// can do TransformParticle with it like any other particle system.
	const matrix3x4_t& GetLocalSpaceTransform() const;
	void			SetLocalSpaceTransform(const matrix3x4_t& transform);

	// This expands the bbox to contain the specified point. Returns true if bbox changed
	bool			EnlargeBBoxToContain(const Vector& pt);

	// The EZ particle singletons use this - they don't want to be added to all the leaves and drawn through the
	// leaf system - they are specifically told to draw each frame at a certain point.
	void			SetDrawThruLeafSystem(int bDraw);

	// Some view model particle effects want to be drawn right before the view model (after everything else is 
	// drawn).
	void			SetDrawBeforeViewModel(int bDraw);

	// Call this to have the effect removed whenever it safe to do so.
	// This is a lot safer than calling CParticleMgr::RemoveEffect.
	int				GetRemoveFlag() { return GetFlag(FLAGS_REMOVE); }
	void			SetRemoveFlag() { SetFlag(FLAGS_REMOVE, 1); }

	// Set this flag to tell the particle manager to simulate your particles even
	// if the particle system isn't visible. Tempents and fast effects can always use
	// this if they want since they want to simulate their particles until they go away.
	// This flag is ON by default.
	int				GetAlwaysSimulate() { return GetFlag(FLAGS_ALWAYSSIMULATE); }
	void			SetAlwaysSimulate(int bAlwaysSimulate) { SetFlag(FLAGS_ALWAYSSIMULATE, bAlwaysSimulate); }

	void			SetIsNewParticleSystem(void) { SetFlag(FLAGS_NEW_PARTICLE_SYSTEM, 1); }
	// Set if the effect was drawn the previous frame.
	// This can be used by particle effect classes
	// to decide whether or not they want to spawn
	// new particles - if they weren't drawn, then
	// they can 'freeze' the particle system to avoid
	// overhead.
	int				WasDrawnPrevFrame() { return GetFlag(FLAGS_DRAWN_PREVFRAME); }
	void			SetWasDrawnPrevFrame(int bWasDrawnPrevFrame) { SetFlag(FLAGS_DRAWN_PREVFRAME, bWasDrawnPrevFrame); }

	// When the effect is in camera space mode, then the transforms are setup such that
	// the particle vertices are specified in camera space (in CParticleDraw) rather than world space. 
	//
	// This makes it faster to specify the particles - you only have to transform the center 
	// by CParticleMgr::GetModelView then add to X and Y to build the quad.
	//
	// Effects that want to specify verts (in CParticleDraw) in world space should set this to false and
	// ignore CParticleMgr::GetModelView.
	//
	// Camera space mode is ON by default.
	int				IsEffectCameraSpace() { return GetFlag(FLAGS_CAMERASPACE); }
	void			SetEffectCameraSpace(int bCameraSpace) { SetFlag(FLAGS_CAMERASPACE, bCameraSpace); }

	// This tells it whether or not to apply the local transform to the matrix returned by CParticleMgr::GetModelView().
	// Usually, you'll want this, so you can just say TransformParticle( pMgr->GetModelView(), vPos ), but you may want
	// to manually apply your local transform before saying TransformParticle.
	//
	// This is ON by default.
	int				GetAutoApplyLocalTransform() const { return GetFlag(FLAGS_AUTOAPPLYLOCALTRANSFORM); }
	void			SetAutoApplyLocalTransform(int b) { SetFlag(FLAGS_AUTOAPPLYLOCALTRANSFORM, b); }

	// If this is true, then the bbox is calculated from particle positions. This works
	// fine if you always simulate (SetAlwaysSimulateFlag) so the system can become visible
	// if it moves into the PVS. If you don't use this, then you should call SetBBox at 
	// least once to tell the particle manager where your entity is.
	int				GetAutoUpdateBBox() { return GetFlag(FLAGS_AUTOUPDATEBBOX); }
	void			SetAutoUpdateBBox(int bAutoUpdate) { SetFlag(FLAGS_AUTOUPDATEBBOX, bAutoUpdate); }

	// Get the current number of particles in the effect.
	int				GetNumActiveParticles();

	// The is the max size of the particles for use in bounding	computation
	void			SetParticleCullRadius(float flMaxParticleRadius);

	// Build a list of all active particles, returns actual count filled in
	int				GetActiveParticleList(int nCount, Particle** ppParticleList);

	// detect origin/bbox changes and update leaf system if necessary
	void			DetectChanges();

private:
	// Change flags..
	void			SetFlag(int flag, int bOn) { if (bOn) m_Flags |= flag; else m_Flags &= ~flag; }
	int				GetFlag(int flag) const { return m_Flags & flag; }

	void			Init(IParticleMgr* pMgr, IParticleEffect* pSim);
	void			Term();

	// Get rid of the specified particle.
	void			RemoveParticle(Particle* pParticle);

	void			StartDrawMaterialParticles(
		CEffectMaterial* pMaterial,
		float flTimeDelta,
		IMesh*& pMesh,
		CMeshBuilder& builder,
		ParticleDraw& particleDraw,
		bool bWireframe);

	int				DrawMaterialParticles(
		bool bBucketSort,
		CEffectMaterial* pMaterial,
		float flTimeDelta,
		bool bWireframe
	);

	void			GrowBBoxFromParticlePositions(CEffectMaterial* pMaterial, bool& bboxSet, Vector& bbMin, Vector& bbMax);

	void			RenderStart(VMatrix& mTempModel, VMatrix& mTempView);
	void			RenderEnd(VMatrix& mModel, VMatrix& mView);

	void			BBoxCalcStart(Vector& bbMin, Vector& bbMax);
	void			BBoxCalcEnd(bool bboxSet, Vector& bbMin, Vector& bbMax);

	void			DoBucketSort(
		CEffectMaterial* pMaterial,
		float* zCoords,
		int nZCoords,
		float minZ,
		float maxZ);

	int				GetRemovalInProgressFlag() { return GetFlag(FLAGS_REMOVALINPROGRESS); }
	void			SetRemovalInProgressFlag() { SetFlag(FLAGS_REMOVALINPROGRESS, 1); }

	// BBox is recalculated before it's put into the tree for the first time.
	int				GetNeedsBBoxUpdate() { return GetFlag(FLAGS_NEEDS_BBOX_UPDATE); }
	void			SetNeedsBBoxUpdate(int bFirstUpdate) { SetFlag(FLAGS_NEEDS_BBOX_UPDATE, bFirstUpdate); }

	// Set on creation and cleared after the first PostRender (whether or not the system was rendered).
	int				GetFirstFrameFlag() { return GetFlag(FLAGS_FIRST_FRAME); }
	void			SetFirstFrameFlag(int bFirstUpdate) { SetFlag(FLAGS_FIRST_FRAME, bFirstUpdate); }

	int				WasDrawn() { return GetFlag(FLAGS_DRAWN); }
	void			SetDrawn(int bDrawn) { SetFlag(FLAGS_DRAWN, bDrawn); }

	// Update m_Min/m_Max. Returns false and sets the bbox to the sort origin if there are no particles.
	bool			RecalculateBoundingBox();

	CEffectMaterial* GetEffectMaterial(CParticleSubTexture* pSubTexture);

	// IClientRenderable overrides.
public:

	virtual const Vector& GetRenderOrigin(void);
	virtual const QAngle& GetRenderAngles(void);
	virtual const matrix3x4_t& RenderableToWorldTransform();
	virtual void					GetRenderBounds(Vector& mins, Vector& maxs);
	virtual bool					ShouldDraw(void);
	virtual bool					IsTransparent(void);
	virtual int						DrawModel(int flags);


private:

	enum
	{
		FLAGS_REMOVE = (1 << 0),	// Set in SetRemoveFlag
		FLAGS_REMOVALINPROGRESS = (1 << 1), // Set while the effect is being removed to prevent
		// infinite recursion.
		FLAGS_NEEDS_BBOX_UPDATE = (1 << 2),	// This is set until the effect's bbox has been updated once.
		FLAGS_AUTOUPDATEBBOX = (1 << 3),	// Update bbox automatically? Cleared in SetBBox.
		FLAGS_ALWAYSSIMULATE = (1 << 4), // See SetAlwaysSimulate.
		FLAGS_DRAWN = (1 << 5),	// Set if the effect is drawn through the leaf system.
		FLAGS_DRAWN_PREVFRAME = (1 << 6),	// Set if the effect was drawn the previous frame.
		// This can be used by particle effect classes
		// to decide whether or not they want to spawn
		// new particles - if they weren't drawn, then
		// they can 'freeze' the particle system to avoid
		// overhead.
		FLAGS_CAMERASPACE = (1 << 7),	// See SetEffectCameraSpace.
		FLAGS_DRAW_THRU_LEAF_SYSTEM = (1 << 8),	// This is the default - do the effect's visibility through the leaf system.
		FLAGS_DRAW_BEFORE_VIEW_MODEL = (1 << 9),// Draw before the view model? If this is set, it assumes FLAGS_DRAW_THRU_LEAF_SYSTEM goes off.
		FLAGS_AUTOAPPLYLOCALTRANSFORM = (1 << 10), // Automatically apply the local transform to CParticleMgr::GetModelView()'s matrix.
		FLAGS_FIRST_FRAME = (1 << 11),	// Cleared after the first frame that this system exists (so it can simulate after rendering once).
		FLAGS_NEW_PARTICLE_SYSTEM = (1 << 12) // uses new particle system
	};


	VMatrix m_LocalSpaceTransform;
	bool m_bLocalSpaceTransformIdentity;	// If this is true, then m_LocalSpaceTransform is assumed to be identity.

	// Bounding box. Stored in WORLD space.
	Vector							m_Min;
	Vector							m_Max;

	// paramter copies to detect changes
	Vector							m_LastMin;
	Vector							m_LastMax;

	// The particle cull size
	float							m_flParticleCullRadius;

	// Number of active particles.
	unsigned short					m_nActiveParticles;

	// See CParticleMgr::m_FrameCode.
	unsigned short					m_FrameCode;

	// For CParticleMgr's list index.
	unsigned short					m_ListIndex;

	IParticleEffect* m_pSim;
	IParticleMgr* m_pParticleMgr;

	// Combination of the CParticleEffectBinding::FLAGS_ flags.
	int								m_Flags;

	// Materials this effect is using.
	enum { EFFECT_MATERIAL_HASH_SIZE = 8 };
	CEffectMaterial* m_EffectMaterialHash[EFFECT_MATERIAL_HASH_SIZE];

	// For faster iteration.
	CUtlLinkedList<CEffectMaterial*, unsigned short> m_Materials;

	// auto updates the bbox after N frames
	unsigned short					m_UpdateBBoxCounter;
};

// ------------------------------------------------------------------------ //
// CParticleEffectBinding inlines.
// ------------------------------------------------------------------------ //

inline const matrix3x4_t& CParticleEffectBinding::GetLocalSpaceTransform() const
{
	return m_LocalSpaceTransform.As3x4();
}

enum
{
	TOOLPARTICLESYSTEMID_INVALID = -1,
};

// ------------------------------------------------------------------------------------------------ //
// CParticleEffect is the base class that you can derive from to make a particle effect.
// These can be used two ways:
//
// 1. Allocate a CParticleEffect-based object using the class's static Create() function. This gives
//    you back a smart pointer that handles the reference counting for you.
//
// 2. Contain a CParticleEffect object in your class.
// ------------------------------------------------------------------------------------------------ //

class CParticleEffect : public IParticleEffect
{
public:
	DECLARE_CLASS_NOBASE( CParticleEffect );

	friend class CRefCountAccessor;

	// Call this before adding a bunch of particles to give it a rough estimate of where
	// your particles are for sorting amongst other translucent entities.
	void				SetSortOrigin( const Vector &vSortOrigin );

	PMaterialHandle		GetPMaterial(const char *name);
	
	Particle*			AddParticle( unsigned int particleSize, PMaterialHandle material, const Vector &origin );

	CParticleEffectBinding&	GetBinding()	{ return m_ParticleEffect; }

	const char *GetEffectName();

	void AddFlags( int iFlags ) { m_Flags |= iFlags; }
	void RemoveFlags( int iFlags ) { m_Flags &= ~iFlags; }

	void SetDontRemove( bool bSet )
	{
		if( bSet )
			AddFlags( FLAG_DONT_REMOVE );
		else
			RemoveFlags( FLAG_DONT_REMOVE );
	}

// IParticleEffect overrides
public:

	virtual void				SetParticleCullRadius( float radius );
	virtual void				NotifyRemove( void );
	virtual const Vector &		GetSortOrigin();
	virtual void				NotifyDestroyParticle( Particle* pParticle );
	virtual void				Update( float flTimeDelta );

	// All Create() functions should call this so the effect deletes itself
	// when it is removed from the particle manager.
	void						SetDynamicallyAllocated( bool bDynamic=true );

	virtual bool				ShouldSimulate() const { return m_bSimulate; }
	virtual void				SetShouldSimulate( bool bSim ) { m_bSimulate = bSim; }

	int							AllocateToolParticleEffectId();
	int							GetToolParticleEffectId() const;
protected:
								CParticleEffect( const char *pDebugName );
	virtual						~CParticleEffect();

	// Returns nonzero if Release() has been called.
	int							IsReleased();
	
	enum
	{
		FLAG_ALLOCATED = (1<<1),	// Most of the CParticleEffects are dynamically allocated but
								// some are member variables of a class. If they're member variables.
		FLAG_DONT_REMOVE = (1<<2),
	};

	// Used to track down bugs.
	char const					*m_pDebugName;

	CParticleEffectBinding		m_ParticleEffect;
	Vector						m_vSortOrigin;
	
	int							m_Flags;		// Combination of CParticleEffect::FLAG_

	bool						m_bSimulate;
	int							m_nToolParticleEffectId;

private:
	// Update the reference count.
	void						AddRef();
	void						Release();
	
	int							m_RefCount;		// When this goes to zero and the effect has no more active
												// particles, (and it's dynamically allocated), it will delete itself.

	CParticleEffect( const CParticleEffect & ); // not defined, not accessible
};

inline int CParticleEffect::GetToolParticleEffectId() const
{
	return m_nToolParticleEffectId;
}

inline int CParticleEffect::AllocateToolParticleEffectId()
{
	m_nToolParticleEffectId = ParticleMgr()->AllocateToolParticleEffectId();
	return m_nToolParticleEffectId;
}


//-----------------------------------------------------------------------------
// Particle flags
//-----------------------------------------------------------------------------
enum SimpleParticleFlag_t
{
	SIMPLE_PARTICLE_FLAG_WINDBLOWN = 0x1,
	SIMPLE_PARTICLE_FLAG_NO_VEL_DECAY = 0x2	// Used by the blood spray emitter. By default, it decays the
											// particle velocity.
};

class SimpleParticle : public Particle
{
public:
	SimpleParticle() : m_iFlags(0) {}

	// AddSimpleParticle automatically initializes these fields.
	Vector		m_vecVelocity;
	float		m_flRoll;
	float		m_flDieTime;	// How long it lives for.
	float		m_flLifetime;	// How long it has been alive for so far.
	unsigned char	m_uchColor[3];
	unsigned char	m_uchStartAlpha;
	unsigned char	m_uchEndAlpha;
	unsigned char	m_uchStartSize;
	unsigned char	m_uchEndSize;
	unsigned char 	m_iFlags;	// See SimpleParticleFlag_t above
	float		m_flRollDelta;
};



// CSimpleEmitter implements a common way to simulate and render particles.
//
// Effects can add particles to the particle manager and point at CSimpleEmitter
// for the effect so they don't have to implement the simulation code. It simulates
// velocity, and fades their alpha from invisible to solid and back to invisible over their lifetime.
//
// Particles you add using this effect must use the class CParticleSimple::Particle.
class CSimpleEmitter : public CParticleEffect
{
// IParticleEffect overrides.
public:

	DECLARE_CLASS( CSimpleEmitter, CParticleEffect );

	static CSmartPtr<CSimpleEmitter>	Create( const char *pDebugName );

	virtual void	SimulateParticles( CParticleSimulateIterator *pIterator );
	virtual void	RenderParticles( CParticleRenderIterator *pIterator );

	void			SetNearClip( float nearClipMin, float nearClipMax );

	void			SetDrawBeforeViewModel( bool state = true );

	SimpleParticle*	AddSimpleParticle( PMaterialHandle hMaterial, const Vector &vOrigin, float flDieTime=3, unsigned char uchSize=10 );
	
// Overridables for variants like CEmberEffect.
protected:
					CSimpleEmitter( const char *pDebugName = NULL );
	virtual			~CSimpleEmitter();

	virtual	float	UpdateAlpha( const SimpleParticle *pParticle );
	virtual float	UpdateScale( const SimpleParticle *pParticle );
	virtual	float	UpdateRoll( SimpleParticle *pParticle, float timeDelta );
	virtual	void	UpdateVelocity( SimpleParticle *pParticle, float timeDelta );
	virtual Vector	UpdateColor( const SimpleParticle *pParticle );

	float			m_flNearClipMin;
	float			m_flNearClipMax;

private:
	CSimpleEmitter( const CSimpleEmitter & ); // not defined, not accessible
};

//==================================================
// EmberEffect
//==================================================

class CEmberEffect : public CSimpleEmitter
{
public:
							CEmberEffect( const char *pDebugName );
	static CSmartPtr<CEmberEffect>	Create( const char *pDebugName );

	virtual void UpdateVelocity( SimpleParticle *pParticle, float timeDelta );
	virtual Vector UpdateColor( const SimpleParticle *pParticle );

private:
	CEmberEffect( const CEmberEffect & ); // not defined, not accessible
};


//==================================================
// FireSmokeEffect
//==================================================

class CFireSmokeEffect : public CSimpleEmitter
{
public:
								CFireSmokeEffect( const char *pDebugName );
	static CSmartPtr<CFireSmokeEffect>	Create( const char *pDebugName );

	virtual void UpdateVelocity( SimpleParticle *pParticle, float timeDelta );
	virtual float UpdateAlpha( const SimpleParticle *pParticle );

protected:
	VPlane	m_planeClip;

private:
	CFireSmokeEffect( const CFireSmokeEffect & ); // not defined, not accessible
};


//==================================================
// CFireParticle
//==================================================

class CFireParticle : public CSimpleEmitter
{
public:
							CFireParticle( const char *pDebugName );
	static CSmartPtr<CFireParticle>	Create( const char *pDebugName );
	
	virtual Vector UpdateColor( const SimpleParticle *pParticle );

private:
	CFireParticle( const CFireParticle & ); // not defined, not accessible
};


#endif // PARTICLES_SIMPLE_H
