
//***************************************************************************
// GcpServiceAccountAuth.h : interface for the CGcpAccessTokenProvider class.
//
//***************************************************************************

#ifndef UC_GCPSERVICEACCOUNTAUTH_H
#define UC_GCPSERVICEACCOUNTAUTH_H

#include <GcpService/Base64UrlUtil.h>
#include <GcpService/JsonFieldExtract.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>
#include <cstdint>

//***************************************************************************
// @struct GcpServiceAccountCredentials
// @brief GCP 서비스 계정 키 JSON 파일에서 뽑아낸, JWT 서명에 필요한 필드.
//***************************************************************************
struct GcpServiceAccountCredentials
{
	std::string clientEmail;   // "client_email" — JWT의 iss(발급자) 클레임에 사용
	std::string privateKeyPem; // "private_key" — RS256 서명에 사용할 PEM 개인키
	std::string tokenUri;      // "token_uri" (보통 "https://oauth2.googleapis.com/token")
};

//***************************************************************************
// @brief GCP 서비스 계정 키 JSON 파일 내용을 파싱합니다.
// @param json 서비스 계정 키 JSON 전체 텍스트
// @param out [OUT] 파싱 결과
// @return bool client_email/private_key/token_uri 세 필드를 전부 찾았는지 여부
// @details json_extract::FindString()은 중첩을 구분하지 않는 최소 추출기다 —
//          서비스 계정 키 JSON은 중첩 없는 평면 구조라 이 정도로 충분하다.
//***************************************************************************
inline bool ParseGcpServiceAccountJson(std::string_view json, GcpServiceAccountCredentials& out)
{
	bool ok = true;
	ok &= json_extract::FindString(json, "client_email", out.clientEmail);
	ok &= json_extract::FindString(json, "private_key", out.privateKeyPem);
	ok &= json_extract::FindString(json, "token_uri", out.tokenUri);
	return ok;
}

//***************************************************************************
// @brief RSA 개인키(PEM)로 데이터에 RS256(RSASSA-PKCS1-v1_5 + SHA-256) 서명을 만듭니다.
// @param privateKeyPem PEM 형식 RSA 개인키
// @param data 서명할 데이터
// @param outSignature [OUT] 서명 바이트 (raw, base64 인코딩 전)
// @return bool 성공 여부 (PEM 파싱 실패, 서명 알고리즘 오류 등이면 false)
//***************************************************************************
inline bool SignRs256(std::string_view privateKeyPem, std::string_view data, std::string& outSignature)
{
	BIO* bio = BIO_new_mem_buf(privateKeyPem.data(), static_cast<int>(privateKeyPem.size()));
	if( bio == nullptr )
		return false;

	EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
	BIO_free(bio);
	if( pkey == nullptr )
		return false;

	bool success = false;
	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	if( ctx != nullptr )
	{
		if( EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) == 1 )
		{
			size_t sigLen = 0;
			if( EVP_DigestSign(ctx, nullptr, &sigLen, reinterpret_cast<const unsigned char*>(data.data()), data.size()) == 1 )
			{
				outSignature.resize(sigLen);
				if( EVP_DigestSign(ctx, reinterpret_cast<unsigned char*>(outSignature.data()), &sigLen,
					reinterpret_cast<const unsigned char*>(data.data()), data.size()) == 1 )
				{
					outSignature.resize(sigLen); // 실제 서명 길이로 맞춤 (사전 조회값과 동일한 게 보통이지만 방어적으로)
					success = true;
				}
			}
		}
		EVP_MD_CTX_free(ctx);
	}

	EVP_PKEY_free(pkey);
	return success;
}

//***************************************************************************
// @brief 서명된 JWT(compact JWS, "header.claims.signature")를 만듭니다.
// @param creds 서비스 계정 자격증명
// @param scope OAuth2 스코프 (예: "https://www.googleapis.com/auth/firebase.messaging")
// @param nowUnixSeconds 현재 시각(UNIX epoch 초) — 순수 함수로 만들기 위해
//        시스템 시계 대신 인자로 받는다(테스트에서 고정 시각을 넣어 결정론적으로
//        검증 가능).
// @param outJwt [OUT] 완성된 JWT 문자열
// @return bool 성공 여부 (RS256 서명 실패 시 false)
// @details 이 함수는 순수 함수다(네트워크 I/O 없음) — SignRs256()만 내부에서
//          호출하고 그 외에는 문자열 조립뿐이라 타이머/스레드 없이 단위 테스트
//          가능하다. exp는 iat+3600(1시간, Google OAuth2가 허용하는 최대값)으로
//          고정한다.
//***************************************************************************
inline bool BuildSignedJwt(const GcpServiceAccountCredentials& creds, std::string_view scope, int64_t nowUnixSeconds, std::string& outJwt)
{
	// JWT 헤더 (RFC 7519) — RS256 고정이라 필드가 정적임, 문자열로 직접 조립.
	static const char* header = R"({"alg":"RS256","typ":"JWT"})";

	// 클레임: iss(발급자=서비스 계정 이메일), scope, aud(토큰 발급 엔드포인트),
	// iat(발급 시각), exp(만료 시각, 1시간 후) — Google의 JWT-bearer 플로우
	// (RFC 7523) 규격에 맞춘 필드 구성.
	int64_t exp = nowUnixSeconds + 3600;
	std::string claims = "{\"iss\":\"" + creds.clientEmail +
		"\",\"scope\":\"" + std::string(scope) +
		"\",\"aud\":\"" + creds.tokenUri +
		"\",\"iat\":" + std::to_string(nowUnixSeconds) +
		",\"exp\":" + std::to_string(exp) + "}";

	std::string signingInput = base64url::Encode(header) + "." + base64url::Encode(claims);

	std::string signature;
	if( !SignRs256(creds.privateKeyPem, signingInput, signature) )
		return false;

	outJwt = signingInput + "." + base64url::Encode(signature);
	return true;
}

//***************************************************************************
// @brief JWT assertion을 토큰 엔드포인트로 교환해 액세스 토큰을 받아오는 콜백.
// @details 실제 HTTP 호출(CHttpClient::PostForm 등)은 이 콜백 뒤에 캡슐화된다
//          — CGcpAccessTokenProvider 자신은 어떤 HTTP 클라이언트를 쓰는지
//          전혀 몰라도 된다(mock으로 이 파일을 단위 테스트할 수 있는 이유).
//***************************************************************************
using GcpTokenFetchFn = std::function<void(std::string jwtAssertion,
	std::function<void(bool success, std::string accessToken, int64_t expiresInSeconds)> onDone)>;

//***************************************************************************
// @class CGcpAccessTokenProvider
// @brief GCP 서비스 계정 자격증명으로 OAuth2 액세스 토큰을 발급/캐싱/자동 갱신한다.
//
// @details
// GetAccessToken()을 호출할 때마다 매번 새로 토큰을 발급받지 않는다 —
// 캐싱된 토큰이 아직 유효하면(만료 60초 전까지를 "유효"로 침) 즉시(동기적으로)
// 콜백을 부르고, 만료됐거나 처음 호출이면 JWT를 새로 만들어 서명하고
// GcpTokenFetchFn으로 교환한 뒤 캐싱, 대기 중이던 모든 콜백에 결과를
// 한꺼번에 통지한다(동시에 여러 GetAccessToken() 호출이 들어와도 토큰
// 교환 요청은 1번만 나감 — 아래 _pendingCallbacks 참고).
//
// @par 사용 방법 (Usage Example):
// @code{.cpp}
// // 1. GCP 서비스 계정 JSON 파싱
// std::string serviceAccountJson = LoadFile("service_account.json");
// GcpServiceAccountCredentials creds;
// if (!ParseGcpServiceAccountJson(serviceAccountJson, creds)) {
//     // 파싱 에러 처리
//     return;
// }
//
// // 2. HTTP 전송 콜백 작성 (GcpTokenFetchFn 구현)
// auto tokenFetcher = [](std::string jwtAssertion,
//                        std::function<void(bool, std::string, int64_t)> onDone) {
//     // HTTP POST 요청 구성 (oauth2.googleapis.com/token)
//     // Header: Content-Type: application/x-www-form-urlencoded
//     // Body: grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion={jwtAssertion}
//     
//     g_httpClient.PostForm("https://oauth2.googleapis.com/token",
//         {
//             {"grant_type", "urn:ietf:params:oauth:grant-type:jwt-bearer"},
//             {"assertion", jwtAssertion}
//         },
//         [onDone](bool httpSuccess, const std::string& responseJson) {
//             if (!httpSuccess) {
//                 onDone(false, "", 0);
//                 return;
//             }
//             // responseJson에서 access_token과 expires_in 파싱
//             std::string accessToken;
//             int64_t expiresIn = 0;
//             json_extract::FindString(responseJson, "access_token", accessToken);
//             json_extract::FindInt64(responseJson, "expires_in", expiresIn); // 보통 3600초
//             
//             onDone(!accessToken.empty(), accessToken, expiresIn);
//         });
// };
//
// // 3. CGcpAccessTokenProvider 객체 생성
// std::string scope = "https://www.googleapis.com/auth/firebase.messaging";
// CGcpAccessTokenProvider provider(creds, scope, tokenFetcher);
//
// // 4. 액세스 토큰 요청 (API 호출 시점마다 실행)
// provider.GetAccessToken([](bool success, std::string accessToken) {
//     if (!success) {
//         // 토큰 발급 실패 처리
//         return;
//     }
//     // 발급되거나 캐싱된 accessToken을 HTTP 요청 Header에 실어 보내기
//     // Authorization: Bearer {accessToken}
// });
// @endcode
//***************************************************************************
class CGcpAccessTokenProvider
{
public:
	//***************************************************************************
	// @brief CGcpAccessTokenProvider 생성자
	// @param creds 서비스 계정 자격증명
	// @param scope 요청할 OAuth2 스코프
	// @param fetchFn JWT를 액세스 토큰으로 교환하는 콜백 (실제 HTTP 호출 캡슐화)
	//***************************************************************************
	CGcpAccessTokenProvider(GcpServiceAccountCredentials creds, std::string scope, GcpTokenFetchFn fetchFn)
		: _creds(std::move(creds)), _scope(std::move(scope)), _fetchFn(std::move(fetchFn))
	{
	}

	//***************************************************************************
	// @brief 유효한 액세스 토큰을 얻습니다. 캐싱된 토큰이 유효하면 즉시(동기)
	//        콜백을 호출하고, 아니면 갱신을 시작한 뒤 완료 시 콜백을 호출합니다.
	// @param onDone success==false면 JWT 서명 실패 또는 토큰 교환 실패
	//***************************************************************************
	void GetAccessToken(std::function<void(bool success, std::string accessToken)> onDone)
	{
		int64_t now = NowUnixSeconds();
		bool haveCached = false;
		std::string cachedCopy;

		{
			std::lock_guard<std::mutex> guard(_lock);
			if( !_cachedToken.empty() && now < _expiryUnixSeconds - kRefreshMarginSeconds )
			{
				haveCached = true;
				cachedCopy = _cachedToken;
			}
			else
			{
				_pendingCallbacks.push_back(std::move(onDone));
				if( _pendingCallbacks.size() > 1 )
					return; // 이미 갱신 요청이 진행 중 — 이번 호출은 대기열에만 등록
			}
		}

		// 콜백은 항상 락 밖에서 호출한다 — 콜백 안에서 GetAccessToken()이 재진입
		// 호출되는 경우(예: 실패 시 바로 재시도) 자기 자신을 락으로 교착시키지
		// 않기 위함.
		if( haveCached )
		{
			onDone(true, cachedCopy);
			return;
		}

		std::string jwt;
		if( !BuildSignedJwt(_creds, _scope, now, jwt) )
		{
			NotifyAllPending(false, "");
			return;
		}

		_fetchFn(jwt, [this](bool success, std::string accessToken, int64_t expiresInSeconds)
			{
				if( success )
				{
					std::lock_guard<std::mutex> guard(_lock);
					_cachedToken = accessToken;
					_expiryUnixSeconds = NowUnixSeconds() + expiresInSeconds;
				}
				NotifyAllPending(success, success ? accessToken : std::string());
			});
	}

private:
	//***************************************************************************
	// @brief 대기 중이던 모든 GetAccessToken() 콜백에 결과를 통지하고 대기열을 비웁니다.
	// @param success 토큰 발급/갱신 성공 여부
	// @param accessToken 발급된 액세스 토큰 문자열
	//***************************************************************************
	void NotifyAllPending(bool success, std::string accessToken)
	{
		std::vector<std::function<void(bool, std::string)>> callbacks;
		{
			std::lock_guard<std::mutex> guard(_lock);
			callbacks.swap(_pendingCallbacks);
		}
		for( auto& cb : callbacks )
			cb(success, accessToken);
	}

	//***************************************************************************
	// @brief 현재 시각을 UNIX epoch 초(seconds) 단위로 구합니다.
	// @return int64_t UNIX epoch 초
	//***************************************************************************
	static int64_t NowUnixSeconds()
	{
		return std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}

private:
	static constexpr int64_t kRefreshMarginSeconds = 60; // 만료 60초 전부터는 미리 갱신 대상으로 취급

	GcpServiceAccountCredentials _creds; // 서비스 계정 자격증명
	std::string _scope;                  // 요청 OAuth2 스코프
	GcpTokenFetchFn _fetchFn;            // JWT -> 액세스 토큰 교환 콜백

	std::mutex _lock;                                                       // 아래 캐시/대기열 보호
	std::string _cachedToken;                                               // 캐싱된 액세스 토큰
	int64_t _expiryUnixSeconds = 0;                                         // 캐싱된 토큰의 만료 시각(UNIX epoch 초)
	std::vector<std::function<void(bool, std::string)>> _pendingCallbacks; // 갱신 진행 중 쌓인 대기 콜백
};

#endif // ndef UC_GCPSERVICEACCOUNTAUTH_H