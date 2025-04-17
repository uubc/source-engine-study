//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "usercmd.h"
#include "igamemovement.h"
#include "mathlib/mathlib.h"
#include "client.h"
#include "player_command.h"
#include "movehelper_server.h"
#include "game/server/iservervehicle.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IGameMovement *g_pGameMovement;
extern ConVar sv_noclipduringpause;

