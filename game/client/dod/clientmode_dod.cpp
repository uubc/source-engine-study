//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "cbase.h"
#include "clientmode_dod.h"
#include "cdll_client_int.h"
#include "iinput.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include <vgui_controls/AnimationController.h>
#include "ivmodemanager.h"
#include "buymenu.h"
#include "filesystem.h"
#include "vgui/IVGui.h"

#include "view_shared.h"
#include "ivrenderview.h"
#include "model_types.h"
#include "iefx.h"
#include "dlight.h"
#include <imapoverview.h>
#include <KeyValues.h>
#include "text_message.h"
#include "dod_shareddefs.h"
#include "c_dod_player.h"
#include "bitbuf.h"

#include "vgui/ILocalize.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHudChat;








