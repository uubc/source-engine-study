//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_SAVERESTORE_H
#define PHYSICS_SAVERESTORE_H

#if defined( _WIN32 )
#pragma once
#endif

#include "tier1/utlpriorityqueue.h"
#include "entitylist_base.h"

class ISaveRestoreBlockHandler;
class IPhysicsObject;
class CPhysCollide;
class IVModelInfo;

//-----------------------------------------------------------------------------

struct BBox_t
{
	Vector mins, maxs;
};

struct Sphere_t
{
	float radius;
};


struct PhysObjectHeader_t
{
	PhysObjectHeader_t()
	{
		memset(this, 0, sizeof(*this));
	}

	PhysInterfaceId_t 	type;
	CBaseHandle			hEntity;
	string_t			fieldName;
	int 				nObjects;
	string_t			modelName;
	BBox_t				bbox;
	Sphere_t			sphere;
	int					iCollide;

	DECLARE_SIMPLE_DATADESC();
};

struct PhysBlockHeader_t
{
	int				nSaved;
	IPhysicsObject* pWorldObject;

	inline void Clear()
	{
		nSaved = 0;
		pWorldObject = 0;
	}

	DECLARE_SIMPLE_DATADESC();
};

class CPhysSaveRestoreBlockHandler : public IPhysSaveRestoreBlockHandler
{
	struct QueuedItem_t;
public:
	CPhysSaveRestoreBlockHandler(IEntityList* pEntityList);

	const char* GetBlockName();

	//---------------------------------

	virtual void PreSave(CSaveRestoreData*);

	//---------------------------------

	virtual void Save(ISave* pSave);

	//---------------------------------

	virtual void WriteSaveHeaders(ISave* pSave);

	//---------------------------------

	virtual void PostSave();

	//---------------------------------

	virtual void PreRestore();

	//---------------------------------

	virtual void ReadRestoreHeaders(IRestore* pRestore);

	//---------------------------------

	virtual void Restore(IRestore* pRestore, bool);

	//---------------------------------

	void RestoreBlock(IRestore* pRestore, const PhysObjectHeader_t& header);


	//---------------------------------

	virtual int GetModelIndexFromHeader(const PhysObjectHeader_t& header) = 0;

	void RestorePhysicsObjectAndModel(IRestore* pRestore, const PhysObjectHeader_t& header, CPhysSaveRestoreBlockHandler::QueuedItem_t* pItem, int nObjects);

	//---------------------------------

	virtual void PostRestore();

	//---------------------------------

	void QueueSave(IHandleEntity* pOwner, typedescription_t* pTypeDesc, void** ppPhysObj, PhysInterfaceId_t type);

	//---------------------------------

	void QueueRestore(IHandleEntity* pOwner, typedescription_t* pTypeDesc, void** ppPhysObj, PhysInterfaceId_t type);

	//---------------------------------

	void SavePhysicsObject(ISave* pSave, IHandleEntity* pOwner, void* pObject, PhysInterfaceId_t type);

	//---------------------------------

	void RestorePhysicsObject(IRestore* pRestore, const PhysObjectHeader_t& header, void** ppObject, const CPhysCollide* pCollide = NULL);
	//-----------------------------------------------------
// IEntityListener methods
// This object is only a listener during restore	
	virtual void OnEntityCreated(IHandleEntity* pEntity);

	//---------------------------------

	virtual void OnEntityDeleted(IHandleEntity* pEntity);

	//-----------------------------------------------------
	// IPhysSaveRestoreManager methods

	virtual void NoteBBox(const Vector& mins, const Vector& maxs, CPhysCollide* pCollide);

	//---------------------------------

	virtual void AssociateModel(IPhysicsObject* pObject, int modelIndex);

	//---------------------------------

	virtual void AssociateModel(IPhysicsObject* pObject, const CPhysCollide* pModel);

	//---------------------------------

	virtual void ForgetModel(IPhysicsObject* pObject);

	//---------------------------------

	virtual void ForgetAllModels();

	//---------------------------------

	string_t GetModelName(IPhysicsObject* pObject);

	//---------------------------------

	BBox_t* GetBBox(IPhysicsObject* pObject);

	//---------------------------------
	IEntityList* m_pEntityList = NULL;
private:
	struct QueuedItem_t
	{
		PhysObjectHeader_t	  header;
		void** ppPhysObj;
		IEntityList* pEntityList;
	};

	class CEntityRestoreSet : public CUtlVector<QueuedItem_t>
	{
	public:
		int Add(IHandleEntity* pOwner, typedescription_t* pTypeDesc, void** ppPhysObj, PhysInterfaceId_t type);
		QueuedItem_t* FindItem(string_t itemFieldName);
	};
	
	//---------------------------------

	static bool SaveQueueFunc(const QueuedItem_t& left, const QueuedItem_t& right);

	//---------------------------------

	CUtlPriorityQueue<QueuedItem_t> 			m_QueuedSaves;
	CUtlMap<IHandleEntity*, CEntityRestoreSet*>	m_QueuedRestores;
	bool 										m_fDoLoad;

	//---------------------------------

	CUtlMap<IPhysicsObject*, int>					m_PhysObjectModels;
	CUtlMap<IPhysicsObject*, const CPhysCollide*>	m_PhysObjectCustomModels;
	CUtlMap<const CPhysCollide*, BBox_t>			m_PhysCollideBBoxModels;

	//---------------------------------

	PhysBlockHeader_t							m_blockHeader;
};

class CServerPhysSaveRestoreBlockHandler : public CPhysSaveRestoreBlockHandler, public IEntityListener<IServerEntity>
{
public:
	CServerPhysSaveRestoreBlockHandler(IEntityList* pEntityList) 
		:CPhysSaveRestoreBlockHandler(pEntityList)
	{
	
	}
	//-----------------------------------------------------
	// IEntityListener methods
	// This object is only a listener during restore	
	virtual void OnEntityCreated(IServerEntity* pEntity);

	//---------------------------------

	virtual void OnEntityDeleted(IServerEntity* pEntity);

	virtual int GetModelIndexFromHeader(const PhysObjectHeader_t& header);
};

class CClientPhysSaveRestoreBlockHandler : public CPhysSaveRestoreBlockHandler 
{
public:
	CClientPhysSaveRestoreBlockHandler(IEntityList* pEntityList)
		:CPhysSaveRestoreBlockHandler(pEntityList)
	{

	}
	virtual int GetModelIndexFromHeader(const PhysObjectHeader_t& header);
};

//=============================================================================

#endif // PHYSICS_SAVERESTORE_H
