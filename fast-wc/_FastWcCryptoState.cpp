#include "_FastWcCryptoState.h"

namespace crypto {
	void EvpCtxDeleter::operator()(EVP_CIPHER_CTX* ctx) const noexcept
	{
		EVP_CIPHER_CTX_free(ctx);
	}

	_FastWcCryptoState::~_FastWcCryptoState() noexcept
	{
		OPENSSL_cleanse(key.data(), key.size());
		OPENSSL_cleanse(iv.data(), iv.size());
		valid = false;
	}
}