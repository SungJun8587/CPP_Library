
//***************************************************************************
// RioServiceHelper.h : interface for the CRioServiceHelper class.
//
//***************************************************************************

#ifndef UC_RIOSERVICEHELPER_H
#define UC_RIOSERVICEHELPER_H

#include <Network/RIO/RioCommon.h>
#include <Network/RIO/RioCore.h>
#include <Network/RIO/RioEventPool.h>
#include <Network/RIO/RioBuffer.h>

//***************************************************************************
// @class CRioServiceHelper
// @brief CRioServerService / CRioClientService가 공통으로 쓰는 RIO 엔진 시작/정지 절차
//
// @details
//      두 서비스는 이벤트 풀 초기화 → CRioCore 초기화 → 워커 구동 → 글로벌 수신
//      버퍼 초기화로 시작하고, RequestStop()/Shutdown() → 버퍼 해제 → 이벤트 풀 해제
//      순서로 정지합니다. 이 순서를 한 곳에 두어 서버/클라이언트가 같은 경로를
//      쓰게 하고, 시작 도중 실패하면 그때까지 만든 자원을 역순으로 되돌립니다.
//
//      정지 순서를 멤버 소멸 순서에 맡기지 않는 이유: 서비스 멤버 선언 순서상
//      _globalRecvBuffer/_eventPool이 _rioCore보다 먼저 파괴되는데, 아직 drain되지
//      않은 outstanding I/O가 남은 채 CRioBuffer/CRioEventPool 소멸자가 실행되면
//      assert/terminate가 발생하기 때문입니다.
//***************************************************************************
class CRioServiceHelper final
{
public:
	CRioServiceHelper() = delete;
	~CRioServiceHelper() = delete;

	CRioServiceHelper(const CRioServiceHelper&) = delete;
	CRioServiceHelper& operator=(const CRioServiceHelper&) = delete;

public:
	//***************************************************************************
	// @brief 이벤트 풀, RIO 코어, 워커 스레드 그룹, 글로벌 수신 버퍼를 순서대로 준비합니다.
	// @param core 초기화할 RIO 코어
	// @param eventPool 코어가 사용할 이벤트 풀 (서비스 소유)
	// @param globalRecvBuffer 생성할 글로벌 수신 버퍼가 담길 참조
	// @param cqIdentifier CQ 식별자 (Rio::kServerCqIdentifier / Rio::kClientCqIdentifier)
	// @param workerThreadCount 워커 스레드 수 (0이면 CRioCore가 자동 산정)
	// @return bool 모두 성공하면 true. 실패하면 그때까지 만든 자원을 정리한 뒤 false.
	//***************************************************************************
	static bool StartCore(CRioCore& core, CRioEventPool& eventPool, CRioBufferRef& globalRecvBuffer, ULONG_PTR cqIdentifier, uint32 workerThreadCount);

	//***************************************************************************
	// @brief RIO 코어를 정지하고 글로벌 수신 버퍼와 이벤트 풀을 해제합니다.
	// @details Shutdown()이 outstanding I/O를 모두 drain하지 못하면(타임아웃/CQ 오염)
	//          그 I/O가 버퍼와 이벤트를 아직 참조하므로 해제하지 않고 남겨 둡니다.
	// @return bool drain까지 정상 완료되어 자원을 해제했으면 true
	//***************************************************************************
	static bool StopCore(const CRioCoreRef& core, CRioBufferRef& globalRecvBuffer, CRioEventPool& eventPool);
};

#endif // ndef UC_RIOSERVICEHELPER_H