//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-server neutral effects interface
//
// $NoKeywords: $
//=============================================================================//

#ifndef IEFFECTS_H
#define IEFFECTS_H

#ifdef _WIN32
#pragma once
#endif

#include "platform.h"
#include "shake.h"
#include "basetypes.h"
#include "mathlib/vector.h"
#include "interface.h"
//#include "ipredictionsystem.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class Vector;
class CGameTrace;
typedef CGameTrace trace_t;

//-----------------------------------------------------------------------------
// Particle attachment methods
//-----------------------------------------------------------------------------
enum ParticleAttachment_t
{
	PATTACH_ABSORIGIN = 0,			// Create at absorigin, but don't follow
	PATTACH_ABSORIGIN_FOLLOW,		// Create at absorigin, and update to follow the entity
	PATTACH_CUSTOMORIGIN,			// Create at a custom origin, but don't follow
	PATTACH_POINT,					// Create on attachment point, but don't follow
	PATTACH_POINT_FOLLOW,			// Create on attachment point, and update to follow the entity

	PATTACH_WORLDORIGIN,			// Used for control points that don't attach to an entity

	PATTACH_ROOTBONE_FOLLOW,		// Create at the root bone of the entity, and update to follow

	MAX_PATTACH_TYPES,
};

struct te_tf_particle_effects_colors_t
{
	Vector m_vecColor1;
	Vector m_vecColor2;
};

struct te_tf_particle_effects_control_point_t
{
	ParticleAttachment_t m_eParticleAttachment;
	Vector m_vecOffset;
};

class CEffectData
{
public:
	Vector m_vOrigin;
	Vector m_vStart;
	Vector m_vNormal;
	QAngle m_vAngles;
	int		m_fFlags;
	CBaseHandle m_hEntity;
	float	m_flScale;
	float	m_flMagnitude;
	float	m_flRadius;
	int		m_nAttachmentIndex;
	short	m_nSurfaceProp;

	// Some TF2 specific things
	int		m_nMaterial;
	int		m_nDamageType;
	int		m_nHitBox;

	unsigned char	m_nColor;

	// Color customizability
	bool							m_bCustomColors;
	te_tf_particle_effects_colors_t	m_CustomColors;

	bool									m_bControlPoint1;
	te_tf_particle_effects_control_point_t	m_ControlPoint1;

	CEffectData() {
		m_vOrigin.Init();
		m_vStart.Init();
		m_vNormal.Init();
		m_vAngles.Init();

		m_fFlags = 0;
		m_hEntity = NULL;
		m_flScale = 1.f;
		m_nAttachmentIndex = 0;
		m_nSurfaceProp = 0;

		m_flMagnitude = 0.0f;
		m_flRadius = 0.0f;

		m_nMaterial = 0;
		m_nDamageType = 0;
		m_nHitBox = 0;

		m_nColor = 0;

		m_bCustomColors = false;
		m_CustomColors.m_vecColor1.Init();
		m_CustomColors.m_vecColor2.Init();

		m_bControlPoint1 = false;
		m_ControlPoint1.m_eParticleAttachment = PATTACH_ABSORIGIN;
		m_ControlPoint1.m_vecOffset.Init();
	}
};

//-----------------------------------------------------------------------------
// Client-server neutral effects interface
//-----------------------------------------------------------------------------
#define IEFFECTS_INTERFACE_VERSION	"IEffects001"
abstract_class IEffects
{
public:
	//
	// Particle effects
	//
	//virtual void Beam( const Vector &Start, const Vector &End, int nModelIndex, 
	//	int nHaloIndex, unsigned char frameStart, unsigned char frameRate,
	//	float flLife, unsigned char width, unsigned char endWidth, unsigned char fadeLength, 
	//	unsigned char noise, unsigned char red, unsigned char green,
	//	unsigned char blue, unsigned char brightness, unsigned char speed) = 0;

	//-----------------------------------------------------------------------------
	// Purpose: Emits smoke sprites.
	// Input  : origin - Where to emit the sprites.
	//			scale - Sprite scale * 10.
	//			framerate - Framerate at which to animate the smoke sprites.
	//-----------------------------------------------------------------------------
	virtual void Smoke( const Vector &origin, int modelIndex, float scale, float framerate ) = 0;

	virtual void Sparks( const Vector &position, int nMagnitude = 1, int nTrailLength = 1, const Vector *pvecDir = NULL ) = 0;

	virtual void Dust( const Vector &pos, const Vector &dir, float size, float speed ) = 0;

	virtual void MuzzleFlash( const Vector &vecOrigin, const QAngle &vecAngles, float flScale, int iType ) = 0;

	// like ricochet, but no sound
	virtual void MetalSparks( const Vector &position, const Vector &direction ) = 0; 

	virtual void EnergySplash( const Vector &position, const Vector &direction, bool bExplosive = false ) = 0;

	virtual void Ricochet( const Vector &position, const Vector &direction ) = 0;

	// FIXME: Should these methods remain in this interface? Or go in some 
	// other client-server neutral interface?
	virtual float Time() = 0;
	virtual bool IsServer() = 0;

	// Used by the playback system to suppress sounds
	virtual void SuppressEffectsSounds( bool bSuppress ) = 0;

	virtual void WaterRipple(const Vector& origin, float scale, Vector* pColor, float flLifetime = 1.5, float flAlpha = 1) = 0;
	virtual void GunshotSplash(const Vector& origin, const Vector& normal, float scale) = 0;
	virtual void GunshotSlimeSplash(const Vector& origin, const Vector& normal, float scale) = 0;
	virtual void GetSplashLighting(Vector position, Vector* color, float* luminosity) = 0;

	virtual void DispatchEffect(const char* pName, const CEffectData& data) = 0;
	virtual void DispatchEffect(const char* pName, const CEffectData& data, IRecipientFilter& filter) = 0;
};


//-----------------------------------------------------------------------------
// Client-server neutral effects interface accessor
//-----------------------------------------------------------------------------
extern IEffects *g_pEffects;
			   

#endif // IEFFECTS_H
