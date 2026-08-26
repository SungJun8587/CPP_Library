
//***************************************************************************
// GcpAuthHttpClient.h : Helper to create GcpTokenFetchFn using CHttpClient
//
//***************************************************************************

#ifndef __GCPAUTHHTTPCLIENT_H__
#define __GCPSERVICEACCOUNTAUTH_H__

#ifndef	__JSONFIELDEXTRACT_H__
#include <GcpService/JsonFieldExtract.h>
#endif

#ifndef	__GCPSERVICEACCOUNTAUTH_H__
#include <GcpService/GcpServiceAccountAuth.h>
#endif

#ifndef	__HTTPCLIENT_H__
#include <Network/HTTP/HttpClient.h>
#endif

namespace gcp_auth
{
	//***************************************************************************
		// @brief CHttpClient 인스턴스를 사용하여 OAuth2 토큰 교환용 GcpTokenFetchFn을 생성합니다.
		// @param httpClient CHttpClient의 포인터 (nullptr 전달 시 실패 처리)
		// @return GcpTokenFetchFn CGcpAccessTokenProvider 생성 시 전달할 콜백 함수
		// @details Google OAuth2 엔드포인트("https://oauth2.googleapis.com/token")로
		//          JWT assertion을 POST(x-www-form-urlencoded)로 전송하고, 
		//          응답받은 JSON에서 access_token과 expires_in을 추출하여 반환합니다.
		//***************************************************************************
	inline GcpTokenFetchFn CreateTokenFetcher(CHttpClient* httpClient)
	{
		return [httpClient](std::string jwtAssertion, std::function<void(bool success, std::string accessToken, int64_t expiresInSeconds)> onDone)
			{
				if( httpClient == nullptr )
				{
					onDone(false, "", 0);
					return;
				}

				// Google OAuth2 토큰 교환 요청 (RFC 7523 JWT-bearer profile)
				httpClient->PostForm("https://oauth2.googleapis.com/token",
					{
						{"grant_type", "urn:ietf:params:oauth:grant-type:jwt-bearer"},
						{"assertion", jwtAssertion}
					},
					[onDone](HttpResponse resp)
					{
						// HTTP 통신 실패 또는 Status Code가 200 OK가 아닌 경우
						if( !resp.success || resp.statusCode != 200 )
						{
							// TODO: 필요 시 내부 로거를 통해 resp.statusCode 및 resp.body 출력
							// 예: LOG_ERROR("OAuth2 Token Exchange Failed: code=%d, body=%s", resp.statusCode, resp.body.c_str());
							onDone(false, "", 0);
							return;
						}

						std::string token;
						int64_t expiresIn = 0;

						// JSON 응답에서 access_token 파싱
						if( !json_extract::FindString(resp.body, "access_token", token) )
						{
							onDone(false, "", 0);
							return;
						}

						// expires_in (보통 3600초) 파싱
						json_extract::FindInt(resp.body, "expires_in", expiresIn);

						onDone(true, token, expiresIn);
					});
			};
	}
}

#endif // ndef __GCPAUTHHTTPCLIENT_H__