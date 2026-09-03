
//***************************************************************************
// ClusterSpinMap.h : interface for the CClusterSpinMap class.
//
//***************************************************************************

#ifndef UC_CLUSTERSPINMAP_H
#define UC_CLUSTERSPINMAP_H

#include <BaseRedefineDataType.h>
#include <Thread/PlatformLock.h>

#include <map>
#include <functional>
#include <utility>
#include <cassert>

//***************************************************************************
// @class CClusterSpinMap
// @brief Red-Black Tree(O(log N)) 기반의 클러스터링 분산 스핀 맵 클래스
// @details 
// [특징 및 사용 권장 대상]
//   1. 데이터 정렬 필요성: Key 순서에 따른 정렬 상태 유지 및 범위 검색(Range Query)이 필요한 경우에 적합합니다.
//   2. 안정적인 성능: 해시 충돌 위험 없이 최악의 경우에도 O(log N)의 시간 복잡도를 보장합니다.
//   3. const 안전성: const 참조 및 const 멤버 함수를 완벽히 지원하여 임시 객체/Rvalue 넘김이 가능합니다.
// [요구 조건]
//   - Key 타입에 `<` (Less Than) 연산자가 정의되어 있어야 합니다. (클러스터 배치용 std::hash도 필요)
// [주의 사항]
//   - 단순 Key 조회/삽입 작업만 수행 시 CClusterSpinUnorderedMap(O(1)) 대비 성능 및 락 경합 면에서 불리할 수 있습니다.
//
// [클러스터 개수(nClusterCnt) 최적화 가이드 및 선정 이유 (예: 16개 기준)]
//   1. 2의 제곱수 최적화: 16 등 2의 제곱수 크기로 설정 시, 내부 클러스터 인덱스 연산(hash % nClusterCnt)을 
//      최적화 컴파일러가 나눗셈 대신 비트 연산(hash & (nClusterCnt - 1))으로 최적화할 가능성이 높습니다
//      (단, 언어 차원에서 보장되는 것은 아니며 nClusterCnt가 컴파일타임 상수이므로 그 자체로도 충분히 최적화됩니다).
//   2. 락 경합 대폭 완화: 단일 락 구조 대비 동시 접근 시 충돌 확률을 클러스터 개수 수준으로 대폭 줄여줍니다.
//   3. 코어 구조와의 조화: 일반적인 8~16코어(하이퍼스레딩 포함 16~32스레드) 상용 서버 환경에서 
//      워커 스레드들이 서로 다른 락을 참조할 확률을 높여 병목을 효과적으로 해소합니다.
//   4. 락 배열은 캐시라인(64byte) 단위로 패딩되어 인접 클러스터 락 간 false sharing을 방지합니다.
// [예시]
//   - 대규모 하이엔드 서버 환경(32코어 이상, 수천 명 동접): 32 또는 64 권장
//   - 소규모 서버 또는 테스트 환경(2 ~ 4코어): 8 또는 16 권장
// [주의 사항]
//   - getSize()/ClearObjectMap()은 각 클러스터 단위로는 안전하지만, 전체 클러스터를 아우르는
//     하나의 순간을 나타내는 atomic snapshot은 아닙니다(클러스터별로 순차적으로 락을 걸고 풉니다).
//   - WriteLockByIdx/ReadLockByIdx로 여러 클러스터의 락을 동시에 보유해야 하는 경우,
//     교착(deadlock)을 피하려면 반드시 클러스터 인덱스 오름차순으로 획득해야 합니다.
//*************************************************************************** 
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock = true, typename Hash = std::hash<T1>>
class CClusterSpinMap
{
	static_assert(nClusterCnt > 0, "nClusterCnt must be greater than 0");

public:
	typedef CMap< T1, T2 >                 ObjectMap;
	typedef std::pair< const T1, T2 >       ObjectMapPair;

	CClusterSpinMap(void) = default;
	// 상속을 위한 base class로 설계되지 않았고(복사/이동도 금지) 다형 소멸이 필요 없으므로
	// virtual 제거 — vtable 포인터/간접 호출 비용을 없앱니다.
	~CClusterSpinMap(void) = default;

	// PRWLock 배열을 멤버로 갖는 구조이므로 복사/이동 시 락 상태가 깨질 수 있어 금지
	CClusterSpinMap(const CClusterSpinMap&) = delete;
	CClusterSpinMap& operator=(const CClusterSpinMap&) = delete;
	CClusterSpinMap(CClusterSpinMap&&) = delete;
	CClusterSpinMap& operator=(CClusterSpinMap&&) = delete;

public:
	size_t      getSize();
	[[nodiscard]] bool	InsertObject(const T1& key, T2 object);
	[[nodiscard]] bool	InsertObject(T1&& key, T2 object);
	T2          FindObject(const T1& key);
	[[nodiscard]] bool	FindObject(const T1& key, T2& object);
	[[nodiscard]] bool	EraseObject(const T1& key);

	//***************************************************************************
	// @brief 클러스터 인덱스로 내부 해시맵 참조를 반환합니다. (전체 순회용)
	// @param idx 클러스터 인덱스 (0 ~ nClusterCnt - 1)
	// @return ObjectMap& 해당 클러스터의 맵 참조
	//***************************************************************************
	ObjectMap& GetClusterMapByIdx(__int32 idx) {
		assert(idx >= 0 && idx < nClusterCnt);
		return m_ObjectMaps[idx];
	}
	const ObjectMap& GetClusterMapByIdx(__int32 idx) const {
		assert(idx >= 0 && idx < nClusterCnt);
		return m_ObjectMaps[idx];
	}

	//***************************************************************************
	// @brief 클러스터 인덱스로 직접 쓰기 락을 획득합니다. (외부 제어용)
	// @details bInnerLock 설정과 무관하게 항상 실제로 락을 겁니다.
	//          bInnerLock=false 상태에서 GetClusterMapByIdx/GetObjectMap으로
	//          raw 맵에 접근할 때 반드시 이 메서드로 감싸야 합니다.
	//***************************************************************************
	void WriteLockByIdx(__int32 idx, const char* name = nullptr) {
		lockWrite(idx, name);
	}

	//***************************************************************************
	// @brief 클러스터 인덱스로 직접 쓰기 락을 해제합니다. (외부 제어용, 항상 동작)
	//***************************************************************************
	void WriteUnlockByIdx(__int32 idx, const char* name = nullptr) {
		unlockWrite(idx, name);
	}

	//***************************************************************************
	// @brief 클러스터 인덱스로 직접 읽기 락을 획득합니다. (외부 제어용, 항상 동작)
	//***************************************************************************
	void ReadLockByIdx(__int32 idx, const char* name = nullptr) {
		lockRead(idx, name);
	}

	//***************************************************************************
	// @brief 클러스터 인덱스로 직접 읽기 락을 해제합니다. (외부 제어용, 항상 동작)
	//***************************************************************************
	void ReadUnlockByIdx(__int32 idx, const char* name = nullptr) {
		unlockRead(idx, name);
	}

	//***************************************************************************
	// @brief 키에 해당하는 클러스터의 읽기 락을 획득합니다. (외부 제어용, 항상 동작)
	//***************************************************************************
	void ReadLock(const T1& key, const char* name = nullptr) {
		lockRead(getClusterIdx(key), name);
	}

	//***************************************************************************
	// @brief 키에 해당하는 클러스터의 읽기 락을 해제합니다. (외부 제어용, 항상 동작)
	//***************************************************************************
	void ReadUnlock(const T1& key, const char* name = nullptr) {
		unlockRead(getClusterIdx(key), name);
	}

	//***************************************************************************
	// @brief 키에 해당하는 클러스터의 쓰기 락을 획득합니다. (외부 제어용, 항상 동작)
	//***************************************************************************
	void WriteLock(const T1& key, const char* name = nullptr) {
		lockWrite(getClusterIdx(key), name);
	}

	//***************************************************************************
	// @brief 키에 해당하는 클러스터의 쓰기 락을 해제합니다. (외부 제어용, 항상 동작)
	//***************************************************************************
	void WriteUnlock(const T1& key, const char* name = nullptr) {
		unlockWrite(getClusterIdx(key), name);
	}

	//***************************************************************************
	// @brief 키에 해당하는 클러스터의 내부 맵 참조를 반환합니다.
	//***************************************************************************
	ObjectMap& GetObjectMap(const T1& key) {
		return m_ObjectMaps[getClusterIdx(key)];
	}
	const ObjectMap& GetObjectMap(const T1& key) const {
		return m_ObjectMaps[getClusterIdx(key)];
	}

	//***************************************************************************
	// @brief 설정된 전체 클러스터 개수를 반환합니다.
	//***************************************************************************
	__int32 GetClusterCnt(void) const {
		return nClusterCnt;
	}

	//***************************************************************************
	// @brief 전체 클러스터를 순회하며 맵 데이터를 비웁니다.
	//***************************************************************************
	void ClearObjectMap(void) {
		clearObjectMap();
	}

	//***************************************************************************
	// @brief bInnerLock=false 환경에서 GetObjectMap()으로 raw 맵을 직접 다룰 때 쓰는
	//        공개 RAII 락 가드. ReadLock(key)/ReadUnlock(key)을 예외 안전하게 감쌉니다.
	//***************************************************************************
	class ReadLockGuard
	{
	public:
		ReadLockGuard(CClusterSpinMap& owner, const T1& key, const char* name = nullptr)
			: m_owner(owner), m_idx(owner.getClusterIdx(key)), m_name(name)
		{
			m_owner.lockRead(m_idx, m_name);
		}
		~ReadLockGuard()
		{
			m_owner.unlockRead(m_idx, m_name);
		}
		ReadLockGuard(const ReadLockGuard&) = delete;
		ReadLockGuard& operator=(const ReadLockGuard&) = delete;
	private:
		CClusterSpinMap& m_owner;
		__int32 m_idx;
		const char* m_name;
	};

	//***************************************************************************
	// @brief WriteLock(key)/WriteUnlock(key)을 예외 안전하게 감싸는 공개 RAII 가드.
	//***************************************************************************
	class WriteLockGuard
	{
	public:
		WriteLockGuard(CClusterSpinMap& owner, const T1& key, const char* name = nullptr)
			: m_owner(owner), m_idx(owner.getClusterIdx(key)), m_name(name)
		{
			m_owner.lockWrite(m_idx, m_name);
		}
		~WriteLockGuard()
		{
			m_owner.unlockWrite(m_idx, m_name);
		}
		WriteLockGuard(const WriteLockGuard&) = delete;
		WriteLockGuard& operator=(const WriteLockGuard&) = delete;
	private:
		CClusterSpinMap& m_owner;
		__int32 m_idx;
		const char* m_name;
	};

	//***************************************************************************
	// @brief ReadLockByIdx(idx)/ReadUnlockByIdx(idx)를 예외 안전하게 감싸는 공개 RAII 가드.
	//        GetClusterMapByIdx()와 조합한 전체 순회 작업에 사용합니다.
	//***************************************************************************
	class ReadLockGuardByIdx
	{
	public:
		ReadLockGuardByIdx(CClusterSpinMap& owner, __int32 idx, const char* name = nullptr)
			: m_owner(owner), m_idx(idx), m_name(name)
		{
			m_owner.lockRead(m_idx, m_name);
		}
		~ReadLockGuardByIdx()
		{
			m_owner.unlockRead(m_idx, m_name);
		}
		ReadLockGuardByIdx(const ReadLockGuardByIdx&) = delete;
		ReadLockGuardByIdx& operator=(const ReadLockGuardByIdx&) = delete;
	private:
		CClusterSpinMap& m_owner;
		__int32 m_idx;
		const char* m_name;
	};

	//***************************************************************************
	// @brief WriteLockByIdx(idx)/WriteUnlockByIdx(idx)를 예외 안전하게 감싸는 공개 RAII 가드.
	//        여러 클러스터를 동시에 잠글 경우, 클래스 상단 주석대로 idx 오름차순으로
	//        가드를 생성해야 교착을 피할 수 있습니다.
	//***************************************************************************
	class WriteLockGuardByIdx
	{
	public:
		WriteLockGuardByIdx(CClusterSpinMap& owner, __int32 idx, const char* name = nullptr)
			: m_owner(owner), m_idx(idx), m_name(name)
		{
			m_owner.lockWrite(m_idx, m_name);
		}
		~WriteLockGuardByIdx()
		{
			m_owner.unlockWrite(m_idx, m_name);
		}
		WriteLockGuardByIdx(const WriteLockGuardByIdx&) = delete;
		WriteLockGuardByIdx& operator=(const WriteLockGuardByIdx&) = delete;
	private:
		CClusterSpinMap& m_owner;
		__int32 m_idx;
		const char* m_name;
	};

protected:
	//***************************************************************************
	// @brief 키를 바탕으로 맵이 위치할 클러스터 인덱스를 계산합니다.
	// @details 클러스터 "배치"는 해시로 분산시키고, 클러스터 내부 정렬(std::map)은
	//          T1의 operator< 로 이루어지므로 T1이 정수형이 아니어도 동작합니다.
	//***************************************************************************
	__int32 getClusterIdx(const T1& key) const {
		return static_cast<__int32>(Hash{}(key) % static_cast<size_t>(nClusterCnt));
	}

	// 실제 락 조작 (bInnerLock 값과 무관하게 항상 실행) — WriteLockByIdx 등
	// "외부 제어용" 공개 API 및 아래의 내부 자동 락 헬퍼가 공통으로 사용합니다.
	void lockRead(__int32 nClusterIdx, const char* name = nullptr) {
		assert(nClusterIdx >= 0 && nClusterIdx < nClusterCnt);
		m_ObjectLocks[nClusterIdx].lock.ReadLock(name);
	}
	void unlockRead(__int32 nClusterIdx, const char* name = nullptr) {
		assert(nClusterIdx >= 0 && nClusterIdx < nClusterCnt);
		m_ObjectLocks[nClusterIdx].lock.ReadUnlock(name);
	}
	void lockWrite(__int32 nClusterIdx, const char* name = nullptr) {
		assert(nClusterIdx >= 0 && nClusterIdx < nClusterCnt);
		m_ObjectLocks[nClusterIdx].lock.WriteLock(name);
	}
	void unlockWrite(__int32 nClusterIdx, const char* name = nullptr) {
		assert(nClusterIdx >= 0 && nClusterIdx < nClusterCnt);
		m_ObjectLocks[nClusterIdx].lock.WriteUnlock(name);
	}

	// 내부 자동 락 (InsertObject/FindObject/EraseObject/getSize/ClearObjectMap 전용)
	// bInnerLock=false 이면 컴파일 타임에 완전히 제거됩니다.
	// bInnerLock은 템플릿 비타입 파라미터(컴파일타임 상수)이므로 if constexpr 사용.
	void readLock(__int32 nClusterIdx, const char* name = nullptr) {
		if constexpr( bInnerLock ) lockRead(nClusterIdx, name);
	}
	void readUnlock(__int32 nClusterIdx, const char* name = nullptr) {
		if constexpr( bInnerLock ) unlockRead(nClusterIdx, name);
	}
	void writeLock(__int32 nClusterIdx, const char* name = nullptr) {
		if constexpr( bInnerLock ) lockWrite(nClusterIdx, name);
	}
	void writeUnlock(__int32 nClusterIdx, const char* name = nullptr) {
		if constexpr( bInnerLock ) unlockWrite(nClusterIdx, name);
	}

	void clearObjectMap(void);

	//***************************************************************************
	// @brief 내부 자동 락(readLock/writeLock)을 예외 안전하게 관리하는 최소 RAII 가드.
	// @details try_emplace/find/erase 및 T1/T2/Hash의 사용자 정의 연산(operator=,
	//          비교 등)에서 예외가 발생해도 스코프 종료 시 항상 unlock이 보장됩니다.
	//          bInnerLock=false면 readLock/writeLock 자체가 no-op이므로 가드 비용도 없습니다.
	//***************************************************************************
	class InnerReadGuard
	{
	public:
		InnerReadGuard(CClusterSpinMap& owner, __int32 idx, const char* name)
			: m_owner(owner), m_idx(idx), m_name(name)
		{
			m_owner.readLock(m_idx, m_name);
		}
		~InnerReadGuard()
		{
			m_owner.readUnlock(m_idx, m_name);
		}
		InnerReadGuard(const InnerReadGuard&) = delete;
		InnerReadGuard& operator=(const InnerReadGuard&) = delete;
	private:
		CClusterSpinMap& m_owner;
		__int32 m_idx;
		const char* m_name;
	};

	class InnerWriteGuard
	{
	public:
		InnerWriteGuard(CClusterSpinMap& owner, __int32 idx, const char* name)
			: m_owner(owner), m_idx(idx), m_name(name)
		{
			m_owner.writeLock(m_idx, m_name);
		}
		~InnerWriteGuard()
		{
			m_owner.writeUnlock(m_idx, m_name);
		}
		InnerWriteGuard(const InnerWriteGuard&) = delete;
		InnerWriteGuard& operator=(const InnerWriteGuard&) = delete;
	private:
		CClusterSpinMap& m_owner;
		__int32 m_idx;
		const char* m_name;
	};

protected:
	// 캐시라인 단위로 패딩하여 인접 클러스터 락 간 false sharing을 방지합니다.
	struct alignas(64) PaddedLock
	{
		PRWLock lock;
	};

	ObjectMap   m_ObjectMaps[nClusterCnt];   // 클러스터별로 데이터를 저장하는 맵 배열 (총 nClusterCnt개)
	PaddedLock  m_ObjectLocks[nClusterCnt];  // 각 클러스터의 동시성 제어를 위한 읽기/쓰기 락(RWLock) 배열 (패딩 적용)
};

#include "ClusterSpinMap.inl"

#endif // ndef UC_CLUSTERSPINMAP_H