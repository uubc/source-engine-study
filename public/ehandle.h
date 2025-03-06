//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef EHANDLE_H
#define EHANDLE_H
#ifdef _WIN32
#pragma once
#endif

//#if defined( _DEBUG ) && defined( GAME_DLL )
//#include "tier0/dbg.h"
//#include "cbase.h"
//#endif


//#include "const.h"
#include "basehandle.h"
#include "networkvar.h"
//#include "entitylist_base.h"

// Network ehandle wrapper.
#if defined( CLIENT_DLL ) || defined( GAME_DLL )
inline void NetworkVarConstruct(CBaseHandle& x) {}

// -------------------------------------------------------------------------------------------------- //
// CHandle.
// -------------------------------------------------------------------------------------------------- //
template< class T >
class CHandle : public CBaseHandle
{
public:

	CHandle() = default;
	CHandle(int iEntry, int iSerialNumber);
	CHandle(const CBaseHandle& handle);
	CHandle(T* pVal);

	// The index should have come from a call to ToInt(). If it hasn't, you're in trouble.
	static CHandle<T> FromIndex(int index);

	T* Get() const;
	void	Set(const T* pVal);

	operator T* ();
	operator T* () const;

	bool	operator !() const;
	bool	operator==(T* val) const;
	bool	operator!=(T* val) const;
	const CBaseHandle& operator=(const T* val);

	T* operator->() const;
};


// ----------------------------------------------------------------------- //
// Inlines.
// ----------------------------------------------------------------------- //

template<class T>
CHandle<T>::CHandle(int iEntry, int iSerialNumber)
{
	Init(iEntry, iSerialNumber);
}


template<class T>
CHandle<T>::CHandle(const CBaseHandle& handle)
	: CBaseHandle(handle)
{
}


template<class T>
CHandle<T>::CHandle(T* pObj)
{
	Term();
	Set(pObj);
}


template<class T>
inline CHandle<T> CHandle<T>::FromIndex(int index)
{
	CHandle<T> ret;
	ret.m_Index = index;
	return ret;
}


//template<class T>
//inline T* CHandle<T>::Get() const
//{
//	return (T*)CBaseHandle::Get();
//}


template<class T>
inline CHandle<T>::operator T* ()
{
	return Get();
}

template<class T>
inline CHandle<T>::operator T* () const
{
	return Get();
}


template<class T>
inline bool CHandle<T>::operator !() const
{
	return !Get();
}

template<class T>
inline bool CHandle<T>::operator==(T* val) const
{
	return Get() == val;
}

template<class T>
inline bool CHandle<T>::operator!=(T* val) const
{
	return Get() != val;
}

template<class T>
void CHandle<T>::Set(const T* pVal)
{
	CBaseHandle::Set(reinterpret_cast<const IHandleEntity*>(pVal));
}

template<class T>
inline const CBaseHandle& CHandle<T>::operator=(const T* val)
{
	Set(val);
	return *this;
}

template<class T>
T* CHandle<T>::operator -> () const
{
	return Get();
}

template<class T>
inline T* CHandle<T>::Get() const
{
	return (T*)EntityList()->GetBaseEntityFromHandle(*this);
}

template< class Type, class T, int (*OffsetFun)(void), void (T::* ChangeFun)(void*), bool debug = false >
class CNetworkHandleBase// : public CNetworkVarBase< CBaseHandle, T, OffsetFun, ChangeFun, debug >
{
	//typedef CNetworkVarBase< CBaseHandle, T, OffsetFun, ChangeFun > NetworkVarBaseClass;
public:

	const Type* operator=(const Type* val)
	{
		return Set(val);
	}

	template<class C>
	const Type& operator=(const CNetworkHandleBase<C, T, OffsetFun, ChangeFun >& val)
	{
		const CHandle<Type>& handle = Set(val.m_Value);//NetworkVarBaseClass::
		return *(const Type*)handle.Get();
	}

	bool operator !() const
	{
		return !m_Value.Get();//NetworkVarBaseClass::
	}

	operator Type* () const
	{
		return static_cast<Type*>(m_Value.Get());//NetworkVarBaseClass::
	}

	//operator int() {
	//	return CNetworkVarBase<CBaseHandle, Changer>::m_Value.ToInt();
	//}

	const Type* Set(const Type* val)
	{
		if (m_Value != val)//NetworkVarBaseClass::
		{
			NetworkStateChanged();//NetworkVarBaseClass::
			m_Value = val;//NetworkVarBaseClass::
		}
		return val;
	}

	CBaseHandle& GetForModify()
	{
		NetworkStateChanged();
		return m_Value;
	}

	Type* Get() const
	{
		return static_cast<Type*>(m_Value.Get());//NetworkVarBaseClass::
	}

	bool IsValid() const {
		return m_Value.IsValid();//NetworkVarBaseClass::
	}

	int ToInt() {
		return m_Value.ToInt();//NetworkVarBaseClass::
	}

	int GetEntryIndex() const {
		return m_Value.GetEntryIndex();//NetworkVarBaseClass::
	}
	int GetSerialNumber() const {
		return m_Value.GetSerialNumber();//NetworkVarBaseClass::
	}

	void Init(int iEntity, int iSerialNum) {
		m_Value.Init(iEntity, iSerialNum);//NetworkVarBaseClass::
	}

	void Term() {
		m_Value.Term();//NetworkVarBaseClass::
	}

	Type* operator->() const
	{
		return static_cast<Type*>(m_Value.Get());//NetworkVarBaseClass::
	}

	bool operator==(const Type* val) const
	{
		return m_Value == val;//NetworkVarBaseClass::
	}

	bool operator!=(const Type* val) const
	{
		return m_Value != val;//NetworkVarBaseClass::
	}

	CHandle<Type> m_Value;

protected:
	inline void NetworkStateChanged()
	{
		//Changer::NetworkStateChanged( this );
		CHECK_USENETWORKVARS((T*)(((char*)this) - OffsetFun())->*ChangeFun)(this);
	}
};

#define CNetworkHandle( type, name ) \
		static int GetOffsetOf_##name(){\
			return MyOffsetOf(ThisClass, name);\
		}\
		void NetworkStateChangedDispatch_##name( void *ptr ) \
		{ \
			NetworkStateChanged(ptr);\
		}\
		public:\
		CNetworkHandleBase<type, ThisClass, &GetOffsetOf_##name, &ThisClass::NetworkStateChangedDispatch_##name> name;

#define CNetworkHandleForDerived( type, name ) \
		virtual void NetworkStateChanged_##name() {} \
		virtual void NetworkStateChanged_##name( void *pVar ) {} \
		static int GetOffsetOf_##name(){\
			return MyOffsetOf(ThisClass, name);\
		}\
		void NetworkStateChangedDispatch_##name( void *ptr ) \
		{ \
			NetworkStateChanged_##name(ptr);\
		}\
		public:\
		CNetworkHandleBase<type, ThisClass, &GetOffsetOf_##name, &ThisClass::NetworkStateChangedDispatch_##name> name;

#define CNetworkHandle2( type, name ) \
		static int GetOffsetOf_##name(){\
			return MyOffsetOf(ThisClass, name);\
		}\
		void NetworkStateChangedDispatch_##name( void *ptr ) \
		{ \
			NetworkStateChanged(ptr);\
		}\
		public:\
		CNetworkHandleBase<type, ThisClass, &GetOffsetOf_##name, &ThisClass::NetworkStateChangedDispatch_##name,true> name;

#define CNetworkHandleForDerived2( type, name ) \
		virtual void NetworkStateChanged_##name() {} \
		virtual void NetworkStateChanged_##name( void *pVar ) {} \
		static int GetOffsetOf_##name(){\
			return MyOffsetOf(ThisClass, name);\
		}\
		void NetworkStateChangedDispatch_##name( void *ptr ) \
		{ \
			NetworkStateChanged_##name(ptr);\
		}\
		public:\
		CNetworkHandleBase<type, ThisClass, &GetOffsetOf_##name, &ThisClass::NetworkStateChangedDispatch_##name,true> name;

#endif

#endif // EHANDLE_H
