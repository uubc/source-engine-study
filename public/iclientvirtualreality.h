//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains the IClientVirtualReality interface, which is implemented in 
//			client.dll and called by engine.dll
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef ICLIENTVIRTUALREALITY_H
#define ICLIENTVIRTUALREALITY_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "tier1/refcount.h"
#include "appframework/IAppSystem.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// important enumeration
//-----------------------------------------------------------------------------

// NOTE NOTE NOTE!!!!  If you up this, grep for "NEW_INTERFACE" to see if there is anything
// waiting to be enabled during an interface revision.
#define CLIENTVIRTUALREALITY_INTERFACE_VERSION "ClientVirtualReality001"

//-----------------------------------------------------------------------------
// The ISourceVirtualReality interface
//-----------------------------------------------------------------------------

enum HeadtrackMovementMode_t
{
	HMM_SHOOTFACE_MOVEFACE = 0,		// Shoot from your face, move along your face.
	HMM_SHOOTFACE_MOVETORSO,		// Shoot from your face, move the direction your torso is facing.
	HMM_SHOOTMOUSE_MOVEFACE,		// Shoot from the mouse cursor which moves within the HUD, move along your face.
	HMM_SHOOTBOUNDEDMOUSE_LOOKFACE_MOVEFACE,	// Shoot from the mouse cursor which moves, bounded within the HUD, move along your face.
	HMM_SHOOTBOUNDEDMOUSE_LOOKFACE_MOVEMOUSE,	// Shoot from the mouse cursor which moves, bounded within the HUD, move along your weapon (the "mouse")

	// The following are not intended to be user-selectable modes, they are used by e.g. followcam stuff.
	HMM_SHOOTMOVELOOKMOUSEFACE,		// Shoot & move & look along the mouse cursor (i.e. original unchanged gameplay), face just looks on top of that.
	HMM_SHOOTMOVEMOUSE_LOOKFACE,	// Shoot & move along the mouse cursor (i.e. original unchanged gameplay), face just looks.
	HMM_SHOOTMOVELOOKMOUSE,			// Shoot, move and look along the mouse cursor - HMD orientation is completely ignored!

	HMM_LAST,

	HMM_NOOVERRIDE = HMM_LAST		// Used as a retrun from ShouldOverrideHeadtrackControl(), not an actual mode.
};

abstract_class IClientVirtualReality : public IAppSystem
{
public:
	virtual ~IClientVirtualReality() {}

	// Placeholder for API revision
	virtual bool Connect( CreateInterfaceFn factory ) = 0;
	virtual void Disconnect() = 0;
	virtual void *QueryInterface( const char *pInterfaceName ) = 0;
	virtual InitReturnVal_t Init() = 0;
	virtual void Shutdown() = 0;

	// the interface

	// Draw the main menu in VR mode
	virtual void DrawMainMenu() = 0;
};



//-----------------------------------------------------------------------------

extern IClientVirtualReality *g_pClientVR;


#endif // ICLIENTVIRTUALREALITY_H
