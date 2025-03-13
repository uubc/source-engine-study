//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== tf_client.cpp ========================================================

  HL2 client/server game specific stuff

*/

#include "cbase.h"
#include "player.h"
//#include "physics.h"
#include "game.h"
#include "ai_network.h"
#include "ai_node.h"
#include "ai_hull.h"
#include "shake.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"
#include "cs_player.h"
#include "cs_gamerules.h"
#include "cs_bot.h"
#include "tier0/vprof.h"
#include "teamplayroundbased_gamerules.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

