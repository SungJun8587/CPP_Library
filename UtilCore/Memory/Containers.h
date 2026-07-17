
//***************************************************************************
// Containers.h : interface for Override STL Container.
//
//***************************************************************************

#ifndef __CONTAINERS_H__
#define __CONTAINERS_H__

#pragma once

#ifndef	__ALLOCATOR_H__
#include <Memory/Allocator.h>
#endif

#include <vector>
#include <list>
#include <forward_list>
#include <queue>
#include <stack>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>

//***************************************************************************
// 시퀀스 컨테이너 (Sequence Containers)
template<class _TMain, class _Ax = StlAllocator<_TMain>>
class CVector : public std::vector<_TMain, _Ax> {
public:
	using std::vector<_TMain, _Ax>::vector;
};

template<class _TMain, class _Ax = StlAllocator<_TMain>>
class CList : public std::list<_TMain, _Ax> {
public:
	using std::list<_TMain, _Ax>::list;
};

template<class _TMain, class _Ax = StlAllocator<_TMain>>
class CForwardList : public std::forward_list<_TMain, _Ax> {
public:
	using std::forward_list<_TMain, _Ax>::forward_list;
};

template<class _TMain, class _Ax = StlAllocator<_TMain>>
class CDeque : public std::deque<_TMain, _Ax> {
public:
	using std::deque<_TMain, _Ax>::deque;
};

//***************************************************************************
// 컨테이너 어댑터 (Container Adapters)
template<class _TMain, class _Container = CDeque<_TMain>>
class CQueue : public std::queue<_TMain, _Container> {
public:
	using std::queue<_TMain, _Container>::queue;
};

template<class _TMain, class _Container = CVector<_TMain>, class _Pr = std::less<typename _Container::value_type>>
using CPriorityQueue = std::priority_queue<_TMain, _Container, _Pr>; // 💡 using 별칭은 생성자 상속이 필요 없습니다.

template<class _TMain, class _Container = CDeque<_TMain>>
class CStack : public std::stack<_TMain, _Container> {
public:
	using std::stack<_TMain, _Container>::stack;
};

//***************************************************************************
// 연관 컨테이너 (Associative Containers)
template<class _Kty, class _Pr = std::less<_Kty>, class _Alloc = StlAllocator<_Kty>>
class CSet : public std::set<_Kty, _Pr, _Alloc> {
public:
	using std::set<_Kty, _Pr, _Alloc>::set;
};

template<class _Kty, class _TMain, class _Pr = std::less<_Kty>, class _Alloc = StlAllocator<std::pair<const _Kty, _TMain>>>
class CMap : public std::map<_Kty, _TMain, _Pr, _Alloc> {
public:
	using std::map<_Kty, _TMain, _Pr, _Alloc>::map;
};

template<class _Kty, class _TMain, class _Pr = std::less<_Kty>, class _Alloc = StlAllocator<std::pair<const _Kty, _TMain>>>
class CMultiMap : public std::multimap<_Kty, _TMain, _Pr, _Alloc> {
public:
	using std::multimap<_Kty, _TMain, _Pr, _Alloc>::multimap;
};

//***************************************************************************
// 정렬되지 않은 연관 컨테이너 (Unordered Associative Containers)
template<class _Kty, class _Hasher = std::hash<_Kty>, class _Keyeq = std::equal_to<_Kty>, class _Alloc = StlAllocator<_Kty>>
class CUnorderedSet : public std::unordered_set<_Kty, _Hasher, _Keyeq, _Alloc> {
public:
	using std::unordered_set<_Kty, _Hasher, _Keyeq, _Alloc>::unordered_set;
};

template<class _Kty, class _TMain, class _Hasher = std::hash<_Kty>, class _Keyeq = std::equal_to<_Kty>, class _Alloc = StlAllocator<std::pair<const _Kty, _TMain>>>
class CUnorderedMap : public std::unordered_map<_Kty, _TMain, _Hasher, _Keyeq, _Alloc> {
public:
	using std::unordered_map<_Kty, _TMain, _Hasher, _Keyeq, _Alloc>::unordered_map;
};

#endif // ndef __CONTAINERS_H__