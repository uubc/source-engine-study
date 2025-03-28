//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "server_class.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ServerClass* g_pServerClassHead = NULL;

ServerClass* GetAllServerClasses(void) 
{
	return g_pServerClassHead;
}