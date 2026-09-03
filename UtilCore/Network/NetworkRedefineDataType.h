
//***************************************************************************
// This File include Information about overriding the network data type.
// 
//***************************************************************************

#ifndef UC_NETWORKREDEFINEDATATYPE_H
#define UC_NETWORKREDEFINEDATATYPE_H

#include <BaseRedefineDataType.h>
#include <BaseMacro.h>

USING_SHARED_PTR(CSession);
USING_SHARED_PTR(CNetService);

USING_SHARED_PTR(CIocpCore);
USING_SHARED_PTR(CIocpObject);
USING_SHARED_PTR(CIocpListener);
USING_SHARED_PTR(CSendBuffer);
USING_SHARED_PTR(CSendBufferChunk);
USING_SHARED_PTR(CIocpSession);
USING_SHARED_PTR(CIocpServerService);
USING_SHARED_PTR(CIocpClientService);

USING_SHARED_PTR(CRioObject);
USING_SHARED_PTR(CRioCore);
USING_SHARED_PTR(CRioListener);
USING_SHARED_PTR(CRioBuffer);
USING_SHARED_PTR(CRioSession);
USING_SHARED_PTR(CRioServerService);
USING_SHARED_PTR(CRioClientService);

USING_SHARED_PTR(CHttpRequestBuilder);
USING_SHARED_PTR(CHttpResponseBuilder);
USING_SHARED_PTR(IHttpConnPool);
USING_SHARED_PTR(CHttpSessionIocp);
USING_SHARED_PTR(CHttpSessionRio);
USING_SHARED_PTR(CHttpConnPoolManager);
USING_SHARED_PTR(CHttpClient);

#endif // ndef UC_NETWORKREDEFINEDATATYPE_H