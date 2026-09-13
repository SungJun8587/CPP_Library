
//***************************************************************************
// PacketDispatcher.h : interface for the CPacketDispatcher class.
//
//***************************************************************************

#ifndef UC_PACKETDISPATCHER_H
#define UC_PACKETDISPATCHER_H

#include "Packet.h"

#include <unordered_map>
#include <cstddef>

//***************************************************************************
// @brief 범용 패킷 핸들러 함수 포인터.
// @details context는 이 패킷을 처리할 세션/객체(예: CChatSession*)를
//          void*로 받는다 — 이 파일은 그 실제 타입을 전혀 모른다. 각
//          서버는 타입 안전한 얇은 어댑터(ChatPacketDispatcher.h 등)를
//          둬서 이 void*를 자신의 세션 타입으로 감춘다.
//***************************************************************************
using PacketHandler = void(*)(void* context, const PacketHeader* header);

//***************************************************************************
// @brief 패킷 디스패처 처리 결과. 프로토콜/서버 종류와 무관하게 공통이다.
//***************************************************************************
enum class EPacketDispatchResult
{
	Handled,		// 정상 처리됨
	UnknownType,	// 등록되지 않은 패킷 타입
	SizeViolation,	// header->size가 minSize보다 작거나, bufferSize를 넘어감(프로토콜 위반)
};

//***************************************************************************
// @brief 네트워크 프로그램 공용 패킷 디스패처.
// @details 패킷 타입(uint16) -> 핸들러 매핑만 다루며, 특정 Session/Protocol/
// PacketType enum을 전혀 모른다 — 그래서 Chat/Game/Login/Gateway 서버가
// 전부 이 하나의 구현(과 하나의 등록 테이블)을 공유할 수 있다.
//
// 정적 초기화 단계에서 각 핸들러 모듈이 Register()로 자신을 등록하고,
// 그 이후(서버가 실제로 패킷을 받기 시작한 시점)엔 테이블이 읽기 전용
// 이라는 전제 하에 Register()/Dispatch() 어디에도 락을 쓰지 않는다.
//
// [정적 라이브러리 링킹 주의] REGISTER_PACKET_HANDLER로 자체 등록하는
// .cpp가 정적 라이브러리(.lib/.a)로 묶여 링크되는 구조라면, 아무도 그
// 심볼을 직접 참조하지 않으므로(등록 부작용만 있을 뿐 호출되는 함수가
// 없음) 링커가 해당 오브젝트 전체를 통째로 버릴 수 있다 — 그 경우
// /WHOLEARCHIVE(MSVC) 또는 --whole-archive(GCC/Clang)로 강제 포함시켜야
// 한다. .exe에 직접 컴파일해 넣는 구성이면 문제되지 않는다.
//***************************************************************************
class CPacketDispatcher
{
public:
	static void Register(uint16 type, uint16 minSize, PacketHandler handler);
	static EPacketDispatchResult Dispatch(void* context, const PacketHeader* header, size_t bufferSize);

private:
	//***************************************************************************
	// @brief 패킷 타입별 최소 크기 및 핸들러를 관리하는 엔트리 구조체.
	//***************************************************************************
	struct Entry
	{
		uint16			minSize; // 패킷의 최소 허용 크기
		PacketHandler	handler; // 패킷 처리 핸들러 함수
	};

	static std::unordered_map<uint16, Entry>& GetTable();
};

//***************************************************************************
// @brief 정적 패킷 핸들러 등록 RAII 객체.
// @details 네임스페이스(파일) 스코프에 static 인스턴스를 두면 프로그램 시작 시
//          생성자가 실행되며 자동 등록된다.
//***************************************************************************
struct PacketRegistrar
{
	//***************************************************************************
	// @brief 생성 시점에 패킷 핸들러를 디스패처에 등록합니다.
	// @param type 등록할 패킷 타입
	// @param minSize 패킷의 최소 허용 크기
	// @param handler 패킷을 처리할 콜백 함수 포인터
	//***************************************************************************
	PacketRegistrar(uint16 type, uint16 minSize, PacketHandler handler)
	{
		CPacketDispatcher::Register(type, minSize, handler);
	}
};

//***************************************************************************
// @brief 패킷 핸들러 등록 매크로(범용).
// @details context가 이미 void*라서 세션 타입을 모르는 핸들러(드묾)를 직접
//          등록할 때 쓴다 — 보통은 각 서버가 REGISTER_CHAT_PACKET_HANDLER
//          같은 타입 안전한 자체 매크로를 대신 쓴다(ChatPacketDispatcher.h 참고).
//          __LINE__을 변수명에 섞어, 한 파일에 등록이 여러 개 있어도 이름
//          충돌이 나지 않게 한다.
//***************************************************************************
#define UC_PACKET_REGISTRAR_NAME_INNER(line) sPacketRegistrar_##line
#define UC_PACKET_REGISTRAR_NAME(line) UC_PACKET_REGISTRAR_NAME_INNER(line)

#define REGISTER_PACKET_HANDLER(TypeValue, PacketStruct, HandlerFunc) \
	static PacketRegistrar UC_PACKET_REGISTRAR_NAME(__LINE__)( \
		static_cast<uint16>(TypeValue), sizeof(PacketStruct), &HandlerFunc)

#endif // ndef UC_PACKETDISPATCHER_H