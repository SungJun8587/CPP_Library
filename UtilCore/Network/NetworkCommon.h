
//***************************************************************************
// NetworkCommon.h: Common header file for network-related defines.
//
//***************************************************************************

#ifndef __NETWORKCOMMON_H__
#define __NETWORKCOMMON_H__

#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <winternl.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

#include <Network/NetAddress.h>
#include <Network/SocketUtils.h>

#include <Network/IOCP/IocpCommon.h>
#include <Network/IOCP/SendBuffer.h>
#include <Network/IOCP/RecvBuffer.h>
#include <Network/IOCP/RingBuffer.h>
#include <Network/IOCP/IocpEvent.h>
#include <Network/IOCP/IocpCore.h>
#include <Network/IOCP/IocpListener.h>
#include <Network/IOCP/IocpSession.h>

#include <Network/RIO/RioCommon.h>
#include <Network/RIO/RioObject.h>
#include <Network/RIO/RioEvent.h>
#include <Network/RIO/RioEventPool.h>
#include <Network/RIO/RioCore.h>
#include <Network/RIO/RioBuffer.h>
#include <Network/RIO/RioWorker.h>
#include <Network/RIO/RioSend.h>
#include <Network/RIO/RioReceive.h>
#include <Network/RIO/RioSession.h>
#include <Network/RIO/RioSessionManager.h>
#include <Network/RIO/RioServer.h>

#endif // __NETWORKCOMMON_H__
