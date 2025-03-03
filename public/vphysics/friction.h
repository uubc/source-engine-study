//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FRICTION_H
#define FRICTION_H
#ifdef _WIN32
#pragma once
#endif

#include "vphysics_interface.h"

// NOTE: This is an iterator for the contact points on an object
// NOTE: This should only be used temporarily.  Holding one of these
// NOTE: across collision callbacks or calls into simulation will cause errors!
// NOTE: VPHYSICS may choose to make the data contained within this object invalid 
// NOTE: any time simulation is run.
class IPhysicsFrictionSnapshot
{
public:
	virtual ~IPhysicsFrictionSnapshot() {}

	virtual bool IsValid() = 0;

	// Object 0 is this object, Object 1 is the other object
	virtual IPhysicsObject *GetObject( int index ) = 0;
	virtual int GetMaterial( int index ) = 0;

	virtual void GetContactPoint( Vector &out ) = 0;
	
	// points away from source object
	virtual void GetSurfaceNormal( Vector &out ) = 0;
	virtual float GetNormalForce() = 0;
	virtual float GetEnergyAbsorbed() = 0;

	// recompute friction (useful if dynamically altering materials/mass)
	virtual void RecomputeFriction() = 0;
	// clear all friction force at this contact point
	virtual void ClearFrictionForce() = 0;

	virtual void MarkContactForDelete() = 0;
	virtual void DeleteAllMarkedContacts( bool wakeObjects ) = 0;

	// Move to the next friction data for this object
	virtual void NextFrictionData() = 0;
	virtual float GetFrictionCoefficient() = 0;
};

inline void PhysComputeSlideDirection(IPhysicsObject* pPhysics, const Vector& inputVelocity, const AngularImpulse& inputAngularVelocity,
	Vector* pOutputVelocity, Vector* pOutputAngularVelocity, float minMass)
{
	Vector velocity = inputVelocity;
	AngularImpulse angVel = inputAngularVelocity;
	//Vector pos;

	IPhysicsFrictionSnapshot* pSnapshot = pPhysics->CreateFrictionSnapshot();
	while (pSnapshot->IsValid())
	{
		IPhysicsObject* pOther = pSnapshot->GetObject(1);
		if (!pOther->IsMoveable() || pOther->GetMass() > minMass)
		{
			Vector normal;
			pSnapshot->GetSurfaceNormal(normal);

			// BUGBUG: Figure out the correct rotation clipping equation
			if (pOutputAngularVelocity)
			{
				angVel = normal * DotProduct(angVel, normal);
#if 0
				pSnapshot->GetContactPoint(point);
				Vector point, dummy;
				AngularImpulse angularClip, clip2;

				pPhysics->CalculateVelocityOffset(normal, point, dummy, angularClip);
				VectorNormalize(angularClip);
				float proj = DotProduct(angVel, angularClip);
				if (proj > 0)
				{
					angVel -= angularClip * proj;
				}
				CrossProduct(angularClip, normal, clip2);
				proj = DotProduct(angVel, clip2);
				if (proj > 0)
				{
					angVel -= clip2 * proj;
				}
				//NDebugOverlay::Line( point, point - normal * 20, 255, 0, 0, true, 0.1 );
#endif
			}

			// Determine how far along plane to slide based on incoming direction.
			// NOTE: Normal points away from this object
			float proj = DotProduct(velocity, normal);
			if (proj > 0.0f)
			{
				velocity -= normal * proj;
			}
		}
		pSnapshot->NextFrictionData();
	}
	pPhysics->DestroyFrictionSnapshot(pSnapshot);

	//NDebugOverlay::Line( pos, pos + unitVel * 20, 0, 0, 255, true, 0.1 );

	if (pOutputVelocity)
	{
		*pOutputVelocity = velocity;
	}
	if (pOutputAngularVelocity)
	{
		*pOutputAngularVelocity = angVel;
	}
}

#endif // FRICTION_H
