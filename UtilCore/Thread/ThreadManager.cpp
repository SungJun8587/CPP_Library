
//***************************************************************************
// ThreadManager.cpp: implementation of the CThreadManager class.
//
//***************************************************************************

#include "pch.h"
#include "ThreadManager.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

CThreadManager::CThreadManager()
    : _threads()
    , _lock()
{
}

CThreadManager::~CThreadManager()
{
    JoinThreads();
}

//***************************************************************************
// @brief 새 워커 스레드를 생성합니다.
// @param fncCallback 스레드 진입 후 실행할 콜백 함수
// @return 스레드 생성 성공 시 true, 전체 종료 중이라 거부된 경우 false
//***************************************************************************
bool CThreadManager::CreateThread(std::function<void(void)> fncCallback)
{
    std::lock_guard<std::mutex> lock(_lock);

    if( _bShuttingDown.load() ) return false;

    _threads.emplace_back([this, fncCallback]() {
        InitTLS();
        try
        {
            fncCallback();
        }
        catch( ... )
        {
            // 예외 발생 시에도 TLS 정리 보장.
            // 아래 재던지는 std::thread 진입 함수를 벗어나므로 std::terminate()로
            // 이어진다(로그 없이 프로세스 종료). 콜백 내부 예외는 콜백에서 직접 처리할 것.
            DestroyTLS();
            throw;
        }
        DestroyTLS();
        });

    return true;
}

//***************************************************************************
// @brief 보유한 모든 워커 스레드가 종료될 때까지 대기(join)한 뒤 목록을 비웁니다.
//***************************************************************************
void CThreadManager::JoinThreads()
{
    std::vector<std::thread> localThreads;
    {
        std::lock_guard<std::mutex> lock(_lock);

        _bShuttingDown.store(true);

        localThreads = std::move(_threads);
        _threads.clear();
    }

    for( auto& th : localThreads )
        if( th.joinable() )
            th.join();
}

//***************************************************************************
// @brief 관리 중인 스레드가 없을 때 종료 플래그를 해제합니다(재시작 허용).
// @return 해제했으면 true, 스레드가 남아 있어 유지했으면 false
//***************************************************************************
bool CThreadManager::ResetShutdown()
{
    std::lock_guard<std::mutex> lock(_lock);

    if( !_threads.empty() ) return false;

    _bShuttingDown.store(false);
    return true;
}

//***************************************************************************
// @brief 생성된 스레드 목록의 뒤에서부터 지정된 개수만큼 Join한 뒤 목록에서 제거합니다.
// @param count Join할 스레드 개수
//***************************************************************************
void CThreadManager::JoinLastThreads(size_t count)
{
    std::vector<std::thread> localThreads;
    {
        std::lock_guard<std::mutex> lock(_lock);

        if( count == 0 || _threads.empty() ) return;

        size_t actualCount = (std::min)(count, _threads.size());
        size_t startIndex = _threads.size() - actualCount;

        localThreads.reserve(actualCount);
        for( size_t i = startIndex; i < _threads.size(); ++i )
            localThreads.push_back(std::move(_threads[i]));

        _threads.erase(_threads.begin() + startIndex, _threads.end());
    }

    for( auto& th : localThreads )
        if( th.joinable() )
            th.join();
}

//***************************************************************************
// @brief 특정 인덱스(_threads 상의 위치)에 해당하는 스레드 하나를 Join합니다.
// @param index Join 대상 스레드의 현재 인덱스
//***************************************************************************
void CThreadManager::JoinThreadByIndex(size_t index)
{
    std::thread th;
    {
        std::lock_guard<std::mutex> lock(_lock);

        if( index >= _threads.size() ) return;
        if( !_threads[index].joinable() ) return;

        th = std::move(_threads[index]);
        _threads.erase(_threads.begin() + index); // 목록에서 제거
    }

    if( th.joinable() )
        th.join();
}

//***************************************************************************
// @brief 특정 스레드 ID를 가진 스레드를 찾아 Join합니다.
// @param threadId std::thread::get_id()로 얻은 대상 스레드의 ID
//***************************************************************************
void CThreadManager::JoinThreadById(std::thread::id threadId)
{
    std::thread th;
    {
        std::lock_guard<std::mutex> lock(_lock);

        auto it = std::find_if(_threads.begin(), _threads.end(),
            [&](std::thread& t) { return t.get_id() == threadId; });

        if( it != _threads.end() && it->joinable() )
        {
            th = std::move(*it);
            _threads.erase(it); // 목록에서 제거
        }
    }

    if( th.joinable() )
        th.join();
}

//***************************************************************************
// @brief 현재 관리 중인(아직 Join되지 않은) 스레드 총 개수를 반환합니다.
// @return 현재 관리 중인 스레드 개수
//***************************************************************************
size_t CThreadManager::GetThreadCount() const
{
    std::lock_guard<std::mutex> lock(_lock);
    return _threads.size();
}

//***************************************************************************
// @brief 스레드 진입 시 호출되어 스레드 로컬 ID(LThreadId)를 발급합니다.
//***************************************************************************
void CThreadManager::InitTLS()
{
    static std::atomic<uint32> SThreadId = 1;
    LThreadId = SThreadId.fetch_add(1);
}

//***************************************************************************
// @brief 스레드 종료 직전 호출되어 스레드 로컬 자원을 정리합니다.
//***************************************************************************
void CThreadManager::DestroyTLS()
{
#ifdef UC_MEMORY_H
    CMemory::FlushCurrentThreadCache();
#endif
}