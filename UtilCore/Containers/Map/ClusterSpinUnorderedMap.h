
//***************************************************************************
// ClusterSpinUnorderedMap.h : interface for the CClusterSpinUnorderedMap class.
//
//***************************************************************************

#ifndef UC_CLUSTERSPINUNORDEREDMAP_H
#define UC_CLUSTERSPINUNORDEREDMAP_H

#include <BaseRedefineDataType.h>
#include <Thread/PlatformLock.h>

#include <unordered_map>

//***************************************************************************
// @class CClusterSpinUnorderedMap
// @brief Hash Table(O(1)) 기반의 클러스터링 분산 스핀 맵 클래스
// @details 
// [특징 및 사용 권장 대상]
//   1. 단일 조회 최적화: 세션 매니저, 유저/객체 매니저 등 정렬이 필요 없고 SessionID/GUID 기반의 
//      단일 검색, 삽입, 삭제가 지배적인 기능에 가장 이상적입니다.
//   2. 락 경합 극소화: 평균 O(1) 처리 속도를 통해 스핀락(SpinLock) 점유 시간을 최소화하여 
//      멀티스레드 동시 접근 시 최고 수준의 런타임 성능을 제공합니다.
//   3. const 안전성: const 참조 및 const 멤버 함수를 완벽히 지원하여 임시 객체/Rvalue 넘김이 가능합니다.
// [요구 조건]
//   - Key 타입에 대한 std::hash(또는 커스텀 Hash) 연산 및 `==` 연산자가 정의되어 있어야 합니다.
//
// [클러스터 개수(nClusterCnt) 최적화 가이드 및 선정 이유 (예: 16개 기준)]
//   1. 2의 제곱수 최적화: 16 등 2의 제곱수 크기로 설정 시, 내부 해시 인덱스 연산(key % nClusterCnt)을 
//      컴파일러가 느린 나눗셈 대신 매우 빠른 비트 연산(key & (nClusterCnt - 1))으로 자동 최적화합니다.
//   2. 락 경합 대폭 완화: 단일 락 구조 대비 동시 접근 시 충돌 확률을 클러스터 개수 수준으로 대폭 줄여줍니다.
//   3. 코어 구조와의 조화: 일반적인 8~16코어(하이퍼스레딩 포함 16~32스레드) 상용 서버 환경에서 
//      워커 스레드들이 서로 다른 락을 참조할 확률을 높여 병목을 효과적으로 해소합니다.
// [예시]
//   - 대규모 하이엔드 서버 환경(32코어 이상, 수천 명 동접): 32 또는 64 권장
//   - 소규모 서버 또는 테스트 환경(2 ~ 4코어): 8 또는 16 권장
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock = true, typename Hash = std::hash<T1>>
class CClusterSpinUnorderedMap
{
public:
	typedef CUnorderedMap< T1, T2, Hash >	ObjectMap;
	typedef std::pair< const T1, T2 >		ObjectMapPair;

	CClusterSpinUnorderedMap(void);
	virtual ~CClusterSpinUnorderedMap(void);

public:
	size_t		getSize();
	bool		InsertObject(T1 key, T2 object);
	T2			FindObject(T1 key);
	bool		FindObject(T1 key, T2& object);
	bool		EraseObject(T1 key);

	//***************************************************************************
	// @brief 클러스터 인덱스로 내부 해시맵 참조를 반환합니다. (전체 순회용)
	// @param idx 클러스터 인덱스 (0 ~ nClusterCnt - 1)
	// @return ObjectMap& 해당 클러스터의 맵 참조
	//***************************************************************************
	ObjectMap& GetClusterMapByIdx(__int32 idx) {
		return m_ObjectMaps[idx];
	}

	//***************************************************************************
	// @brief 클러스터 인덱스로 직접 쓰기 락을 획득합니다. (외부 제어용)
	// @param idx 클러스터 인덱스
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void WriteLockByIdx(__int32 idx, const char* name = nullptr) {
		writeLock(idx, name);
	}

	//***************************************************************************
	// @brief 클러스터 인덱스로 직접 쓰기 락을 해제합니다. (외부 제어용)
	// @param idx 클러스터 인덱스
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void WriteUnlockByIdx(__int32 idx, const char* name = nullptr) {
		writeUnlock(idx, name);
	}

	//***************************************************************************
	// @brief 클러스터 인덱스로 직접 읽기 락을 획득합니다. (외부 제어용)
	// @param idx 클러스터 인덱스
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void ReadLockByIdx(__int32 idx, const char* name = nullptr) {
		readLock(idx, name);
	}

	//***************************************************************************
	// @brief 클러스터 인덱스로 직접 읽기 락을 해제합니다. (외부 제어용)
	// @param idx 클러스터 인덱스
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void ReadUnlockByIdx(__int32 idx, const char* name = nullptr) {
		readUnlock(idx, name);
	}

	//***************************************************************************
	// @brief 키에 해당하는 클러스터의 읽기 락을 획득합니다.
	// @param key 락을 식별할 클러스터 키
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void ReadLock(const T1& key, const char* name = nullptr) {
		readLock(getClusterIdx(key), name);
	}

	//***************************************************************************
	// @brief 키에 해당하는 클러스터의 읽기 락을 해제합니다.
	// @param key 락을 식별할 클러스터 키
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void ReadUnlock(const T1& key, const char* name = nullptr) {
		readUnlock(getClusterIdx(key), name);
	}

	//***************************************************************************
	// @brief 키에 해당하는 클러스터의 쓰기 락을 획득합니다.
	// @param key 락을 식별할 클러스터 키
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void WriteLock(const T1& key, const char* name = nullptr) {
		writeLock(getClusterIdx(key), name);
	}

	//***************************************************************************
	// @brief 키에 해당하는 클러스터의 쓰기 락을 해제합니다.
	// @param key 락을 식별할 클러스터 키
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void WriteUnlock(const T1& key, const char* name = nullptr) {
		writeUnlock(getClusterIdx(key), name);
	}

	//***************************************************************************
	// @brief 키에 해당하는 클러스터의 내부 해시맵 참조를 반환합니다.
	// @param key 클러스터를 결정할 키
	// @return ObjectMap& 해당 클러스터의 맵 참조
	//***************************************************************************
	ObjectMap& GetObjectMap(const T1& key) {
		return m_ObjectMaps[getClusterIdx(key)];
	}

	//***************************************************************************
	// @brief 설정된 전체 클러스터 개수를 반환합니다.
	// @return __int32 클러스터 총 개수
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

protected:
	//***************************************************************************
	// @brief 키를 바탕으로 해시맵이 위치할 클러스터 인덱스를 계산합니다.
	// @param key 클러스터 인덱스를 구할 키
	// @return __int32 클러스터 인덱스
	//***************************************************************************
	__int32 getClusterIdx(const T1& key) const {
		return static_cast<__int32>(Hash{}(key) % nClusterCnt);
	}

	//***************************************************************************
	// @brief 특정 클러스터 인덱스의 읽기 락을 획득합니다. (내부용)
	// @param nClusterIdx 클러스터 인덱스
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void readLock(__int32 nClusterIdx, const char* name = nullptr) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].ReadLock(name);
	}

	//***************************************************************************
	// @brief 특정 클러스터 인덱스의 읽기 락을 해제합니다. (내부용)
	// @param nClusterIdx 클러스터 인덱스
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void readUnlock(__int32 nClusterIdx, const char* name = nullptr) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].ReadUnlock(name);
	}

	//***************************************************************************
	// @brief 특정 클러스터 인덱스의 쓰기 락을 획득합니다. (내부용)
	// @param nClusterIdx 클러스터 인덱스
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void writeLock(__int32 nClusterIdx, const char* name = nullptr) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].WriteLock(name);
	}

	//***************************************************************************
	// @brief 특정 클러스터 인덱스의 쓰기 락을 해제합니다. (내부용)
	// @param nClusterIdx 클러스터 인덱스
	// @param name 락 추적용 이름 (선택 사항)
	//***************************************************************************
	void writeUnlock(__int32 nClusterIdx, const char* name = nullptr) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].WriteUnlock(name);
	}

	void clearObjectMap(void);

public:
	ObjectMap	m_ObjectMaps[nClusterCnt];	// 클러스터별로 데이터를 저장하는 해시맵 배열 (총 nClusterCnt개)
	PRWLock		m_ObjectLocks[nClusterCnt];	// 각 클러스터의 동시성 제어를 위한 읽기/쓰기 락(RWLock) 배열
};

#include "ClusterSpinUnorderedMap.inl"

#endif // ndef UC_CLUSTERSPINUNORDEREDMAP_H