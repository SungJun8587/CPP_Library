
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

#include <thread>
#include <functional>
#include <atomic>
#include <mutex>

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
#include <random>
#include <locale>
#include <codecvt>
#include <fstream>
#include <iterator>
#include <cctype>
#include <sstream>
#include <filesystem>
#include <algorithm>

#include <iostream>
using namespace std;

#include <BaseDefine.h>
#include <BaseRedefineDataType.h>
#include <BaseTLS.h>
#include <BaseMacro.h>

/***********************************************************************************************/
// External Library
/***********************************************************************************************/

#pragma comment(lib, LIB_NAME("libtcmalloc_minimal"))
#pragma comment(lib, LIB_NAME("libiconv"))
#pragma comment(lib, LIB_NAME("xlnt"))
#pragma comment(lib, LIB_NAME("libcrypto"))
#pragma comment(lib, LIB_NAME("libssl"))
#pragma comment(lib, LIB_NAME("libmySQL"))

#include <gperftools/tcmalloc.h>
#include <iconv.h>
#include <xlnt/xlnt.hpp>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <mysql.h>

/***********************************************************************************************/

#include <Util/BaseFile.h>
#include <Util/EventLog.h>
#include <Util/Log.h>

#include <Thread/CriticalSection.h>
#include <Thread/DeadLockProfiler.h>
#include <Thread/SpinLock.h>
#include <Thread/SRWLock.h>
#include <Thread/ThreadManager.h>

#include <Memory/MemoryPool.h>
#include <Memory/Memory.h>
#include <Memory/CustomAllocator.h>
#include <Memory/Containers.h>
#include <Memory/ObjectPool.h>
#include <Memory/ClusteredMap.h>
#include <Memory/OrderedMap.h>
#include <Memory/UnOrderedMap.h>
#include <Memory/Singleton.h>
#include <Memory/MemBuffer.h>
#include <Memory/Stream.h>

#include <BaseGlobal.h>

#include <JSON/JSONBase.h>
#include <JSON/JSONParser.h>

#include <XML/XMLParser.h>

#include <Excel/XlntUtil.h>

#include <Util/CalculatedElapsedTime.h>
#include <Util/IconvUtil.h>
#include <Util/ConvertCharset.h>
#include <Util/CommonUtil.h>
#include <Util/DateTimeUtil.h>
#include <Util/Regular.h>
#include <Util/StringUtil.h>
#include <Util/FileUtil.h>
#include <Util/ShellUtil.h>
#include <Util/WebUtil.h>
#include <Util/Endian.h>

#include <ServerConnectInfo.h>
#include <ServerConfig.h>

#include <DB/DBCommon.h>
#include <DB/AdoDB.h>
#include <DB/BaseODBC.h>
#include <DB/DBBind.h>
#include <DB/DBModel.h>
#include <DB/DBSyncBind.h>
#include <DB/DBQueryProcess.h>
#include <DB/DBSynchronizer.h>
#include <DB/DBAsyncSrv.h>
#include <DB/OdbcConnPool.h>
#include <DB/OdbcAsyncSrv.h>

#include <DB/MySQL/BaseMySQL.h>
#include <DB/MySQL/MySQLConnPool.h>
#include <DB/MySQL/MySQLAsyncSrv.h>

#include <ServiceSvr.h>

#endif // ndef __UTILCORE_H__