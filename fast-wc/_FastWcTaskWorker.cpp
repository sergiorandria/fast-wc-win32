#include "_FastWcTaskWorker.h"
#include "_FastWcErrorDisplay.h"
#include "_ProjMacro.h"
#include <cstring>
#include <memory>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <bit>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <string>

namespace tp {
    _FastWcTaskWorker::_FastWcTaskWorker() { 
        uid = idCounter++; 
        std::memset(&_taskData, 0x0000, FAST_WC_TASK_WORKER_SIZE); 

        // Define key and iv once, to improve 
        // encrypt/decrypt operations
        if (RAND_bytes(_cryptoState.key.data(), static_cast<int>(crypto::AES_KEY_SIZE)) != 1 ||
            RAND_bytes(_cryptoState.iv.data(), static_cast<int>(crypto::AES_IV_SIZE)) != 1)
        {
            throw std::runtime_error("RAND_bytes failed: " + crypto::ssl_error_string());
        }
    };

    _FastWcTaskWorker::~_FastWcTaskWorker() {
        if (_taskCleanup) {
            _taskCleanup(_taskData);
        }
    }

    _FastWcTaskWorker::_FastWcTaskWorker(_FastWcTaskWorker&& obj) noexcept 
        : uid(idCounter++)
    {
        std::memcpy(_taskData, obj._taskData, sizeof(_taskData));
        _taskInvoke = std::exchange(obj._taskInvoke, nullptr);
        _taskMove = std::exchange(obj._taskMove, nullptr);
        _taskCleanup = std::exchange(obj._taskCleanup, nullptr);
        //encryptTaskDataMem();
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
            //encryptTaskDataMem();
            
        }

        return *this;
    }

    void _FastWcTaskWorker::operator()() {
        //decryptTaskDataMem();
        _taskInvoke(const_cast<void*>(static_cast<const void*>(_taskData)));
        //encryptTaskDataMem();
    }

    _FastWcTaskWorker::operator bool() const noexcept {
        return _taskInvoke != nullptr;
    }

    char& _FastWcTaskWorker::operator[](std::size_t index)
    {
        if (index <= 0 || index > FAST_WC_TASK_WORKER_SIZE)
        {
            throw std::runtime_error("operator[]");
        }

        // Return encrypted value
        return _taskData[index];
    }

    const char& _FastWcTaskWorker::operator[](std::size_t index) const
    {
        if (index <= 0 || index > FAST_WC_TASK_WORKER_SIZE)
        {
            throw std::runtime_error("operator[]");
        }

        // Return encrypted value
        return _taskData[index];
    }

    bool _FastWcTaskWorker::verifyTaskDataIntegrity()
    {
        auto key = _cryptoState.key.data(); 
        auto iv = _cryptoState.iv.data();

        // If encrypt is called
        if (_cryptoState.valid)
        {
            // Trying to decrypt _taskData 
            const unsigned char* src = reinterpret_cast<const unsigned char*>(_taskData);

            crypto::EvpCtxPtr ctx = crypto::make_evp_ctx();
            
            if (EVP_DecryptInit_ex(ctx.get(),
                EVP_aes_256_cbc(),
                nullptr,
                key,
                iv) != 1)
            {
                throw std::runtime_error("EVP_DecryptInit_ex failed: " + crypto::ssl_error_string());
            }

            const unsigned char* ciphertextPtr = src + crypto::AES_IV_SIZE;
            const std::size_t    ciphertextSize = FAST_WC_TASK_WORKER_SIZE - crypto::AES_IV_SIZE;
            
            std::vector<unsigned char> plaintext(ciphertextSize + EVP_MAX_BLOCK_LENGTH);
            int len = 0;
            int totalLen = 0;
            
            if (EVP_DecryptUpdate(ctx.get(),
                plaintext.data(),
                &len,
                ciphertextPtr,
                static_cast<int>(ciphertextSize)) != 1)
            {
                return false;
            }

            if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &len) != 1)
            {
                // Wrong key, corrupted data, or bad padding ,all surface here
                return false;
            }

            return true;
        }
    }

    // The iv is prepended to the ciphertext so decrypt() is self-contained.
    void _FastWcTaskWorker::encryptTaskDataMem()
    {
        if (_cryptoState.valid)
        {
            // Already encrypted
            return;
        }

        const std::vector<unsigned char> plaintext(
            reinterpret_cast<const unsigned char*>(_taskData),
            reinterpret_cast<const unsigned char*>(_taskData) + FAST_WC_TASK_WORKER_SIZE
        );

        _cryptoState.valid = true;

        crypto::EvpCtxPtr ctx = crypto::make_evp_ctx();

        if (EVP_EncryptInit_ex(ctx.get(),
            EVP_aes_256_cbc(),
            nullptr,
            _cryptoState.key.data(),
            _cryptoState.iv.data()) != 1)
        {
            throw std::runtime_error("EVP_EncryptInit_ex failed: " + crypto::ssl_error_string());
        }

        // ciphertext can be at most plaintext + one full block of padding
        std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
        int len = 0;
        int totalLen = 0;

        if (EVP_EncryptUpdate(ctx.get(),
            ciphertext.data(),
            &len,
            plaintext.data(),
            static_cast<int>(plaintext.size())) != 1)
        {
            throw std::runtime_error("EVP_EncryptUpdate failed: " + crypto::ssl_error_string());
        }

        totalLen = len;

        if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + totalLen, &len) != 1)
        {
            throw std::runtime_error("EVP_EncryptFinal_ex failed: " + crypto::ssl_error_string());
        }

        totalLen += len;
        ciphertext.resize(totalLen);

        const std::size_t requiredSize = crypto::AES_IV_SIZE + ciphertext.size();
        if (requiredSize > FAST_WC_TASK_WORKER_SIZE)
        {
            throw std::runtime_error(
                "encryptTaskDataMem: ciphertext too large for task buffer ("
                + std::to_string(requiredSize) + " > "
                + std::to_string(FAST_WC_TASK_WORKER_SIZE) + ")"
            );
        }

        unsigned char* dst = reinterpret_cast<unsigned char*>(_taskData);
        std::memcpy(dst, _cryptoState.iv.data(), crypto::AES_IV_SIZE);
        std::memcpy(dst + crypto::AES_IV_SIZE, ciphertext.data(), ciphertext.size());
    }

    void _FastWcTaskWorker::decryptTaskDataMem()
    {
        if (!_cryptoState.valid)
        {
            throw std::logic_error(
                "decryptTaskDataMem: no valid crypto state — was encryptTaskDataMem called?");
        }

        static_assert(FAST_WC_TASK_WORKER_SIZE > crypto::AES_IV_SIZE,
            "AES_IV_SIZE is greater than fast wc than FAST_WC_TASK_WORKER_SIZE");

        // The IV was prepended by encryptTaskDataMem.
        // We read it back here instead of trusting _cryptoState.iv
        // so that the layout is always the single source of truth.
        const unsigned char* src = reinterpret_cast<const unsigned char*>(_taskData);

        std::array<unsigned char, crypto::AES_IV_SIZE> iv{};
        std::memcpy(iv.data(), src, crypto::AES_IV_SIZE);

        const unsigned char* ciphertextPtr = src + crypto::AES_IV_SIZE;
        const std::size_t    ciphertextSize = FAST_WC_TASK_WORKER_SIZE - crypto::AES_IV_SIZE;

        crypto::EvpCtxPtr ctx = crypto::make_evp_ctx();

        if (EVP_DecryptInit_ex(ctx.get(),
            EVP_aes_256_cbc(),
            nullptr,
            _cryptoState.key.data(),
            iv.data()) != 1)
        {
            throw std::runtime_error("EVP_DecryptInit_ex failed: " + crypto::ssl_error_string());
        }

        std::vector<unsigned char> plaintext(ciphertextSize + EVP_MAX_BLOCK_LENGTH);
        int len = 0;
        int totalLen = 0;

        if (EVP_DecryptUpdate(ctx.get(),
            plaintext.data(),
            &len,
            ciphertextPtr,
            static_cast<int>(ciphertextSize)) != 1)
        {
            throw std::runtime_error("EVP_DecryptUpdate failed: " + crypto::ssl_error_string());
        }

        totalLen = len;

        if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + totalLen, &len) != 1)
        {
            // Wrong key, corrupted data, or bad padding ,all surface here
            throw std::runtime_error("EVP_DecryptFinal_ex failed (bad key/padding): "
                + crypto::ssl_error_string());
        }

        totalLen += len;
        plaintext.resize(totalLen);

        std::memset(_taskData, 0, FAST_WC_TASK_WORKER_SIZE);
        std::memcpy(_taskData, plaintext.data(), std::min(static_cast<std::size_t>(totalLen), (std::size_t)FAST_WC_TASK_WORKER_SIZE));

        OPENSSL_cleanse(_cryptoState.key.data(), crypto::AES_KEY_SIZE);
        OPENSSL_cleanse(_cryptoState.iv.data(), crypto::AES_IV_SIZE);
        _cryptoState.valid = false;
    }
}

std::size_t tp::hardware_concurrency() {
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