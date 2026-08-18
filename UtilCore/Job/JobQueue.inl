
//***************************************************************************
// JobQueue.inl : implementation of the CJobQueue class.
//
//***************************************************************************

//***************************************************************************
// @brief 멤버 함수 기반의 비동기 작업을 큐에 등록합니다.
// @detail 객체의 소유권을 공유 형태로 확보한 뒤, 해당 멤버 함수와 인자들을 캡슐화한 작업 객체를 생성하여 큐에 푸시합니다.
// @param memFunc 실행할 클래스의 멤버 함수 포인터
// @param args 멤버 함수에 전달할 가변 인자들
//***************************************************************************
template<typename T, typename Ret, typename... Args>
inline void CJobQueue::DoAsync(Ret(T::* memFunc)(Args...), Args... args)
{
	shared_ptr<T> owner = static_pointer_cast<T>(shared_from_this());
	Push(CObjectPool<CJob>::MakeShared(owner, memFunc, std::forward<Args>(args)...));
}

//***************************************************************************
// @brief 지정된 시간(밀리초) 이후에 실행될 멤버 함수 기반의 타이머 작업을 예약합니다.
// @detail 객체의 소유권을 확보하고 작업 객체를 생성한 후, 전역 타이머 관리자를 통해 지정된 지연 시간 뒤에 실행되도록 예약합니다.
// @param tickAfterMs 작업을 지연시킬 시간 (밀리초 단위)
// @param memFunc 예약할 클래스의 멤버 함수 포인터
// @param args 멤버 함수에 전달할 가변 인자들
//***************************************************************************
template<typename T, typename Ret, typename... Args>
inline void CJobQueue::DoTimer(uint64 tickAfterMs, Ret(T::* memFunc)(Args...), Args... args)
{
	shared_ptr<T> owner = static_pointer_cast<T>(shared_from_this());
	CJobRef job = CObjectPool<CJob>::MakeShared(owner, memFunc, std::forward<Args>(args)...);
	if( gpJobTimer != nullptr ) gpJobTimer->Reserve(tickAfterMs, shared_from_this(), job);
}