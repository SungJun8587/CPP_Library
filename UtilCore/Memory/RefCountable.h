
//***************************************************************************
// RefCountable.h : interface for the RefCountable, TSharedPtr, and TWeakPtr classes.
//
//***************************************************************************

#ifndef __REFCOUNTABLE_H__
#define __REFCOUNTABLE_H__

#include <cassert>
#include <atomic>

// forward declaration
template<typename T> class TSharedPtr;
template<typename T> class TWeakPtr;

//***************************************************************************
// @struct RefControlBlock
// @brief 참조 횟수 관리를 위한 내부 제어 블록 구조체.
//
// @details
// 침입형(Intrusive) 구조에서 객체가 삭제된 후에도 약한 참조(Weak Ptr)가 
// 안전하게 존재 여부와 유효성을 확인할 수 있도록 분리된 제어 블록입니다.
// 강한 참조 횟수(refCount)와 약한 참조 횟수(weakCount)를 원자적으로 관리합니다.
//
// @note
// refCount는 객체 생성 시 1로 시작합니다. 이는 "객체를 최초로 감싸는
// TSharedPtr 한 개"를 이미 포함한 값이므로, 그 최초의 TSharedPtr는
// 추가로 AddRef를 호출하지 않습니다(TSharedPtr(T* ptr) 참고).
// 이후 복사/변환으로 소유자가 늘어날 때만 AddRef가 호출됩니다.
//***************************************************************************
struct RefControlBlock
{
    std::atomic<int32> refCount{ 1 };   // 강한 참조 횟수 (TSharedPtr 개수)
    std::atomic<int32> weakCount{ 1 };  // 약한 참조 횟수 (TWeakPtr 개수)
    std::atomic<bool>  claimed{ false };// 최초 raw pointer 소유권 선점 여부 (이중 wrapping 방지용)

    //***************************************************************************
    // @brief 강한 참조 횟수를 1 증가시킵니다.
    void AddRef() { ++refCount; }

    //***************************************************************************
    // @brief 약한 참조 횟수를 1 증가시킵니다.
    void AddWeakRef() { ++weakCount; }

    //***************************************************************************
    // @brief 약한 참조 횟수를 1 감소시키며, 강한 참조와 약한 참조가 모두 소멸하면 제어 블록을 해제합니다.
    void ReleaseWeakRef()
    {
        if( --weakCount == 0 )
        {
            delete this; // 제어 블록 메모리 해제
        }
    }
};

//***************************************************************************
// @class RefCountable
// @brief 참조 횟수(Reference Count) 기반의 메모리 관리를 위한 기저 클래스.
//
// @details
// 객체 스스로가 자신이 참조되고 있는 횟수를 관리할 수 있는 침입형(Intrusive) 베이스 클래스입니다.
// TSharedPtr과 TWeakPtr을 모두 지원하도록 내부 제어 블록(RefControlBlock)을 관리합니다.
//
// @example
// ```cpp
// class Monster : public RefCountable
// {
// public:
//     void TakeDamage(int damage) { /* ... */ }
// };
// 
// TSharedPtr<Monster> monster(new Monster());
// TWeakPtr<Monster> weakMonster = monster;
// if (TSharedPtr<Monster> locked = weakMonster.Lock())
// {
//     locked->TakeDamage(10);
// }
// ```
//***************************************************************************
class RefCountable
{
public:
    //***************************************************************************
    // @brief 객체를 생성하고 제어 블록을 할당하며 초기 참조 횟수를 1로 설정합니다.
    RefCountable()
    {
        _controlBlock = new RefControlBlock();
    }

    //***************************************************************************
    // @brief 가상 소멸자. 객체 파괴 시 제어 블록의 약한 참조 카운트를 정리합니다.
    virtual ~RefCountable()
    {
        if( _controlBlock )
        {
            _controlBlock->ReleaseWeakRef();
            _controlBlock = nullptr;
        }
    }

    //***************************************************************************
    // @brief 현재 강한 참조 횟수를 반환합니다.
    // @return int32 현재 강한 참조 횟수
    int32 GetRefCount() const { return _controlBlock ? _controlBlock->refCount.load() : 0; }

    //***************************************************************************
    // @brief 현재 약한 참조 횟수를 반환합니다.
    // @return int32 현재 약한 참조 횟수
    int32 GetWeakRefCount() const { return _controlBlock ? _controlBlock->weakCount.load() : 0; }

    //***************************************************************************
    // @brief 강한 참조 횟수를 1 증가시킵니다.
    // @return int32 증가된 이후의 강한 참조 횟수
    int32 AddRef()
    {
        if( _controlBlock )
            _controlBlock->AddRef();
        return GetRefCount();
    }

    //***************************************************************************
    // @brief 강한 참조 횟수를 1 감소시키며, 0이 될 경우 객체를 자동으로 소멸시킵니다.
    // @return int32 감소된 이후의 강한 참조 횟수
    int32 ReleaseRef()
    {
        if( !_controlBlock )
            return 0;

        int32 count = --_controlBlock->refCount;
        if( count == 0 )
        {
            delete this; // 객체 소멸
        }
        return count;
    }

    //***************************************************************************
    // @brief 내부 제어 블록 포인터를 반환합니다.
    // @return RefControlBlock* 제어 블록 포인터
    RefControlBlock* GetControlBlock() const { return _controlBlock; }

protected:
    RefControlBlock* _controlBlock = nullptr; // 메모리 및 참조 상태 관리 제어 블록
};

//***************************************************************************
// @class TSharedPtr
// @brief RefCountable 상속 객체를 위한 안전한 스마트 포인터 템플릿.
//
// @details
// 침입형 스마트 포인터로, 별도의 제어 블록 할당 오버헤드를 최소화하면서
// 복사, 이동, 그리고 상속 관계 간의 형변환을 안전하게 지원합니다.
//***************************************************************************
template<typename T>
class TSharedPtr
{
    template<typename U> friend class TSharedPtr;
    template<typename U> friend class TWeakPtr;

public:
    //***************************************************************************
    // @brief 빈 스마트 포인터를 생성합니다. (nullptr 초기화)
    TSharedPtr() = default;

    //***************************************************************************
    // @brief 원시 포인터를 받아 스마트 포인터를 생성합니다.
    // @param ptr 관리할 대상 원시 포인터
    // @note 새로 생성된 RefCountable 객체는 refCount가 이미 1로 시작하므로
    //       (그 1이 바로 이 최초의 TSharedPtr를 의미), 여기서는 AddRef를
    //       호출하지 않고 그대로 소유권만 가져옵니다. AddRef를 호출하면
    //       refCount가 2가 되어 마지막 TSharedPtr가 소멸해도 객체가
    //       해제되지 않는 누수가 발생합니다.
    // @warning 동일한 raw pointer로 TSharedPtr를 두 번 이상 생성하면 안 됩니다
    //          (이중 소유 → 이중 해제). control block의 claimed 플래그를 CAS로
    //          선점해 최초 1회만 소유권을 가져가도록 강제하며, 이미 다른
    //          TSharedPtr가 선점한 raw pointer로 다시 생성을 시도하면 assert로
    //          알리고 빈 포인터(nullptr)로 생성됩니다(디버그 빌드 기준).
    explicit TSharedPtr(T* ptr)
    {
        if( ptr )
        {
            RefControlBlock* block = ptr->GetControlBlock();
            bool expected = false;
            if( block && block->claimed.compare_exchange_strong(expected, true) )
            {
                _ptr = ptr;
                _controlBlock = block;
            }
            else
            {
                // 이미 다른 TSharedPtr가 이 raw pointer를 선점함 -> 이중 소유 방지
                assert(false && "TSharedPtr: 이 raw pointer는 이미 다른 TSharedPtr가 관리 중입니다.");
            }
        }
    }

    //***************************************************************************
    // @brief 복사 생성자. 대상 포인터와 제어 블록의 참조 횟수를 증가시킵니다.
    // @param rhs 복사할 다른 스마트 포인터
    TSharedPtr(const TSharedPtr& rhs) { Set(rhs._ptr, rhs._controlBlock); }

    //***************************************************************************
    // @brief 이동 생성자. 소유권을 안전하게 이전합니다.
    // @param rhs 이동할 다른 스마트 포인터
    TSharedPtr(TSharedPtr&& rhs) noexcept
    {
        _ptr = rhs._ptr;
        _controlBlock = rhs._controlBlock;
        rhs._ptr = nullptr;
        rhs._controlBlock = nullptr;
    }

    //***************************************************************************
    // @brief 상속 관계 변환을 지원하는 템플릿 복사 생성자.
    // @param rhs 변환할 다른 타입의 스마트 포인터
    template<typename U>
    TSharedPtr(const TSharedPtr<U>& rhs) { Set(static_cast<T*>(rhs._ptr), rhs._controlBlock); }

    //***************************************************************************
    // @brief 소멸자. 관리 중인 객체의 참조 횟수를 감소시킵니다.
    ~TSharedPtr() { Release(); }

public:
    //***************************************************************************
    // @brief 복사 대입 연산자.
    // @param rhs 대입할 다른 스마트 포인터
    // @return TSharedPtr& 자기 자신의 참조
    TSharedPtr& operator=(const TSharedPtr& rhs)
    {
        if( _ptr != rhs._ptr )
        {
            Release();
            Set(rhs._ptr, rhs._controlBlock);
        }
        return *this;
    }

    //***************************************************************************
    // @brief 이동 대입 연산자. (자기 자신 대입 방어 코드 포함)
    // @param rhs 이동할 다른 스마트 포인터
    // @return TSharedPtr& 자기 자신의 참조
    TSharedPtr& operator=(TSharedPtr&& rhs) noexcept
    {
        if( this != &rhs )
        {
            Release();
            _ptr = rhs._ptr;
            _controlBlock = rhs._controlBlock;
            rhs._ptr = nullptr;
            rhs._controlBlock = nullptr;
        }
        return *this;
    }

    bool        operator==(const TSharedPtr& rhs) const { return _ptr == rhs._ptr; }
    bool        operator==(T* ptr) const { return _ptr == ptr; }
    bool        operator!=(const TSharedPtr& rhs) const { return _ptr != rhs._ptr; }
    bool        operator!=(T* ptr) const { return _ptr != ptr; }
    bool        operator<(const TSharedPtr& rhs) const { return _ptr < rhs._ptr; }
    T* operator*() { return _ptr; }
    const T* operator*() const { return _ptr; }
    // 암시적 형변환 연산자 제거 완료 (.Get() 메서드 사용 권장)
    T* operator->() { return _ptr; }
    const T* operator->() const { return _ptr; }

    //***************************************************************************
    // @brief 포인터가 비어있는지 확인합니다.
    // @return true 비어있음, false 유효한 객체를 가리킴
    bool        IsNull() const { return _ptr == nullptr; }

    //***************************************************************************
    // @brief 관리 중인 원시 포인터를 반환합니다.
    // @return T* 원시 포인터
    T* Get() const { return _ptr; }

private:
    //***************************************************************************
    // @brief 이미 참조 카운트가 증가된 상태에서 소유권만 넘겨받는 내부 전용 생성자.
    // @details TWeakPtr::Lock()에서 compare_exchange로 refCount를 원자적으로
    //          증가시킨 직후 사용합니다. 여기서 다시 AddRef를 하면 이중 증가가
    //          되므로 절대 호출하지 않습니다.
    TSharedPtr(T* ptr, RefControlBlock* block, bool /*alreadyRefCounted*/) noexcept
        : _ptr(ptr), _controlBlock(block)
    {
    }

    //***************************************************************************
    // @brief 포인터와 제어 블록을 설정하고 대상 객체의 강한 참조 횟수를 증가시킵니다.
    // @param ptr 설정할 원시 포인터
    // @param block 연동할 제어 블록 (기본값은 객체로부터 획득)
    inline void Set(T* ptr, RefControlBlock* block = nullptr)
    {
        _ptr = ptr;
        if( ptr )
        {
            _controlBlock = block ? block : ptr->GetControlBlock();
            if( _controlBlock )
                _controlBlock->AddRef();
        }
    }

    //***************************************************************************
    // @brief 현재 관리 중인 객체의 참조를 해제하고 포인터를 초기화합니다.
    inline void Release()
    {
        if( _ptr != nullptr )
        {
            _ptr->ReleaseRef();
            _ptr = nullptr;
            _controlBlock = nullptr;
        }
    }

private:
    T* _ptr = nullptr;         // 관리 중인 원시 포인터
    RefControlBlock* _controlBlock = nullptr;// 참조 횟수 관리용 제어 블록
};

//***************************************************************************
// @class TWeakPtr
// @brief 순환 참조 방지를 위한 약한 포인터 템플릿.
//
// @details
// 객체의 소유권을 주장하지 않고 관찰만 수행하며, 객체가 소멸된 상태여도
// 안전하게 유효성 검증을 거친 뒤 `Lock()`을 통해 `TSharedPtr`로 전환하여 사용할 수 있습니다.
//***************************************************************************
template<typename T>
class TWeakPtr
{
public:
    //***************************************************************************
    // @brief 빈 약한 포인터를 생성합니다.
    TWeakPtr() = default;

    //***************************************************************************
    // @brief TSharedPtr로부터 약한 포인터를 생성합니다. (약한 참조 카운트 증가)
    // @param rhs 관찰할 대상 스마트 포인터
    TWeakPtr(const TSharedPtr<T>& rhs)
    {
        _ptr = rhs._ptr;
        _controlBlock = rhs._controlBlock;
        if( _controlBlock )
        {
            _controlBlock->AddWeakRef();
        }
    }

    //***************************************************************************
    // @brief 복사 생성자. 약한 참조 카운트를 증가시킵니다.
    // @param rhs 복사할 다른 약한 포인터
    TWeakPtr(const TWeakPtr& rhs)
    {
        _ptr = rhs._ptr;
        _controlBlock = rhs._controlBlock;
        if( _controlBlock )
        {
            _controlBlock->AddWeakRef();
        }
    }

    //***************************************************************************
    // @brief 이동 생성자. 소유권을 안전하게 이전합니다.
    // @param rhs 이동할 다른 약한 포인터
    TWeakPtr(TWeakPtr&& rhs) noexcept
    {
        _ptr = rhs._ptr;
        _controlBlock = rhs._controlBlock;
        rhs._ptr = nullptr;
        rhs._controlBlock = nullptr;
    }

    //***************************************************************************
    // @brief 소멸자. 약한 참조 카운트를 감소시킵니다.
    ~TWeakPtr()
    {
        Release();
    }

    //***************************************************************************
    // @brief TSharedPtr로부터 값을 대입받습니다.
    // @param rhs 대입할 스마트 포인터
    // @return TWeakPtr& 자기 자신의 참조
    TWeakPtr& operator=(const TSharedPtr<T>& rhs)
    {
        Release();
        _ptr = rhs._ptr;
        _controlBlock = rhs._controlBlock;
        if( _controlBlock )
        {
            _controlBlock->AddWeakRef();
        }
        return *this;
    }

    //***************************************************************************
    // @brief 복사 대입 연산자.
    // @param rhs 대입할 다른 약한 포인터
    // @return TWeakPtr& 자기 자신의 참조
    TWeakPtr& operator=(const TWeakPtr& rhs)
    {
        if( this != &rhs )
        {
            Release();
            _ptr = rhs._ptr;
            _controlBlock = rhs._controlBlock;
            if( _controlBlock )
            {
                _controlBlock->AddWeakRef();
            }
        }
        return *this;
    }

    //***************************************************************************
    // @brief 이동 대입 연산자. (자기 자신 대입 방어 코드 포함)
    // @param rhs 이동할 다른 약한 포인터
    // @return TWeakPtr& 자기 자신의 참조
    TWeakPtr& operator=(TWeakPtr&& rhs) noexcept
    {
        if( this != &rhs )
        {
            Release();
            _ptr = rhs._ptr;
            _controlBlock = rhs._controlBlock;
            rhs._ptr = nullptr;
            rhs._controlBlock = nullptr;
        }
        return *this;
    }

    //***************************************************************************
    // @brief 객체가 여전히 유효한지 확인하고 TSharedPtr로 변환합니다.
    // @details refCount가 0이 아닌 동안에만 원자적으로(compare_exchange) 증가시킨
    //          뒤 소유권을 넘깁니다. load() 후 별도로 AddRef하는 방식은 그 사이에
    //          다른 스레드가 마지막 참조를 반환해 객체를 delete할 수 있어
    //          use-after-free가 발생할 수 있으므로 사용하지 않습니다.
    // @return TSharedPtr<T> 유효한 경우 객체를 관리하는 스마트 포인터, 아닐 경우 빈 포인터
    TSharedPtr<T> Lock() const
    {
        if( _controlBlock == nullptr )
            return TSharedPtr<T>();

        int32 oldCount = _controlBlock->refCount.load(std::memory_order_relaxed);
        while( oldCount != 0 )
        {
            if( _controlBlock->refCount.compare_exchange_weak(
                oldCount, oldCount + 1,
                std::memory_order_acq_rel, std::memory_order_relaxed) )
            {
                // 이미 refCount를 증가시켰으므로 AddRef를 다시 호출하지 않는
                // 내부 전용 생성자를 사용합니다.
                return TSharedPtr<T>(_ptr, _controlBlock, true);
            }
            // compare_exchange_weak 실패 시 oldCount가 최신값으로 갱신되어 루프 재시도
        }
        return TSharedPtr<T>();
    }

    //***************************************************************************
    // @brief 대상 객체가 소멸되었거나 비어있는지 확인합니다.
    // @return true 비어있거나 소멸됨, false 유효함
    bool IsNull() const
    {
        return _controlBlock == nullptr || _controlBlock->refCount.load() == 0;
    }

private:
    //***************************************************************************
    // @brief 약한 참조 제어권을 해제하고 포인터를 초기화합니다.
    void Release()
    {
        if( _controlBlock != nullptr )
        {
            _controlBlock->ReleaseWeakRef();
            _ptr = nullptr;
            _controlBlock = nullptr;
        }
    }

private:
    T* _ptr = nullptr;         // 관찰 중인 원시 포인터 (객체 소멸 후 댕글링 상태일 수 있음)
    RefControlBlock* _controlBlock = nullptr;// 유효성 검증용 제어 블록
};

#endif // __REFCOUNTABLE_H__