
//***************************************************************************
// RedisParser.h : interface for the CRedisParser class.
//
//***************************************************************************

#ifndef UC_REDISPARSER_H
#define UC_REDISPARSER_H

#include <Redis/RedisProtocol.h>

//***************************************************************************
// @brief RESP 파싱 결과.
// @details [추가 — 버그 수정] 예전엔 TryParse()가 bool 하나만 반환해서,
//          "데이터가 아직 부족함"과 "RESP 형식 자체가 깨짐"을 호출부가
//          구분할 방법이 없었다 — CRedisClient::ProcessRecv()는 둘 다
//          "그냥 더 기다린다"로 처리하고 있었는데, 그러면 스트림이
//          영구적으로 손상돼도 연결을 절대 안 끊고 응답을 하염없이
//          기다리는 상태가 될 수 있었다(외부 리뷰로 발견). 이제 셋으로
//          나눠서, ProtocolError일 때만 호출부가 연결을 끊도록 한다.
//***************************************************************************
// @brief RESP 프로토콜 스트림 파싱을 담당하는 클래스
// @details 소켓으로부터 수신된 원시 바이트를 Feed()로 누적하고, TryParse()를
//          반복 호출하여 누적된 버퍼 안에 완성되어 있는 RESP 값들을 순서대로
//          꺼내온다. 데이터가 아직 부족해 값 하나를 완성할 수 없으면
//          TryParse()는 NeedMoreData를 반환하며 버퍼 상태를 그대로 보존하므로,
//          다음 Feed() 이후 다시 호출하면 이어서 파싱을 재개한다. 미완성
//          데이터의 보관 책임은 이 클래스(_pendingBuffer) 하나로 일원화되어
//          있으며, 호출자는 수신한 바이트를 그대로 Feed()에 전달하고 그
//          바이트에 대한 소유권(수신 버퍼 커서 이동 등)을 즉시 넘기면 된다.
//
//          [추가 — 방어 강화] 다음 세 가지 상한을 새로 도입했다(외부
//          리뷰로 발견 — 전부 지금까지 아무 제한이 없던 항목들):
//          - kMaxPendingBufferSize: Feed()가 계속 호출되는데 완전한 값을
//            한 번도 못 만드는 상황(예: 매우 큰 길이를 선언해놓고 데이터가
//            천천히 조금씩만 오는 경우)이 지속되면 _pendingBuffer가
//            무한정 커질 수 있었다 — 이 상한을 넘기면 Feed()가 false를
//            반환한다.
//          - kMaxArrayElements: 배열 원소 개수를 버퍼 크기로만 상한을
//            뒀었는데, 그 버퍼 크기 자체가 무제한이면 소용없었다 — 별도
//            고정 상한을 둔다.
//          - kMaxParseDepth: 중첩 배열(배열 안에 배열...)이 재귀 호출
//            깊이를 무한정 늘릴 수 있어 스택 오버플로 위험이 있었다.
//
// @code
// // 사용 예시:
// CRedisParser parser;
// if (!parser.Feed(pRecvBuffer, nRecvBytes))
// {
//     // 상한 초과 — 연결을 끊는 것이 안전하다
// }
//
// RedisValue result;
// for (;;)
// {
//     ERedisParseResult r = parser.TryParse(result);
//     if (r == ERedisParseResult::Success) { /* 처리 */ continue; }
//     if (r == ERedisParseResult::NeedMoreData) break; // 정상 — 다음 Feed() 기다림
//     /* ProtocolError */ break; // 연결을 끊어야 함
// }
// @endcode
//***************************************************************************
class CRedisParser
{
public:
	CRedisParser();
	~CRedisParser();

	//***************************************************************************
	// @brief 파서 내부 상태 및 미완성 버퍼를 초기화함
	//***************************************************************************
	void Reset();

	//***************************************************************************
	// @brief 수신된 원시 바이트를 내부 누적 버퍼에 추가함
	// @param pBuffer 수신 데이터 버퍼 포인터
	// @param nBytes 수신된 바이트 크기
	// @return true: 정상적으로 추가됨. false: kMaxPendingBufferSize 상한을
	//         초과함 — 호출부는 이 연결을 끊는 것이 안전하다(정상적인
	//         Redis 응답이라면 절대 이 상한을 넘지 않아야 하므로, 넘었다는
	//         것 자체가 스트림이 이미 신뢰할 수 없는 상태라는 신호다).
	//***************************************************************************
	bool Feed(const char* pBuffer, const int32 nBytes);

	//***************************************************************************
	// @brief 누적 버퍼에 이미 들어있는 데이터만으로 완전한 RESP 값 하나를 꺼냄
	// @param outValue 파싱된 결과 객체 (출력). Success일 때만 갱신되고,
	//        NeedMoreData/ProtocolError일 때는 건드리지 않는다 — 내부적으로
	//        임시 객체에 파싱한 뒤 완전히 성공했을 때만 여기로 옮기므로,
	//        배열을 파싱하다 중간에 데이터가 끊긴 경우에도 outValue가
	//        일부만 채워진 상태로 새어나가지 않는다.
	// @return Success/NeedMoreData/ProtocolError — enum 설명 참고. 실패
	//         시에도(NeedMoreData 포함) 버퍼 상태는 보존되어 다음 Feed()
	//         이후 이어서 재시도 가능하다.
	//***************************************************************************
	ERedisParseResult TryParse(RedisValue& outValue);

private:
	ERedisParseResult ParseValue(const char* pBuffer, const int32 nSize, int32& nOffset, RedisValue& outValue, const int32 nDepth);
	ERedisParseResult ReadLine(const char* pBuffer, const int32 nSize, int32& nOffset, std::string& strLine);

private:
	// 파서 한계값(kMaxPendingBufferSize/kMaxArrayElements/kMaxParseDepth)은 RedisDefine.h의 Redis 네임스페이스에 있다.

private:
	std::string _pendingBuffer;		// 미완성 패킷 버퍼링 (누적 버퍼의 유일한 소유자)

	// 직전 TryParse()가 NeedMoreData로 끝났을 때, 그 값을 완성하는 데 최소한 필요한 누적 바이트 수.
	// 큰 Bulk String이 여러 조각으로 도착하는 동안 매 조각마다 처음부터 다시 파싱(문자열 복사 포함)하는
	// 낭비를 막는다. 누적 버퍼는 Feed()로만 늘어나고 Success일 때만 줄어들어, 이 값 미만이면 재시도가 무의미하다.
	size_t _nMinBytesNeeded = 0;
};

#endif // ndef UC_REDISPARSER_H