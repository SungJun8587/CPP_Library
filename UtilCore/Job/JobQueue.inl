
//***************************************************************************
// JobQueue.inl : inline template implementations for CJobQueue.
//
//***************************************************************************

template<typename T, typename Ret, typename... Args>
inline void CJobQueue::DoAsync(Ret(T::* memFunc)(Args...), Args... args)
{
	shared_ptr<T> owner = static_pointer_cast<T>(shared_from_this());
	Push(CObjectPool<CJob>::MakeShared(owner, memFunc, std::forward<Args>(args)...));
}

template<typename T, typename Ret, typename... Args>
inline void CJobQueue::DoTimer(uint64 tickAfterMs, Ret(T::* memFunc)(Args...), Args... args)
{
	shared_ptr<T> owner = static_pointer_cast<T>(shared_from_this());
	CJobRef job = CObjectPool<CJob>::MakeShared(owner, memFunc, std::forward<Args>(args)...);
	if( gpJobTimer != nullptr ) gpJobTimer->Reserve(tickAfterMs, shared_from_this(), job);
}