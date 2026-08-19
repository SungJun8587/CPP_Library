
//***************************************************************************
// IocpEchoSession.cpp : implementation of the CIocpEchoServerSession, CIocpEchoClientSession class.
//
//***************************************************************************

#include "pch.h"
#include "IocpEchoSession.h"

//***************************************************************************
// @brief CIocpEchoServerSession 객체를 생성합니다.
//***************************************************************************
CIocpEchoServerSession::CIocpEchoServerSession()
{
}

//***************************************************************************
// @brief CIocpEchoServerSession 객체를 소멸합니다.
//***************************************************************************
CIocpEchoServerSession::~CIocpEchoServerSession()
{
}

//***************************************************************************
// @brief 클라이언트가 성공적으로 접속했을 때 호출되는 오버라이드 함수입니다.
// @details 부모 클래스의 연결 초기화 로직을 수행하고 접속 성공 로그를 출력합니다.
//***************************************************************************
void CIocpEchoServerSession::OnConnected()
{
    CIocpSession::OnConnected();

    uint64 sessionId = GetSessionId();

    // 세션 종료 사유 가져오기
    Iocp::CloseReason reason = GetCloseReason();

    LOG_INFO(_T("[IOCP Session] Connected! (Session ID: %llu)"), sessionId);
}

//***************************************************************************
// @brief 클라이언트와의 연결이 끊어졌을 때 호출되는 오버라이드 함수입니다.
// @details 부모 클래스의 연결 해제 로직을 수행하고 연결 종료 로그를 출력합니다.
//***************************************************************************
void CIocpEchoServerSession::OnDisconnected()
{
    CIocpSession::OnDisconnected();

    uint64 sessionId = GetSessionId();

    // 세션 종료 사유 가져오기
    Iocp::CloseReason reason = GetCloseReason();

    // 사유를 문자열(또는 정수 코드로) 변환해서 함께 출력
    LPCTSTR reasonStr = _T("Unknown");
    switch( reason )
    {
    case Iocp::CloseReason::None:               reasonStr = _T("None"); break;
    case Iocp::CloseReason::RemoteClosed:       reasonStr = _T("RemoteClosed (Client Disconnected)"); break;
    case Iocp::CloseReason::SocketError:        reasonStr = _T("SocketError"); break;
    case Iocp::CloseReason::RingBufferOverflow: reasonStr = _T("RingBufferOverflow"); break;
    case Iocp::CloseReason::ForcedClose:        reasonStr = _T("ForcedClose (Server Initiated)"); break;
    case Iocp::CloseReason::InternalError:      reasonStr = _T("InternalError"); break;
    }

    LOG_INFO(_T("[IOCP Session] Disconnected! (Session ID: %llu, Reason: %s)"), sessionId, reasonStr);
}

//***************************************************************************
// @brief 클라이언트로부터 패킷을 수신했을 때 호출되는 오버라이드 함수입니다.
// @details 수신된 데이터를 문자열 형태로 로그에 출력하고, 받은 데이터를 그대로 
//          다시 클라이언트에게 전송(Echo)합니다.
// @param buffer 수신된 데이터가 담긴 바이트 버퍼 포인터
// @param len 수신된 데이터의 길이 (바이트 단위)
// @return 처리한 데이터의 바이트 길이를 반환합니다.
//***************************************************************************
int32 CIocpEchoServerSession::OnRecv(BYTE* buffer, int32 len)
{
    // 1. UTF-8 바이트를 유니코드(UTF-16 / std::wstring)로 올바르게 변환
    int wlen = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<char*>(buffer), len, nullptr, 0);
    _tstring receivedStr;
    if( wlen > 0 )
    {
        receivedStr.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<char*>(buffer), len, &receivedStr[0], wlen);
    }
    LOG_DEBUG(_T("[IOCP Session Received] %s (Len: %d)"), receivedStr.c_str(), len);

    // 받은 데이터를 그대로 클라이언트에게 전송 (Echo)
    Send(buffer, static_cast<uint16_t>(len));

    return len;
}

//***************************************************************************
// @brief 패킷 전송이 완료되었을 때 호출되는 오버라이드 함수입니다.
// @param len 전송된 데이터의 길이 (바이트 단위)
//***************************************************************************
void CIocpEchoServerSession::OnSend(int32 len)
{
	// 전송 완료 후 추가 처리 (필요시 구현)
}

//***************************************************************************
// @brief 서버에 성공적으로 접속했을 때 호출되는 오버라이드 함수입니다.
//***************************************************************************
void CIocpEchoClientSession::OnConnected()
{
	CIocpSession::OnConnected();
	LOG_INFO(_T("[Client] Connected to Server!"));
}

//***************************************************************************
// @brief 서버로부터 패킷을 수신했을 때 호출되는 오버라이드 함수입니다.
// @param buffer 수신된 데이터가 담긴 바이트 버퍼 포인터
// @param len 수신된 데이터의 길이 (바이트 단위)
// @return 처리한 데이터의 바이트 길이를 반환합니다.
//***************************************************************************
int32 CIocpEchoClientSession::OnRecv(BYTE* buffer, int32 len)
{
    // 1. UTF-8 바이트를 유니코드(UTF-16 / std::wstring)로 올바르게 변환
    int wlen = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<char*>(buffer), len, nullptr, 0);
    _tstring message;
    if( wlen > 0 )
    {
        message.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<char*>(buffer), len, &message[0], wlen);
    }
    LOG_DEBUG(_T("[Client Received] %s"), message.c_str());

    return len;
}


