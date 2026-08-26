
//***************************************************************************
// TlsFilter.h : interface for the CTlsFilter class.
//
//***************************************************************************

#ifndef __TLSFILTER_H__
#define __TLSFILTER_H__

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <functional>
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include <algorithm>

#pragma comment(lib, LIB_NAME("libssl"))
#pragma comment(lib, LIB_NAME("libcrypto"))

//***************************************************************************
// @brief 실제 소켓으로 암호문(ciphertext)을 내보내는 콜백. 세션의 Send()를
//        그대로 감싸서 넘기면 된다. 시그니처는 CSession::Send()와 동일하게
//        uint16_t 청크 단위 — CTlsFilter 내부에서 65535바이트 단위로 자동
//        분할해 호출한다(CHttpClientCore::BeginRequest()와 동일한 제약/패턴).
//***************************************************************************
using TlsRawSendFn = std::function<bool(const void*, uint16_t)>;

// 복호화된 평문이 도착했을 때 통지. data는 이 콜백이 끝나면 무효화되는 임시
// 버퍼를 가리키므로, 호출부가 더 오래 보관해야 한다면 콜백 안에서 복사해야 함.
using TlsPlaintextRecvHandler = std::function<void(const char* data, size_t len)>;

// 핸드셰이크 완료(성공) 또는 실패를 통지. success==false면 호출부는 세션을
// 폐기해야 한다(인증서 검증 실패, 프로토콜 오류 등 복구 불가능한 상태).
using TlsHandshakeCompleteHandler = std::function<void(bool success)>;

//***************************************************************************
// @brief 기본 클라이언트 SSL_CTX를 생성합니다 (TLS 1.2 이상, 피어 인증서 검증
//        활성화, OS 기본 신뢰 저장소 사용).
// @return SSL_CTX* 생성된 컨텍스트(실패 시 nullptr). SSL_CTX는 생성 비용이
//         커서 세션마다 새로 만들지 않고, 이 함수로 한 번 만든 뒤 여러
//         CTlsFilter 인스턴스(=여러 커넥션)가 공유하는 게 정석이다 —
//         CIocpCoreRef/CRioCoreRef를 여러 세션이 공유하는 것과 같은 패턴.
//         호출부가 소유권을 가지며, 다 쓴 뒤 SSL_CTX_free()로 해제해야 한다.
//***************************************************************************
inline SSL_CTX* CreateDefaultClientSslCtx()
{
	SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
	if( ctx == nullptr )
		return nullptr;

	SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

	if( SSL_CTX_set_default_verify_paths(ctx) != 1 )
	{
		SSL_CTX_free(ctx);
		return nullptr;
	}

	return ctx;
}

//***************************************************************************
// @class CTlsFilter
// @brief IOCP/RIO 비동기 I/O 위에 TLS를 얹기 위한 엔진 비의존 필터
//
// @details
//      OpenSSL의 SSL_read/SSL_write는 기본적으로 소켓 BIO에 직접 연결해 쓰는
//      동기 API라, 이 프로젝트의 완료 통지 기반 비동기 모델과 맞지 않는다.
//      그래서 SSL 객체를 실제 소켓과 연결하지 않고 메모리 BIO(BIO_s_mem) 두
//      개에만 연결한 뒤(SSL_set_bio(ssl, rbio, wbio)), 암호문의 입출력을
//      전부 이 클래스가 직접 펌핑한다:
//        - 소켓에서 수신한 암호문 -> FeedNetworkData() -> BIO_write(rbio) ->
//          SSL_do_handshake()/SSL_read() -> 복호화된 평문을 콜백으로 전달
//        - 상위(HTTP 계층)가 보낼 평문 -> SendPlaintext() -> SSL_write() ->
//          BIO_read(wbio)로 나온 암호문을 rawSend 콜백으로 실제 소켓에 전송
//      메모리 BIO는 절대 블로킹하지 않고(WANT_READ/WANT_WRITE 없이 항상 즉시
//      반환) 필요하면 동적으로 자라므로, 이 안에서 이뤄지는 모든 SSL_* 호출은
//      완료 통지를 기다릴 필요 없이 그 자리에서 끝난다 — 실제로 "기다려야
//      하는" 지점은 오직 소켓 I/O(FeedNetworkData가 호출되길 기다리는 것,
//      rawSend가 실제로 전송을 마치길 기다리는 것)뿐이며 그건 세션 계층이
//      이미 비동기로 처리한다.
//
//      [스레드 안전성] 이 클래스는 자체 뮤텍스로 SSL/BIO 조작을 보호한다 —
//      HTTP 요청 송신(SendPlaintext, 임의의 사용자 스레드에서 호출될 수 있음)과
//      네트워크 수신 처리(FeedNetworkData, IOCP/RIO 워커 스레드에서 호출)가
//      동시에 같은 SSL* 객체를 건드릴 수 있는데, OpenSSL은 같은 SSL* 객체에
//      대한 동시 호출을 스레드 세이프하게 보장하지 않는다. 단, 콜백
//      (rawSend/onPlaintext/onHandshakeDone) 호출은 항상 락을 놓은 뒤에
//      수행한다 — 콜백 안에서 재진입(예: 응답 완료 콜백이 곧바로 다음
//      SendPlaintext()를 부르는 경우)이 일어나도 자기 자신을 락으로 교착시키지
//      않기 위함이다(std::mutex는 재귀 획득을 지원하지 않음).
//
//      [클라이언트 전용] SNI(SSL_set_tlsext_host_name)와 호스트네임 기반
//      인증서 검증(SSL_set1_host)을 설정하므로 클라이언트 모드 전용이다.
//      서버 모드가 필요해지면 별도 초기화 경로가 필요하다.
//***************************************************************************
class CTlsFilter
{
public:
	CTlsFilter() = default;

	//***************************************************************************
	// @brief 소멸자. SSL_free()가 내부적으로 SSL_set_bio()로 연결해둔 rbio/wbio도
	//        함께 해제하므로 별도로 BIO_free()를 호출하지 않는다.
	//***************************************************************************
	~CTlsFilter()
	{
		if( _ssl != nullptr )
			SSL_free(_ssl);
	}

	CTlsFilter(const CTlsFilter&) = delete;
	CTlsFilter& operator=(const CTlsFilter&) = delete;

	//***************************************************************************
	// @brief 필터를 초기화합니다 (클라이언트 모드).
	// @param ctx 여러 커넥션이 공유하는 SSL_CTX (CreateDefaultClientSslCtx() 등으로 생성)
	// @param sniHostname TLS SNI 확장 및 인증서 호스트네임 검증에 사용할 hostname
	// @param rawSend 실제 소켓으로 암호문을 내보내는 콜백
	// @param onPlaintext 복호화된 평문 도착 통지 콜백
	// @param onHandshakeDone 핸드셰이크 완료/실패 통지 콜백
	// @return bool 초기화 성공 여부 (SSL_new/BIO 생성 실패 시 false)
	//***************************************************************************
	bool Initialize(SSL_CTX* ctx, const std::string& sniHostname,
		TlsRawSendFn rawSend, TlsPlaintextRecvHandler onPlaintext, TlsHandshakeCompleteHandler onHandshakeDone)
	{
		_rawSend = std::move(rawSend);
		_onPlaintext = std::move(onPlaintext);
		_onHandshakeDone = std::move(onHandshakeDone);

		_ssl = SSL_new(ctx);
		if( _ssl == nullptr )
			return false;

		BIO* rbio = BIO_new(BIO_s_mem());
		BIO* wbio = BIO_new(BIO_s_mem());
		if( rbio == nullptr || wbio == nullptr )
		{
			if( rbio != nullptr ) BIO_free(rbio);
			if( wbio != nullptr ) BIO_free(wbio);
			SSL_free(_ssl);
			_ssl = nullptr;
			return false;
		}

		// SSL_set_bio() 호출 이후로는 rbio/wbio의 소유권이 SSL 객체로 넘어간다
		// (SSL_free()가 같이 해제함) — 여기서 별도로 BIO_free()하면 안 됨.
		SSL_set_bio(_ssl, rbio, wbio);
		SSL_set_connect_state(_ssl); // 클라이언트 모드

		if( !sniHostname.empty() )
		{
			SSL_set_tlsext_host_name(_ssl, sniHostname.c_str());
			// SSL_set1_host()가 있어야 핸드셰이크 중 실제로 인증서의
			// CN/SAN을 이 hostname과 대조 검증한다 — SNI(SSL_set_tlsext_host_name)
			// 는 서버에 어떤 hostname을 원하는지 알려줄 뿐, 그 자체로는
			// 인증서 검증에 관여하지 않는다(둘을 혼동하면 SNI만 설정하고
			// 실제로는 아무 인증서나 통과하는 구멍이 생김).
			SSL_set1_host(_ssl, sniHostname.c_str());
		}

		return true;
	}

	//***************************************************************************
	// @brief 핸드셰이크를 시작합니다. TCP 연결 완료 직후(세션의 OnConnected())
	//        1회 호출해야 합니다.
	//***************************************************************************
	void StartHandshake()
	{
		std::vector<char> outgoing;
		bool completed = false;
		bool failed = false;

		{
			std::lock_guard<std::mutex> guard(_lock);
			ProcessSslLocked(outgoing, completed, failed);
		}

		FlushAndNotify(outgoing, completed, failed);
	}

	//***************************************************************************
	// @brief 소켓에서 수신한 암호문을 밀어넣고 핸드셰이크/복호화를 진행합니다.
	// @param data 수신 바이트 포인터 (암호문)
	// @param len data의 길이
	//***************************************************************************
	void FeedNetworkData(const char* data, size_t len)
	{
		std::vector<char> outgoing;
		std::string plaintext;
		bool completed = false;
		bool failed = false;

		{
			std::lock_guard<std::mutex> guard(_lock);

			BIO* rbio = SSL_get_rbio(_ssl);
			BIO_write(rbio, data, static_cast<int>(len));

			ProcessSslLocked(outgoing, completed, failed);

			if( !failed && _handshakeComplete )
				DrainPlaintextLocked(plaintext);
		}

		FlushAndNotify(outgoing, completed, failed);

		if( !plaintext.empty() && _onPlaintext )
			_onPlaintext(plaintext.data(), plaintext.size());
	}

	//***************************************************************************
	// @brief 평문을 암호화해서 실제 소켓으로 전송합니다.
	// @param data 전송할 평문 버퍼
	// @param size data의 길이
	// @return bool 성공 여부. 핸드셰이크가 아직 안 끝났거나 SSL_write 자체가
	//         실패하면 false — 호출부(세션)가 커넥션을 폐기해야 한다.
	//***************************************************************************
	bool SendPlaintext(const void* data, uint16_t size)
	{
		std::vector<char> outgoing;
		bool writeOk = false;

		{
			std::lock_guard<std::mutex> guard(_lock);

			if( !_handshakeComplete )
				return false;

			int written = SSL_write(_ssl, data, static_cast<int>(size));
			if( written == static_cast<int>(size) )
				writeOk = true;
			// 메모리 BIO는 절대 가득 차지 않으므로(WANT_WRITE 없음) SSL_write는
			// 이 구성에서 부분 쓰기 없이 전체 성공 또는 실패(<=0)만 일어난다.

			DrainCiphertextLocked(outgoing);
		}

		if( !outgoing.empty() )
			SendChunked(outgoing.data(), outgoing.size());

		return writeOk;
	}

	//***************************************************************************
	// @brief 핸드셰이크가 완료됐는지 반환합니다.
	//***************************************************************************
	bool IsHandshakeComplete() const noexcept { return _handshakeComplete; }

private:
	//***************************************************************************
	// @brief SSL_do_handshake()를 진행하고, 이번 호출로 나온 암호문/완료/실패
	//        상태를 로컬 변수에 담아 반환합니다 (락 보유 중에만 호출).
	// @param outgoing [OUT] 이번 호출로 wbio에 쌓인 암호문 (아직 전송 안 됨)
	// @param completed [OUT] 이번 호출로 핸드셰이크가 막 완료됐는지 여부
	// @param failed [OUT] 핸드셰이크가 복구 불가능하게 실패했는지 여부
	//***************************************************************************
	void ProcessSslLocked(std::vector<char>& outgoing, bool& completed, bool& failed)
	{
		completed = false;
		failed = false;

		if( _handshakeComplete )
			return;

		int ret = SSL_do_handshake(_ssl);
		DrainCiphertextLocked(outgoing);

		if( ret == 1 )
		{
			_handshakeComplete = true;
			completed = true;
			return;
		}

		int err = SSL_get_error(_ssl, ret);
		if( err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE )
		{
			// 더 받아야(또는 방금 만든 outgoing을 내보내야) 진행 가능 — 정상 대기.
			return;
		}

		// SSL_ERROR_SSL(프로토콜/인증서 오류) 등 복구 불가능한 실패.
		failed = true;
	}

	//***************************************************************************
	// @brief 핸드셰이크 완료 이후 도착한 애플리케이션 데이터를 전부 복호화해
	//        하나의 문자열로 모읍니다 (락 보유 중에만 호출).
	// @param plaintext [OUT] 이번 호출로 복호화된 평문 전체 (여러 TLS 레코드에
	//        걸쳐 있어도 하나로 합쳐서 반환 — 상위 HTTP 파서는 바이트 경계에
	//        의존하지 않으므로 문제없음)
	//***************************************************************************
	void DrainPlaintextLocked(std::string& plaintext)
	{
		char buf[16384];
		for( ;;)
		{
			int n = SSL_read(_ssl, buf, sizeof(buf));
			if( n > 0 )
			{
				plaintext.append(buf, static_cast<size_t>(n));
				continue;
			}

			// WANT_READ: 더 받아야 함(정상). ZERO_RETURN: 상대가 TLS
			// close_notify를 보냄(정상 종료) — 두 경우 다 여기서는 조용히
			// 루프를 빠져나간다. 소켓 자체의 종료 감지는 세션 계층(recv 0바이트
			// 등)이 별도로 처리하므로 이 필터가 추가로 통지할 필요는 없다.
			break;
		}
	}

	//***************************************************************************
	// @brief wbio에 쌓인 암호문을 전부 꺼내 outgoing에 추가합니다 (락 보유 중에만 호출).
	//***************************************************************************
	void DrainCiphertextLocked(std::vector<char>& outgoing)
	{
		BIO* wbio = SSL_get_wbio(_ssl);
		char buf[16384];
		int n;
		while( (n = BIO_read(wbio, buf, sizeof(buf))) > 0 )
			outgoing.insert(outgoing.end(), buf, buf + n);
	}

	//***************************************************************************
	// @brief 락 밖에서 암호문 전송 + 핸드셰이크 완료/실패 콜백을 호출합니다.
	//***************************************************************************
	void FlushAndNotify(const std::vector<char>& outgoing, bool completed, bool failed)
	{
		if( !outgoing.empty() )
			SendChunked(outgoing.data(), outgoing.size());

		if( completed && _onHandshakeDone )
			_onHandshakeDone(true);
		else if( failed && _onHandshakeDone )
			_onHandshakeDone(false);
	}

	//***************************************************************************
	// @brief rawSend(uint16_t 청크 제약)에 맞춰 65535바이트 단위로 분할 전송합니다.
	//***************************************************************************
	void SendChunked(const char* data, size_t len)
	{
		if( !_rawSend )
			return;

		size_t offset = 0;
		while( offset < len )
		{
			size_t chunk = (std::min)(len - offset, static_cast<size_t>(65535));
			if( !_rawSend(data + offset, static_cast<uint16_t>(chunk)) )
				return; // 전송 실패 — 세션이 곧 끊길 것이므로 나머지는 포기
			offset += chunk;
		}
	}

private:
	SSL* _ssl = nullptr; // rbio/wbio가 SSL_set_bio()로 연결돼 있어 SSL_free()가 같이 해제함

	TlsRawSendFn _rawSend;                        // 암호문을 실제 소켓으로 내보내는 콜백
	TlsPlaintextRecvHandler _onPlaintext;          // 복호화된 평문 도착 통지 콜백
	TlsHandshakeCompleteHandler _onHandshakeDone;  // 핸드셰이크 완료/실패 통지 콜백

	std::mutex _lock;                  // SSL/BIO 조작 보호 (콜백 호출 중에는 놓음 — 클래스 설명 참고)
	bool _handshakeComplete = false;   // 핸드셰이크 완료 여부
};

#endif // ndef __TLSFILTER_H__