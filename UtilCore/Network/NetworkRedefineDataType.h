
//***************************************************************************
// This File include Information about overriding the network data type.
// 
//***************************************************************************

#ifndef __NETWORKREDEFINEDATATYPE_H__
#define __NETWORKREDEFINEDATATYPE_H__

#pragma once

#ifndef	__BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

USING_SHARED_PTR(CSession);
USING_SHARED_PTR(CNetService);

USING_SHARED_PTR(CIocpCore);
USING_SHARED_PTR(CIocpObject);
USING_SHARED_PTR(CIocpListener);
USING_SHARED_PTR(CSendBuffer);
USING_SHARED_PTR(CSendBufferChunk);
USING_SHARED_PTR(CIocpSession)
USING_SHARED_PTR(CIocpServerService);
USING_SHARED_PTR(CIocpClientService);

USING_SHARED_PTR(CRioObject);
USING_SHARED_PTR(CRioCore);
USING_SHARED_PTR(CRioListener);
USING_SHARED_PTR(CRioBuffer);
USING_SHARED_PTR(CRioSession);
USING_SHARED_PTR(CRioServerService);
USING_SHARED_PTR(CRioClientService);

#endif // ndef __NETWORKREDEFINEDATATYPE_H__