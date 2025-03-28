//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VARIANT_T_H
#define VARIANT_T_H
#ifdef _WIN32
#pragma once
#endif


#include "datamap.h"
#include "string_t.h"
#include "mathlib/vmatrix.h"
#include "basehandle.h"

//
// A variant class for passing data in entity input/output connections.
//
class variant_t
{
public:
	union
	{
		bool bVal;
		string_t iszVal;
		int iVal;
		float flVal;
		float vecVal[3];
		color32 rgbaVal;
	};
	CBaseHandle eVal; // this can't be in the union because it has a constructor.

	fieldtype_t fieldType;

public:

	// constructor
	variant_t() : fieldType(FIELD_VOID), iVal(0) {}

	inline bool Bool( void ) const						{ return( fieldType == FIELD_BOOLEAN ) ? bVal : false; }
	inline const char *String(IEntityList* pEntityList) const { return( fieldType == FIELD_STRING ) ? STRING(iszVal) : ToString(pEntityList); }
	inline string_t StringID( void ) const				{ return( fieldType == FIELD_STRING ) ? iszVal : NULL_STRING; }
	inline int Int( void ) const						{ return( fieldType == FIELD_INTEGER ) ? iVal : 0; }
	inline float Float( void ) const					{ return( fieldType == FIELD_FLOAT ) ? flVal : 0; }
	inline const IHandleEntity* Entity(IEntityList* pEntityList) const;
	inline color32 Color32(void) const					{ return rgbaVal; }
	inline void Vector3D(Vector &vec) const;

	fieldtype_t FieldType( void ) { return fieldType; }

	void SetBool( bool b ) { bVal = b; fieldType = FIELD_BOOLEAN; }
	void SetString( string_t str ) { iszVal = str, fieldType = FIELD_STRING; }
	void SetInt( int val ) { iVal = val, fieldType = FIELD_INTEGER; }
	void SetFloat( float val ) { flVal = val, fieldType = FIELD_FLOAT; }
	void SetEntity(IHandleEntity* val) { eVal = val; fieldType = FIELD_EHANDLE; }
	void SetVector3D( const Vector &val ) { vecVal[0] = val[0]; vecVal[1] = val[1]; vecVal[2] = val[2]; fieldType = FIELD_VECTOR; }
	void SetPositionVector3D( const Vector &val ) { vecVal[0] = val[0]; vecVal[1] = val[1]; vecVal[2] = val[2]; fieldType = FIELD_POSITION_VECTOR; }
	void SetColor32( color32 val ) { rgbaVal = val; fieldType = FIELD_COLOR32; }
	void SetColor32( int r, int g, int b, int a ) { rgbaVal.r = r; rgbaVal.g = g; rgbaVal.b = b; rgbaVal.a = a; fieldType = FIELD_COLOR32; }
	void Set( fieldtype_t ftype, void *data );
	void SetOther(IEntityList* pEntityList, void *data );
	bool Convert(IEntityList* pEntityList, fieldtype_t newType );



protected:

	//
	// Returns a string representation of the value without modifying the variant.
	//
	const char *ToString(IEntityList* pEntityList) const;

	friend class CVariantSaveDataOps;
};


//-----------------------------------------------------------------------------
// Purpose: Returns this variant as a vector.
//-----------------------------------------------------------------------------
inline void variant_t::Vector3D(Vector &vec) const
{
	if (( fieldType == FIELD_VECTOR ) || ( fieldType == FIELD_POSITION_VECTOR ))
	{
		vec[0] =  vecVal[0];
		vec[1] =  vecVal[1];
		vec[2] =  vecVal[2];
	}
	else
	{
		vec = vec3_origin;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns this variant as an EHANDLE.
//-----------------------------------------------------------------------------
inline const IHandleEntity* variant_t::Entity(IEntityList* pEntityList) const
{
	if ( fieldType == FIELD_EHANDLE )
		return pEntityList->GetBaseEntityFromHandle(eVal);
	return NULL;
}

////////////////////////// variant_t implementation //////////////////////////

// BUGBUG: Add support for function pointer save/restore to variants
// BUGBUG: Must pass datamap_t to read/write fields 
inline void variant_t::Set(fieldtype_t ftype, void* data)
{
	fieldType = ftype;

	switch (ftype)
	{
	case FIELD_BOOLEAN:		bVal = *((bool*)data);				break;
	case FIELD_CHARACTER:	iVal = *((char*)data);				break;
	case FIELD_SHORT:		iVal = *((short*)data);			break;
	case FIELD_INTEGER:		iVal = *((int*)data);				break;
	case FIELD_STRING:		iszVal = *((string_t*)data);		break;
	case FIELD_FLOAT:		flVal = *((float*)data);			break;
	case FIELD_COLOR32:		rgbaVal = *((color32*)data);		break;

	case FIELD_VECTOR:
	case FIELD_POSITION_VECTOR:
	{
		vecVal[0] = ((float*)data)[0];
		vecVal[1] = ((float*)data)[1];
		vecVal[2] = ((float*)data)[2];
		break;
	}

	case FIELD_EHANDLE:		eVal = *((CBaseHandle*)data);			break;
	case FIELD_CLASSPTR:	eVal = ((IHandleEntity*)data)->GetRefEHandle();		break;
	case FIELD_VOID:
	default:
		iVal = 0; fieldType = FIELD_VOID;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copies the value in the variant into a block of memory
// Input  : *data - the block to write into
//-----------------------------------------------------------------------------
inline void variant_t::SetOther(IEntityList* pEntityList, void* data)
{
	switch (fieldType)
	{
	case FIELD_BOOLEAN:		*((bool*)data) = bVal != 0;		break;
	case FIELD_CHARACTER:	*((char*)data) = iVal;				break;
	case FIELD_SHORT:		*((short*)data) = iVal;			break;
	case FIELD_INTEGER:		*((int*)data) = iVal;				break;
	case FIELD_STRING:		*((string_t*)data) = iszVal;		break;
	case FIELD_FLOAT:		*((float*)data) = flVal;			break;
	case FIELD_COLOR32:		*((color32*)data) = rgbaVal;		break;

	case FIELD_VECTOR:
	case FIELD_POSITION_VECTOR:
	{
		((float*)data)[0] = vecVal[0];
		((float*)data)[1] = vecVal[1];
		((float*)data)[2] = vecVal[2];
		break;
	}

	case FIELD_EHANDLE:		*((CBaseHandle*)data) = eVal;			break;
	case FIELD_CLASSPTR:	*((IHandleEntity**)data) = pEntityList->GetBaseEntityFromHandle(eVal);		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Converts the variant to a new type. This function defines which I/O
//			types can be automatically converted between. Connections that require
//			an unsupported conversion will cause an error message at runtime.
// Input  : newType - the type to convert to
// Output : Returns true on success, false if the conversion is not legal
//-----------------------------------------------------------------------------
inline bool variant_t::Convert(IEntityList* pEntityList, fieldtype_t newType)
{
	if (newType == fieldType)
	{
		return true;
	}

	//
	// Converting to a null value is easy.
	//
	if (newType == FIELD_VOID)
	{
		Set(FIELD_VOID, NULL);
		return true;
	}

	//
	// FIELD_INPUT accepts the variant type directly.
	//
	if (newType == FIELD_INPUT)
	{
		return true;
	}

	switch (fieldType)
	{
	case FIELD_INTEGER:
	{
		switch (newType)
		{
		case FIELD_FLOAT:
		{
			SetFloat((float)iVal);
			return true;
		}

		case FIELD_BOOLEAN:
		{
			SetBool(iVal != 0);
			return true;
		}
		}
		break;
	}

	case FIELD_FLOAT:
	{
		switch (newType)
		{
		case FIELD_INTEGER:
		{
			SetInt((int)flVal);
			return true;
		}

		case FIELD_BOOLEAN:
		{
			SetBool(flVal != 0);
			return true;
		}
		}
		break;
	}

	//
	// Everyone must convert from FIELD_STRING if possible, since
	// parameter overrides are always passed as strings.
	//
	case FIELD_STRING:
	{
		switch (newType)
		{
		case FIELD_INTEGER:
		{
			if (iszVal != NULL_STRING)
			{
				SetInt(atoi(STRING(iszVal)));
			}
			else
			{
				SetInt(0);
			}
			return true;
		}

		case FIELD_FLOAT:
		{
			if (iszVal != NULL_STRING)
			{
				SetFloat(atof(STRING(iszVal)));
			}
			else
			{
				SetFloat(0);
			}
			return true;
		}

		case FIELD_BOOLEAN:
		{
			if (iszVal != NULL_STRING)
			{
				SetBool(atoi(STRING(iszVal)) != 0);
			}
			else
			{
				SetBool(false);
			}
			return true;
		}

		case FIELD_VECTOR:
		{
			Vector tmpVec = vec3_origin;
			if (sscanf(STRING(iszVal), "[%f %f %f]", &tmpVec[0], &tmpVec[1], &tmpVec[2]) == 0)
			{
				// Try sucking out 3 floats with no []s
				sscanf(STRING(iszVal), "%f %f %f", &tmpVec[0], &tmpVec[1], &tmpVec[2]);
			}
			SetVector3D(tmpVec);
			return true;
		}

		case FIELD_COLOR32:
		{
			int nRed = 0;
			int nGreen = 0;
			int nBlue = 0;
			int nAlpha = 255;

			sscanf(STRING(iszVal), "%d %d %d %d", &nRed, &nGreen, &nBlue, &nAlpha);
			SetColor32(nRed, nGreen, nBlue, nAlpha);
			return true;
		}

		case FIELD_EHANDLE:
		{
			// convert the string to an entity by locating it by classname
			IHandleEntity* ent = NULL;
			if (iszVal != NULL_STRING)
			{
				// FIXME: do we need to pass an activator in here?
				ent = pEntityList->FindHandleEntityByName(NULL, iszVal);
			}
			SetEntity(ent);
			return true;
		}
		}

		break;
	}

	case FIELD_EHANDLE:
	{
		switch (newType)
		{
		case FIELD_STRING:
		{
			// take the entities targetname as the string
			string_t iszStr = NULL_STRING;
			if (eVal != NULL)
			{
				SetString(pEntityList->GetBaseEntityFromHandle(eVal)->GetEntityName());
			}
			return true;
		}
		}
		break;
	}
	}

	// invalid conversion
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: All types must be able to display as strings for debugging purposes.
// Output : Returns a pointer to the string that represents this value.
//
//			NOTE: The returned pointer should not be stored by the caller as
//				  subsequent calls to this function will overwrite the contents
//				  of the buffer!
//-----------------------------------------------------------------------------
inline const char* variant_t::ToString(IEntityList* pEntityList) const
{
	COMPILE_TIME_ASSERT(sizeof(string_t) == sizeof(intp));

	static char szBuf[512];

	switch (fieldType)
	{
	case FIELD_STRING:
	{
		return(STRING(iszVal));
	}

	case FIELD_BOOLEAN:
	{
		if (bVal == 0)
		{
			Q_strncpy(szBuf, "false", sizeof(szBuf));
		}
		else
		{
			Q_strncpy(szBuf, "true", sizeof(szBuf));
		}
		return(szBuf);
	}

	case FIELD_INTEGER:
	{
		Q_snprintf(szBuf, sizeof(szBuf), "%i", iVal);
		return(szBuf);
	}

	case FIELD_FLOAT:
	{
		Q_snprintf(szBuf, sizeof(szBuf), "%g", flVal);
		return(szBuf);
	}

	case FIELD_COLOR32:
	{
		Q_snprintf(szBuf, sizeof(szBuf), "%d %d %d %d", (int)rgbaVal.r, (int)rgbaVal.g, (int)rgbaVal.b, (int)rgbaVal.a);
		return(szBuf);
	}

	case FIELD_VECTOR:
	{
		Q_snprintf(szBuf, sizeof(szBuf), "[%g %g %g]", (double)vecVal[0], (double)vecVal[1], (double)vecVal[2]);
		return(szBuf);
	}

	case FIELD_VOID:
	{
		szBuf[0] = '\0';
		return(szBuf);
	}

	case FIELD_EHANDLE:
	{
		const char* pszName = (Entity(pEntityList)) ? STRING(Entity(pEntityList)->GetEntityName()) : "<<null entity>>";
		Q_strncpy(szBuf, pszName, 512);
		return (szBuf);
	}
	}

	return("No conversion to string");
}

#endif // VARIANT_T_H
