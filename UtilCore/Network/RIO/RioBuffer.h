
//**********************************************************************************************************************
// RioBuffer.h : interface for the CRioBuffer class.
//
//**********************************************************************************************************************

#ifndef __RIOBUFFER_H__
#define __RIOBUFFER_H__

#ifndef __RIOCOMMON_H__
#include <Network/RIO/RioCommon.h>
#endif

#ifndef __LOCKFREESLOTSTACK_H__
#include <Containers/Stack/LockFreeSlotStack.h>
#endif

//***************************************************************************
// @class CRioBuffer
// @brief Windows RIO Registered Buffer를 관리하는 lock-free 슬롯 버퍼.
//
// @details
//      하나의 연속된 메모리 영역을 RIORegisterBuffer()로 등록하고,
//      고정 크기의 슬롯으로 분할하여 런타임에서 lock-free 방식으로
//      슬롯을 할당/반납합니다.
//
//      실제 슬롯의 Pop/Push는 CLockFreeSlotStack이 담당합니다.
//
//***************************************************************************
//
// [RIO Function Table Ownership]
//
//      CRioBuffer는 RIO_EXTENSION_FUNCTION_TABLE을 소유하지 않습니다.
//
//      CRioCore
//          |
//          +--> RIO_EXTENSION_FUNCTION_TABLE 소유
//                         |
//                         | const pointer
//                         v
//                     CRioBuffer
//
//      따라서 CRioBuffer가 살아있는 동안 _rioTable이 가리키는
//      RIO_EXTENSION_FUNCTION_TABLE의 lifetime은 반드시 보장되어야 합니다.
//
//      일반적인 사용 관계:
//
//          CRioCore
//              |
//              +--> CRioBuffer
//
//      즉 CRioCore가 CRioBuffer보다 먼저 파괴되면 안 됩니다.
//
//***************************************************************************
//
// [Concurrency Contract]
//
//      CRioBuffer는 다음과 같은 lifecycle / hot-path lock ordering을
//      사용합니다.
//
//      Initialize()
//          |
//          +--> _lifecycleMutex (exclusive)
//
//      Shutdown()
//          |
//          +--> _lifecycleMutex (exclusive)
//
//      AllocSlot()
//          |
//          +--> _lifecycleMutex (shared)
//              +--> CLockFreeSlotStack::Pop()
//
//      FreeSlot()
//          |
//          +--> _lifecycleMutex (shared)
//              +--> slot state CAS
//              +--> CLockFreeSlotStack::Push()
//
//      즉:
//
//          Initialize / Shutdown
//              = exclusive lock
//
//          AllocSlot / FreeSlot
//              = shared lock
//
//      이 구조를 통해 Shutdown()이 _allocatedCount == 0을 검사할 때
//      동시에 진행 중인 AllocSlot()/FreeSlot()이 존재하지 않음을 보장합니다.
//
//***************************************************************************
//
// [IMPORTANT LIFETIME RULE]
//
//      AllocSlot() / FreeSlot()은 반드시 CRioBuffer 객체의 lifetime이
//      유지되는 동안 호출되어야 합니다.
//
//      또한 _rioTable은 CRioCore가 소유하므로 CRioBuffer보다 CRioCore가
//      먼저 파괴되어서는 안 됩니다.
//
//***************************************************************************
//
// [Shutdown Contract]
//
//      Shutdown()은 다음 조건을 만족해야 실제 RIO buffer resource를
//      파괴합니다.
//
//          1. _initialized == true
//          2. _allocatedCount == 0
//          3. 모든 AllocSlot()/FreeSlot() 호출이 완료됨
//             (exclusive lifecycle lock으로 보장)
//          4. RIO buffer registration이 유효함
//
//      allocated slot이 남아 있으면 Shutdown()은 실패하고 resource를
//      유지합니다.
//
//***************************************************************************
//
// [Slot State]
//
//      Free:
//          slot을 할당할 수 있음
//
//      Allocated:
//          현재 caller가 소유하고 있음
//
//      AllocSlot():
//          Free -> Allocated CAS
//
//      FreeSlot():
//          Allocated -> Free CAS
//
//      따라서 동일 slot에 대한 double-free 및 stale free를 검출합니다.
//
//***************************************************************************
class CRioBuffer
{
public:
    CRioBuffer() noexcept;
    ~CRioBuffer() noexcept;

    CRioBuffer(const CRioBuffer&) = delete;
    CRioBuffer& operator=(const CRioBuffer&) = delete;

    CRioBuffer(CRioBuffer&&) = delete;
    CRioBuffer& operator=(CRioBuffer&&) = delete;

public:
    bool Initialize(const RIO_EXTENSION_FUNCTION_TABLE* rioTable, uint32_t slotCount, uint32_t slotSize, size_t alignment = 64) noexcept;
    bool Shutdown() noexcept;

public:
    bool AllocSlot(uint32_t& outSlotIndex) noexcept;
    bool FreeSlot(uint32_t slotIndex) noexcept;

public:
    bool GetRioBuffer(uint32_t slotIndex, RIO_BUF& outBuffer) const noexcept;
    void* GetSlotAddress(uint32_t slotIndex) noexcept;
    const void* GetSlotAddress(uint32_t slotIndex) const noexcept;

public:
    //***************************************************************************
    // @brief RIO 등록 버퍼 ID를 반환합니다.
    //***************************************************************************
    RIO_BUFFERID GetBufferId() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(_lifecycleMutex);
        return _bufferId;
    }

    //***************************************************************************
    // @brief 전체 슬롯 개수를 반환합니다.
    //***************************************************************************
    uint32_t GetSlotCount() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(_lifecycleMutex);
        return _slotCount;
    }

    //***************************************************************************
    // @brief 개별 슬롯 크기를 반환합니다.
    //***************************************************************************
    uint32_t GetSlotSize() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(_lifecycleMutex);
        return _slotSize;
    }

    //***************************************************************************
    // @brief 전체 등록 버퍼 크기를 반환합니다.
    //***************************************************************************
    size_t GetTotalSize() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(_lifecycleMutex);
        return _totalSize;
    }

    //***************************************************************************
    // @brief 현재 할당된 슬롯 개수를 반환합니다.
    //***************************************************************************
    uint32_t GetAllocatedCount() const noexcept
    {
        return _allocatedCount.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // @brief 현재 사용 가능한 슬롯 개수를 반환합니다.
    //***************************************************************************
    uint32_t GetFreeCount() const noexcept;

    //***************************************************************************
    // @brief 초기화 여부를 반환합니다.
    //***************************************************************************
    bool IsInitialized() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(_lifecycleMutex);
        return _initialized;
    }

    bool IsSlotAllocated(uint32_t slotIndex) const noexcept;

private:
    static bool IsPowerOfTwo(size_t value) noexcept;
    static bool IsValidAlignment(size_t alignment) noexcept;

    bool ValidateSlotIndex(uint32_t slotIndex) const noexcept;
    bool ValidateBufferParameters(uint32_t slotCount, uint32_t slotSize, size_t alignment) const noexcept;
    bool ValidateRioTable(const RIO_EXTENSION_FUNCTION_TABLE* rioTable) const noexcept;

    bool AllocateMemory(size_t totalSize, size_t alignment) noexcept;
    void ReleaseMemory() noexcept;

    bool RegisterBuffer() noexcept;
    void UnregisterBuffer() noexcept;

    void ResetRuntimeState() noexcept;

private:
    // 객체 라이프사이클(Initialize/Shutdown vs Alloc/Free) 동기화를 위한 락 (Shared: Alloc/Free, Exclusive: Init/Shutdown)
    mutable std::shared_mutex _lifecycleMutex;

    // CRioCore가 소유하는 RIO 함수 테이블을 참조한다.
    // CRioBuffer는 소유하지 않는다.
    const RIO_EXTENSION_FUNCTION_TABLE* _rioTable{ nullptr };

    void* _buffer{ nullptr };                       // RIO로 등록할 할당된 연속 메모리 블록의 시작 주소
    RIO_BUFFERID _bufferId{ RIO_INVALID_BUFFERID }; // RIORegisterBuffer()로 등록 후 발급받은 Windows RIO 버퍼 핸들/식별자

    //***************************************************************************
    // 슬롯 설정 및 메모리 크기 정보
    //***************************************************************************
    uint32_t _slotCount{ 0 };  // 버퍼 내 전체 슬롯 개수
    uint32_t _slotSize{ 0 };   // 개별 슬롯 1개의 크기 (Bytes)
    size_t _totalSize{ 0 };    // 전체 메모리 할당 크기 (Bytes)
    size_t _alignment{ 0 };    // 메모리 바이트 정렬 단위 (예: 64 Bytes)
    
    std::unique_ptr<std::atomic<uint8_t>[]> _slotState; // 각 슬롯의 현재 할당 상태(Free / Allocated)를 CAS로 추적하여 Double-Free를 검출하는 원자적 상태 배열
    std::unique_ptr<CLockFreeSlotStack> _freeStack;     // 사용 가능한 슬롯 인덱스(0 ~ _slotCount - 1)를 Pop/Push 방식으로 관리하는 Lock-Free 스택
    std::atomic<uint32_t> _allocatedCount{ 0 };         // 현재 외부에서 할당하여 사용 중인 슬롯의 총 개수 (원자적 카운터)
    
    bool _initialized{ false };     // 버퍼 정상 초기화 완료 여부 플래그 (_lifecycleMutex 보호 하에 접근)
};

#endif // ndef __RIOBUFFER_H__