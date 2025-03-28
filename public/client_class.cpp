//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "client_class.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ClientClass* g_pClientClassHead = NULL;

ClientClass* GetAllClientClasses(void)
{
	return g_pClientClassHead;
}


