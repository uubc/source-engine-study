//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "prediction.h"
#include "c_dod_player.h"
#include "igamemovement.h"


class CDODPrediction : public CPrediction
{
DECLARE_CLASS( CDODPrediction, CPrediction );

public:
	
};

// Expose interface to engine
// Expose interface to engine
static CDODPrediction g_Prediction;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CDODPrediction, IPrediction, VCLIENT_PREDICTION_INTERFACE_VERSION, g_Prediction );

CPrediction *prediction = &g_Prediction;
IPrediction* g_pClientSidePrediction = &g_Prediction;
