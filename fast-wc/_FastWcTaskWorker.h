#pragma once
#include <cstddef>
#include <type_traits>

#include "_FastWcCryptoState.h"

constexpr auto FAST_WC_TASK_WORKER_SIZE = 256;

namespace tp {
	static std::size_t idCounter = 0; 

	template <typename T, typename DecayedType> 
	constexpr bool is_decay_equal = std::is_same_v<std::decay_t<T>, DecayedType>;

	size_t hardware_concurrency();

	class _FastWcTaskWorker
	{
	public: 
		_FastWcTaskWorker();
		~_FastWcTaskWorker();

		_FastWcTaskWorker(const _FastWcTaskWorker&) = delete;
		_FastWcTaskWorker& operator=(const _FastWcTaskWorker&) = delete;

		_FastWcTaskWorker(_FastWcTaskWorker&&) noexcept; 
		_FastWcTaskWorker& operator=(_FastWcTaskWorker&&) noexcept;

		template <typename Callable>
		_FastWcTaskWorker(Callable&& task);

		void operator()(); 
		explicit operator bool() const noexcept;

	private: 
		std::size_t uid; 
		crypto::_FastWcCryptoState _cryptoState;
		alignas(std::max_align_t) char _taskData[FAST_WC_TASK_WORKER_SIZE];

		void (*_taskInvoke)(void*)		= nullptr;
		void (*_taskMove)(void*, void*) = nullptr;
		void (*_taskCleanup)(void*)		= nullptr;

		// For extreme low-level control over the stack
		// and debug memory analysis
		char& operator[](std::size_t index);
		const char& operator[](std::size_t index) const;

		bool verifyTaskDataIntegrity();
		void encryptTaskDataMem();
		void decryptTaskDataMem();
	};

	template <typename Callable>
	inline _FastWcTaskWorker::_FastWcTaskWorker(Callable&& task)
	{
		using DecayedCallable = std::decay_t<Callable>;

		static_assert(std::is_same_v<Callable, DecayedCallable>);

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
}