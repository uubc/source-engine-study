//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NPC_COMBINES_H
#define NPC_COMBINES_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_combine.h"

//=========================================================
//	>> CNPC_CombineS
//=========================================================
class CNPC_CombineS : public CNPC_Combine
{
	DECLARE_CLASS( CNPC_CombineS, CNPC_Combine );
#if HL2_EPISODIC
	DECLARE_DATADESC();
#endif

public: 
	void		Spawn( void );
	void		Precache( void );
	void		DeathSound( const ITakeDamageInfo&info );
	void		PrescheduleThink( void );
	void		BuildScheduleTestBits( void );
	int			SelectSchedule ( void );
	float		GetHitgroupDamageMultiplier( int iHitGroup, const ITakeDamageInfo&info );
	void		HandleAnimEvent( animevent_t *pEvent );
	void		OnChangeActivity( Activity eNewActivity );
	void		Event_Killed( const ITakeDamageInfo&info );
	void		OnListened();

	void		ClearAttackConditions( void );

	bool		m_fIsBlocking;

	bool		IsLightDamage( const ITakeDamageInfo&info );
	bool		IsHeavyDamage( const ITakeDamageInfo&info );

	virtual	bool		AllowedToIgnite( void ) { return true; }

private:
	bool		ShouldHitPlayer( const Vector &targetDir, float targetDist );

#if HL2_EPISODIC
public:
	Activity	NPC_TranslateActivity( Activity eNewActivity );

protected:
	/// whether to use the more casual march anim in ep2_outland_05
	int			m_iUseMarch;
#endif

};

#endif // NPC_COMBINES_H
