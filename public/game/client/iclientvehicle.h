//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef ICLIENTVEHICLE_H
#define ICLIENTVEHICLE_H

#ifdef _WIN32
#pragma once
#endif

#include "game/shared/IVehicle.h"

class C_BaseEntity;
class C_BaseCombatCharacter;
class C_BasePlayer;
class Vector;
class QAngle;
class IMoveHelper;
class CMoveData;

//-----------------------------------------------------------------------------
// Purpose: All client vehicles must implement this interface.
//-----------------------------------------------------------------------------
abstract_class IClientVehicle
{
public:
	// Get and set the current driver. Use PassengerRole_t enum in shareddefs.h for adding passengers
	virtual C_BaseCombatCharacter* GetPassenger(int nRole = VEHICLE_ROLE_DRIVER) = 0;
	virtual int						GetPassengerRole(C_BaseCombatCharacter* pPassenger) = 0;

	// Where is the passenger seeing from?
	virtual void			GetVehicleViewPosition(int nRole, Vector* pOrigin, QAngle* pAngles, float* pFOV = NULL) = 0;

	// Does the player use his normal weapons while in this mode?
	virtual bool			IsPassengerUsingStandardWeapons(int nRole = VEHICLE_ROLE_DRIVER) = 0;

	// Process movement
	virtual void			SetupMove(C_BasePlayer* player, CUserCmd* ucmd, IMoveHelper* pHelper, CMoveData* move) = 0;
	virtual void			ProcessMovement(C_BasePlayer* pPlayer, CMoveData* pMoveData) = 0;
	virtual void			FinishMove(C_BasePlayer* player, CUserCmd* ucmd, CMoveData* move) = 0;

	// Process input
	virtual void			ItemPostFrame(C_BasePlayer* pPlayer) = 0;
	// When a player is in a vehicle, here's where the camera will be
	virtual void GetVehicleFOV( float &flFOV ) = 0;

	// Allows the vehicle to restrict view angles, blend, etc.
	virtual void UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd ) = 0;

	// Hud redraw...
	virtual void DrawHudElements() = 0;

	// Is this predicted?
	virtual bool IsPredicted() const = 0;

	// Get the entity associated with the vehicle.
	virtual C_BaseEntity *GetVehicleEnt() = 0;

	// Allows the vehicle to change the near clip plane
	virtual void GetVehicleClipPlanes( float &flZNear, float &flZFar ) const = 0;
	
	// Allows vehicles to choose their own curves for players using joysticks
	virtual int GetJoystickResponseCurve() const = 0;

#ifdef HL2_CLIENT_DLL
	// Ammo in the vehicles
	virtual int GetPrimaryAmmoType() const = 0;
	virtual int GetPrimaryAmmoClip() const = 0;
	virtual bool PrimaryAmmoUsesClips() const = 0;
	virtual int GetPrimaryAmmoCount() const = 0;
#endif
};


#endif // ICLIENTVEHICLE_H
