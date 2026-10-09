
//***************************************************************************
// DBAsyncRegistry.h : interface for the DBAsyncRegistry functions.
//
//***************************************************************************

#ifndef UC_DBASYNCREGISTRY_H
#define UC_DBASYNCREGISTRY_H

#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

class CDBAsyncSrvHandler;

//***************************************************************************
// @brief DECLARE_DBASYNC_HANDLER_EX가 만든 핸들러의 서비스 등록을 지연시키는 대기 목록
// @details 핸들러는 전역(정적) 초기화 시점, 즉 main() 이전에 만들어진다. 이 시점에는
//          BaseGlobal::Init()이 아직 불리지 않아 gpMemory가 없으므로, StlAllocator /
//          PoolAllocator를 쓰는 객체(CDbServiceManager의 COdbcAsyncSrv 등)를 만들거나
//          건드리면 안 된다. 그래서 정적 초기화 단계에서는 "어느 서비스에 어떤 핸들러를
//          등록할지"만 이 목록에 적어 두고, 실제 Regist() 호출은 각 서비스의
//          StartService()가 시작 직전에 Flush()로 한꺼번에 수행한다.
//          - 이 파일은 <functional>/<vector>/<mutex>의 기본 할당자만 사용한다
//            (gpMemory에 의존하지 않는다).
//          - 대기 목록과 락은 함수 지역 static이라 번역 단위 간 초기화 순서에 무관하다.
//          - Flush()는 워커 스레드가 시작되기 전에 불리므로 Regist()가 맵을 갱신해도
//            다른 스레드와 경합하지 않는다.
//***************************************************************************
namespace DBAsyncRegistry
{
	using Registrar = std::function<void()>;

	//***************************************************************************
	// @brief 대기 목록과 그 락을 반환한다(최초 사용 시점 생성).
	//***************************************************************************
	struct State
	{
		std::mutex					lock;		// pending 보호
		std::vector<Registrar>		pending;	// 아직 서비스에 등록되지 않은 등록 작업
	};

	inline State& GetState()
	{
		static State state;
		return state;
	}

	//***************************************************************************
	// @brief 핸들러의 서비스 등록을 대기 목록에 예약한다.
	// @details DECLARE_DBASYNC_HANDLER_EX의 정적 멤버 초기화식에서 호출된다.
	//          doRegister는 이 시점에 호출되지 않고 Flush()에서 호출된다.
	// @param handler 등록할 핸들러(예약이 끝난 뒤에도 호출부가 계속 보관한다)
	// @param doRegister handler를 받아 대상 서비스의 Regist()를 호출하는 함수 객체
	// @return 전달받은 handler
	//***************************************************************************
	template<typename Fn>
	std::shared_ptr<CDBAsyncSrvHandler> Defer(std::shared_ptr<CDBAsyncSrvHandler> handler, Fn doRegister)
	{
		State& st = GetState();
		std::lock_guard<std::mutex> guard(st.lock);
		st.pending.push_back([handler, doRegister]() { doRegister(handler); });
		return handler;
	}

	//***************************************************************************
	// @brief 대기 중인 등록 작업을 모두 실행하고 목록을 비운다.
	// @details 각 서비스의 StartService()가 시작 직전에 호출한다. 여러 번 불려도
	//          이미 실행한 작업은 다시 실행되지 않는다. 등록 작업은 락을 풀고 실행하므로
	//          작업 안에서 서비스 객체를 처음 만들어도(예: CDbServiceManager::Instance())
	//          교착이 생기지 않는다.
	//***************************************************************************
	inline void Flush()
	{
		State& st = GetState();

		std::vector<Registrar> work;
		{
			std::lock_guard<std::mutex> guard(st.lock);
			work.swap(st.pending);
		}

		for( auto& fn : work )
			fn();
	}
}

#endif // ndef UC_DBASYNCREGISTRY_H