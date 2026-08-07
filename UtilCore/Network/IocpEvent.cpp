//***************************************************************************
// IocpEvent.cpp: implementation of the CIocpEvent class.
//
//***************************************************************************

#include "pch.h"
#include "IocpEvent.h"

//***************************************************************************
// @brief IocpEvent 생성자
// @param type 이벤트 타입
//***************************************************************************
CIocpEvent::CIocpEvent(EventType type) : eventType(type)
{
	Init();
}

//***************************************************************************
// @brief OVERLAPPED 구조체 내부 필드 초기화
//***************************************************************************
void CIocpEvent::Init()
{
	OVERLAPPED::hEvent = 0;
	OVERLAPPED::Internal = 0;
	OVERLAPPED::InternalHigh = 0;
	OVERLAPPED::Offset = 0;
	OVERLAPPED::OffsetHigh = 0;
}