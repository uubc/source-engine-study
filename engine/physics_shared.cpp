//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game & Client shared functions moved from physics.cpp
//
//=============================================================================//
//#include "cbase.h"
#include "ragdoll_shared.h"
#include "edict.h"
#include "PlayerState.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
//IPhysics			*physics = NULL;
//IPhysicsObject		*g_PhysWorldObject = NULL;
//IPhysicsCollision	*physcollision = NULL;
//IPhysicsEnvironment	*physenv = NULL;
//#ifdef PORTAL
//IPhysicsEnvironment	*physenv_main = NULL;
//#endif
//IPhysicsSurfaceProps *physprops = NULL;
// UNDONE: This hash holds both entity & IPhysicsObject pointer pairs
// UNDONE: Split into separate hashes?
//IPhysicsObjectPairHash *g_EntityCollisionHash = NULL;

const char *SURFACEPROP_MANIFEST_FILE = "scripts/surfaceproperties_manifest.txt";

const objectparams_t g_PhysDefaultObjectParams =
{
	NULL,
	1.0, //mass
	1.0, // inertia
	0.1f, // damping
	0.1f, // rotdamping
	0.05f, // rotIntertiaLimit
	"DEFAULT",
	NULL,// game data
	0.f, // volume (leave 0 if you don't have one or call physcollision->CollideVolume() to compute it)
	1.0f, // drag coefficient
	true,// enable collisions?
};

// Prints warnings if any entity think functions take longer than this many milliseconds
#ifdef _DEBUG
#define DEF_THINK_LIMIT "20"
#else
#define DEF_THINK_LIMIT "10"
#endif

ConVar think_limit("think_limit", DEF_THINK_LIMIT, FCVAR_REPLICATED, "Maximum think time in milliseconds, warning is printed if this is exceeded.");
ConVar sv_portal_collision_sim_bounds_x("sv_portal_collision_sim_bounds_x", "200", FCVAR_REPLICATED, "Size of box used to grab collision geometry around placed portals. These should be at the default size or larger only!");
ConVar sv_portal_collision_sim_bounds_y("sv_portal_collision_sim_bounds_y", "200", FCVAR_REPLICATED, "Size of box used to grab collision geometry around placed portals. These should be at the default size or larger only!");
ConVar sv_portal_collision_sim_bounds_z("sv_portal_collision_sim_bounds_z", "252", FCVAR_REPLICATED, "Size of box used to grab collision geometry around placed portals. These should be at the default size or larger only!");
ConVar sv_portal_trace_vs_world("sv_portal_trace_vs_world", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Use traces against portal environment world geometry");
ConVar sv_portal_trace_vs_displacements("sv_portal_trace_vs_displacements", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Use traces against portal environment displacement geometry");
ConVar sv_portal_trace_vs_holywall("sv_portal_trace_vs_holywall", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Use traces against portal environment carved wall");
ConVar sv_portal_trace_vs_staticprops("sv_portal_trace_vs_staticprops", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Use traces against portal environment static prop geometry");
ConVar sv_use_transformed_collideables("sv_use_transformed_collideables", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Disables traces against remote portal moving entities using transforms to bring them into local space.");

ConVar	sv_gravity("sv_gravity", "600", FCVAR_NOTIFY | FCVAR_REPLICATED, "World gravity.");
ConVar	sv_maxvelocity("sv_maxvelocity", "3500", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Maximum speed any ballistically moving object is allowed to attain per axis.");
ConVar	sv_friction("sv_friction", "4", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "World friction.");
ConVar	sv_stopspeed("sv_stopspeed", "100", FCVAR_NOTIFY | FCVAR_REPLICATED , "Minimum stopping speed when on ground.");//FCVAR_DEVELOPMENTONLY
ConVar	sv_bounce("sv_bounce", "0", FCVAR_NOTIFY | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Bounce multiplier for when physically simulated objects collide with other objects.");
ConVar	npc_vphysics("npc_vphysics", "0");

float GetCurrentGravity(void)
{
#if defined( TF_CLIENT_DLL ) || defined( TF_DLL )
	if (TFGameRules())
	{
		return (sv_gravity.GetFloat() * TFGameRules()->GetGravityMultiplier());
	}
#endif 

	return sv_gravity.GetFloat();
}

//-----------------------------------------------------------------------------
// Returns the actual gravity
//-----------------------------------------------------------------------------
float GetActualGravity(IEngineObject* pEnt)
{
	float ent_gravity = pEnt->GetGravity();
	if (ent_gravity == 0.0f)
	{
		ent_gravity = 1.0f;
	}

	return ent_gravity * GetCurrentGravity();
}

// solid_t parsing
class CSolidSetDefaults : public IVPhysicsKeyHandler
{
public:
	virtual void ParseKeyValue(void* pData, const char* pKey, const char* pValue);
	virtual void SetDefaults(void* pData);

	unsigned int GetContentsMask() { return m_contentsMask; }

private:
	unsigned int m_contentsMask;
};


void CSolidSetDefaults::ParseKeyValue( void *pData, const char *pKey, const char *pValue )
{
	if ( !Q_stricmp( pKey, "contents" ) )
	{
		m_contentsMask = atoi( pValue );
	}
}

void CSolidSetDefaults::SetDefaults( void *pData )
{
	solid_t *pSolid = (solid_t *)pData;
	pSolid->params = g_PhysDefaultObjectParams;
	m_contentsMask = CONTENTS_SOLID;
}

CSolidSetDefaults g_SolidSetup;
IVPhysicsKeyHandler* g_pSolidSetup = &g_SolidSetup;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &mins - 
//			&maxs - 
// Output : CPhysCollide
//-----------------------------------------------------------------------------
CPhysCollide *PhysCreateBbox(IEntityList* pEntityList, const Vector &minsIn, const Vector &maxsIn )
{
	// UNDONE: Track down why this causes errors for the player controller and adjust/enable
	//float radius = 0.5 - DIST_EPSILON;
	Vector mins = minsIn;// + Vector(radius, radius, radius);
	Vector maxs = maxsIn;// - Vector(radius, radius, radius);

	// VPHYSICS caches/cleans up these
	CPhysCollide *pResult = pEntityList->PhysGetCollision()->BBoxToCollide(mins, maxs);

	pEntityList->PhysSaveRestoreBlockHandler()->NoteBBox(mins, maxs, pResult);
	
	return pResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			&mins - 
//			&maxs - 
//			&origin - 
//			isStatic - 
// Output : static IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateBox( IHandleEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, bool isStatic )
{
	int modelIndex = pEntity->GetEngineObject()->GetModelIndex();
	const char *pSurfaceProps = "flesh";
	solid_t solid;
	PhysGetDefaultAABBSolid( solid );
	Vector dims = maxs - mins;
	solid.params.volume = dims.x * dims.y * dims.z;

	if ( modelIndex )
	{
		const model_t *model = pEntity->GetEntityList()->GetModelInfo()->GetModel(modelIndex);
		if ( model )
		{
			IStudioHdr* studioHdr = pEntity->GetEntityList()->GetModelInfo()->GetStudiomodel( model );
			if (!studioHdr) {
				studioHdr = pEntity->GetEntityList()->GetModelInfo()->GetStudiomodel(model);
			}
			if (studioHdr && studioHdr->IsValid())
			{
				pSurfaceProps = studioHdr->Studio_GetDefaultSurfaceProps(  );
			}
		}
	}
	Q_strncpy( solid.surfaceprop, pSurfaceProps, sizeof( solid.surfaceprop ) );

	CPhysCollide *pCollide = PhysCreateBbox(pEntity->GetEntityList(), mins, maxs );
	if ( !pCollide )
		return NULL;
	
	return PhysModelCreateCustom( pEntity, pCollide, origin, vec3_angle, STRING(pEntity->GetEngineObject()->GetModelName()), isStatic, &solid );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			&mins - 
//			&maxs - 
//			&origin - 
//			isStatic - 
// Output : static IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateOBB( IHandleEntity *pEntity, const Vector &mins, const Vector &maxs, const Vector &origin, const QAngle &angle, bool isStatic )
{
	int modelIndex = pEntity->GetEngineObject()->GetModelIndex();
	const char *pSurfaceProps = "flesh";
	solid_t solid;
	PhysGetDefaultAABBSolid( solid );
	Vector dims = maxs - mins;
	solid.params.volume = dims.x * dims.y * dims.z;

	if ( modelIndex )
	{
		const model_t *model = pEntity->GetEntityList()->GetModelInfo()->GetModel( modelIndex );
		if ( model )
		{
			IStudioHdr* studioHdr = pEntity->GetEntityList()->GetModelInfo()->GetStudiomodel( model );
			if (studioHdr->IsValid()) 
			{
				pSurfaceProps = studioHdr->Studio_GetDefaultSurfaceProps(  );
			}
		}
	}
	Q_strncpy( solid.surfaceprop, pSurfaceProps, sizeof( solid.surfaceprop ) );

	CPhysCollide *pCollide = PhysCreateBbox(pEntity->GetEntityList(), mins, maxs );
	if ( !pCollide )
		return NULL;
	
	return PhysModelCreateCustom( pEntity, pCollide, origin, angle, STRING(pEntity->GetEngineObject()->GetModelName()), isStatic, &solid );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &solid - 
//			*pEntity - 
//			modelIndex - 
//			solidIndex - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhysModelParseSolidByIndex( solid_t &solid, IHandleEntity *pEntity, int modelIndex, int solidIndex )
{
	vcollide_t *pCollide = pEntity->GetEntityList()->GetModelInfo()->GetVCollide( modelIndex );
	if ( !pCollide )
		return false;

	bool parsed = false;

	memset( &solid, 0, sizeof(solid) );
	solid.params = g_PhysDefaultObjectParams;

	IVPhysicsKeyParser *pParse = pEntity->GetEntityList()->PhysGetCollision()->VPhysicsKeyParserCreate( pCollide->pKeyValues );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			solid_t tmpSolid;
			memset( &tmpSolid, 0, sizeof(tmpSolid) );
			tmpSolid.params = g_PhysDefaultObjectParams;

			pParse->ParseSolid( &tmpSolid, &g_SolidSetup );

			if ( solidIndex < 0 || tmpSolid.index == solidIndex )
			{
				parsed = true;
				solid = tmpSolid;
				// just to be sure we aren't ever getting a non-zero solid by accident
				Assert( solidIndex >= 0 || solid.index == 0 );
				break;
			}
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	pEntity->GetEntityList()->PhysGetCollision()->VPhysicsKeyParserDestroy( pParse );

	// collisions are off by default
	solid.params.enableCollisions = true;

	solid.params.pGameData = static_cast<void *>(pEntity);
	solid.params.pName = STRING(pEntity->GetEngineObject()->GetModelName());
	return parsed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &solid - 
//			*pEntity - 
//			modelIndex - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhysModelParseSolid( solid_t &solid, IHandleEntity *pEntity, int modelIndex )
{
	return PhysModelParseSolidByIndex( solid, pEntity, modelIndex, -1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &solid - 
//			*pEntity - 
//			*pCollide - 
//			solidIndex - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PhysModelParseSolidByIndex( solid_t &solid, IHandleEntity *pEntity, vcollide_t *pCollide, int solidIndex )
{
	bool parsed = false;

	memset( &solid, 0, sizeof(solid) );
	solid.params = g_PhysDefaultObjectParams;

	IVPhysicsKeyParser *pParse = pEntity->GetEntityList()->PhysGetCollision()->VPhysicsKeyParserCreate( pCollide->pKeyValues );
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();
		if ( !strcmpi( pBlock, "solid" ) )
		{
			solid_t tmpSolid;
			memset( &tmpSolid, 0, sizeof(tmpSolid) );
			tmpSolid.params = g_PhysDefaultObjectParams;

			pParse->ParseSolid( &tmpSolid, &g_SolidSetup );

			if ( solidIndex < 0 || tmpSolid.index == solidIndex )
			{
				parsed = true;
				solid = tmpSolid;
				// just to be sure we aren't ever getting a non-zero solid by accident
				Assert( solidIndex >= 0 || solid.index == 0 );
				break;
			}
		}
		else
		{
			pParse->SkipBlock();
		}
	}
	pEntity->GetEntityList()->PhysGetCollision()->VPhysicsKeyParserDestroy( pParse );

	// collisions are off by default
	solid.params.enableCollisions = true;

	solid.params.pGameData = static_cast<void *>(pEntity);
	solid.params.pName = STRING(pEntity->GetEngineObject()->GetModelName());
	return parsed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			modelIndex - 
//			&origin - 
//			&angles - 
//			*pSolid - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreate( IHandleEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles, solid_t *pSolid )
{
	if ( !pEntity->GetEntityList()->PhysGetEnv())
		return NULL;

	vcollide_t *pCollide = pEntity->GetEntityList()->GetModelInfo()->GetVCollide( modelIndex );
	if ( !pCollide || !pCollide->solidCount )
		return NULL;
	
	solid_t tmpSolid;
	if ( !pSolid )
	{
		pSolid = &tmpSolid;
		if ( !PhysModelParseSolidByIndex( tmpSolid, pEntity, pCollide, -1 ) )
			return NULL;
	}

	int surfaceProp = -1;
	if ( pSolid->surfaceprop[0] )
	{
		surfaceProp = pEntity->GetEntityList()->PhysGetProps()->GetSurfaceIndex( pSolid->surfaceprop );
	}
	IPhysicsObject *pObject = pEntity->GetEntityList()->PhysGetEnv()->CreatePolyObject( pCollide->solids[pSolid->index], surfaceProp, origin, angles, &pSolid->params );
	//PhysCheckAdd( pObject, STRING(pEntity->m_iClassname) );

	if ( pObject )
	{
		if (pEntity->GetEntityList()->GetModelInfo()->GetModelType(pEntity->GetEntityList()->GetModelInfo()->GetModel(modelIndex)) == mod_brush )
		{
			unsigned int contents = pEntity->GetEntityList()->GetModelInfo()->GetModelContents( modelIndex );
			Assert(contents!=0);
			// HACKHACK: contents is used to filter collisions
			// HACKHACK: So keep solid on for water brushes since they should pass collision rules (as triggers)
			if ( contents & MASK_WATER )
			{
				contents |= CONTENTS_SOLID;
			}
			if ( contents != pObject->GetContents() && contents != 0 )
			{
				pObject->SetContents( contents );
				pObject->RecheckCollisionFilter();
			}
		}

		pEntity->GetEntityList()->PhysSaveRestoreBlockHandler()->AssociateModel(pObject, modelIndex);
	}

	return pObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			modelIndex - 
//			&origin - 
//			&angles - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateUnmoveable( IHandleEntity *pEntity, int modelIndex, const Vector &origin, const QAngle &angles )
{
	if ( !pEntity->GetEntityList()->PhysGetEnv())
		return NULL;

	vcollide_t *pCollide = pEntity->GetEntityList()->GetModelInfo()->GetVCollide( modelIndex );
	if ( !pCollide || !pCollide->solidCount )
		return NULL;

	solid_t solid;

	if ( !PhysModelParseSolidByIndex( solid, pEntity, pCollide, -1 ) )
		return NULL;

	// collisions are off by default
	solid.params.enableCollisions = true;
	//solid.params.mass = 1.0;
	int surfaceProp = -1;
	if ( solid.surfaceprop[0] )
	{
		surfaceProp = pEntity->GetEntityList()->PhysGetProps()->GetSurfaceIndex( solid.surfaceprop );
	}
	solid.params.pGameData = static_cast<void *>(pEntity);
	solid.params.pName = STRING(pEntity->GetEngineObject()->GetModelName());
	IPhysicsObject *pObject = pEntity->GetEntityList()->PhysGetEnv()->CreatePolyObjectStatic( pCollide->solids[0], surfaceProp, origin, angles, &solid.params );

	//PhysCheckAdd( pObject, STRING(pEntity->m_iClassname) );
	if ( pObject )
	{
		if (pEntity->GetEntityList()->GetModelInfo()->GetModelType(pEntity->GetEntityList()->GetModelInfo()->GetModel(modelIndex)) == mod_brush )
		{
			unsigned int contents = pEntity->GetEntityList()->GetModelInfo()->GetModelContents( modelIndex );
			Assert(contents!=0);
			if ( contents != pObject->GetContents() && contents != 0 )
			{
				pObject->SetContents( contents );
				pObject->RecheckCollisionFilter();
			}
		}
		pEntity->GetEntityList()->PhysSaveRestoreBlockHandler()->AssociateModel(pObject, modelIndex);
	}

	return pObject;
}


//-----------------------------------------------------------------------------
// Purpose: Create a vphysics object based on an existing collision model
// Input  : *pEntity - 
//			*pModel - 
//			&origin - 
//			&angles - 
//			*pName - 
//			isStatic - 
//			*pSolid - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysModelCreateCustom( IHandleEntity *pEntity, const CPhysCollide *pModel, const Vector &origin, const QAngle &angles, const char *pName, bool isStatic, solid_t *pSolid )
{
	if ( !pEntity->GetEntityList()->PhysGetEnv())
		return NULL;

	solid_t tmpSolid;
	if ( !pSolid )
	{
		PhysGetDefaultAABBSolid( tmpSolid );
		pSolid = &tmpSolid;
	}
	int surfaceProp = pEntity->GetEntityList()->PhysGetProps()->GetSurfaceIndex( pSolid->surfaceprop );
	pSolid->params.pGameData = static_cast<void *>(pEntity);
	pSolid->params.pName = pName;
	IPhysicsObject *pObject = NULL;
	if ( isStatic )
	{
		pObject = pEntity->GetEntityList()->PhysGetEnv()->CreatePolyObjectStatic( pModel, surfaceProp, origin, angles, &pSolid->params );
	}
	else
	{
		pObject = pEntity->GetEntityList()->PhysGetEnv()->CreatePolyObject( pModel, surfaceProp, origin, angles, &pSolid->params );
	}

	if ( pObject )
		pEntity->GetEntityList()->PhysSaveRestoreBlockHandler()->AssociateModel(pObject, pModel);

	return pObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//			radius - 
//			&origin - 
//			&solid - 
// Output : IPhysicsObject
//-----------------------------------------------------------------------------
IPhysicsObject *PhysSphereCreate( IHandleEntity *pEntity, float radius, const Vector &origin, solid_t &solid )
{
	if ( !pEntity->GetEntityList()->PhysGetEnv())
		return NULL;

	int surfaceProp = -1;
	if ( solid.surfaceprop[0] )
	{
		surfaceProp = pEntity->GetEntityList()->PhysGetProps()->GetSurfaceIndex( solid.surfaceprop );
	}

	solid.params.pGameData = static_cast<void *>(pEntity);
	IPhysicsObject *pObject = pEntity->GetEntityList()->PhysGetEnv()->CreateSphereObject( radius, surfaceProp, origin, vec3_angle, &solid.params, false );

	return pObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PhysGetDefaultAABBSolid( solid_t &solid )
{
	solid.params = g_PhysDefaultObjectParams;
	solid.params.mass = 85.0f;
	solid.params.inertia = 1e24f;
	Q_strncpy( solid.surfaceprop, "default", sizeof( solid.surfaceprop ) );
}

//-----------------------------------------------------------------------------
// Purpose: Destroy a physics object
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void PhysDestroyObject(IEntityList* pEntityList, IPhysicsObject *pObject, IHandleEntity *pEntity )
{
	pEntityList->PhysSaveRestoreBlockHandler()->ForgetModel(pObject);

	
	if ( pObject )
		pObject->SetGameData( NULL );

	pEntityList->PhysGetEntityCollisionHash()->RemoveAllPairsForObject( pObject );
	if ( pEntity && pEntity->GetEngineObject()->IsMarkedForDeletion() )
	{
		pEntityList->PhysGetEntityCollisionHash()->RemoveAllPairsForObject( pEntity );
	}

	if (pEntityList->PhysGetEnv())
	{
		pEntityList->PhysGetEnv()->DestroyObject( pObject );
	}
}

void AddSurfacepropFile( const char *pFileName, IPhysicsSurfaceProps *pProps, IFileSystem *pFileSystem )
{
	// Load file into memory
	FileHandle_t file = pFileSystem->Open( pFileName, "rb", "GAME" );

	if ( file )
	{
		int len = pFileSystem->Size( file );

		// read the file
		int nBufSize = len+1;
		if ( IsXbox() )
		{
			nBufSize = AlignValue( nBufSize , 512 );
		}
		char *buffer = (char *)stackalloc( nBufSize );
		pFileSystem->ReadEx( buffer, nBufSize, len, file );
		pFileSystem->Close( file );
		buffer[len] = 0;
		pProps->ParseSurfaceData( pFileName, buffer );
		// buffer is on the stack, no need to free
	}
	else
	{
		Error( "Unable to load surface prop file '%s' (referenced by manifest file '%s')\n", pFileName, SURFACEPROP_MANIFEST_FILE );
	}
}

void PhysParseSurfaceData( IPhysicsSurfaceProps *pProps, IFileSystem *pFileSystem )
{
	KeyValues *manifest = new KeyValues( SURFACEPROP_MANIFEST_FILE );
	if ( manifest->LoadFromFile( pFileSystem, SURFACEPROP_MANIFEST_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			if ( !Q_stricmp( sub->GetName(), "file" ) )
			{
				// Add
				AddSurfacepropFile( sub->GetString(), pProps, pFileSystem );
				continue;
			}

			Warning( "surfaceprops::Init:  Manifest '%s' with bogus file type '%s', expecting 'file'\n", 
				SURFACEPROP_MANIFEST_FILE, sub->GetName() );
		}
	}
	else
	{
		Error( "Unable to load manifest file '%s'\n", SURFACEPROP_MANIFEST_FILE );
	}

	manifest->deleteThis();
}

void PhysCreateVirtualTerrain( IHandleEntity *pWorld, const objectparams_t &defaultParams )
{
	if ( !pWorld->GetEntityList()->PhysGetEnv())
		return;

	char nameBuf[1024];
	for ( int i = 0; i < MAX_MAP_DISPINFO; i++ )
	{
		CPhysCollide *pCollide = pWorld->GetEntityList()->GetModelInfo()->GetCollideForVirtualTerrain( i );
		if ( pCollide )
		{
			solid_t solid;
			solid.params = defaultParams;
			solid.params.enableCollisions = true;
			solid.params.pGameData = static_cast<void *>(pWorld);
			Q_snprintf(nameBuf, sizeof(nameBuf), "vdisp_%04d", i );
			solid.params.pName = nameBuf;
			int surfaceData = pWorld->GetEntityList()->PhysGetProps()->GetSurfaceIndex( "default" );
			// create this as part of the world
			IPhysicsObject *pObject = pWorld->GetEntityList()->PhysGetEnv()->CreatePolyObjectStatic( pCollide, surfaceData, vec3_origin, vec3_angle, &solid.params );
			pObject->SetCallbackFlags( pObject->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
		}
	}
}

IPhysicsObject *PhysCreateWorld_Shared( IHandleEntity *pWorld, vcollide_t *pWorldCollide, const objectparams_t &defaultParams )
{
	solid_t solid;
	fluid_t fluid;

	if ( !pWorld->GetEntityList()->PhysGetEnv())
		return NULL;

	int surfaceData = pWorld->GetEntityList()->PhysGetProps()->GetSurfaceIndex( "default" );

	objectparams_t params = defaultParams;
	params.pGameData = static_cast<void *>(pWorld);
	params.pName = "world";

	IPhysicsObject *pWorldPhysics = pWorld->GetEntityList()->PhysGetEnv()->CreatePolyObjectStatic(
		pWorldCollide->solids[0], surfaceData, vec3_origin, vec3_angle, &params );

	// hint - saves vphysics some work
	pWorldPhysics->SetCallbackFlags( pWorldPhysics->GetCallbackFlags() | CALLBACK_NEVER_DELETED );

	//PhysCheckAdd( world, "World" );
	// walk the world keys in case there are some fluid volumes to create
	IVPhysicsKeyParser *pParse = pWorld->GetEntityList()->PhysGetCollision()->VPhysicsKeyParserCreate( pWorldCollide->pKeyValues );

	bool bCreateVirtualTerrain = false;
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();

		if ( !strcmpi( pBlock, "solid" ) || !strcmpi( pBlock, "staticsolid" ) )
		{
			solid.params = defaultParams;
			pParse->ParseSolid( &solid, &g_SolidSetup );
			solid.params.enableCollisions = true;
			solid.params.pGameData = static_cast<void *>(pWorld);
			solid.params.pName = "world";
			int surfaceData = pWorld->GetEntityList()->PhysGetProps()->GetSurfaceIndex( "default" );

			// already created world above
			if ( solid.index == 0 )
				continue;

			if ( !pWorldCollide->solids[solid.index] )
			{
				// this implies that the collision model is a mopp and the physics DLL doesn't support that.
				bCreateVirtualTerrain = true;
				continue;
			}
			// create this as part of the world
			IPhysicsObject *pObject = pWorld->GetEntityList()->PhysGetEnv()->CreatePolyObjectStatic( pWorldCollide->solids[solid.index],
				surfaceData, vec3_origin, vec3_angle, &solid.params );

			// invalid collision model or can't create, ignore
			if (!pObject)
				continue;

			pObject->SetCallbackFlags( pObject->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
			Assert( g_SolidSetup.GetContentsMask() != 0 );
			pObject->SetContents( g_SolidSetup.GetContentsMask() );

			if ( !pWorldPhysics )
			{
				pWorldPhysics = pObject;
			}
		}
		else if ( !strcmpi( pBlock, "fluid" ) )
		{
			pParse->ParseFluid( &fluid, NULL );

			// create a fluid for floating
			if ( fluid.index > 0 )
			{
				solid.params = defaultParams;	// copy world's params
				solid.params.enableCollisions = true;
				solid.params.pName = "fluid";
				solid.params.pGameData = static_cast<void *>(pWorld);
				fluid.params.pGameData = static_cast<void *>(pWorld);
				int surfaceData = pWorld->GetEntityList()->PhysGetProps()->GetSurfaceIndex( fluid.surfaceprop );
				// create this as part of the world
				IPhysicsObject *pWater = pWorld->GetEntityList()->PhysGetEnv()->CreatePolyObjectStatic( pWorldCollide->solids[fluid.index],
					surfaceData, vec3_origin, vec3_angle, &solid.params );

				pWater->SetCallbackFlags( pWater->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
				pWorld->GetEntityList()->PhysGetEnv()->CreateFluidController( pWater, &fluid.params );
			}
		}
		else if ( !strcmpi( pBlock, "materialtable" ) )
		{
			int surfaceTable[128];
			memset( surfaceTable, 0, sizeof(surfaceTable) );

			pParse->ParseSurfaceTable( surfaceTable, NULL );
			pWorld->GetEntityList()->PhysGetProps()->SetWorldMaterialIndexTable( surfaceTable, 128 );
		}
		else if ( !strcmpi(pBlock, "virtualterrain" ) )
		{
			bCreateVirtualTerrain = true;
			pParse->SkipBlock();
		}
		else
		{
			// unknown chunk???
			pParse->SkipBlock();
		}
	}
	pWorld->GetEntityList()->PhysGetCollision()->VPhysicsKeyParserDestroy( pParse );

	if ( bCreateVirtualTerrain && pWorld->GetEntityList()->PhysGetCollision()->SupportsVirtualMesh() )
	{
		PhysCreateVirtualTerrain( pWorld, defaultParams );
	}
	return pWorldPhysics;
}

//-----------------------------------------------------------------------------
// Purpose: Game ray-traces in vphysics.
//-----------------------------------------------------------------------------
void CPhysicsGameTrace::VehicleTraceRay( const Ray_t &ray, void *pVehicle, trace_t *pTrace )
{
	IHandleEntity *pBaseEntity = static_cast<IHandleEntity*>( pVehicle );
	UTIL_TraceRay(pBaseEntity->GetEntityList(), ray, MASK_SOLID, pBaseEntity, COLLISION_GROUP_NONE, pTrace );
}

//-----------------------------------------------------------------------------
// Purpose: Game ray-traces in vphysics.
//-----------------------------------------------------------------------------
void CPhysicsGameTrace::VehicleTraceRayWithWater( const Ray_t &ray, void *pVehicle, trace_t *pTrace )
{
	IHandleEntity *pBaseEntity = static_cast<IHandleEntity*>( pVehicle );
	UTIL_TraceRay(pBaseEntity->GetEntityList(), ray, MASK_SOLID|MASK_WATER, pBaseEntity, COLLISION_GROUP_NONE, pTrace );
}

//-----------------------------------------------------------------------------
// Purpose: Test to see if a vehicle point is in water.
//-----------------------------------------------------------------------------
bool CPhysicsGameTrace::VehiclePointInWater( const Vector &vecPoint, void* pVehicle)
{
	IHandleEntity* pBaseEntity = static_cast<IHandleEntity*>(pVehicle);
	return ( ( UTIL_PointContents(pBaseEntity->GetEntityList(), vecPoint ) & MASK_WATER ) != 0 );
}




bool PhysHasContactWithOtherInDirection( IPhysicsObject *pPhysics, const Vector &dir )
{
	bool hit = false;
	void *pGameData = pPhysics->GetGameData();
	IPhysicsFrictionSnapshot *pSnapshot = pPhysics->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject( 1 );
		if ( pOther->GetGameData() != pGameData )
		{
			Vector normal;
			pSnapshot->GetSurfaceNormal( normal );
			if ( DotProduct(normal,dir) > 0 )
			{
				hit = true;
				break;
			}
		}
		pSnapshot->NextFrictionData();
	}
	pPhysics->DestroyFrictionSnapshot( pSnapshot );

	return hit;
}


void PhysForceClearVelocity( IPhysicsObject *pPhys )
{
	IPhysicsFrictionSnapshot *pSnapshot = pPhys->CreateFrictionSnapshot();
	// clear the velocity of the rigid body
	Vector vel;
	AngularImpulse angVel;
	vel.Init();
	angVel.Init();
	pPhys->SetVelocity( &vel, &angVel );
	// now clear the "strain" stored in the contact points
	while ( pSnapshot->IsValid() )
	{
		pSnapshot->ClearFrictionForce();
		pSnapshot->RecomputeFriction();
		pSnapshot->NextFrictionData();
	}
	pPhys->DestroyFrictionSnapshot( pSnapshot );
}


void PhysFrictionEffect(IEntityList* pEntityList, Vector &vecPos, Vector vecVel, float energy, int surfaceProps, int surfacePropsHit )
{
	Vector invVecVel = -vecVel;
	VectorNormalize( invVecVel );

	surfacedata_t *psurf = pEntityList->PhysGetProps()->GetSurfaceData( surfaceProps );
	surfacedata_t *phit = pEntityList->PhysGetProps()->GetSurfaceData( surfacePropsHit );

	switch ( phit->game.material )
	{
	case CHAR_TEX_DIRT:
		
		if ( energy < MASS10_SPEED2ENERGY(15) )
			break;
		
		pEntityList->GetEffects()->Dust(vecPos, invVecVel, 1, 16);
		break;

	case CHAR_TEX_CONCRETE:
		
		if ( energy < MASS10_SPEED2ENERGY(28) )
			break;
		
		pEntityList->GetEffects()->Dust( vecPos, invVecVel, 1, 16 );
		break;
	}
	
	//Metal sparks
	if ( energy > MASS10_SPEED2ENERGY(50) )
	{
		// make sparks for metal/concrete scrapes with enough energy
		if ( psurf->game.material == CHAR_TEX_METAL || psurf->game.material == CHAR_TEX_GRATE )
		{	
			switch ( phit->game.material )
			{
			case CHAR_TEX_CONCRETE:
			case CHAR_TEX_METAL:

				pEntityList->GetEffects()->MetalSparks( vecPos, invVecVel );
				break;									
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Precaches a surfaceproperties string name if it's set.
// Input  : idx - 
// Output : static void
//-----------------------------------------------------------------------------
static HSOUNDSCRIPTHANDLE PrecachePhysicsSoundByStringIndex(IEntityList* pEntityList, int idx )
{
	// Only precache if a value was set in the script file...
	if ( idx != 0 )
	{
		return g_pSoundEmitterSystem->PrecacheScriptSound(pEntityList->PhysGetProps()->GetString( idx ) );
	}

	return SOUNDEMITTER_INVALID_HANDLE;
}

//-----------------------------------------------------------------------------
// Purpose: Iterates all surfacedata sounds and precaches them
// Output : static void
//-----------------------------------------------------------------------------
void PrecachePhysicsSounds(IEntityList* pEntityList)
{
	// precache the surface prop sounds
	for ( int i = 0; i < pEntityList->PhysGetProps()->SurfacePropCount(); i++ )
	{
		surfacedata_t *pprop = pEntityList->PhysGetProps()->GetSurfaceData( i );
		Assert( pprop );

		pprop->soundhandles.stepleft = PrecachePhysicsSoundByStringIndex(pEntityList, pprop->sounds.stepleft );
		pprop->soundhandles.stepright = PrecachePhysicsSoundByStringIndex(pEntityList, pprop->sounds.stepright );
		pprop->soundhandles.impactSoft = PrecachePhysicsSoundByStringIndex(pEntityList, pprop->sounds.impactSoft );
		pprop->soundhandles.impactHard = PrecachePhysicsSoundByStringIndex(pEntityList, pprop->sounds.impactHard );
		pprop->soundhandles.scrapeSmooth = PrecachePhysicsSoundByStringIndex(pEntityList, pprop->sounds.scrapeSmooth );
		pprop->soundhandles.scrapeRough = PrecachePhysicsSoundByStringIndex(pEntityList, pprop->sounds.scrapeRough );
		pprop->soundhandles.bulletImpact = PrecachePhysicsSoundByStringIndex(pEntityList, pprop->sounds.bulletImpact );
		pprop->soundhandles.rolling = PrecachePhysicsSoundByStringIndex(pEntityList, pprop->sounds.rolling );
		pprop->soundhandles.breakSound = PrecachePhysicsSoundByStringIndex(pEntityList, pprop->sounds.breakSound );
		pprop->soundhandles.strainSound = PrecachePhysicsSoundByStringIndex(pEntityList, pprop->sounds.strainSound );
	}
}

extern CGlobalVars g_ServerGlobalVariables;
extern IEngineTrace* g_pEngineTraceServer;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPhysicsPushedEntities::CPhysicsPushedEntities(void) : m_rgPusher(8, 8), m_rgMoved(32, 32)
{
	m_flMoveTime = -1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Store off entity and copy original origin to temporary array
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::AddEntity(IServerEntity* ent)
{
	int i = m_rgMoved.AddToTail();
	m_rgMoved[i].m_pEntity = ent;
	m_rgMoved[i].m_vecStartAbsOrigin = ent->GetEngineObject()->GetAbsOrigin();
}


//-----------------------------------------------------------------------------
// Unlink + relink the pusher list so we can actually do the push
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::UnlinkPusherList(int* pPusherHandles)
{
	for (int i = m_rgPusher.Count(); --i >= 0; )
	{
		pPusherHandles[i] = partition->HideElement(m_rgPusher[i].m_pEntity->GetEngineObject()->GetPartitionHandle());
	}
}

void CPhysicsPushedEntities::RelinkPusherList(int* pPusherHandles)
{
	for (int i = m_rgPusher.Count(); --i >= 0; )
	{
		partition->UnhideElement(m_rgPusher[i].m_pEntity->GetEngineObject()->GetPartitionHandle(), pPusherHandles[i]);
	}
}


//-----------------------------------------------------------------------------
// Compute the direction to move the rotation blocker
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::ComputeRotationalPushDirection(IServerEntity* pBlocker, const RotatingPushMove_t& rotPushMove, Vector* pMove, IServerEntity* pRoot)
{
	// calculate destination position
	// "start" is relative to the *root* pusher, world orientation
	Vector start = pBlocker->GetEngineObject()->GetCollisionOrigin();
	if (pRoot->GetEngineObject()->GetSolid() == SOLID_VPHYSICS)
	{
		// HACKHACK: Use move dir to guess which corner of the box determines contact and rotate the box so
		// that corner remains in the same local position.
		// BUGBUG: This will break, but not as badly as the previous solution!!!
		Vector vecAbsMins, vecAbsMaxs;
		pBlocker->GetEngineObject()->WorldSpaceAABB(&vecAbsMins, &vecAbsMaxs);
		start.x = (pMove->x < 0) ? vecAbsMaxs.x : vecAbsMins.x;
		start.y = (pMove->y < 0) ? vecAbsMaxs.y : vecAbsMins.y;
		start.z = (pMove->z < 0) ? vecAbsMaxs.z : vecAbsMins.z;

		if (pBlocker->IsPlayer())
		{
			// notify the player physics code so it can use vphysics to keep players from getting stuck
			pBlocker->AsHandlePlayer()->SetPhysicsFlag(PFLAG_GAMEPHYSICS_ROTPUSH, true);
		}
	}

	// org is pusher local coordinate of start
	Vector local;
	// transform starting point into local space
	VectorITransform(start, rotPushMove.startLocalToWorld, local);
	// rotate local org into world space at end of rotation
	Vector end;
	VectorTransform(local, rotPushMove.endLocalToWorld, end);

	// move is the difference (in world space) that the move will push this object
	VectorSubtract(end, start, *pMove);
}

class CTraceFilterPushFinal : public CTraceFilterSimple
{
	typedef CTraceFilterSimple BaseClass;
	typedef CTraceFilterPushFinal ThisClass;;

public:
	CTraceFilterPushFinal(IServerEntity* pEntity, int nCollisionGroup)
		: CTraceFilterSimple(pEntity, nCollisionGroup)
	{

	}

	bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
	{
		Assert(dynamic_cast<IServerEntity*>(pHandleEntity));
		IServerEntity* pTestEntity = static_cast<IServerEntity*>(pHandleEntity);

		// UNDONE: This should really filter to just the pushing entities
		if (pTestEntity->GetEngineObject()->GetMoveType() == MOVETYPE_VPHYSICS &&
			pTestEntity->GetEngineObject()->VPhysicsGetObject() && pTestEntity->GetEngineObject()->VPhysicsGetObject()->IsMoveable())
			return false;

		return BaseClass::ShouldHitEntity(pHandleEntity, contentsMask);
	}

};

bool CPhysicsPushedEntities::IsPushedPositionValid(IServerEntity* pBlocker)
{
	CTraceFilterPushFinal pushFilter(pBlocker, pBlocker->GetEngineObject()->GetCollisionGroup());

	trace_t trace;
	pBlocker->GetEntityList()->AsServerEntityList()->GetEngineWorld()->TraceEntity(pBlocker->GetEngineObject(), pBlocker->GetEngineObject()->GetAbsOrigin(), pBlocker->GetEngineObject()->GetAbsOrigin(), pBlocker->PhysicsSolidMaskForEntity(), &pushFilter, &trace);

	return !trace.startsolid;
}

//-----------------------------------------------------------------------------
// Speculatively checks to see if all entities in this list can be pushed
//-----------------------------------------------------------------------------
bool CPhysicsPushedEntities::SpeculativelyCheckPush(PhysicsPushedInfo_t& info, const Vector& vecAbsPush, bool bRotationalPush)
{
	IServerEntity* pBlocker = info.m_pEntity;

	// See if it's possible to move the entity, but disable all pushers in the hierarchy first
	int* pPusherHandles = (int*)stackalloc(m_rgPusher.Count() * sizeof(int));
	UnlinkPusherList(pPusherHandles);
	CTraceFilterPushMove pushFilter(pBlocker, pBlocker->GetEngineObject()->GetCollisionGroup());

	Vector pushDestPosition = pBlocker->GetEngineObject()->GetAbsOrigin() + vecAbsPush;
	pBlocker->GetEntityList()->AsServerEntityList()->GetEngineWorld()->TraceEntity(pBlocker->GetEngineObject(), pBlocker->GetEngineObject()->GetAbsOrigin(), pushDestPosition,
		pBlocker->PhysicsSolidMaskForEntity(), &pushFilter, &info.m_Trace);

	RelinkPusherList(pPusherHandles);
	info.m_bPusherIsGround = false;
	if (pBlocker->GetEngineObject()->GetGroundEntity() && pBlocker->GetEngineObject()->GetGroundEntity()->GetRootMoveParent()->GetOuter() == m_rgPusher[0].m_pEntity)
	{
		info.m_bPusherIsGround = true;
	}

	bool bIsUnblockable = (m_bIsUnblockableByPlayer && (pBlocker->IsPlayer() || pBlocker->IsNPC())) ? true : false;
	if (bIsUnblockable)
	{
		pBlocker->GetEngineObject()->SetAbsOrigin(pushDestPosition);
	}
	else
	{
		// Move the blocker into its new position
		if (info.m_Trace.fraction)
		{
			pBlocker->GetEngineObject()->SetAbsOrigin(info.m_Trace.endpos);
		}

		// We're not blocked if the blocker is point-sized or non-solid
		if (pBlocker->GetEngineObject()->IsPointSized() || !pBlocker->GetEngineObject()->IsSolid() ||
			pBlocker->GetEngineObject()->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS))
		{
			return true;
		}

		if ((!bRotationalPush) && (info.m_Trace.fraction == 1.0))
		{
			//Assert( pBlocker->PhysicsTestEntityPosition() == false );
			if (!IsPushedPositionValid(pBlocker))
			{
				Warning("Interpenetrating entities! (%s and %s)\n",
					pBlocker->GetClassname(), m_rgPusher[0].m_pEntity->GetClassname());
			}

			return true;
		}
	}

	// Check to see if we're still blocked by the pushers
	// FIXME: If the trace fraction == 0 can we early out also?
	info.m_bBlocked = !IsPushedPositionValid(pBlocker);

	if (!info.m_bBlocked)
		return true;

	// if the player is blocking the train try nudging him around to fix accumulated error
	if (bIsUnblockable)
	{
		Vector org = pBlocker->GetEngineObject()->GetAbsOrigin();
		for (int checkCount = 0; checkCount < 4; checkCount++)
		{
			Vector move;
			MatrixGetColumn(m_rgPusher[0].m_pEntity->GetEngineObject()->EntityToWorldTransform(), checkCount >> 1, move);

			// alternate movements 1/2" in each direction
			float factor = (checkCount & 1) ? -0.5f : 0.5f;
			pBlocker->GetEngineObject()->SetAbsOrigin(org + move * factor);
			info.m_bBlocked = !IsPushedPositionValid(pBlocker);
			if (!info.m_bBlocked)
				return true;
		}
		pBlocker->GetEngineObject()->SetAbsOrigin(pushDestPosition);

#ifndef TF_DLL
		DevMsg(1, "Ignoring player blocking train!\n");
#endif
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Speculatively checks to see if all entities in this list can be pushed
//-----------------------------------------------------------------------------
bool CPhysicsPushedEntities::SpeculativelyCheckRotPush(const RotatingPushMove_t& rotPushMove, IServerEntity* pRoot)
{
	Vector vecAbsPush;
	m_nBlocker = -1;
	for (int i = m_rgMoved.Count(); --i >= 0; )
	{
		ComputeRotationalPushDirection(m_rgMoved[i].m_pEntity, rotPushMove, &vecAbsPush, pRoot);
		if (!SpeculativelyCheckPush(m_rgMoved[i], vecAbsPush, true))
		{
			m_nBlocker = i;
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Speculatively checks to see if all entities in this list can be pushed
//-----------------------------------------------------------------------------
bool CPhysicsPushedEntities::SpeculativelyCheckLinearPush(const Vector& vecAbsPush)
{
	m_nBlocker = -1;
	for (int i = m_rgMoved.Count(); --i >= 0; )
	{
		if (!SpeculativelyCheckPush(m_rgMoved[i], vecAbsPush, false))
		{
			m_nBlocker = i;
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Causes all entities in the list to touch triggers from their prev position
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::FinishPushers()
{
	// We succeeded! Now that we know the final location of all entities,
	// touch triggers + update physics objects + do other fixup
	for (int i = m_rgPusher.Count(); --i >= 0; )
	{
		PhysicsPusherInfo_t& info = m_rgPusher[i];

		// Cause touch functions to be called
		// FIXME: Need to make moved entities not touch triggers until we know we're ok
		// FIXME: it'd be better for the engine to just have a touch method
		info.m_pEntity->GetEngineObject()->PhysicsTouchTriggers(&info.m_vecStartAbsOrigin);

		info.m_pEntity->GetEngineObject()->UpdatePhysicsShadowToCurrentPosition(g_ServerGlobalVariables.frametime);
	}
}


//-----------------------------------------------------------------------------
// Causes all entities in the list to touch triggers from their prev position
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::FinishRotPushedEntity(IServerEntity* pPushedEntity, const RotatingPushMove_t& rotPushMove)
{
	// Impart angular velocity of push onto pushed objects
	if (pPushedEntity->IsPlayer())
	{
		QAngle angVel = pPushedEntity->GetEngineObject()->GetLocalAngularVelocity();
		angVel[1] = rotPushMove.amove[1];
		pPushedEntity->GetEngineObject()->SetLocalAngularVelocity(angVel);

		// Look up associated client
		pPushedEntity->AsHandlePlayer()->PlayerData()->fixangle = FIXANGLE_RELATIVE;
		// Because we can run multiple ticks per server frame, accumulate a total offset here instead of straight
		//  setting it.  The engine will reset anglechange to 0 when the message is actually sent to the client
		pPushedEntity->AsHandlePlayer()->PlayerData()->anglechange += rotPushMove.amove;
	}
	else
	{
		QAngle angles = pPushedEntity->GetEngineObject()->GetAbsAngles();

		// only rotate YAW with pushing.  Freely rotateable entities should either use VPHYSICS
		// or be set up as children
		angles.y += rotPushMove.amove.y;
		pPushedEntity->GetEngineObject()->SetAbsAngles(angles);
	}
}


//-----------------------------------------------------------------------------
// Causes all entities in the list to touch triggers from their prev position
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::FinishPush(bool bIsRotPush, const RotatingPushMove_t* pRotPushMove)
{
	FinishPushers();

	for (int i = m_rgMoved.Count(); --i >= 0; )
	{
		PhysicsPushedInfo_t& info = m_rgMoved[i];
		IServerEntity* pPushedEntity = info.m_pEntity;

		// Cause touch functions to be called
		// FIXME: it'd be better for the engine to just have a touch method
		info.m_pEntity->GetEngineObject()->PhysicsTouchTriggers(&info.m_vecStartAbsOrigin);
		info.m_pEntity->GetEngineObject()->UpdatePhysicsShadowToCurrentPosition(g_ServerGlobalVariables.frametime);
		IServerNPC* pNPC = info.m_pEntity->AsHandleNPC();
		if (info.m_bPusherIsGround && pNPC)
		{
			pNPC->NotifyPushMove();
		}


		// Register physics impacts...
		if (info.m_Trace.m_pEnt)
		{
			pPushedEntity->GetEngineObject()->PhysicsImpact((IEngineObjectServer*)info.m_Trace.m_pEnt->GetEngineObject(), info.m_Trace);
		}

		if (bIsRotPush)
		{
			FinishRotPushedEntity(pPushedEntity, *pRotPushMove);
		}
	}
}

// save initial state when beginning a push sequence
void CPhysicsPushedEntities::BeginPush(IServerEntity* pRoot)
{
	m_rgMoved.RemoveAll();
	m_rgPusher.RemoveAll();

	m_rootPusherStartLocalOrigin = pRoot->GetEngineObject()->GetLocalOrigin();
	m_rootPusherStartLocalAngles = pRoot->GetEngineObject()->GetLocalAngles();
	m_rootPusherStartLocaltime = pRoot->GetEngineObject()->GetLocalTime();
}

// store off a list of what has changed - so vphysicsUpdate can undo this if the object gets blocked
void CPhysicsPushedEntities::StoreMovedEntities(physicspushlist_t& list)
{
	list.localMoveTime = m_rootPusherStartLocaltime;
	list.localOrigin = m_rootPusherStartLocalOrigin;
	list.localAngles = m_rootPusherStartLocalAngles;
	list.pushedCount = CountMovedEntities();
	Assert(list.pushedCount < ARRAYSIZE(list.pushedEnts));
	if (list.pushedCount > ARRAYSIZE(list.pushedEnts))
	{
		list.pushedCount = ARRAYSIZE(list.pushedEnts);
	}
	for (int i = 0; i < list.pushedCount; i++)
	{
		list.pushedEnts[i] = m_rgMoved[i].m_pEntity;
		list.pushVec[i] = m_rgMoved[i].m_pEntity->GetEngineObject()->GetAbsOrigin() - m_rgMoved[i].m_vecStartAbsOrigin;
	}
}

//-----------------------------------------------------------------------------
// Registers a blockage
//-----------------------------------------------------------------------------
IServerEntity* CPhysicsPushedEntities::RegisterBlockage()
{
	Assert(m_nBlocker >= 0);

	// Generate a PhysicsImpact against the blocker...
	PhysicsPushedInfo_t& info = m_rgMoved[m_nBlocker];
	if (info.m_Trace.m_pEnt)
	{
		info.m_pEntity->GetEngineObject()->PhysicsImpact((IEngineObjectServer*)info.m_Trace.m_pEnt->GetEngineObject(), info.m_Trace);
	}

	// This is the dude 
	return info.m_pEntity;
}


//-----------------------------------------------------------------------------
// Purpose: Restore entities that might have been moved
// Input  : fromrotation - if the move is from a rotation, then angular move must also be reverted
//			*amove - 
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::RestoreEntities()
{
	// Reset all of the pushed entities to get them back into place also
	for (int i = m_rgMoved.Count(); --i >= 0; )
	{
		m_rgMoved[i].m_pEntity->GetEngineObject()->SetAbsOrigin(m_rgMoved[i].m_vecStartAbsOrigin);
	}
}




//-----------------------------------------------------------------------------
// Purpose: This is a trace filter that only hits an exclusive list of entities
//-----------------------------------------------------------------------------
class CTraceFilterAgainstEntityList : public ITraceFilter
{
public:
	virtual bool ShouldHitEntity(IHandleEntity* pEntity, int contentsMask)
	{
		for (int i = m_entityList.Count() - 1; i >= 0; --i)
		{
			if (m_entityList[i] == pEntity)
				return true;
		}

		return false;
	}

	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_ENTITIES_ONLY;
	}

	void AddEntityToHit(IHandleEntity* pEntity)
	{
		m_entityList.AddToTail(pEntity);
	}

	CUtlVector<IHandleEntity*>	m_entityList;
};

//-----------------------------------------------------------------------------
// Generates a list of potential blocking entities
//-----------------------------------------------------------------------------
class CPushBlockerEnum : public IPartitionEnumerator
{
public:
	CPushBlockerEnum(CPhysicsPushedEntities* pPushedEntities) : m_pPushedEntities(pPushedEntities)
	{
		// All elements are part of the same hierarchy, so they all have
		// the same root, so it doesn't matter which one we grab
		m_pRootHighestParent = m_pPushedEntities->m_rgPusher[0].m_pEntity->GetEngineObject()->GetRootMoveParent()->GetOuter();
		++s_nEnumCount;

		m_collisionGroupCount = 0;
		for (int i = m_pPushedEntities->m_rgPusher.Count(); --i >= 0; )
		{
			if (!m_pPushedEntities->m_rgPusher[i].m_pEntity->GetEngineObject()->IsSolid())
				continue;

			m_pushersOnly.AddEntityToHit(m_pPushedEntities->m_rgPusher[i].m_pEntity);
			int collisionGroup = m_pPushedEntities->m_rgPusher[i].m_pEntity->GetEngineObject()->GetCollisionGroup();
			AddCollisionGroup(collisionGroup);
		}

	}

	virtual IterationRetval_t EnumElement(IHandleEntity* pHandleEntity)
	{
		IServerEntity* pCheck = GetPushableEntity(pHandleEntity);
		if (!pCheck)
			return ITERATION_CONTINUE;

		// Mark it as seen
		pCheck->GetEngineObject()->SetPushEnumCount(s_nEnumCount);
		m_pPushedEntities->AddEntity(pCheck);

		return ITERATION_CONTINUE;
	}

private:

	inline void AddCollisionGroup(int collisionGroup)
	{
		for (int i = 0; i < m_collisionGroupCount; i++)
		{
			if (m_collisionGroups[i] == collisionGroup)
				return;
		}
		if (m_collisionGroupCount < ARRAYSIZE(m_collisionGroups))
		{
			m_collisionGroups[m_collisionGroupCount] = collisionGroup;
			m_collisionGroupCount++;
		}
	}

	bool IsStandingOnPusher(IServerEntity* pCheck)
	{
		IServerEntity* pGroundEnt = pCheck->GetEngineObject()->GetGroundEntity() ? pCheck->GetEngineObject()->GetGroundEntity()->GetOuter() : NULL;
		if (pCheck->GetEngineObject()->GetFlags() & FL_ONGROUND || pGroundEnt)
		{
			for (int i = m_pPushedEntities->m_rgPusher.Count(); --i >= 0; )
			{
				if (m_pPushedEntities->m_rgPusher[i].m_pEntity == pGroundEnt)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool IntersectsPushers(IServerEntity* pTest)
	{
		trace_t tr;

		ICollideable* pCollision = pTest->GetCollideable();
		g_pEngineTraceServer->SweepCollideable(pCollision, pTest->GetEngineObject()->GetAbsOrigin(), pTest->GetEngineObject()->GetAbsOrigin(), pCollision->GetCollisionAngles(),
			pTest->PhysicsSolidMaskForEntity(), &m_pushersOnly, &tr);

		return tr.startsolid;
	}

	IServerEntity* GetPushableEntity(IHandleEntity* pHandleEntity)
	{
		IServerEntity* pCheck = (IServerEntity*)pHandleEntity->GetEntityList()->GetBaseEntityFromHandle(pHandleEntity->GetRefEHandle());
		if (!pCheck)
			return NULL;

		// Don't bother if we've already seen this one...
		if (pCheck->GetEngineObject()->GetPushEnumCount() == s_nEnumCount)
			return NULL;

		if (!pCheck->GetEngineObject()->IsSolid())
			return NULL;

		if (pCheck->GetEngineObject()->GetMoveType() == MOVETYPE_PUSH ||
			pCheck->GetEngineObject()->GetMoveType() == MOVETYPE_NONE ||
			pCheck->GetEngineObject()->GetMoveType() == MOVETYPE_VPHYSICS ||
			pCheck->GetEngineObject()->GetMoveType() == MOVETYPE_NOCLIP)
		{
			return NULL;
		}

		bool bCollide = false;
		for (int i = 0; i < m_collisionGroupCount; i++)
		{
			if (pCheck->GetEntityList()->GetWorld()->ShouldCollide(pCheck->GetEngineObject()->GetCollisionGroup(), m_collisionGroups[i]))
			{
				bCollide = true;
				break;
			}
		}
		if (!bCollide)
			return NULL;
		// We're not pushing stuff we're hierarchically attached to
		IEngineObjectServer* pCheckHighestParent = pCheck->GetEngineObject()->GetRootMoveParent();
		if (pCheckHighestParent->GetOuter() == m_pRootHighestParent)
			return NULL;

		// If we're standing on the pusher or any rigidly attached child
		// of the pusher, we don't need to bother checking for interpenetration
		if (!IsStandingOnPusher(pCheck))
		{
			// Our surrounding boxes are touching. But we may well not be colliding....
			// see if the ent's bbox is inside the pusher's final position
			if (!IntersectsPushers(pCheck))
				return NULL;
		}

		// NOTE: This is pretty tricky here. If a rigidly attached child comes into
		// contact with a pusher, we *cannot* push the child. Instead, we must push
		// the highest parent of that child.
		return pCheckHighestParent->GetOuter();
	}

private:
	static int s_nEnumCount;
	CPhysicsPushedEntities* m_pPushedEntities;
	IServerEntity* m_pRootHighestParent;
	CTraceFilterAgainstEntityList	m_pushersOnly;
	int m_collisionGroups[8];
	int m_collisionGroupCount;
};

int CPushBlockerEnum::s_nEnumCount = 0;

//-----------------------------------------------------------------------------
// Generates a list of potential blocking entities
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::GenerateBlockingEntityList()
{
	VPROF("CPhysicsPushedEntities::GenerateBlockingEntityList");

	m_rgMoved.RemoveAll();
	CPushBlockerEnum blockerEnum(this);

	for (int i = m_rgPusher.Count(); --i >= 0; )
	{
		IServerEntity* pPusher = m_rgPusher[i].m_pEntity;

		// Don't bother if the pusher isn't solid
		if (!pPusher->GetEngineObject()->IsSolid() || pPusher->GetEngineObject()->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS))
		{
			continue;
		}

		Vector vecAbsMins, vecAbsMaxs;
		pPusher->GetEngineObject()->WorldSpaceAABB(&vecAbsMins, &vecAbsMaxs);
		partition->EnumerateElementsInBox(PARTITION_ENGINE_NON_STATIC_EDICTS, vecAbsMins, vecAbsMaxs, false, &blockerEnum);

		//Go back throught the generated list.
	}
}

//-----------------------------------------------------------------------------
// Generates a list of potential blocking entities
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::GenerateBlockingEntityListAddBox(const Vector& vecMoved)
{
	VPROF("CPhysicsPushedEntities::GenerateBlockingEntityListAddBox");

	m_rgMoved.RemoveAll();
	CPushBlockerEnum blockerEnum(this);

	for (int i = m_rgPusher.Count(); --i >= 0; )
	{
		IServerEntity* pPusher = m_rgPusher[i].m_pEntity;

		// Don't bother if the pusher isn't solid
		if (!pPusher->GetEngineObject()->IsSolid() || pPusher->GetEngineObject()->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS))
		{
			continue;
		}

		Vector vecAbsMins, vecAbsMaxs;
		pPusher->GetEngineObject()->WorldSpaceAABB(&vecAbsMins, &vecAbsMaxs);
		for (int iAxis = 0; iAxis < 3; ++iAxis)
		{
			if (vecMoved[iAxis] >= 0.0f)
			{
				vecAbsMins[iAxis] -= vecMoved[iAxis];
			}
			else
			{
				vecAbsMaxs[iAxis] -= vecMoved[iAxis];
			}
		}

		partition->EnumerateElementsInBox(PARTITION_ENGINE_NON_STATIC_EDICTS, vecAbsMins, vecAbsMaxs, false, &blockerEnum);

		//Go back throught the generated list.
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets a list of all entities hierarchically attached to the root 
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::SetupAllInHierarchy(IServerEntity* pParent)
{
	if (!pParent)
		return;

	VPROF("CPhysicsPushedEntities::SetupAllInHierarchy");

	// Make sure to snack the position +before+ relink because applying the
	// rotation (which occurs in relink) will put it at the final location
	// NOTE: The root object at this point is actually at its final position.
	// We'll fix that up later
	int i = m_rgPusher.AddToTail();
	m_rgPusher[i].m_pEntity = pParent;
	m_rgPusher[i].m_vecStartAbsOrigin = pParent->GetEngineObject()->GetAbsOrigin();

	IEngineObjectServer* pChild;
	for (pChild = pParent->GetEngineObject()->FirstMoveChild(); pChild != NULL; pChild = pChild->NextMovePeer())
	{
		SetupAllInHierarchy(pChild->GetOuter());
	}
}


//-----------------------------------------------------------------------------
// Purpose: Rotates the root entity, fills in the pushmove structure
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::RotateRootEntity(IServerEntity* pRoot, float movetime, RotatingPushMove_t& rotation)
{
	VPROF("CPhysicsPushedEntities::RotateRootEntity");

	rotation.amove = pRoot->GetEngineObject()->GetLocalAngularVelocity() * movetime;
	rotation.origin = pRoot->GetEngineObject()->GetAbsOrigin();

	// Knowing the initial + ending basis is needed for determining
	// which corner we're pushing 
	MatrixCopy(pRoot->GetEngineObject()->EntityToWorldTransform(), rotation.startLocalToWorld);

	// rotate the pusher to it's final position
	QAngle angles = pRoot->GetEngineObject()->GetLocalAngles();
	angles += pRoot->GetEngineObject()->GetLocalAngularVelocity() * movetime;

	pRoot->GetEngineObject()->SetLocalAngles(angles);

	// Compute the change in absangles
	MatrixCopy(pRoot->GetEngineObject()->EntityToWorldTransform(), rotation.endLocalToWorld);
}


//-----------------------------------------------------------------------------
// Purpose: Tries to rotate an entity hierarchy, returns the blocker if any
//-----------------------------------------------------------------------------
IServerEntity* CPhysicsPushedEntities::PerformRotatePush(IServerEntity* pRoot, float movetime)
{
	VPROF("CPhysicsPushedEntities::PerformRotatePush");

	m_bIsUnblockableByPlayer = (pRoot->GetEngineObject()->GetFlags() & FL_UNBLOCKABLE_BY_PLAYER) ? true : false;
	// Build a list of this entity + all its children because we're going to try to move them all
	// This will also make sure each entity is linked in the appropriate place
	// with correct absboxes
	m_rgPusher.RemoveAll();
	SetupAllInHierarchy(pRoot);

	// save where we rotated from, in case we're blocked
	QAngle angPrevAngles = pRoot->GetEngineObject()->GetLocalAngles();

	// Apply the rotation
	RotatingPushMove_t	rotPushMove;
	RotateRootEntity(pRoot, movetime, rotPushMove);

	// Next generate a list of all entities that could potentially be intersecting with
	// any of the children in their new locations...
	GenerateBlockingEntityList();

	// Now we have a unique list of things that could potentially block our push
	// and need to be pushed out of the way. Lets try to push them all out of the way.
	// If we fail, undo it all
	if (!SpeculativelyCheckRotPush(rotPushMove, pRoot))
	{
		IServerEntity* pBlocker = RegisterBlockage();
		pRoot->GetEngineObject()->SetLocalAngles(angPrevAngles);
		RestoreEntities();
		return pBlocker;
	}

	FinishPush(true, &rotPushMove);
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Linearly moves the root entity
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::LinearlyMoveRootEntity(IServerEntity* pRoot, float movetime, Vector* pAbsPushVector)
{
	VPROF("CPhysicsPushedEntities::LinearlyMoveRootEntity");

	// move the pusher to it's final position
	Vector move = pRoot->GetEngineObject()->GetLocalVelocity() * movetime;
	Vector origin = pRoot->GetEngineObject()->GetLocalOrigin();
	origin += move;
	pRoot->GetEngineObject()->SetLocalOrigin(origin);

	// Store off the abs push vector
	*pAbsPushVector = pRoot->GetEngineObject()->GetAbsVelocity() * movetime;
}


//-----------------------------------------------------------------------------
// Purpose: Tries to linearly push an entity hierarchy, returns the blocker if any
//-----------------------------------------------------------------------------
IServerEntity* CPhysicsPushedEntities::PerformLinearPush(IServerEntity* pRoot, float movetime)
{
	VPROF("CPhysicsPushedEntities::PerformLinearPush");

	m_flMoveTime = movetime;

	m_bIsUnblockableByPlayer = (pRoot->GetEngineObject()->GetFlags() & FL_UNBLOCKABLE_BY_PLAYER) ? true : false;
	// Build a list of this entity + all its children because we're going to try to move them all
	// This will also make sure each entity is linked in the appropriate place
	// with correct absboxes
	m_rgPusher.RemoveAll();
	SetupAllInHierarchy(pRoot);

	// save where we started from, in case we're blocked
	Vector vecPrevOrigin = pRoot->GetEngineObject()->GetLocalOrigin();

	// Move the root (and all children) into its new position
	Vector vecAbsPush;
	LinearlyMoveRootEntity(pRoot, movetime, &vecAbsPush);

	// Next generate a list of all entities that could potentially be intersecting with
	// any of the children in their new locations...
	GenerateBlockingEntityListAddBox(vecAbsPush);

	// Now we have a unique list of things that could potentially block our push
	// and need to be pushed out of the way. Lets try to push them all out of the way.
	// If we fail, undo it all
	if (!SpeculativelyCheckLinearPush(vecAbsPush))
	{
		IServerEntity* pBlocker = RegisterBlockage();
		pRoot->GetEngineObject()->SetLocalOrigin(vecPrevOrigin);
		RestoreEntities();
		return pBlocker;
	}

	FinishPush();
	return NULL;
}

CPhysicsPushedEntities s_PushedEntities;
#ifndef TF_DLL
CPhysicsPushedEntities* g_pPushedEntities = &s_PushedEntities;
#endif
