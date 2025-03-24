//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "tier3/tier3.h"
#include "datacache/imdlcache.h"
#include "engine/ivmodelinfo.h"
#include "datamap.h"
#include "isaverestore.h"
#include "saverestoretypes.h"
#include "iserverentity.h"
#include "icliententity.h"
#include "physics_shared.h"
#include "physics_saverestore.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

static short PHYS_SAVE_RESTORE_VERSION = 5;


BEGIN_SIMPLE_DATADESC( PhysBlockHeader_t )
	DEFINE_FIELD( nSaved,	FIELD_INTEGER ),
	// NOTE: We want to save the actual address here for remapping, so use an integer
	DEFINE_FIELD( pWorldObject, FIELD_POINTER ),	
END_DATADESC()

//#if defined(_STATIC_LINKED) && defined(CLIENT_DLL)
//const char *g_ppszPhysTypeNames[PIID_NUM_TYPES] =
//{
//	"Unknown",
//	"IPhysicsObject",
//	"IPhysicsFluidController",
//	"IPhysicsSpring",
//	"IPhysicsConstraintGroup",
//	"IPhysicsConstraint",
//	"IPhysicsShadowController",
//	"IPhysicsPlayerController",
//	"IPhysicsMotionController",
//	"IPhysicsVehicleController",
//};
//#endif

//-----------------------------------------------------------------------------

BEGIN_SIMPLE_DATADESC( PhysObjectHeader_t )
  	DEFINE_FIELD( type,			FIELD_INTEGER ),
  	DEFINE_FIELD( hEntity,		FIELD_EHANDLE ),
  	DEFINE_FIELD( fieldName,	FIELD_STRING ),
  	DEFINE_FIELD( nObjects,		FIELD_INTEGER ),
  	DEFINE_FIELD( modelName,	FIELD_STRING ),

	// Silence, Classcheck!
	// DEFINE_FIELD( bbox, BBox_t ),
	// DEFINE_FIELD( sphere, Sphere_t ),

  	DEFINE_FIELD( bbox.mins,	FIELD_VECTOR ),
  	DEFINE_FIELD( bbox.maxs,	FIELD_VECTOR ),
  	DEFINE_FIELD( sphere.radius, FIELD_FLOAT ),
  	DEFINE_FIELD( iCollide,		FIELD_INTEGER ),
END_DATADESC()


CPhysSaveRestoreBlockHandler::CPhysSaveRestoreBlockHandler(IEntityList* pEntityList)
{
	m_pEntityList = pEntityList;
	m_QueuedSaves.SetLessFunc( SaveQueueFunc );
	SetDefLessFunc( m_QueuedRestores );
	SetDefLessFunc( m_PhysObjectModels );
	SetDefLessFunc( m_PhysObjectCustomModels );
	SetDefLessFunc( m_PhysCollideBBoxModels );
}

const char * CPhysSaveRestoreBlockHandler::GetBlockName()
{
	return "Physics";
}

//---------------------------------

void CPhysSaveRestoreBlockHandler::PreSave( CSaveRestoreData * )
{
	m_blockHeader.Clear();
}
	
//---------------------------------

void CPhysSaveRestoreBlockHandler::Save( ISave *pSave )
{
	m_blockHeader.pWorldObject = m_pEntityList->PhysGetWorldObject();
	m_blockHeader.nSaved = m_QueuedSaves.Count();

	while ( m_QueuedSaves.Count() )
	{
		const QueuedItem_t &item = m_QueuedSaves.ElementAtHead();
			
		IHandleEntity *pOwner = m_pEntityList->GetBaseEntityFromHandle(item.header.hEntity);
			
		if ( pOwner )
		{
			pSave->WriteAll( &item.header );
			pSave->StartBlock(); //  Need block here in case entity is NULL on load
			if ( item.header.nObjects )
			{
				for ( int i = 0; i < item.header.nObjects; i++ )
				{
					// Starting a block here allows the implementation of any individual physics
					// class save/load to change non-trivially while retaining the overall
					// integrity of the savefile.
					pSave->StartBlock();
					SavePhysicsObject( pSave, pOwner, item.ppPhysObj[i], item.header.type );
					pSave->EndBlock();
				}
			}
			// else, it will simply be recreated on restore
			pSave->EndBlock();
		}
		m_QueuedSaves.RemoveAtHead();
	}
}
	
//---------------------------------

void CPhysSaveRestoreBlockHandler::WriteSaveHeaders( ISave *pSave )
{
	pSave->WriteShort( &PHYS_SAVE_RESTORE_VERSION );
	pSave->WriteAll( &m_blockHeader );
}
	
//---------------------------------

void CPhysSaveRestoreBlockHandler::PostSave()
{
	m_QueuedSaves.Purge();
}
	
//---------------------------------

void CPhysSaveRestoreBlockHandler::PreRestore()
{
	// UNDONE: This never runs!!!!
	if (m_pEntityList->PhysGetEnv())
	{
		physprerestoreparams_t params;
		params.recreatedObjectCount = 0;
		m_pEntityList->PhysGetEnv()->PreRestore( params );
	}
}
	
//---------------------------------

void CPhysSaveRestoreBlockHandler::ReadRestoreHeaders( IRestore *pRestore )
{
	// No reason why any future version shouldn't try to retain backward compatability. The default here is to not do so.
	short version = pRestore->ReadShort();
	m_fDoLoad = ( version == PHYS_SAVE_RESTORE_VERSION );

	pRestore->ReadAll( &m_blockHeader );
}

//---------------------------------
	
void CPhysSaveRestoreBlockHandler::Restore( IRestore *pRestore, bool )
{
	if ( m_fDoLoad )
	{
		if (m_pEntityList->PhysGetEnv())
		{
			physprerestoreparams_t params;
			params.recreatedObjectCount = 1;
			params.recreatedObjectList[0].pNewObject = m_pEntityList->PhysGetWorldObject();
			params.recreatedObjectList[0].pOldObject = m_blockHeader.pWorldObject;
			m_pEntityList->PhysGetEnv()->PreRestore( params );
		}

		PhysObjectHeader_t header;

		while ( m_blockHeader.nSaved-- )
		{
			pRestore->ReadAll( &header );
			pRestore->StartBlock();
				
			if ( header.hEntity != NULL )
			{
				RestoreBlock( pRestore, header );
			}

			pRestore->EndBlock();
		}
	}
}
	
//---------------------------------
	
void CPhysSaveRestoreBlockHandler::RestoreBlock( IRestore *pRestore, const PhysObjectHeader_t &header )
{
	IHandleEntity *  pOwner  = m_pEntityList->GetBaseEntityFromHandle(header.hEntity);
	unsigned short iQueued = m_QueuedRestores.Find( pOwner );
		
	if ( iQueued != m_QueuedRestores.InvalidIndex() )
	{
		MDLCACHE_CRITICAL_SECTION();
		if ( pOwner->ShouldSavePhysics() && header.nObjects > 0 )
		{
			QueuedItem_t *pItem = m_QueuedRestores[iQueued]->FindItem( header.fieldName );
				
			if ( pItem )
			{
				int nObjects = MIN( header.nObjects, pItem->header.nObjects );
				if ( pItem->header.type == PIID_IPHYSICSOBJECT && nObjects == 1 )
				{
					RestorePhysicsObjectAndModel( pRestore, header, pItem, nObjects );
				}
				else
				{
					void **ppPhysObj = pItem->ppPhysObj;
						
					for ( int i = 0; i < nObjects; i++ )
					{
						pRestore->StartBlock();
						RestorePhysicsObject( pRestore, header, ppPhysObj + i );
						pRestore->EndBlock();
						if ( header.type == PIID_IPHYSICSMOTIONCONTROLLER )
						{
							void *pObj = ppPhysObj[i];
							IPhysicsMotionController *pController = (IPhysicsMotionController *)pObj;
							if ( pController )
							{
								// If the entity is the motion callback handler, then automatically set it
								// NOTE: This is usually the case
								IMotionEvent *pEvent = dynamic_cast<IMotionEvent *>(pOwner);
								if ( pEvent )
								{
									pController->SetEventHandler( pEvent );
								}
							}
						}
					}
				}
			}
		}
		else
			pOwner->CreateVPhysics();
	}
}
	
//---------------------------------

void CPhysSaveRestoreBlockHandler::RestorePhysicsObjectAndModel( IRestore *pRestore, const PhysObjectHeader_t &header, QueuedItem_t *pItem, int nObjects )
{
	if ( nObjects == 1 )
	{
		pRestore->StartBlock();
			
		CPhysCollide *pPhysCollide   = NULL;
		int 		  modelIndex 	 = -1;
		bool 		  fCustomCollide = false;
			
		if ( header.modelName != NULL_STRING )
		{
			modelIndex = GetModelIndexFromHeader(header);
			if ( modelIndex != -1 )
			{
				vcollide_t *pCollide = m_pEntityList->GetModelInfo()->GetVCollide( modelIndex );
				if ( pCollide )
				{
					if ( pCollide->solidCount > 0 && pCollide->solids && header.iCollide < pCollide->solidCount )
						pPhysCollide = pCollide->solids[header.iCollide];
				}
			}
		}
		else if ( header.bbox.mins != vec3_origin || header.bbox.maxs != vec3_origin )
		{
			pPhysCollide = PhysCreateBbox(m_pEntityList, header.bbox.mins, header.bbox.maxs );
			fCustomCollide = true;
		}
		else if ( header.sphere.radius != 0 )
		{
			// HACKHACK: Handle spheres here!!!
			if ( !(*pItem->ppPhysObj) )
			{
				RestorePhysicsObject( pRestore, header, pItem->ppPhysObj, NULL );
			}
			return;
		}
			
		if ( pPhysCollide )
		{
			if ( !(*pItem->ppPhysObj) )
			{
				RestorePhysicsObject( pRestore, header, pItem->ppPhysObj, pPhysCollide );
				if ( (*pItem->ppPhysObj) )
				{
					IPhysicsObject *pObject = (IPhysicsObject *)(*pItem->ppPhysObj);
					if ( !fCustomCollide )
					{
						AssociateModel( pObject, modelIndex );
					}
					else
					{
						AssociateModel( pObject, pPhysCollide );
					}
				}
				else
					DevMsg( "Failed to restore physics object\n" );
			}
			else
				DevMsg( "Physics object pointer unexpectedly non-null before restore. Should be creating physics object in CreatePhysics()?\n" );
		}
		else
			DevMsg( "Failed to reestablish collision model for object\n" );
				
		pRestore->EndBlock();
	}
	else
		DevMsg( "Don't know how to reconsitite models for physobj array \n" );
}
	
//---------------------------------
	
void CPhysSaveRestoreBlockHandler::PostRestore()
{
	if (m_pEntityList->PhysGetEnv())
		m_pEntityList->PhysGetEnv()->PostRestore();

	unsigned short i = m_QueuedRestores.FirstInorder();
	while ( i != m_QueuedRestores.InvalidIndex() )
	{
		delete m_QueuedRestores[i];
		i = m_QueuedRestores.NextInorder( i );
	}
		
	m_QueuedRestores.RemoveAll();
}
	
//---------------------------------
	
void CPhysSaveRestoreBlockHandler::QueueSave( IHandleEntity *pOwner, typedescription_t *pTypeDesc, void **ppPhysObj, PhysInterfaceId_t type )
{
	if ( !pOwner )
		return;

	bool fOnlyNotingExistence = !pOwner->ShouldSavePhysics();
		
	QueuedItem_t item;
		
	item.pEntityList = m_pEntityList;
	item.ppPhysObj		= ppPhysObj;
	item.header.hEntity = pOwner;
	item.header.type	= type;
	item.header.nObjects = ( !fOnlyNotingExistence ) ? pTypeDesc->fieldSize : 0;
	item.header.fieldName = m_pEntityList->AllocPooledString( pTypeDesc->fieldName );
																// A pooled string is used here because there is no way
																// right now to save a non-string_t string and have it 
																// compressed in the save symbol tables. Furthermore,
																// the field name would normally be in the string
																// pool anyway. (toml 12-10-02)
	item.header.modelName = NULL_STRING;
	memset( &item.header.bbox, 0, sizeof( item.header.bbox ) );
	item.header.sphere.radius = 0;
		
	if ( !fOnlyNotingExistence && type == PIID_IPHYSICSOBJECT )
	{
		// Don't doing the box thing for things like wheels on cars
		IPhysicsObject *pPhysObj = (IPhysicsObject *)(*ppPhysObj);

		if ( pPhysObj )
		{
			item.header.modelName = GetModelName( pPhysObj );
			item.header.iCollide = m_pEntityList->PhysGetCollision()->CollideIndex( pPhysObj->GetCollide() );
			if ( item.header.modelName == NULL_STRING )
			{
				BBox_t *pBBox = GetBBox( pPhysObj );
				if ( pBBox != NULL )
				{
					item.header.bbox = *pBBox;
				}
				else 
				{
					if ( pPhysObj && pPhysObj->GetSphereRadius() != 0 )
					{
						item.header.sphere.radius = pPhysObj->GetSphereRadius();
					}
					else
					{
						DevMsg( "Don't know how to save model for physics object (class \"%s\")\n", pOwner->GetClassname() );
					}
				}
			}
		}
	}

	m_QueuedSaves.Insert( item );
}

//---------------------------------
	
void CPhysSaveRestoreBlockHandler::QueueRestore( IHandleEntity *pOwner, typedescription_t *pTypeDesc, void **ppPhysObj, PhysInterfaceId_t type )
{
	CEntityRestoreSet *pEntitySet = NULL;
	unsigned short 	   iEntitySet = m_QueuedRestores.Find( pOwner );
		
	if ( iEntitySet != m_QueuedRestores.InvalidIndex() )
	{
		pEntitySet = m_QueuedRestores[iEntitySet];
	}
	else
	{
		pEntitySet = new CEntityRestoreSet;
		m_QueuedRestores.Insert( pOwner, pEntitySet );
	}
		
	pEntitySet->Add( pOwner, pTypeDesc, ppPhysObj, type );

	memset( ppPhysObj, 0, pTypeDesc->fieldSize * sizeof( void * ) );
}

//---------------------------------
	
void CPhysSaveRestoreBlockHandler::SavePhysicsObject( ISave *pSave, IHandleEntity *pOwner, void *pObject, PhysInterfaceId_t type )
{
	if (m_pEntityList->PhysGetEnv())
	{
		if ( !pObject )
			return;
		physsaveparams_t params = { pSave, pObject, type };
		m_pEntityList->PhysGetEnv()->Save( params );
	}
}
	
//---------------------------------
	
void CPhysSaveRestoreBlockHandler::RestorePhysicsObject( IRestore *pRestore, const PhysObjectHeader_t &header, void **ppObject, const CPhysCollide *pCollide )
{
	if (m_pEntityList->PhysGetEnv())
	{
		physrestoreparams_t params = { pRestore, ppObject, header.type, m_pEntityList->GetBaseEntityFromHandle(header.hEntity), STRING(header.modelName), pCollide, m_pEntityList->PhysGetEnv(), m_pEntityList->IPhysGameTrace()};
		m_pEntityList->PhysGetEnv()->Restore( params );
	}
}
//-----------------------------------------------------
// IEntityListener methods
// This object is only a listener during restore	
void CPhysSaveRestoreBlockHandler::OnEntityCreated( IHandleEntity *pEntity )
{
}

//---------------------------------

void CPhysSaveRestoreBlockHandler::OnEntityDeleted( IHandleEntity *pEntity )
{
	unsigned short iEntitySet = m_QueuedRestores.Find( pEntity );

	if ( iEntitySet != m_QueuedRestores.InvalidIndex() )
	{
		delete m_QueuedRestores[iEntitySet];
		m_QueuedRestores.RemoveAt( iEntitySet );
	}
}

//-----------------------------------------------------
// IPhysSaveRestoreManager methods
	
void CPhysSaveRestoreBlockHandler::NoteBBox( const Vector &mins, const Vector &maxs, CPhysCollide *pCollide )
{
	if ( pCollide && m_PhysCollideBBoxModels.Find( pCollide ) == m_PhysCollideBBoxModels.InvalidIndex() )
	{
		BBox_t box;
		box.mins = mins;
		box.maxs = maxs;
		m_PhysCollideBBoxModels.Insert( pCollide, box );
	}
}

//---------------------------------
	
void CPhysSaveRestoreBlockHandler::AssociateModel( IPhysicsObject *pObject, int modelIndex )
{
	Assert( m_PhysObjectModels.Find( pObject ) == m_PhysObjectModels.InvalidIndex() );
	m_PhysObjectModels.Insert( pObject, modelIndex );
}

//---------------------------------
	
void CPhysSaveRestoreBlockHandler::AssociateModel( IPhysicsObject *pObject, const CPhysCollide *pModel )
{
	Assert( m_PhysObjectCustomModels.Find( pObject ) == m_PhysObjectCustomModels.InvalidIndex() );
	m_PhysObjectCustomModels.Insert( pObject, pModel );
}

//---------------------------------
	
void CPhysSaveRestoreBlockHandler::ForgetModel( IPhysicsObject *pObject )
{
	if ( !m_PhysObjectModels.Remove( pObject ) )
		m_PhysObjectCustomModels.Remove( pObject );
}

//---------------------------------

void CPhysSaveRestoreBlockHandler::ForgetAllModels()
{
	m_PhysObjectModels.RemoveAll();
	m_PhysObjectCustomModels.RemoveAll();
	m_PhysCollideBBoxModels.RemoveAll();
}

//---------------------------------
	
string_t CPhysSaveRestoreBlockHandler::GetModelName( IPhysicsObject *pObject )
{
	int i = m_PhysObjectModels.Find( pObject );
	if ( i == m_PhysObjectModels.InvalidIndex() )
		return NULL_STRING;
	return m_pEntityList->AllocPooledString(m_pEntityList->GetModelInfo()->GetModelName(m_pEntityList->GetModelInfo()->GetModel( m_PhysObjectModels[i] ) ) );
}
	
//---------------------------------
	
BBox_t * CPhysSaveRestoreBlockHandler::GetBBox( IPhysicsObject *pObject )
{
	int i = m_PhysObjectCustomModels.Find( pObject );
	if ( i == m_PhysObjectCustomModels.InvalidIndex() )
		return NULL;
	i = m_PhysCollideBBoxModels.Find( m_PhysObjectCustomModels[i] );
	if ( i == m_PhysCollideBBoxModels.InvalidIndex() )
		return NULL;
	return &(m_PhysCollideBBoxModels[i]);
}

int CPhysSaveRestoreBlockHandler::CEntityRestoreSet::Add( IHandleEntity *pOwner, typedescription_t *pTypeDesc, void **ppPhysObj, PhysInterfaceId_t type )
{
	int i = AddToTail();
			
	Assert( ppPhysObj );
	Assert( *ppPhysObj == NULL ); // expected field to have been cleared
	Assert( pOwner );

	QueuedItem_t &item = Element( i );
	item.pEntityList		= pOwner->GetEntityList();
	item.ppPhysObj			= ppPhysObj;
	item.header.hEntity 	= pOwner;
	item.header.type		= type;
	item.header.nObjects 	= pTypeDesc->fieldSize;
	item.header.fieldName 	= pOwner->GetEntityList()->AllocPooledString(pTypeDesc->fieldName); 	// See comment in CPhysSaveRestoreBlockHandler::QueueSave()
			
	return i;
}
		
CPhysSaveRestoreBlockHandler::QueuedItem_t * CPhysSaveRestoreBlockHandler::CEntityRestoreSet::FindItem( string_t itemFieldName )
{
	// generally, the set is very small, usually one, so linear search is not too gruesome;
	for ( int i = 0; i < Count(); i++ )
	{
		string_t testName = Element(i).header.fieldName;
		Assert( ( testName == itemFieldName && strcmp( STRING( testName ), STRING( itemFieldName ) ) == 0 ) ||
				( testName != itemFieldName && strcmp( STRING( testName ), STRING( itemFieldName ) ) != 0 ) );
				
		if ( testName == itemFieldName )
			return &(Element(i));
	}
	return NULL;
}
	
//---------------------------------
	
bool CPhysSaveRestoreBlockHandler::SaveQueueFunc( const CPhysSaveRestoreBlockHandler::QueuedItem_t &left, const CPhysSaveRestoreBlockHandler::QueuedItem_t &right )
{
	if ( left.header.type == right.header.type )
		return (left.pEntityList->GetBaseEntityFromHandle(left.header.hEntity)->entindex() > right.pEntityList->GetBaseEntityFromHandle(right.header.hEntity)->entindex() );

	return ( left.header.type > right.header.type );
}


//-----------------------------------------------------
// IEntityListener methods
// This object is only a listener during restore	
void CServerPhysSaveRestoreBlockHandler::OnEntityCreated(IServerEntity* pEntity)
{
	CPhysSaveRestoreBlockHandler::OnEntityCreated(pEntity);
}

//---------------------------------

void CServerPhysSaveRestoreBlockHandler::OnEntityDeleted(IServerEntity* pEntity)
{
	CPhysSaveRestoreBlockHandler::OnEntityDeleted(pEntity);
}

int CServerPhysSaveRestoreBlockHandler::GetModelIndexFromHeader(const PhysObjectHeader_t& header)
{
	int modelIndex = -1;
	IHandleEntity* pGlobalEntity = m_pEntityList->GetBaseEntityFromHandle(header.hEntity);
	if (NULL_STRING != pGlobalEntity->GetEngineObject()->GetGlobalname())
	{
		modelIndex = pGlobalEntity->GetEngineObject()->GetModelIndex();
	}
	else
	{
		modelIndex = m_pEntityList->GetModelInfo()->GetModelIndex(STRING(header.modelName));
		pGlobalEntity = NULL;
	}
	return modelIndex;
}

int CClientPhysSaveRestoreBlockHandler::GetModelIndexFromHeader(const PhysObjectHeader_t& header)
{
	int modelIndex = m_pEntityList->GetModelInfo()->GetModelIndex(STRING(header.modelName));
	return modelIndex;
}





//=============================================================================
