#include "_FastWcTaskWorker.h"
#include "_ProjMacro.h"
#include <type_traits>
#include <memory>
#include <functional>
#include <Windows.h>

tp::_FastWcTaskWorker::_FastWcTaskWorker() = default;

tp::_FastWcTaskWorker::~_FastWcTaskWorker()
{
	if (_taskCleanup) {
		_taskCleanup(_taskData);
	}
}

tp::_FastWcTaskWorker::_FastWcTaskWorker(_FastWcTaskWorker&& obj) noexcept
{
	std::memset(_taskData, 0, sizeof(_taskData));
	if (obj._taskMove) {
		obj._taskMove(_taskData, obj._taskData);
		_taskInvoke = obj._taskInvoke;
		_taskMove = obj._taskMove;
		_taskCleanup = obj._taskCleanup;
		obj._taskInvoke = nullptr;
		obj._taskMove = nullptr;
		obj._taskCleanup = nullptr;
	} 
}

tp::_FastWcTaskWorker& tp::_FastWcTaskWorker::operator=(_FastWcTaskWorker&& obj) noexcept
{
	if (this != &obj) {
		if (_taskCleanup) {
			_taskCleanup(_taskData);
		}

		if (obj._taskMove) {
			obj._taskMove(_taskData, obj._taskData);
			_taskInvoke = obj._taskInvoke;
			_taskMove = obj._taskMove;
			_taskCleanup = obj._taskCleanup;
			obj._taskInvoke = nullptr;
			obj._taskMove = nullptr;
			obj._taskCleanup = nullptr;
		}
		else {
			_taskInvoke = nullptr;
			_taskMove = nullptr;
			_taskCleanup = nullptr;
		}
	}
	
	return *this; 
}

void tp::_FastWcTaskWorker::operator()() const {
	return _taskInvoke(const_cast<void*>(static_cast<const void*>(_taskData)));
}

tp::_FastWcTaskWorker::operator bool() const noexcept {
	return _taskInvoke != nullptr;
}

size_t tp::hardware_concurrency()
{
	size_t concurrency = 0;
	DWORD length = 0;

	if (GetLogicalProcessorInformationEx(RelationAll, nullptr, &length) != FALSE) {
		return concurrency;
	}

	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		return concurrency;
	}

	using BufferType = std::unique_ptr<void, void(*)(void*)>;
	BufferType buffer(std::malloc(length), std::free);

	if (!buffer) {
		return concurrency;
	}

	unsigned char* mem = reinterpret_cast<unsigned char*>(buffer.get());
	if (GetLogicalProcessorInformationEx(RelationAll, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(mem), &length) == false) {
		return concurrency;
	}

	for (DWORD i = 0; i < length; ) {
		auto* proc = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(mem + i);
		if (proc->Relationship == RelationProcessorCore) {
			for (WORD group = 0; group < proc->Processor.GroupCount; ++group) {
				for (KAFFINITY mask = proc->Processor.GroupMask[group].Mask; mask != 0; mask >>= 1) {
					concurrency += mask & 1;
				}
			}
		}

		i += proc->Size;
	}

	return concurrency;
}
