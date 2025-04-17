//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "prediction.h"
#include "c_cs_player.h"
#include "igamemovement.h"

class CCSPrediction : public CPrediction
{
DECLARE_CLASS( CCSPrediction, CPrediction );

public:
	
};


// Expose interface to engine
// Expose interface to engine
static CCSPrediction g_Prediction;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CCSPrediction, IPrediction, VCLIENT_PREDICTION_INTERFACE_VERSION, g_Prediction );

CPrediction *prediction = &g_Prediction;
IPrediction* g_pClientSidePrediction = &g_Prediction;
