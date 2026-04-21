#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <memory>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

namespace crypto
{
    inline constexpr std::size_t AES_KEY_SIZE   = 32; // AES-256
    inline constexpr std::size_t AES_IV_SIZE    = 16; // CBC block size

    struct EvpCtxDeleter
    {
        void operator()(EVP_CIPHER_CTX* ctx) const noexcept;
    };

    using EvpCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, EvpCtxDeleter>;

    inline EvpCtxPtr make_evp_ctx()
    {
        EvpCtxPtr ctx(EVP_CIPHER_CTX_new());
        if (!ctx)
        {
            throw std::runtime_error("EVP_CIPHER_CTX_new() failed");
        }
        return ctx;
    }

    struct CryptoState
    {
        std::array<unsigned char, AES_KEY_SIZE> key{};
        std::array<unsigned char, AES_IV_SIZE>  iv{};
        bool valid = false;


        ~CryptoState() noexcept;

        // Non-copyable, key material must not be duplicated casually
        CryptoState() = default;
        CryptoState(const CryptoState&) = delete;
        CryptoState& operator=(const CryptoState&) = delete;
        CryptoState(CryptoState&&) = default;
        CryptoState& operator=(CryptoState&&) = default;
    };

    inline std::string ssl_error_string() noexcept
    {
        char buf[256]{};
        ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
        return buf;
    }

} // namespace crypto
