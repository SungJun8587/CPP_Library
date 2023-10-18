
//***************************************************************************
// Containers.h : interface for Override STL Container.
//
//***************************************************************************

#pragma once

#include "CustomAllocator.h"

#include <vector>
#include <list>
#include <forward_list>
#include <queue>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>

template<class _TMain, class _Ax = CCustomAllocator<_TMain> >
class CVector : public std::vector<_TMain, _Ax> {};

template<class _TMain, class _Ax = CCustomAllocator<_TMain> >
class CList : public std::list<_TMain, _Ax> {};

template<class _TMain, class _Ax = CCustomAllocator<_TMain> >
class CForwardList : public std::forward_list<_TMain, _Ax> {};

template<class _TMain, class _Ax = CCustomAllocator<_TMain> >
class CDeque : public std::deque<_TMain, _Ax> {};

template<class _TMain, class _Container = CDeque<_TMain> >
class CQueue : public std::queue<_TMain, _Container> {};

template<class _Kty, class _Pr = std::less<_Kty>, class _Alloc = CCustomAllocator<_Kty> >
class CSet : public std::set<_Kty, _Pr, _Alloc> {};

template<class _Kty, class _TMain, class _Pr = std::less<_Kty>, class _Alloc = CCustomAllocator<std::pair<const _Kty, _TMain> > >
class CMap : public std::map<_Kty, _TMain, _Pr, _Alloc> {};

template<class _Kty, class _TMain, class _Pr = std::less<_Kty>, class _Alloc = CCustomAllocator<std::pair<const _Kty, _TMain> > >
class CMultiMap : public std::multimap<_Kty, _TMain, _Pr, _Alloc> {};

template<class _Kty, class _Hasher = std::hash<_Kty>, class _Keyeq = std::equal_to<_Kty>, class _Alloc = CCustomAllocator<_Kty> >
class CUnorderedSet : public std::unordered_set<_Kty, _Hasher, _Keyeq, _Alloc> {};

template<class _Kty, class _TMain, class _Hasher = std::hash<_Kty>, class _Keyeq = std::equal_to<_Kty>, class _Alloc = CCustomAllocator<std::pair<const _Kty, _TMain> > >
class CUnorderedMap : public std::unordered_map<_Kty, _TMain, _Hasher, _Keyeq, _Alloc> {};
