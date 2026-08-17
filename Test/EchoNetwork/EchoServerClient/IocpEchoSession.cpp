
//***************************************************************************
// IocpEchoSession.cpp : implementation of the CIocpEchoSession, CIocpClientEchoSession class.
//
//***************************************************************************

#include "pch.h"
#include "IocpEchoSession.h"

//***************************************************************************
// @brief CIocpEchoSession 객체를 생성합니다.
//***************************************************************************
CIocpEchoSession::CIocpEchoSession()
{
}

//***************************************************************************
// @brief CIocpEchoSession 객체를 소멸합니다.
//***************************************************************************
CIocpEchoSession::~CIocpEchoSession()
{
}

//***************************************************************************
// @brief 클라이언트가 성공적으로 접속했을 때 호출되는 오버라이드 함수입니다.
// @details 부모 클래스의 연결 초기화 로직을 수행하고 접속 성공 로그를 출력합니다.
//***************************************************************************
void CIocpEchoSession::OnConnected()
{
	CIocpSession::OnConnected();
	LOG_INFO(_T("[IOCP Session] Connected!"));
}

//***************************************************************************
// @brief 클라이언트와의 연결이 끊어졌을 때 호출되는 오버라이드 함수입니다.
// @details 부모 클래스의 연결 해제 로직을 수행하고 연결 종료 로그를 출력합니다.
//***************************************************************************
void CIocpEchoSession::OnDisconnect()
{
	CIocpSession::OnDisconnect();
	LOG_INFO(_T("[IOCP Session] Disconnected!"));
}

//***************************************************************************
// @brief 클라이언트로부터 패킷을 수신했을 때 호출되는 오버라이드 함수입니다.
// @details 수신된 데이터를 문자열 형태로 로그에 출력하고, 받은 데이터를 그대로 
//          다시 클라이언트에게 전송(Echo)합니다.
// @param buffer 수신된 데이터가 담긴 바이트 버퍼 포인터
// @param len 수신된 데이터의 길이 (바이트 단위)
// @return 처리한 데이터의 바이트 길이를 반환합니다.
//***************************************************************************
int32 CIocpEchoSession::OnRecv(BYTE* buffer, int32 len)
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
void CIocpEchoSession::OnSend(int32 len)
{
	// 전송 완료 후 추가 처리 (필요시 구현)
}

//***************************************************************************
// @brief 서버에 성공적으로 접속했을 때 호출되는 오버라이드 함수입니다.
//***************************************************************************
void CIocpClientEchoSession::OnConnected()
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
int32 CIocpClientEchoSession::OnRecv(BYTE* buffer, int32 len)
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


