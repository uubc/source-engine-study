//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef PARTICLEPROPERTY_H
#define PARTICLEPROPERTY_H
#ifdef _WIN32
#pragma once
#endif

#include "smartptr.h"
#include "globalvars_base.h"
#include "particles_new.h"
#include "IEffects.h"
//#include "particle_parse.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class C_BaseEntity;
typedef CHandle<C_BaseEntity> EHANDLE;
class CNewParticleEffect;


struct ParticleControlPoint_t
{
	ParticleControlPoint_t()
	{
		iControlPoint = 0;
		iAttachType = PATTACH_ABSORIGIN_FOLLOW;
		iAttachmentPoint = 0;
		vecOriginOffset = vec3_origin;
	}

	int								iControlPoint;
	ParticleAttachment_t			iAttachType;
	int								iAttachmentPoint;
	Vector							vecOriginOffset;
	EHANDLE							hEntity;
};

struct ParticleEffectList_t
{
	ParticleEffectList_t()
	{
		pParticleEffect = NULL;
	}

	CUtlVector<ParticleControlPoint_t>	pControlPoints;
	CSmartPtr<INewParticleEffect>		pParticleEffect;
};

extern int GetAttachTypeFromString( const char *pszString );

//-----------------------------------------------------------------------------
// Encapsulates particle handling for an entity
//-----------------------------------------------------------------------------
class CParticleProperty : public IParticleProperty
{
	DECLARE_CLASS_NOBASE( CParticleProperty );
	DECLARE_EMBEDDED_NETWORKVAR();
#ifdef CLIENT_DLL
	DECLARE_PREDICTABLE();
#endif // CLIENT_DLL
	DECLARE_DATADESC();

public:
	CParticleProperty();
	~CParticleProperty();

	void				Init( C_BaseEntity *pEntity );
	C_BaseEntity			*GetOuter( void ) { return m_pOuter; }

	// Effect Creation
	INewParticleEffect *Create( const char *pszParticleName, ParticleAttachment_t iAttachType, const char *pszAttachmentName );
	INewParticleEffect *Create( const char *pszParticleName, ParticleAttachment_t iAttachType, int iAttachmentPoint = INVALID_PARTICLE_ATTACHMENT, Vector vecOriginOffset = vec3_origin );
	void				AddControlPoint( INewParticleEffect *pEffect, int iPoint, C_BaseEntity *pEntity, ParticleAttachment_t iAttachType, const char *pszAttachmentName = NULL, Vector vecOriginOffset = vec3_origin );
	void				AddControlPoint( int iEffectIndex, int iPoint, C_BaseEntity *pEntity, ParticleAttachment_t iAttachType, int iAttachmentPoint = INVALID_PARTICLE_ATTACHMENT, Vector vecOriginOffset = vec3_origin );

	inline void			SetControlPointParent( INewParticleEffect *pEffect, int whichControlPoint, int parentIdx );
	void				SetControlPointParent( int iEffectIndex, int whichControlPoint, int parentIdx );

	// Commands
	void				StopEmission( INewParticleEffect *pEffect = NULL, bool bWakeOnStop = false, bool bDestroyAsleepSystems = false );
	void				StopEmissionAndDestroyImmediately( INewParticleEffect *pEffect = NULL );

	// kill all particle systems involving a given entity for their control points
	void				StopParticlesInvolving( C_BaseEntity *pEntity );
	void				StopParticlesNamed( const char *pszEffectName, bool bForceRemoveInstantly = false ); ///< kills all particles using the given definition name
	void				StopParticlesWithNameAndAttachment( const char *pszEffectName, int iAttachmentPoint, bool bForceRemoveInstantly = false ); ///< kills all particles using the given definition name

	// Particle System hooks
	void				OnParticleSystemUpdated( INewParticleEffect *pEffect, float flTimeDelta );
	void				OnParticleSystemDeleted( INewParticleEffect *pEffect );

#ifdef CLIENT_DLL
	void				OwnerSetDormantTo( bool bDormant );
#endif

	// Used to replace a particle effect with a different one; attaches the control point updating to the new one
	void				ReplaceParticleEffect( INewParticleEffect *pOldEffect, INewParticleEffect *pNewEffect );

	// Debugging
	void				DebugPrintEffects( void );

	int					FindEffect( const char *pEffectName, int nStart = 0 );
	inline INewParticleEffect *GetParticleEffectFromIdx( int idx );

private:
	int					GetParticleAttachment( C_BaseEntity *pEntity, const char *pszAttachmentName, const char *pszParticleName );
	int					FindEffect( INewParticleEffect *pEffect );
	void				UpdateParticleEffect( ParticleEffectList_t *pEffect, bool bInitializing = false, int iOnlyThisControlPoint = -1 );
	void				UpdateControlPoint( ParticleEffectList_t *pEffect, int iPoint, bool bInitializing );

private:
	C_BaseEntity *m_pOuter;
	CUtlVector<ParticleEffectList_t>	m_ParticleEffects;
	int			m_iDormancyChangedAtFrame;

	friend class C_BaseEntity;
};

#include "particle_property_inlines.h"

#endif // PARTICLEPROPERTY_H
