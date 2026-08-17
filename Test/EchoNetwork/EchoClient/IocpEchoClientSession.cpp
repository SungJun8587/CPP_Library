
//***************************************************************************
// IocpEchoClientSession.cpp : implementation of the CIocpEchoClientSession class.
//
//***************************************************************************

#include "pch.h"
#include "IocpEchoClientSession.h"

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
    if( len <= 0 ) return len;

    // 1. UTF-8 바이트를 유니코드(UTF-16 / std::wstring)로 변환
    int wlen = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<char*>(buffer), len, nullptr, 0);
    _tstring message;
    if( wlen > 0 )
    {
        message.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<char*>(buffer), len, &message[0], wlen);
    }

    LOG_WRITE(ELOG_TYPE::LOG_TYPE_DEBUG, false, _T("[Client Received] %s (Len: %d)"), message.c_str(), len);

    // 대기 플래그를 해제하여 메인 루프가 다음 입력을 받도록 허용
    _waitingForEcho = false;

    return len;
}


