//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HL2 specific input handling
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "kbutton.h"
#include "input.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: HL Input interface
//-----------------------------------------------------------------------------
class CHLInput : public CUserInput
{
public:
};

static CHLInput g_UserInput;

// Expose this interface
IUserInput * g_pUserInput = &g_UserInput;
