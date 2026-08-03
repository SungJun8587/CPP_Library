
//***************************************************************************
// BlockingTaskQueue.h : interface for the CBlockingTaskQueue class.
//
//***************************************************************************

#ifndef __BLOCKINGTASKQUEUE_H__
#define __BLOCKINGTASKQUEUE_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef	__ALLOCATOR_H__
#include <Memory/Allocator.h>
#endif

template<typename T>
class CBlockingTaskQueue
{
public:
    void Push(T item)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(std::move(item));
        }
        _cv.notify_one();
    }

    void PushBatch(std::vector<T>& items)
    {
        if( items.empty() )
            return;

        {
            std::lock_guard<std::mutex> lock(_mutex);
            for( auto& item : items )
                _queue.push(std::move(item));
        }
        _cv.notify_all();
        items.clear();
    }

    bool Pop(T& out)
    {
        std::unique_lock<std::mutex> lock(_mutex);

        _cv.wait(lock, [this]() {
            return !_queue.empty() || _producerDone.load();
            });

        if( _queue.empty() && _producerDone.load() )
            return false;

        out = std::move(_queue.front());
        _queue.pop();
        return true;
    }

    void SetProducerDone()
    {
        _producerDone.store(true);
        _cv.notify_all();
    }

    CBlockingTaskQueue() = default;
    CBlockingTaskQueue(const CBlockingTaskQueue&) = delete;
    CBlockingTaskQueue& operator=(const CBlockingTaskQueue&) = delete;

private:
    std::queue<T>           _queue;
    std::mutex               _mutex;
    std::condition_variable  _cv;
    std::atomic<bool>        _producerDone{ false };
};

#endif // ndef __BLOCKINGTASKQUEUE_H__