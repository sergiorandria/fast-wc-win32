#include "_FastWcTaskWorker.h"
#include "_ProjMacro.h"
#include <memory>
#include <type_traits>

template <typename Callable>
tp::_FastWcTaskWorker::_FastWcTaskWorker(Callable&& task)
{
	using DecayedCallable = std::decay_t<Callable>;

	//ASSERT_DECAY_EQUAL(task, DecayedCallable); 
	//ASSERT_TASK_FITS(task);

	new (_taskData) DecayedCallable(std::forward<Callable>(task));
	_taskInvoke = [](void* data) -> void {
		(*reinterpret_cast<DecayedCallable*>(data))();
	};

	_taskMove = [](void* dest, void* src) -> void {
		new (dest) DecayedCallable(std::move(*reinterpret_cast<DecayedCallable*>(src)));
		reinterpret_cast<DecayedCallable*>(src)->~DecayedCallable();
	};

	_taskCleanup = [](void* data) -> void {
		reinterpret_cast<DecayedCallable*>(data)->~DecayedCallable();
	};
}