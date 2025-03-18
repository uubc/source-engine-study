//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_VIEW_SCENE_H
#define CS_VIEW_SCENE_H
#ifdef _WIN32
#pragma once
#endif

#include "viewrender.h"
#include "igamesystem.h"

//-----------------------------------------------------------------------------
// Purpose: Implements the interview to view rendering for the client .dll
//-----------------------------------------------------------------------------
class CCSViewRender : CAutoGameSystem, public IViewRenderCallBack
{
public:
	CCSViewRender();

	virtual bool Init( void );
	virtual void PreRenderView(const CViewSetup& view, int nClearFlags, int whatToDraw) {}
	virtual void GetScreenFadeDistances( float *min, float *max );
	virtual void Render2DEffectsPreHUD( const CViewSetup &view );
	virtual void Render2DEffectsPostHUD( const CViewSetup &view );
	virtual void RenderPlayerSprites( void );
	virtual void PostRenderView(const CViewSetup& view, int nClearFlags, int whatToDraw) {}
private:

	void PerformFlashbangEffect( const CViewSetup &view );
	void PerformNightVisionEffect( const CViewSetup &view );
	
	ITexture *m_pFlashTexture;
};

#endif //CS_VIEW_SCENE_H