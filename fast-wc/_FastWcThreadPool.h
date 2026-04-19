#pragma once

#include <future>
#include <functional>
#include <atomic>
#include <vector>
#include <chrono>
#include <condition_variable>
#include <iterator>
#include <ranges>

#include "ISingleton.h"
#include "_FastWcAlignedTaskQueue.h"

namespace tp {
	class _FastWcThreadPool : public ISingleton<_FastWcThreadPool>
	{
		friend class ISingleton<_FastWcThreadPool>;
	public:  
		_FastWcThreadPool(const _FastWcThreadPool&) = delete;
		_FastWcThreadPool& operator=(const _FastWcThreadPool&) = delete;
	
		~_FastWcThreadPool(); 

		template <typename Callable, typename... Args>
		auto submit(Callable&& fn, Args &&...args) 
			-> std::future< typename std::invoke_result<Callable, Args...>::type>;
	
		auto enqueue(std::function<void()> task) -> void;

		template <typename Iterator> 
		void enqueueBatch(Iterator begin, Iterator end);

		void shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

		template <typename...Args, std::size_t stealProcStackSz = 256>
		void stealPoolUid(Args...args);

		void workerThread(std::size_t threadIndex);

		std::size_t threadCount() const;

	private: 
		inline _FastWcThreadPool(std::size_t nCore) : cpuCore(nCore) 
		{
			if (nCore == 0) 
				throw std::runtime_error("hardware_concurrency() returned 0");
			
			taskQueues.resize(nCore);
			threads.reserve(nCore);
			for (std::size_t i = 0; i < nCore; ++i) 
			{
				threads.emplace_back(&_FastWcThreadPool::workerThread, this, i);
			}
		};

		std::mutex submitMutex; 
		size_t cpuCore; 
		std::condition_variable tpCv;

		std::vector < std::thread> threads;
		std::vector<tp::_FastWcAlignedTaskQueue> taskQueues; 
		std::atomic_bool stopping = false;
		std::atomic<std::size_t> nextQueueIndex = 0;
		std::atomic<std::size_t> activeThreads = 0;
		std::atomic<std::size_t> pendingTasks = 0;
	};

	template <typename Callable, typename... Args>
	auto _FastWcThreadPool::submit(Callable&& fn, Args &&...args)
		-> std::future< typename std::invoke_result<Callable, Args...>::type> {
		using ReturnType = typename std::invoke_result<Callable, Args...>::type;

		auto task = std::make_shared<std::packaged_task<ReturnType()>>(
			[fn = std::forward<Callable>(fn), ...args = std::forward<Args>(args)]()
			mutable -> ReturnType {
				return fn(std::forward<Args>(args)...);
			}
		);

		std::future<ReturnType> result = task->get_future();
		{
			std::lock_guard<std::mutex> lock(submitMutex);
			if (stopping.load(std::memory_order_acquire)) {
				throw std::runtime_error("ThreadPool is stopping, cannot submit new tasks.");
			}

			std::size_t taskQueueIndex = nextQueueIndex.fetch_add(1, std::memory_order_relaxed) % cpuCore;
			{
				std::unique_lock<std::mutex> lock(taskQueues[taskQueueIndex]._queueMutex);
				taskQueues[taskQueueIndex]._taskQueue.emplace(
					[task]() {
						try {
							// The most important things here is to ensure that the task is executed 
							// and any exception is captured by the packaged_task,
							(*task)();
						}
						catch (...) {
							// Prevent task from death is necessary, 
							// but we can ignore any exception here since it's already captured by the packaged_task
						}
					});
			} activeThreads.fetch_add(1, std::memory_order_release);
		} tpCv.notify_one();

		return result;
	}

	template <typename T, typename U> 
	constexpr bool is_same_v = std::is_same<T, U>::value;

	template<typename Iterator> 
	void _FastWcThreadPool::enqueueBatch(Iterator begin, Iterator end)
	{
		size_t count = 0;
		{
			std::lock_guard<std::mutex> submitLock(submitMutex);

			if (stopping.load(std::memory_order_acquire))
			{
				throw std::runtime_error("Thread pool is stopping, cannot acquire new task");
			}

			for (Iterator it = begin; it != end; ++it)
			{
				size_t taskQueueIndex = nextQueueIndex.fetch_add(1, std::memory_order_relaxed) % cpuCore;
				{
					std::unique_lock<std::mutex> lock(taskQueues[taskQueueIndex]._queueMutex);
					taskQueues[taskQueueIndex]._taskQueue.emplace([task = *it]() -> void {
						try {
							task();
						}
						catch (...) {
							// Prevent any worker from death
						}
					});
				}
				count++;
			} activeThreads.fetch_add(count, std::memory_order_release);
		}

		if (count <= 1)
		{
			tpCv.notify_one();
		}
		else {
			tpCv.notify_all();
		}
	}

	template<typename ...Args, std::size_t stealProcStackSz>
	inline void _FastWcThreadPool::stealPoolUid(Args ...args)
	{
		// What is we create another pool to run simultaneously four branches ? 
		auto* anotherPool = _FastWcThreadPool::Instance((void)0);

		std::vector<std::future> futures;
		futures.reserve(4);

		// How about a low level management of the thread pool 
		// using PROCESS_INFORMATION ?
		//PROCESS_INFORMATION procInfo;
		//procInfo.hProcess = ;
		//procInfo.dwProcessId = ;
		//procInfo.dwThreadId = ;
		
		// If possible, why not divide worker task into atomic task ?
		/*futures.push_back(anotherPool->submit([fn = []() {}, ...args = std::forward<Args>(args)]() {
			
			
		}));
		*/
		//anotherPool->~_FastWcThreadPool();
	}

	_FastWcThreadPool::~_FastWcThreadPool()
	{
		shutdown();
	}

	auto _FastWcThreadPool::enqueue(std::function<void()> task) -> void
	{
		std::lock_guard<std::mutex> lock(submitMutex);
		{
			if (stopping.load(std::memory_order_acquire)) {
				throw std::runtime_error("ThreadPool is stopping, cannot submit new tasks.");
			}

			size_t taskQueueIndex = nextQueueIndex.fetch_add(1, std::memory_order_relaxed) % cpuCore;
			{
				std::unique_lock<std::mutex> lock(taskQueues[taskQueueIndex]._queueMutex);
				taskQueues[taskQueueIndex]._taskQueue.emplace([task = std::move(task)]() -> void {
					try {
						task();
					}
					catch (...) {
						// Prevent task from death is necessary, 
						// but we can ignore any exception here since it's already captured by the packaged_task
					}
				});
			} activeThreads.fetch_add(1, std::memory_order_release);
		} tpCv.notify_one();
	}

	void _FastWcThreadPool::shutdown(std::chrono::milliseconds timeout)
	{
		{
			std::unique_lock lock(submitMutex);
			stopping.store(true, std::memory_order_release);
		} tpCv.notify_all();

		auto deadline = std::chrono::steady_clock::now() + timeout;
		for (auto& thread : threads)
		{
			if (thread.joinable())
			{
				auto remaining = deadline - std::chrono::steady_clock::now();
				if (remaining > std::chrono::microseconds(0)) {
					thread.join();
				}
				else {
					thread.detach();
				}
			}
		}
	}

	void _FastWcThreadPool::workerThread(size_t threadIndex)
	{
		while (true)
		{
			_FastWcTaskWorker task;
			bool foundTask = false;
			{
				std::unique_lock<std::mutex> lock(taskQueues[threadIndex]._queueMutex);
				if (!taskQueues[threadIndex]._taskQueue.empty()) 
				{
					task = std::move(taskQueues[threadIndex]._taskQueue.front());
					taskQueues[threadIndex]._taskQueue.pop();
					foundTask = true;
				}
			}

			if (!foundTask)
			{
				for (std::size_t i = 1; i < cpuCore; ++i)
				{
					std::size_t stealFromIndex = (threadIndex + i) % cpuCore;
					taskQueues[stealFromIndex]._queueMutex.lock();

					std::unique_lock<std::mutex> lock(taskQueues[stealFromIndex]._queueMutex, std::adopt_lock);
					if (lock.owns_lock() && !taskQueues[stealFromIndex]._taskQueue.empty()) {
						task = std::move(taskQueues[stealFromIndex]._taskQueue.front());
						taskQueues[stealFromIndex]._taskQueue.pop();
						foundTask = true;
						break;
					}
				}
			}

			if (foundTask) {
				task();
				activeThreads.fetch_sub(1, std::memory_order_release);
			} else {
				std::unique_lock<std::mutex> lock(submitMutex);
				tpCv.wait(lock, [this, threadIndex] {
					return stopping.load(std::memory_order_acquire) || pendingTasks.load(std::memory_order_acquire) > 0;
				});

				if (stopping.load(std::memory_order_acquire) && activeThreads.load(std::memory_order_acquire) == 0)
				{
					bool hasTask = true;

					while (hasTask)
					{
						hasTask = false;
						std::unique_lock<std::mutex> lock(taskQueues[threadIndex]._queueMutex);

						if (!taskQueues[threadIndex]._taskQueue.empty())
						{
							task = std::move(taskQueues[threadIndex]._taskQueue.front());
							taskQueues[threadIndex]._taskQueue.pop();
							hasTask = true;
							lock.unlock();
							task();
							activeThreads.fetch_sub(1, std::memory_order_release);
						}
					}

					return;
				}
			}
		}
	}

	std::size_t _FastWcThreadPool::threadCount() const
	{
		return cpuCore;
	}
}

