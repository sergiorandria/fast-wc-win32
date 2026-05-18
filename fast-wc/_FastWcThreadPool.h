#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iterator>
#include <ranges>
#include <vector>

#include "ISingleton.h"
#include "_FastWcAlignedTaskQueue.h"
#include "_FastWcCapabilityToken.h"  
#include "_FastWcTaskConcept.h"     

namespace tp {

	class _FastWcThreadPool : public ISingleton<_FastWcThreadPool>
	{
		friend class ISingleton<_FastWcThreadPool>;
	public:
		_FastWcThreadPool(const _FastWcThreadPool&) = delete;
		_FastWcThreadPool& operator=(const _FastWcThreadPool&) = delete;

		~_FastWcThreadPool();

		template <_FastWcSubmittable<> Callable, typename... Args>
		auto submit(const _FastWcCapabilityToken& token, Callable&& fn, Args&&... args)
			-> std::future<std::invoke_result_t<Callable, Args...>>;

		auto enqueue(const _FastWcCapabilityToken& token, _FastWcCallable auto task) -> void;

		template <typename Iterator>
		void enqueueBatch(const _FastWcCapabilityToken& token, Iterator begin, Iterator end);

		void shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

		template <typename... Args, std::size_t stealProcStackSz = 256>
		void stealPoolUid(Args... args);

		std::size_t threadCount() const;

	private:
		void workerThread(std::size_t threadIndex);

		static void validateToken(const _FastWcCapabilityToken& token)
		{
			if (!token.isValid())
				throw std::runtime_error("Invalid or revoked capability token.");
		}

		std::size_t queueIndexForTier(_FastWcPrivilegeTier tier)
		{
			if (tier == _FastWcPrivilegeTier::High)
				return nextHighQueueIndex.fetch_add(1, std::memory_order_relaxed) % highTierCores;
			else
				return highTierCores + (nextLowQueueIndex.fetch_add(1, std::memory_order_relaxed) % lowTierCores);
		}

		bool isHighTierWorker(std::size_t threadIndex) const
		{
			return threadIndex < highTierCores;
		}

		inline _FastWcThreadPool(std::size_t nCore) : cpuCore(nCore)
		{
			if (nCore == 0)
				throw std::runtime_error("hardware_concurrency() returned 0");

			highTierCores = std::max<std::size_t>(1, nCore / 2);
			lowTierCores = nCore - highTierCores;

			taskQueues.resize(nCore);
			threads.reserve(nCore);
			for (std::size_t i = 0; i < nCore; ++i)
				threads.emplace_back(&_FastWcThreadPool::workerThread, this, i);
		}

		std::mutex              submitMutex;
		std::size_t             cpuCore;
		std::size_t             highTierCores;
		std::size_t             lowTierCores;
		std::condition_variable tpCv;

		std::vector<std::thread>                  threads;
		std::vector<tp::_FastWcAlignedTaskQueue>  taskQueues;

		std::atomic_bool          stopping = false;
		std::atomic_bool          draining = false;
		std::atomic<std::size_t>  nextHighQueueIndex = 0;
		std::atomic<std::size_t>  nextLowQueueIndex = 0;
		std::atomic<std::size_t>  pendingTasks = 0;
	};

	template <_FastWcSubmittable<> Callable, typename... Args>
	auto _FastWcThreadPool::submit(const _FastWcCapabilityToken& token, Callable&& fn, Args&&... args)
		-> std::future<std::invoke_result_t<Callable, Args...>>
	{
		validateToken(token);

		using ReturnType = std::invoke_result_t<Callable, Args...>;

		auto task = std::make_shared<std::packaged_task<ReturnType()>>(
			[fn = std::forward<Callable>(fn), ...args = std::forward<Args>(args)]()
			mutable -> ReturnType {
				return fn(std::forward<Args>(args)...);
			}
		);

		std::future<ReturnType> result = task->get_future();
		{
			std::lock_guard<std::mutex> lock(submitMutex);

			if (stopping.load(std::memory_order_acquire))
				throw std::runtime_error("ThreadPool is stopping, cannot submit new tasks.");

			std::size_t taskQueueIndex = queueIndexForTier(token.tier());
			{
				std::unique_lock<std::mutex> queueLock(taskQueues[taskQueueIndex]._queueMutex);
				taskQueues[taskQueueIndex]._taskQueue.emplace(
					[task]() {
						try { (*task)(); }
						catch (...) {}
					});
			}
			pendingTasks.fetch_add(1, std::memory_order_release);
		}
		tpCv.notify_one();

		return result;
	}

	auto _FastWcThreadPool::enqueue(const _FastWcCapabilityToken& token, _FastWcCallable auto task) -> void
	{
		validateToken(token);

		std::lock_guard<std::mutex> lock(submitMutex);

		if (stopping.load(std::memory_order_acquire))
			throw std::runtime_error("ThreadPool is stopping, cannot submit new tasks.");

		std::size_t taskQueueIndex = queueIndexForTier(token.tier());
		{
			std::unique_lock<std::mutex> queueLock(taskQueues[taskQueueIndex]._queueMutex);
			taskQueues[taskQueueIndex]._taskQueue.emplace(
				[t = std::move(task)]() -> void {
					try { t(); }
					catch (...) {}
				});
		}
		pendingTasks.fetch_add(1, std::memory_order_release);
		tpCv.notify_one();
	}

	template <typename Iterator>
	void _FastWcThreadPool::enqueueBatch(const _FastWcCapabilityToken& token, Iterator begin, Iterator end)
	{
		validateToken(token);

		std::size_t count = 0;
		{
			std::lock_guard<std::mutex> submitLock(submitMutex);

			if (stopping.load(std::memory_order_acquire))
				throw std::runtime_error("Thread pool is stopping, cannot acquire new task");

			for (Iterator it = begin; it != end; ++it)
			{
				std::size_t taskQueueIndex = queueIndexForTier(token.tier());
				{
					std::unique_lock<std::mutex> queueLock(taskQueues[taskQueueIndex]._queueMutex);
					taskQueues[taskQueueIndex]._taskQueue.emplace(
						[task = *it]() -> void {
							try { task(); }
							catch (...) {}
						});
				}
				++count;
			}
			pendingTasks.fetch_add(count, std::memory_order_release);
		}

		if (count == 0)
			return;
		else if (count == 1)
			tpCv.notify_one();
		else
			tpCv.notify_all();
	}

	template <typename... Args, std::size_t stealProcStackSz>
	inline void _FastWcThreadPool::stealPoolUid(Args... args) {}

	_FastWcThreadPool::~_FastWcThreadPool()
	{
		shutdown();
	}

	void _FastWcThreadPool::shutdown(std::chrono::milliseconds timeout)
	{
		{
			std::unique_lock lock(submitMutex);
			stopping.store(true, std::memory_order_release);
		}
		tpCv.notify_all();

		auto deadline = std::chrono::steady_clock::now() + timeout;
		for (auto& thread : threads)
		{
			if (thread.joinable())
			{
				auto remaining = deadline - std::chrono::steady_clock::now();
				if (remaining > std::chrono::microseconds(0))
					thread.join();
				else
					thread.detach();
			}
		}
	}

	void _FastWcThreadPool::workerThread(std::size_t threadIndex)
	{
		// Determine this worker's tier range for stealing boundaries.
		const std::size_t tierBegin = isHighTierWorker(threadIndex) ? 0 : highTierCores;
		const std::size_t tierEnd = isHighTierWorker(threadIndex) ? cpuCore : cpuCore;
		// High-tier workers may steal across the full range [0, cpuCore).
		// Low-tier workers steal only within [highTierCores, cpuCore).

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
				for (std::size_t i = tierBegin; i < tierEnd; ++i)
				{
					if (i == threadIndex) continue;

					std::unique_lock<std::mutex> lock(taskQueues[i]._queueMutex, std::try_to_lock);
					if (lock.owns_lock() && !taskQueues[i]._taskQueue.empty())
					{
						task = std::move(taskQueues[i]._taskQueue.front());
						taskQueues[i]._taskQueue.pop();
						foundTask = true;
						break;
					}
				}
			}

			if (foundTask)
			{
				task();
				pendingTasks.fetch_sub(1, std::memory_order_release);
			}
			else
			{
				std::unique_lock<std::mutex> lock(submitMutex);
				tpCv.wait(lock, [this] {
					return stopping.load(std::memory_order_acquire)
						|| pendingTasks.load(std::memory_order_acquire) > 0;
					});

				if (stopping.load(std::memory_order_acquire))
				{
					bool expected = false;
					if (draining.compare_exchange_strong(expected, true))
					{
						lock.unlock();
						for (std::size_t q = 0; q < cpuCore; ++q)
						{
							std::unique_lock<std::mutex> queueLock(taskQueues[q]._queueMutex);
							while (!taskQueues[q]._taskQueue.empty())
							{
								task = std::move(taskQueues[q]._taskQueue.front());
								taskQueues[q]._taskQueue.pop();
								queueLock.unlock();
								task();
								pendingTasks.fetch_sub(1, std::memory_order_release);
								queueLock.lock();
							}
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