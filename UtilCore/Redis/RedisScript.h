
//***************************************************************************
// RedisScript.h : interface for the CRedisScript class.
//
//***************************************************************************

#ifndef UC_REDISSCRIPT_H
#define UC_REDISSCRIPT_H

#include <Redis/RedisService.h>

#include <memory>
#include <string>

//***************************************************************************
// @brief Lua 스크립트를 EVALSHA로 실행하는 공용 헬퍼 (스크립트 본문 전송 생략)
// @details EVAL은 매 호출마다 스크립트 본문 전체를 보내고 Redis도 그때마다 본문의 SHA1을
//          계산한다. 이 클래스는 본문의 SHA1을 생성 시점에 한 번 구해 두고 EVALSHA로 보낸다.
//          Redis가 스크립트 캐시를 갖고 있지 않으면(처음 실행, 재시작, SCRIPT FLUSH) 서버가
//          NOSCRIPT 에러를 돌려주고, 이때만 EVAL로 한 번 다시 보내 스크립트를 적재한다.
//          그 뒤의 호출은 다시 EVALSHA로 나간다. SHA는 본문만으로 정해지므로 SCRIPT LOAD를
//          먼저 보내거나 SHA를 보관·동기화할 필요가 없다.
//
// @details NOSCRIPT가 아닌 에러(연결 오류 포함)는 그대로 콜백에 전달된다. 재시도가 필요하면
//          SendCommandWithRetry()처럼 호출부가 처리한다.
//
// @details 스레드 계약: 생성 후에는 불변이라 여러 스레드에서 동시에 Run()해도 안전하다.
//          콜백은 CRedisService와 같이 생성자에 넘긴 JobQueue를 처리하는 스레드에서 호출된다.
//          Redis Cluster에서는 스크립트가 쓰는 모든 키를 vecKeys로 선언해야 한다(ARGV로 키를
//          넘기는 스크립트는 Cluster에서 쓸 수 없다).
//
// @code
//	static const CRedisScript kConsume("local v = redis.call('GET', KEYS[1]) ... return v");
//	kConsume.Run(*pRedisService, {"UploadToken:abc"}, {}, [](const RedisValue& res) { ... });
// @endcode
//***************************************************************************
class CRedisScript
{
public:
	//***************************************************************************
	// @param strBody Lua 스크립트 본문. SHA1은 이 문자열 그대로(바이트 단위) 계산된다
	//***************************************************************************
	explicit CRedisScript(std::string strBody);

	//***************************************************************************
	// @brief 스크립트 본문을 반환함
	//***************************************************************************
	const std::string& GetBody() const { return *_pBody; }

	//***************************************************************************
	// @brief 본문의 SHA1(소문자 16진 40자)을 반환함 — Redis가 쓰는 스크립트 식별자와 같다
	//***************************************************************************
	const std::string& GetSha() const { return _strSha; }

	//***************************************************************************
	// @brief 기본 노드에서 스크립트를 실행함
	// @param vecKeys KEYS 배열
	// @param vecArgv ARGV 배열
	// @param fnCallback 결과 콜백(생략 가능)
	// @return 전송 성공 여부 (false면 콜백은 호출되지 않는다 — CRedisService::SendCommand와 같음)
	//***************************************************************************
	bool Run(CRedisService& redisService, const CVector<std::string>& vecKeys, const CVector<std::string>& vecArgv, RedisCallback fnCallback = nullptr) const;

	//***************************************************************************
	// @brief 지정한 노드에서 스크립트를 실행함
	//***************************************************************************
	bool Run(CRedisService& redisService, const int16 nNodeId, const CVector<std::string>& vecKeys, const CVector<std::string>& vecArgv, RedisCallback fnCallback = nullptr) const;

	//***************************************************************************
	// @brief 문자열의 SHA1을 소문자 16진 문자열로 계산함(테스트·진단용으로 공개)
	//***************************************************************************
	static std::string ComputeSha1Hex(const std::string& strData);

private:
	std::shared_ptr<const std::string>	_pBody;		// 폴백(EVAL)용 본문. 콜백이 공유해 들고 있어 스크립트 객체보다 오래 살아도 안전하다
	std::string							_strSha;
};

#endif // ndef UC_REDISSCRIPT_H