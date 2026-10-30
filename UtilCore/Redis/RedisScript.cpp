
//***************************************************************************
// RedisScript.cpp: implementation of the CRedisScript class.
//
//***************************************************************************

#include "pch.h"
#include <Redis/RedisHelper.h>
#include "RedisScript.h"

#include <cstdint>

namespace
{
	inline uint32_t RotateLeft(const uint32_t nValue, const int32 nBits)
	{
		return (nValue << nBits) | (nValue >> (32 - nBits));
	}

	bool StartsWith(const std::string& str, const char* pszPrefix)
	{
		return str.compare(0, std::strlen(pszPrefix), pszPrefix) == 0;
	}
}

//***************************************************************************
// @brief 본문을 보관하고 SHA1을 한 번 계산함
//***************************************************************************
CRedisScript::CRedisScript(std::string strBody)
	: _pBody(std::make_shared<const std::string>(std::move(strBody)))
	, _strSha(ComputeSha1Hex(*_pBody))
{
}

//***************************************************************************
// @brief SHA-1(RFC 3174)을 계산해 소문자 16진 문자열로 반환함
// @details Redis가 EVALSHA/SCRIPT LOAD에서 쓰는 식별자와 같은 값이다. 이 클래스만을 위한
//          작은 구현이라 외부 암호 라이브러리(OpenSSL)에 의존하지 않는다 — 보안 용도가 아니다.
//***************************************************************************
std::string CRedisScript::ComputeSha1Hex(const std::string& strData)
{
	uint32_t h[5] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };

	std::string msg = strData;
	const uint64_t nBitLength = static_cast<uint64_t>(strData.size()) * 8u;
	msg.push_back(static_cast<char>(0x80));
	while( msg.size() % 64 != 56 )
		msg.push_back('\0');
	for( int32 i = 7; i >= 0; --i )
		msg.push_back(static_cast<char>((nBitLength >> (i * 8)) & 0xFFu));

	for( size_t nOffset = 0; nOffset < msg.size(); nOffset += 64 )
	{
		uint32_t w[80];
		for( int32 i = 0; i < 16; ++i )
		{
			const unsigned char* p = reinterpret_cast<const unsigned char*>(msg.data()) + nOffset + i * 4;
			w[i] = (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
		}
		for( int32 i = 16; i < 80; ++i )
			w[i] = RotateLeft(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

		uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
		for( int32 i = 0; i < 80; ++i )
		{
			uint32_t f = 0, k = 0;
			if( i < 20 ) { f = (b & c) | (~b & d);			k = 0x5A827999u; }
			else if( i < 40 ) { f = b ^ c ^ d;					k = 0x6ED9EBA1u; }
			else if( i < 60 ) { f = (b & c) | (b & d) | (c & d);	k = 0x8F1BBCDCu; }
			else { f = b ^ c ^ d;					k = 0xCA62C1D6u; }

			const uint32_t nTemp = RotateLeft(a, 5) + f + e + k + w[i];
			e = d;
			d = c;
			c = RotateLeft(b, 30);
			b = a;
			a = nTemp;
		}

		h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
	}

	static const char kHexDigits[] = "0123456789abcdef";
	std::string strHex;
	strHex.reserve(40);
	for( int32 i = 0; i < 5; ++i )
	{
		for( int32 nShift = 28; nShift >= 0; nShift -= 4 )
			strHex.push_back(kHexDigits[(h[i] >> nShift) & 0xFu]);
	}

	return strHex;
}

//***************************************************************************
// @brief 기본 노드에서 스크립트를 실행함
//***************************************************************************
bool CRedisScript::Run(CRedisService& redisService, const CVector<std::string>& vecKeys, const CVector<std::string>& vecArgv, RedisCallback fnCallback) const
{
	return Run(redisService, redisService.GetDefaultNodeId(), vecKeys, vecArgv, std::move(fnCallback));
}

//***************************************************************************
// @brief 지정한 노드에서 EVALSHA로 실행하고, NOSCRIPT이면 EVAL로 한 번 다시 보냄
// @details 보낸 EVALSHA 인자 벡터를 shared_ptr로 보관해 콜백이 공유한다 — 폴백 때 키/인자를
//          다시 모으지 않고, 첫 두 칸(명령, SHA)만 바꿔 EVAL 인자를 만든다. 정상 경로에서는
//          shared_ptr 하나만 복사된다.
//***************************************************************************
bool CRedisScript::Run(CRedisService& redisService, const int16 nNodeId, const CVector<std::string>& vecKeys, const CVector<std::string>& vecArgv, RedisCallback fnCallback) const
{
	auto pArgs = std::make_shared<CVector<std::string>>();
	pArgs->reserve(3 + vecKeys.size() + vecArgv.size());
	pArgs->push_back("EVALSHA");
	pArgs->push_back(_strSha);
	pArgs->push_back(std::to_string(vecKeys.size()));
	pArgs->insert(pArgs->end(), vecKeys.begin(), vecKeys.end());
	pArgs->insert(pArgs->end(), vecArgv.begin(), vecArgv.end());

	CRedisService* pService = &redisService;
	std::shared_ptr<const std::string> pBody = _pBody;

	return redisService.SendCommand(nNodeId, *pArgs,
		[pService, nNodeId, pArgs, pBody, fnUserCallback = std::move(fnCallback)](const RedisValue& res) mutable
		{
			if( res.eType == ERedisType::Error && StartsWith(res.strVal, "NOSCRIPT") )
			{
				// 서버에 스크립트가 없다 — EVAL로 보내면 서버가 스크립트를 캐시에 적재하므로 다음부터는 EVALSHA가 통한다.
				CVector<std::string> evalArgs(*pArgs);
				evalArgs[0] = "EVAL";
				evalArgs[1] = *pBody;

				// SendCommand가 false면 콜백이 호출되지 않으므로, 복사본으로 보내고 실패 시 원본으로 직접 알린다.
				RedisCallback fnForEval = fnUserCallback;
				if( pService->SendCommand(nNodeId, evalArgs, std::move(fnForEval)) )
					return;

				if( fnUserCallback )
				{
					fnUserCallback(Redis::MakeError(Redis::kErrSendRejected));
				}
				return;
			}

			if( fnUserCallback )
				fnUserCallback(res);
		});
}