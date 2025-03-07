//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef SOUNDENVELOPE_H
#define SOUNDENVELOPE_H

#ifdef _WIN32
#pragma once
#endif

#include "ihandleentity.h"
#include "engine/IEngineSound.h"

class CBasePlayer;





class IRecipientFilter;




//-----------------------------------------------------------------------------
// Save/restore
//-----------------------------------------------------------------------------
class ISaveRestoreOps;

ISaveRestoreOps *GetSoundSaveRestoreOps( );

#define DEFINE_SOUNDPATCH(name) \
	{ FIELD_CUSTOM, #name, { offsetof(classNameTypedef,name), 0 }, 1, FTYPEDESC_SAVE, NULL, GetSoundSaveRestoreOps( ), NULL }

extern ISoundEnvelopeController* g_pSoundEnvelopeController;

#endif // SOUNDENVELOPE_H
