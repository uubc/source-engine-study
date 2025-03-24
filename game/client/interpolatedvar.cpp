//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "interpolatedvar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// warning C4660: template-class specialization 'CInterpolatedVar<float>' is already instantiated
#pragma warning( disable : 4660 )

template class CInterpolatedVar<float>;
template class CInterpolatedVar<Vector>;
template class CInterpolatedVar<QAngle>;
template class CInterpolatedVar<C_AnimationLayer>;





ConVar cl_extrapolate_amount( "cl_extrapolate_amount", "0.25", FCVAR_CHEAT, "Set how many seconds the client will extrapolate entities for." );

