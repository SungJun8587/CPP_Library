
#ifndef __UTILCORE_H__
#define __UTILCORE_H__

#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <process.h>
#include <assert.h>
#include <time.h>
#include <tchar.h>
#include <dbghelp.h>
#include <sqlucode.h>
#include <wtypes.h>
#include <atlstr.h>

#include <functional>
#include <string>
#include <memory>
#include <vector>
#include <list>
#include <forward_list>
#include <queue>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <random>
#include <locale>
#include <codecvt>
#include <fstream>
#include <cctype>
#include <sstream>

#include <iostream>
using namespace std;

#include <BaseDefine.h>
#include <BaseRedefineDataType.h>
#include <BaseMacro.h>
#include <BaseGlobal.h>

/***********************************************************************************************/
// External Library
/***********************************************************************************************/

#pragma comment(lib, LIB_NAME("libtcmalloc_minimal"))
#pragma comment(lib, LIB_NAME("cryptlib"))
#pragma comment(lib, "libmySQL.lib")

#include <gperftools/tcmalloc.h>
#include <mysql.h>

/***********************************************************************************************/

#include <LockGuard.h>
#include <CriticalSection.h>
#include <RWLock.h>
#include <Thread.h>
#include <ThreadManager.h>

#include <CustomAllocator.h>
#include <Containers.h>
#include <MemBuffer.h>
#include <ObjectPool.h>
#include <MemoryPool.h>
#include <Memory.h>
#include <ClusteredMap.h>
#include <OrderedMap.h>
#include <UnOrderedMap.h>

#include <Log.h>
#include <BaseFile.h>
#include <EventLog.h>

#include <JSONBase.h>
#include <JSONParser.h>

#include <XMLParser.h>

#include <CalculatedElapsedTime.h>
#include <CommonUtil.h>
#include <DateTimeUtil.h>
#include <Regular.h>
#include <StringUtil.h>
#include <ShellUtil.h>
#include <WebUtil.h>
#include <Singleton.h>
#include <Endian.h>
#include <Stream.h>

#include <ServerConnectInfo.h>
#include <ServerConfig.h>
#include <ServiceSvr.h>

#include <AdoDB.h>
#include <BaseODBC.h>
#include <DBBind.h>
#include <DBModel.h>
#include <DBSynchronizer.h>
#include <DBAsyncSrv.h>
#include <OdbcConnPool.h>
#include <OdbcAsyncSrv.h>

#include <BaseMySQL.h>
#include <MySQLConnPool.h>
#include <MySQLAsyncSrv.h>

#endif // ndef __UTILCORE_H__