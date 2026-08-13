
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

#include <iostream>
using namespace std;

#include <BaseDefine.h>
#include <BaseRedefineDataType.h>
#include <NetworkRedefineDataType.h>
#include <BaseMacro.h>

#include <Util/BaseFile.h>
#include <Util/EventLog.h>
#include <Util/Log.h>

#include <Memory/RawAllocator.h>
#include <Memory/Allocator.h>
#include <Memory/MemoryPool.h>
#include <Memory/Memory.h>
#include <Memory/ObjectPool.h>
#include <Memory/Containers.h>

#include <Memory/Singleton.h>
#include <Memory/MemBuffer.h>
#include <Memory/Stream.h>
#include <Memory/RefCountable.h>

#include <Thread/CacheAlignment.h>
#include <Thread/CriticalSection.h>
#include <Thread/DeadLockProfiler.h>
#include <Thread/SpinLock.h>
#include <Thread/SRWLock.h>
#include <Thread/PlatformLock.h>
#include <Thread/ThreadManager.h>

#include <Containers/Map/ClusterSpinMap.h>
#include <Containers/Map/OrderedMap.h>
#include <Containers/Map/UnOrderedMap.h>

#include <Containers/Queue/QueueCommon.h>
#include <Containers/Queue/BlockingTaskQueue.h>
#include <Containers/Queue/SpinLockQueue.h>
#include <Containers/Queue/DelayedTaskQueue.h>
#include <Containers/Queue/ChunkedSwapQueue.h>
#include <Containers/Queue/DoubleBufferQueue.h>
#include <Containers/Queue/ChunkedBlockingQueue.h>

#include <Containers/Queue/SPSCLockFreeQueue.h>
#include <Containers/Queue/SPMCLockFreeQueue.h>
#include <Containers/Queue/MPSCLockFreeQueue.h>
#include <Containers/Queue/MPMCLockFreeQueue.h>

#include <Containers/Stack/LockFreeSlotStack.h>

#include <Job/Job.h>
#include <Job/JobTimer.h>
#include <Job/JobQueue.h>
#include <Job/GlobalQueue.h>

#include <Network/NetworkCommon.h>

#include <BaseGlobal.h>
#include <BaseTLS.h>

#include <JSON/RapidJSONUtil.h>
#include <XML/RapidXMLUtil.h>
#include <Crypto/CryptoUtil.h>

#include <Util/CalculatedElapsedTime.h>
#include <Util/IconvUtil.h>
#include <Util/WinCharsetConv.h>
#include <Util/EncodingConvert.h>
#include <Util/CommonUtil.h>
#include <Util/DateTimeUtil.h>
#include <Util/Regular.h>
#include <Util/StringUtil.h>
#include <Util/FileUtil.h>
#include <Util/ShellUtil.h>
#include <Util/WebUtil.h>
#include <Util/Endian.h>
#include <Util/BufferReader.h>
#include <Util/BufferWriter.h>
#include <Util/TypeCast.h>

#include <Excel/XlntUtil.h>

#include <ServerConnectInfo.h>
#include <ServerConfig.h>

#include <DB/DBCommon.h>
#include <DB/BaseODBC.h>
#include <DB/DBBind.h>
#include <DB/DBModel.h>
#include <DB/DBSyncBind.h>
#include <DB/DBQueryProcess.h>
#include <DB/DBSchema.h>
#include <DB/DBAsyncSrv.h>
#include <DB/OdbcConnPool.h>
#include <DB/OdbcAsyncSrv.h>

#include <DB/ADO/AdoDB.h>
#include <DB/ADO/AdoConnPool.h>
#include <DB/ADO/AdoAsyncSrv.h>

#include <DB/MySQL/BaseMySQL.h>
#include <DB/MySQL/MySQLConnPool.h>
#include <DB/MySQL/MySQLAsyncSrv.h>

#include <WindowsServiceBase.h>

#endif // ndef __UTILCORE_H__