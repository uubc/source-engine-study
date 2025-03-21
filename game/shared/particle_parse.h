//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef PARTICLE_PARSE_H
#define PARTICLE_PARSE_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "utlstring.h"
#include "ifilelist.h"
#include "IEffects.h"


extern int GetAttachTypeFromString( const char *pszString );

#define PARTICLE_DISPATCH_FROM_ENTITY		(1<<0)
#define PARTICLE_DISPATCH_RESET_PARTICLES	(1<<1)



//-----------------------------------------------------------------------------
// Particle parsing methods
//-----------------------------------------------------------------------------
// Parse the particle manifest file & register the effects within it
// Only needs to be called once per game, unless tools change particle definitions
void ParseParticleEffects( bool bLoadSheets, bool bPrecache );
void ParseParticleEffectsMap( const char *pMapName, bool bLoadSheets, IFileList *pFilesToReload = NULL ); 

// Get a list of the files inside the particle manifest file
void GetParticleManifest( CUtlVector<CUtlString>& list );

// Precaches standard particle systems (only necessary on server)
// Should be called once per level
void PrecacheStandardParticleSystems( );

class IFileList;
void ReloadParticleEffectsInList( IFileList *pFilesToReload );

//-----------------------------------------------------------------------------
// Particle spawning methods
//-----------------------------------------------------------------------------
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, IHandleEntity *pEntity, const char *pszAttachmentName, bool bResetAllParticlesOnEntity = false );
void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, IHandleEntity *pEntity = NULL, int iAttachmentPoint = -1, bool bResetAllParticlesOnEntity = false );
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, QAngle vecAngles, IHandleEntity *pEntity = NULL );
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, Vector vecStart, QAngle vecAngles, IHandleEntity *pEntity = NULL );
void DispatchParticleEffect( int iEffectIndex, Vector vecOrigin, Vector vecStart, QAngle vecAngles, IHandleEntity *pEntity = NULL );

void DispatchParticleEffect( const char *pszParticleName, ParticleAttachment_t iAttachType, IHandleEntity *pEntity, const char *pszAttachmentName, Vector vecColor1, Vector vecColor2, bool bUseColors=true, bool bResetAllParticlesOnEntity = false );
void DispatchParticleEffect( const char *pszParticleName, Vector vecOrigin, QAngle vecAngles, Vector vecColor1, Vector vecColor2, bool bUseColors=true, IHandleEntity *pEntity = NULL, int iAttachType = PATTACH_CUSTOMORIGIN );


void StopParticleEffects( IHandleEntity *pEntity );


#endif // PARTICLE_PARSE_H
