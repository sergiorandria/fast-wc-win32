#include "_FastWcTaskWorker.h"
#include "_ProjMacro.h"
#include <cstring>
#include <memory>
#include <functional>
#include <Windows.h>
#include <bit>

namespace tp {
    _FastWcTaskWorker::_FastWcTaskWorker() { 
        uid = idCounter++; 
        std::memset(&_taskData, 0x0000, FAST_WC_TASK_WORKER_SIZE); 
    };

    _FastWcTaskWorker::~_FastWcTaskWorker() {
        if (_taskCleanup) {
            _taskCleanup(_taskData);
        }
    }

    _FastWcTaskWorker::_FastWcTaskWorker(_FastWcTaskWorker&& obj) noexcept {
        std::memcpy(_taskData, obj._taskData, sizeof(_taskData));
        _taskInvoke = std::exchange(obj._taskInvoke, nullptr);
        _taskMove = std::exchange(obj._taskMove, nullptr);
        _taskCleanup = std::exchange(obj._taskCleanup, nullptr);
    }

    _FastWcTaskWorker& _FastWcTaskWorker::operator=(_FastWcTaskWorker&& obj) noexcept {
        if (this != &obj) {
            if (_taskCleanup) {
                _taskCleanup(_taskData);
            }

            std::memcpy(_taskData, obj._taskData, sizeof(_taskData));
            _taskInvoke = std::exchange(obj._taskInvoke, nullptr);
            _taskMove = std::exchange(obj._taskMove, nullptr);
            _taskCleanup = std::exchange(obj._taskCleanup, nullptr);
        }

        return *this;
    }

    void _FastWcTaskWorker::operator()() const {
        _taskInvoke(const_cast<void*>(static_cast<const void*>(_taskData)));
    }

    _FastWcTaskWorker::operator bool() const noexcept {
        return _taskInvoke != nullptr;
    }
}

size_t tp::hardware_concurrency() {
    size_t concurrency = 0;
    DWORD length = 0;

    if (GetLogicalProcessorInformationEx(RelationAll, nullptr, &length) == FALSE) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return concurrency;
        }
    }

    std::unique_ptr<unsigned char[]> buffer(new unsigned char[length]);
    if (GetLogicalProcessorInformationEx(RelationAll, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.get()), &length) == FALSE) {
        return concurrency;
    }

    for (DWORD i = 0; i < length;) {
        auto* proc = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.get() + i);
        if (proc->Relationship == RelationProcessorCore) {
            for (WORD group = 0; group < proc->Processor.GroupCount; ++group) {
                concurrency += std::popcount(proc->Processor.GroupMask[group].Mask);
            }
        }
        i += proc->Size;
    }

    return concurrency;
}