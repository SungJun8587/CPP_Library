
//***************************************************************************
// RioEchoClientSession.cpp : implementation of the CRioEchoClientSession class.
//
//***************************************************************************

#include "pch.h"
#include "RioEchoClientSession.h"

//***************************************************************************
// @brief RIO 클라이언트가 서버에 성공적으로 접속했을 때 호출되는 오버라이드 함수입니다.
//***************************************************************************
void CRioEchoClientSession::OnConnected()
{
	CRioSession::OnConnected();
	LOG_INFO(_T("[RIO Client] Connected to Server!"));
}

//***************************************************************************
// @brief RIO 클라이언트가 서버와의 연결이 종료되었을 때 호출되는 오버라이드 함수입니다.
// @param reason 세션 종료 사유
//***************************************************************************
void CRioEchoClientSession::OnDisconnected(Rio::CloseReason reason)
{
	CRioSession::OnDisconnected(reason);
	LOG_INFO(_T("[RIO Client] Disconnected from Server! (Reason: %d)"), static_cast<int>(reason));
}

//***************************************************************************
// @brief RIO 클라이언트가 서버로부터 패킷을 수신했을 때 호출되는 오버라이드 함수입니다.
//***************************************************************************
void CRioEchoClientSession::OnDataReceived()
{
	auto& recvBuffer = GetRecvBuffer();

	int64 dataSize = recvBuffer.GetSizeUsed();
	if( dataSize <= 0 )
		return;

	CVector<char> tempBuffer(static_cast<size_t>(dataSize) + 1, 0);
	int64 outDequeueSize = 0;

	if( recvBuffer.Dequeue(tempBuffer.data(), dataSize, &outDequeueSize, true, false) )
	{
		if( outDequeueSize > 0 )
		{
			std::string message(tempBuffer.data(), static_cast<size_t>(outDequeueSize));
			LOG_DEBUG(_T("[RIO Client Received] %s"), message.c_str());
		}
	}
}