//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DOD_VIEW_SCENE_H
#define DOD_VIEW_SCENE_H
#ifdef _WIN32
#pragma once
#endif

#include "viewrender.h"
#include "colorcorrectionmgr.h"
#include "igamesystem.h"

//-----------------------------------------------------------------------------
// Purpose: Implements the interview to view rendering for the client .dll
//-----------------------------------------------------------------------------
class CDODViewRender : CAutoGameSystem, public IViewRenderCallBack
{
public:
	CDODViewRender();

	bool Init( );
	void Shutdown( );
	void PreRenderView(const CViewSetup& view, int nClearFlags, int whatToDraw);
	void GetScreenFadeDistances(float* min, float* max);
	void Render2DEffectsPreHUD(const CViewSetup& view) {}
	void Render2DEffectsPostHUD(const CViewSetup& view) {}
	void RenderPlayerSprites();
	void PostRenderView(const CViewSetup& view, int nClearFlags, int whatToDraw);

	void PerformStunEffect( const CViewSetup &view );

	void InitColorCorrection( );
	void ShutdownColorCorrection( );
	void SetupColorCorrection( );

private:
	ITexture *m_pStunTexture;

	ClientCCHandle_t		m_SpectatorLookupHandle;
	ClientCCHandle_t		m_DeathLookupHandle;
	bool					m_bLookupActive;
};

#endif //DOD_VIEW_SCENE_H