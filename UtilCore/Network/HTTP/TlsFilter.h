
//***************************************************************************
// TlsFilter.h : interface for the CTlsFilter class.
//
//***************************************************************************

#ifndef UC_TLSFILTER_H
#define UC_TLSFILTER_H

#include <BaseRedefineDataType.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <functional>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <algorithm>
#include <utility>

#pragma comment(lib, LIB_NAME("libssl"))
#pragma comment(lib, LIB_NAME("libcrypto"))

//***************************************************************************
// @brief 실제 소켓으로 암호문(ciphertext)을 내보내는 콜백. 세션의 Send()를
//        그대로 감싸서 넘기면 된다. 시그니처는 CSession::Send()와 동일하게
//        uint16 청크 단위 — CTlsFilter 내부에서 65535바이트 단위로 자동
//        분할해 호출한다(CHttpClientCore::BeginRequest()와 동일한 제약/패턴).
//***************************************************************************
using TlsRawSendFn = std::function<bool(const void*, uint16)>;

// 복호화된 평문이 도착했을 때 통지. data는 이 콜백이 끝나면 무효화되는 임시
// 버퍼를 가리키므로, 호출부가 더 오래 보관해야 한다면 콜백 안에서 복사해야 함.
using TlsPlaintextRecvHandler = std::function<void(const char* data, size_t len)>;

// 핸드셰이크 완료(success==true) 또는 복구 불가능한 TLS 오류(success==false)를 통지.
// success==false면 호출부는 세션을 폐기해야 한다 — 핸드셰이크 중의 인증서 검증 실패/프로토콜
// 오류뿐 아니라, 핸드셰이크 이후 레코드 복호화 실패(MAC 오류 등)도 같은 경로로 알린다.
using TlsHandshakeCompleteHandler = std::function<void(bool success)>;

//***************************************************************************
// @brief 기본 클라이언트 SSL_CTX를 생성합니다 (TLS 1.2 이상, 피어 인증서 검증 활성화).
// @details 신뢰할 CA는 OpenSSL의 기본 검증 경로(SSL_CTX_set_default_verify_paths)에서 읽는다.
//          Windows에서 OpenSSL 3.2 이상이면 OS 인증서 저장소(winstore)도 함께 신뢰한다 —
//          Windows의 OpenSSL은 기본 경로에 CA 번들이 없는 경우가 많아 저장소를 읽지 않으면
//          모든 HTTPS 서버 인증서 검증이 실패하기 때문이다.
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

#if defined(_WIN32) && OPENSSL_VERSION_NUMBER >= 0x30200000L
	if( SSL_CTX_load_verify_store(ctx, "org.openssl.winstore://") != 1 )
		ERR_clear_error(); // OS 저장소를 못 읽어도 기본 경로로 계속 시도한다
#endif

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
//      [송신 순서] TLS 레코드는 암호화된 순서 그대로 소켓에 나가야 한다(순서가 바뀌면 상대의
//      MAC 검증이 실패한다). 암호문은 락 안에서 _sendQueue에 쌓고, 락 밖에서는 한 번에 한
//      스레드(_sending)만 큐를 비우며 rawSend를 호출한다 — 다른 스레드는 큐에 쌓기만 하고
//      돌아가고, 먼저 온 스레드가 그 데이터까지 순서대로 내보낸다. 별도의 송신 락이 없어
//      rawSend 안에서 재진입해도 교착되지 않는다.
//
//      [OpenSSL 에러 큐] OpenSSL의 에러 큐는 스레드별이고 I/O 워커 스레드는 여러 커넥션이
//      공유한다. SSL_get_error()는 큐에 남은 오류가 있으면 그것을 이번 호출의 실패로 보고하므로,
//      다른 커넥션이 남긴 오류가 이 커넥션의 정상적인 WANT_READ를 치명적 오류로 바꿀 수 있다.
//      그래서 모든 SSL_do_handshake/SSL_read/SSL_write 직전에 ERR_clear_error()를 호출한다.
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

		if( _ssl != nullptr )
		{
			SSL_free(_ssl); // 재초기화 — 이전 SSL 객체(와 연결된 BIO)를 해제한다
			_ssl = nullptr;
		}
		_handshakeComplete = false;
		_failed = false;
		_sending = false;
		_sendQueue.clear();

		ERR_clear_error();
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
	// @details Initialize()가 실패한(또는 호출되지 않은) 필터면 핸드셰이크 실패로 통지한다.
	//***************************************************************************
	void StartHandshake()
	{
		bool completed = false;
		bool failed = false;

		{
			std::lock_guard<std::mutex> guard(_lock);
			if( _ssl == nullptr )
				failed = true;
			else
				ProcessSslLocked(completed, failed);
		}

		PumpSend();
		Notify(completed, std::string(), failed);
	}

	//***************************************************************************
	// @brief 소켓에서 수신한 암호문을 밀어넣고 핸드셰이크/복호화를 진행합니다.
	// @param data 수신 바이트 포인터 (암호문)
	// @param len data의 길이
	//***************************************************************************
	void FeedNetworkData(const char* data, size_t len)
	{
		std::string plaintext;
		bool completed = false;
		bool failed = false;

		{
			std::lock_guard<std::mutex> guard(_lock);

			if( _ssl == nullptr )
			{
				failed = true; // Initialize()가 실패한 필터 — 세션을 폐기하게 한다
			}
			else if( _failed )
			{
				return; // 이미 실패를 통지했다 — 이후 데이터는 버린다
			}
			else
			{
				BIO_write(SSL_get_rbio(_ssl), data, static_cast<int>(len));

				ProcessSslLocked(completed, failed);

				if( !failed && _handshakeComplete )
				{
					DrainPlaintextLocked(plaintext, failed);
					// SSL_read가 TLS 1.3 KeyUpdate 응답 같은 post-handshake 메시지를 wbio에 쌓았을 수 있다.
					DrainCiphertextLocked();
				}
			}
		}

		PumpSend();
		Notify(completed, plaintext, failed);
	}

	//***************************************************************************
	// @brief 평문을 암호화해서 실제 소켓으로 전송합니다.
	// @param data 전송할 평문 버퍼
	// @param size data의 길이
	// @return bool 성공 여부. 핸드셰이크가 아직 안 끝났거나 SSL_write 자체가
	//         실패하면 false — 호출부(세션)가 커넥션을 폐기해야 한다. true는 암호화와 송신 큐
	//         적재까지 성공했다는 뜻이며, 실제 소켓 송신 실패는 세션 종료 통지로 알려진다.
	//***************************************************************************
	bool SendPlaintext(const void* data, uint16 size)
	{
		bool writeOk = false;

		{
			std::lock_guard<std::mutex> guard(_lock);

			if( _ssl == nullptr || !_handshakeComplete || _failed )
				return false;

			ERR_clear_error();
			const int written = SSL_write(_ssl, data, static_cast<int>(size));
			writeOk = (written == static_cast<int>(size));
			// 메모리 BIO는 절대 가득 차지 않으므로(WANT_WRITE 없음) SSL_write는
			// 이 구성에서 부분 쓰기 없이 전체 성공 또는 실패(<=0)만 일어난다.

			DrainCiphertextLocked();
		}

		PumpSend();
		return writeOk;
	}

	//***************************************************************************
	// @brief 핸드셰이크가 완료됐는지 반환합니다.
	//***************************************************************************
	bool IsHandshakeComplete() const noexcept { return _handshakeComplete; }

private:
	//***************************************************************************
	// @brief SSL_do_handshake()를 진행하고, 이번 호출로 나온 암호문은 _sendQueue에 쌓습니다
	//        (락 보유 중에만 호출).
	// @param completed [OUT] 이번 호출로 핸드셰이크가 막 완료됐는지 여부
	// @param failed [OUT] 핸드셰이크가 복구 불가능하게 실패했는지 여부
	//***************************************************************************
	void ProcessSslLocked(bool& completed, bool& failed)
	{
		completed = false;
		failed = false;

		if( _handshakeComplete )
			return;

		ERR_clear_error();
		const int ret = SSL_do_handshake(_ssl);
		DrainCiphertextLocked();

		if( ret == 1 )
		{
			_handshakeComplete = true;
			completed = true;
			return;
		}

		const int err = SSL_get_error(_ssl, ret);
		if( err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE )
		{
			// 더 받아야(또는 방금 만든 암호문을 내보내야) 진행 가능 — 정상 대기.
			return;
		}

		// SSL_ERROR_SSL(프로토콜/인증서 오류) 등 복구 불가능한 실패.
		_failed = true;
		failed = true;
	}

	//***************************************************************************
	// @brief 핸드셰이크 완료 이후 도착한 애플리케이션 데이터를 전부 복호화해
	//        하나의 문자열로 모읍니다 (락 보유 중에만 호출).
	// @param plaintext [OUT] 이번 호출로 복호화된 평문 전체 (여러 TLS 레코드에
	//        걸쳐 있어도 하나로 합쳐서 반환 — 상위 HTTP 파서는 바이트 경계에
	//        의존하지 않으므로 문제없음)
	// @param failed [OUT] 복호화가 복구 불가능하게 실패했는지 여부 (MAC 오류, 프로토콜 위반 등)
	//***************************************************************************
	void DrainPlaintextLocked(std::string& plaintext, bool& failed)
	{
		char buf[16384];
		for( ;; )
		{
			ERR_clear_error();
			const int n = SSL_read(_ssl, buf, sizeof(buf));
			if( n > 0 )
			{
				plaintext.append(buf, static_cast<size_t>(n));
				continue;
			}

			// WANT_READ: 더 받아야 함(정상). ZERO_RETURN: 상대가 TLS close_notify를 보냄(정상
			// 종료) — 두 경우 다 조용히 루프를 빠져나간다. 소켓 자체의 종료 감지는 세션 계층이
			// 별도로 처리하므로 이 필터가 추가로 통지할 필요는 없다. 그 밖의 오류는 레코드가
			// 깨진 것이라 이 커넥션으로는 더 진행할 수 없다.
			const int err = SSL_get_error(_ssl, n);
			if( err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_ZERO_RETURN )
			{
				_failed = true;
				failed = true;
			}
			break;
		}
	}

	//***************************************************************************
	// @brief wbio에 쌓인 암호문을 전부 꺼내 _sendQueue에 추가합니다 (락 보유 중에만 호출).
	//***************************************************************************
	void DrainCiphertextLocked()
	{
		BIO* wbio = SSL_get_wbio(_ssl);
		char buf[16384];
		int n;
		while( (n = BIO_read(wbio, buf, sizeof(buf))) > 0 )
			_sendQueue.insert(_sendQueue.end(), buf, buf + n);
	}

	//***************************************************************************
	// @brief _sendQueue에 쌓인 암호문을 순서대로 rawSend로 내보냅니다 (락 밖에서 호출).
	// @details 이미 다른 스레드가 큐를 비우는 중(_sending)이면 그 스레드가 이번에 쌓인 데이터까지
	//          내보내므로 그냥 돌아간다 — 큐에 쌓인 순서가 곧 소켓으로 나가는 순서다.
	//***************************************************************************
	void PumpSend()
	{
		std::vector<char> batch;
		{
			std::lock_guard<std::mutex> guard(_lock);
			if( _sending || _sendQueue.empty() )
				return;
			_sending = true;
			batch.swap(_sendQueue);
		}

		try
		{
			for( ;; )
			{
				SendChunked(batch.data(), batch.size());
				batch.clear();

				std::lock_guard<std::mutex> guard(_lock);
				if( _sendQueue.empty() )
				{
					_sending = false;
					return;
				}
				batch.swap(_sendQueue); // 비워진 batch의 버퍼가 _sendQueue로 돌아가 재사용된다
			}
		}
		catch( ... )
		{
			std::lock_guard<std::mutex> guard(_lock);
			_sending = false;
			throw;
		}
	}

	//***************************************************************************
	// @brief 락 밖에서 핸드셰이크 완료 / 평문 도착 / 실패 콜백을 순서대로 호출합니다.
	// @details 완료 -> 평문 -> 실패 순서다. 핸드셰이크가 끝난 같은 수신에서 평문이 함께 도착하고
	//          그 직후 복호화가 실패한 경우에도 받은 데이터를 먼저 전달한다.
	//***************************************************************************
	void Notify(bool completed, const std::string& plaintext, bool failed)
	{
		if( completed && _onHandshakeDone )
			_onHandshakeDone(true);

		if( !plaintext.empty() && _onPlaintext )
			_onPlaintext(plaintext.data(), plaintext.size());

		if( failed && _onHandshakeDone )
			_onHandshakeDone(false);
	}

	//***************************************************************************
	// @brief rawSend(uint16 청크 제약)에 맞춰 65535바이트 단위로 분할 전송합니다.
	//***************************************************************************
	void SendChunked(const char* data, size_t len)
	{
		if( !_rawSend )
			return;

		size_t offset = 0;
		while( offset < len )
		{
			const size_t chunk = (std::min)(len - offset, static_cast<size_t>(65535));
			if( !_rawSend(data + offset, static_cast<uint16>(chunk)) )
				return; // 전송 실패 — 세션이 곧 끊길 것이므로 나머지는 포기
			offset += chunk;
		}
	}

private:
	SSL* _ssl = nullptr; // rbio/wbio가 SSL_set_bio()로 연결돼 있어 SSL_free()가 같이 해제함

	TlsRawSendFn _rawSend;                        // 암호문을 실제 소켓으로 내보내는 콜백
	TlsPlaintextRecvHandler _onPlaintext;          // 복호화된 평문 도착 통지 콜백
	TlsHandshakeCompleteHandler _onHandshakeDone;  // 핸드셰이크 완료/실패 통지 콜백

	std::mutex _lock;                  // SSL/BIO 조작과 아래 송신 큐 상태 보호 (콜백 호출 중에는 놓음 — 클래스 설명 참고)
	std::atomic<bool> _handshakeComplete{ false }; // 핸드셰이크 완료 여부. IsHandshakeComplete()가
	// 락 없이(다른 스레드에서) 읽을 수 있어 atomic — 쓰기는 여전히 _lock 보유 중(ProcessSslLocked)에만 일어남.
	bool _failed = false;              // 복구 불가능한 TLS 오류가 발생했는지 (이후 수신 데이터는 버리고 송신은 거부)
	std::vector<char> _sendQueue;      // 암호화됐지만 아직 rawSend로 내보내지 않은 암호문 (암호화 순서 = 송신 순서)
	bool _sending = false;             // 어떤 스레드가 _sendQueue를 비우는 중인지 (PumpSend() 참고)
};

#endif // ndef UC_TLSFILTER_H