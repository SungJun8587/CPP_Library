
//***************************************************************************
// KeyedTable.h : interface for the CKeyedTable class.
//
//***************************************************************************

#ifndef UC_KEYEDTABLE_H
#define UC_KEYEDTABLE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

//***************************************************************************
// @struct KeyedTableIndexEntry
// @brief CKeyedTable 내부 정렬 인덱스의 한 항목(64비트 키 + 원본 위치).
//***************************************************************************
struct KeyedTableIndexEntry
{
	std::uint64_t	key;	// 정렬 기준 키
	std::uint32_t	pos;	// 원본 행 배열(m_Rows) 내 위치(삽입/파일 순서)
};

//***************************************************************************
// @class CKeyedTable
// @brief 정렬 벡터 + 이진 탐색 인덱스 기반의, 구축 후에는 읽기 전용인 키드
//        테이블. 텍스트 파일이나 DB 결과를 한 번 로드해 캐싱하고 이후에는
//        조회만 반복하는 용도에 적합합니다.
//
// @details
// 원본 행은 Build()에 넘긴 순서를 그대로 보존하고(m_Rows), 별도의 정렬된
// 인덱스(m_Index)로 O(log N) 조회를 지원합니다. 동일 키가 여러 개면
// stable_sort 덕분에 Find()/EqualRange() 모두 원본 순서상 가장 앞선 행부터
// 반환합니다.
//
// 64비트 정수로 표현 가능한 키만 지원합니다(정수 키, 또는 두 정수를 상/하위
// 32비트로 합성한 복합 키). 문자열 등 임의 타입 키가 필요하면 별도 구현이
// 필요합니다.
//
// 스레드 안전성은 스스로 보장하지 않습니다 — 외부에서 읽기/쓰기 락으로
// 감싸 사용하십시오. Build()는 락 밖에서 새 테이블을 완성한 뒤 Swap()으로
// 교체하는 사용 패턴을 권장합니다(락 보유 구간 최소화, 교체 중에도 이전
// 데이터로 조회 가능).
//***************************************************************************
template<class T>
class CKeyedTable
{
public:
	using IndexEntry = KeyedTableIndexEntry;

	//***************************************************************************
	// @brief rows의 각 행에서 keyFn으로 키를 뽑아 정렬 인덱스를 구성합니다.
	// @details rows는 이동되어 이 테이블의 소유가 됩니다(호출 후 비워짐).
	// @tparam KeyFn std::uint64_t(const T&) 형태로 호출 가능한 콜러블
	// @param rows 테이블에 담을 행들(이동됨). 순서가 그대로 보존됨
	// @param keyFn 행 하나를 받아 64비트 키를 반환하는 콜러블
	//***************************************************************************
	template<class KeyFn>
	void Build(std::vector<T>&& rows, KeyFn keyFn)
	{
		std::vector<IndexEntry> index;
		index.reserve(rows.size());
		for( size_t i = 0; i < rows.size(); ++i )
		{
			IndexEntry e;
			e.key = keyFn(rows[i]);
			e.pos = static_cast<std::uint32_t>(i);
			index.push_back(e);
		}

		std::stable_sort(index.begin(), index.end(),
			[](const IndexEntry& a, const IndexEntry& b) { return a.key < b.key; });

		m_Rows.swap(rows);
		m_Index.swap(index);
	}

	//***************************************************************************
	// @brief 다른 테이블과 내용을 교환합니다(락 보유 구간을 최소화하는 리로드용).
	// @param other 교환할 상대 테이블
	//***************************************************************************
	void Swap(CKeyedTable& other)
	{
		m_Rows.swap(other.m_Rows);
		m_Index.swap(other.m_Index);
	}

	// @brief 테이블이 비어 있는지 여부를 반환합니다.
	bool			Empty(void) const { return m_Rows.empty(); }

	// @brief 담긴 행의 개수를 반환합니다.
	size_t			Size(void) const { return m_Rows.size(); }

	// @brief 원본 순서 기준 pos번째 행을 반환합니다.
	// @param pos 행 위치(0 ~ Size()-1). 범위를 벗어나면 동작은 std::vector::operator[]와 동일(UB)
	const T& At(size_t pos) const { return m_Rows[pos]; }

	//***************************************************************************
	// @brief 주어진 키와 일치하는 첫 번째(원본 순서 기준) 행의 위치를 찾습니다.
	// @param key 찾을 키
	// @return int 위치(0 이상), 없으면 -1
	//***************************************************************************
	int Find(std::uint64_t key) const
	{
		const IndexEntry* first = m_Index.data();
		const IndexEntry* last = first + m_Index.size();
		const IndexEntry* it = std::lower_bound(first, last, key,
			[](const IndexEntry& e, std::uint64_t k) { return e.key < k; });

		return (it != last && it->key == key) ? static_cast<int>(it->pos) : -1;
	}

	//***************************************************************************
	// @brief 주어진 키와 일치하는 모든 항목의 구간 [first, second)을 반환합니다.
	//        구간 내 항목은 원본 순서로 정렬되어 있습니다.
	// @param key 찾을 키
	// @return std::pair<const IndexEntry*, const IndexEntry*> 일치 구간 [first, second).
	//         일치하는 항목이 없으면 first == second
	//***************************************************************************
	std::pair<const IndexEntry*, const IndexEntry*> EqualRange(std::uint64_t key) const
	{
		const IndexEntry* first = m_Index.data();
		const IndexEntry* last = first + m_Index.size();
		const IndexEntry* lo = std::lower_bound(first, last, key,
			[](const IndexEntry& e, std::uint64_t k) { return e.key < k; });
		const IndexEntry* hi = std::upper_bound(lo, last, key,
			[](std::uint64_t k, const IndexEntry& e) { return k < e.key; });

		return std::make_pair(lo, hi);
	}

private:
	std::vector<T>			m_Rows;		// 원본 행(Build()에 넘긴 순서 그대로 보존)
	std::vector<IndexEntry>	m_Index;	// 키 오름차순으로 정렬된 조회용 인덱스
};

#endif // ndef UC_KEYEDTABLE_H