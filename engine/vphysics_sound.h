//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VPHYSICS_SOUND_H
#define VPHYSICS_SOUND_H
#ifdef _WIN32
#pragma once
#endif

struct impactsound_t
{
	void* pGameData;
	int				entityIndex;
	int				soundChannel;
	float			volume;
	float			impactSpeed;
	unsigned short	surfaceProps;
	unsigned short	surfacePropsHit;
	Vector			origin;
};

// UNDONE: Use a sorted container and sort by volume/distance?
struct soundlist_t
{
	CUtlVector<impactsound_t>	elements;
	impactsound_t& GetElement(int index) { return elements[index]; }
	impactsound_t& AddElement() { return elements[elements.AddToTail()]; }
	int Count() { return elements.Count(); }
	void RemoveAll() { elements.RemoveAll(); }
};

struct breaksound_t
{
	Vector			origin;
	int				surfacePropsBreak;
};

#endif // VPHYSICS_SOUND_H
