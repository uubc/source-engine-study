//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef EFFECT_DISPATCH_DATA_H
#define EFFECT_DISPATCH_DATA_H
#ifdef _WIN32
#pragma once
#endif

#include "IEffects.h"
#include "particle_parse.h"

#ifdef CLIENT_DLL

	#include "dt_recv.h"
	#include "client_class.h"

	EXTERN_RECV_TABLE( DT_EffectData );
#else

	#include "dt_send.h"
	#include "server_class.h"

	EXTERN_SEND_TABLE( DT_EffectData );

#endif

// NOTE: These flags are specifically *not* networked; so it's placed above the max effect flag bits
#define EFFECTDATA_NO_RECORD 0x80000000

#define MAX_EFFECT_FLAG_BITS 8

#define CUSTOM_COLOR_CP1		9
#define CUSTOM_COLOR_CP2		10

// This is the class that holds whatever data we're sending down to the client to make the effect.
class CEffectDataShared : public CEffectData
{
// Don't mess with stuff below here. DispatchEffect handles all of this.
public:
	CEffectDataShared()
	{

	}

	const CEffectDataShared& operator=(const CEffectData& EffectData) 
	{
		CEffectData::operator=(EffectData);
		return *this;
	}

	int GetEffectNameIndex() { return m_iEffectName; }

private:

	#ifdef CLIENT_DLL
		DECLARE_CLIENTCLASS_NOBASE()
	#else
		DECLARE_SERVERCLASS_NOBASE()
	#endif

	int m_iEffectName;	// Entry in the EffectDispatch network string table. The is automatically handled by DispatchEffect().
};


#define MAX_EFFECT_DISPATCH_STRING_BITS	10
#define MAX_EFFECT_DISPATCH_STRINGS		( 1 << MAX_EFFECT_DISPATCH_STRING_BITS )

#ifdef CLIENT_DLL
bool SuppressingParticleEffects();
void SuppressParticleEffects( bool bSuppress );
#endif

#endif // EFFECT_DISPATCH_DATA_H
