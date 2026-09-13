
//***************************************************************************
// PacketDispatcher.cpp : implementation of the CPacketDispatcher class.
//
//***************************************************************************

#include "pch.h"
#include "PacketDispatcher.h"

//***************************************************************************
// @brief 등록 테이블을 보관하는 함수-로컬 static (Meyer's singleton).
// @return 등록된 핸들러 테이블 참조
//***************************************************************************
std::unordered_map<uint16, CPacketDispatcher::Entry>& CPacketDispatcher::GetTable()
{
	static std::unordered_map<uint16, Entry> table;
	return table;
}

//***************************************************************************
// @brief 패킷 핸들러를 등록합니다.
// @param type 등록할 패킷 타입
// @param minSize 패킷의 최소 허용 크기
// @param handler 패킷을 처리할 콜백 함수
//***************************************************************************
void CPacketDispatcher::Register(uint16 type, uint16 minSize, PacketHandler handler)
{
	ASSERT_CRASH(handler != nullptr);

	auto& table = GetTable();

	// 같은 타입이 중복 등록되면(복붙 실수 등) 조용히 덮어쓰지 않고 바로
	// 알아챌 수 있게 크래시시킨다 — 정적 초기화 시점(프로그램 시작 전)이라
	// 여기서 멈추는 게 런타임에 엉뚱한 핸들러가 호출되는 것보다 안전하다.
	ASSERT_CRASH(table.find(type) == table.end());

	table.emplace(type, Entry{ minSize, handler });
}

//***************************************************************************
// @brief 패킷을 등록된 핸들러로 디스패치합니다.
// @param context 전달할 컨텍스트 포인터
// @param header 수신된 패킷 헤더 포인터
// @param bufferSize 실제 수신된 버퍼의 크기
// @return EPacketDispatchResult 디스패치 처리 결과
//***************************************************************************
EPacketDispatchResult CPacketDispatcher::Dispatch(void* context, const PacketHeader* header, size_t bufferSize)
{
	ASSERT_CRASH(header != nullptr);

	// header->size가 실제로 받아둔 버퍼 범위를 넘어서면, 호출부가 프레이밍
	// 검증을 빠뜨렸거나 실수했다는 뜻이다 — 공용 컴포넌트인 이상 호출부를
	// 전적으로 믿지 않고 여기서 한 번 더 막는다.
	if( bufferSize < sizeof(PacketHeader) || header->size > bufferSize )
		return EPacketDispatchResult::SizeViolation;

	auto& table = GetTable();

	auto it = table.find(header->type);
	if( it == table.end() )
		return EPacketDispatchResult::UnknownType;

	if( header->size < it->second.minSize )
		return EPacketDispatchResult::SizeViolation;

	it->second.handler(context, header);
	return EPacketDispatchResult::Handled;
}