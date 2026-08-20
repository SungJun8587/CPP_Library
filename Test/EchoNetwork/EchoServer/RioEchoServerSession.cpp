
//***************************************************************************
// RioEchoServerSession.cpp : implementation of the CRioEchoServerSession class.
//
//***************************************************************************

#include "pch.h"
#include "RioEchoServerSession.h"

//***************************************************************************
// @brief CRioEchoSession 객체를 생성합니다.
//***************************************************************************
CRioEchoServerSession::CRioEchoServerSession()
{
}

//***************************************************************************
// @brief CRioEchoSession 객체를 소멸합니다.
//***************************************************************************
CRioEchoServerSession::~CRioEchoServerSession()
{
}

//***************************************************************************
// @brief RIO 클라이언트가 성공적으로 접속했을 때 호출되는 오버라이드 함수입니다.
//***************************************************************************
void CRioEchoServerSession::OnConnected()
{
	CRioSession::OnConnected();

	uint64 sessionId = GetSessionId();

	LOG_INFO(_T("[RIO Session] Connected! (Session ID: %llu)"), sessionId);
}

//***************************************************************************
// @brief RIO 클라이언트와의 연결이 끊어졌을 때 호출되는 오버라이드 함수입니다.
// @param reason 세션 종료 사유
//***************************************************************************
void CRioEchoServerSession::OnDisconnected(Rio::CloseReason reason)
{
	CRioSession::OnDisconnected(reason);
	LOG_INFO(_T("[RIO Session] Disconnected! (Reason: %d)"), static_cast<int>(reason));
}

//***************************************************************************
// @brief RIO 환경에서 패킷을 수신했을 때 호출되는 순수 가상 오버라이드 함수입니다.
// @details 수신 링버퍼에서 데이터를 읽어 출력하고, 그대로 클라이언트에게 에코백(Send)합니다.
//***************************************************************************
void CRioEchoServerSession::OnDataReceived()
{
	uint64 sessionId = GetSessionId();

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
			LOG_DEBUG(_T("[RIO Session ID(%llu) Received] %s (Len: %lld)"), sessionId, tempBuffer.data(), outDequeueSize);

			// 받은 데이터를 그대로 클라이언트에게 전송 (Echo)
			Send(tempBuffer.data(), static_cast<uint16_t>(outDequeueSize));
		}
	}
}