#include "_FastWcAlignedTaskQueue.h"

tp::_FastWcAlignedTaskQueue::_FastWcAlignedTaskQueue(tp::_FastWcAlignedTaskQueue&& other) noexcept
{
	if (this != &other) {
		std::lock_guard<std::mutex> lock(other._queueMutex);
		_taskQueue = std::move(other._taskQueue);
	}
}

tp::_FastWcAlignedTaskQueue& tp::_FastWcAlignedTaskQueue::operator=(tp::_FastWcAlignedTaskQueue&& other) noexcept
{
	if (this != &other) {
		std::lock_guard<std::mutex> lock(other._queueMutex);
		_taskQueue = std::move(other._taskQueue);
	}

	return *this;
}

void tp::_FastWcAlignedTaskQueue::push(tp::_FastWcTaskWorker&& task)
{
	std::lock_guard<std::mutex> lock(_queueMutex);
	_taskQueue.push(std::move(task));
}

void tp::_FastWcAlignedTaskQueue::pop(tp::_FastWcTaskWorker&& task)
{
	std::lock_guard<std::mutex> lock(_queueMutex);
	_taskQueue.pop();
}
