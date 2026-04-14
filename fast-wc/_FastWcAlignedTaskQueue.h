#pragma once
#include "_FastWcTaskWorker.h"
#include <mutex>
#include <queue>

namespace tp {
	constexpr auto CACHE_LINE_BUF_SIZE = 128;

	struct alignas(CACHE_LINE_BUF_SIZE) _FastWcAlignedTaskQueue
	{
		std::queue<_FastWcTaskWorker> _taskQueue;
		std::mutex _queueMutex;

		_FastWcAlignedTaskQueue() = default;
		~_FastWcAlignedTaskQueue() = default;

		_FastWcAlignedTaskQueue(_FastWcAlignedTaskQueue&&) noexcept;
		_FastWcAlignedTaskQueue& operator=(_FastWcAlignedTaskQueue&&) noexcept;

		_FastWcAlignedTaskQueue(const _FastWcAlignedTaskQueue&) = delete;
		_FastWcAlignedTaskQueue& operator=(const _FastWcAlignedTaskQueue&) = delete;

		void push(_FastWcTaskWorker&& task);
		void pop(_FastWcTaskWorker&& task);
	};
}

