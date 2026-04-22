#include "_FastWcCapabilityToken.h"

[[nodiscard]] tp::_FastWcCapabilityToken tp::_FastWcTokenAuthority::issue(_FastWcPrivilegeTier tier)
{
	auto     valid = std::make_shared<std::atomic_bool>(true);
	uint64_t id = _nextId.fetch_add(1, std::memory_order_relaxed);
	{
		std::lock_guard lock(_mutex);
		_issued[id] = valid;
	}
	return _FastWcCapabilityToken(id, tier, valid);
}

void tp::_FastWcTokenAuthority::revoke(uint64_t tokenId)
{
	std::lock_guard lock(_mutex);
	if (auto it = _issued.find(tokenId); it != _issued.end()) {
		if (auto ptr = it->second.lock())
			ptr->store(false, std::memory_order_release);
		_issued.erase(it);
	}
}

void tp::_FastWcTokenAuthority::revokeAll()
{
	std::lock_guard lock(_mutex);
	for (auto& [id, weakPtr] : _issued)
		if (auto ptr = weakPtr.lock())
			ptr->store(false, std::memory_order_release);
	_issued.clear();
}

bool tp::_FastWcCapabilityToken::isValid() const noexcept
{
	return _valid && _valid->load(std::memory_order_acquire);
}

tp::_FastWcPrivilegeTier tp::_FastWcCapabilityToken::tier() const noexcept
{
	return _tier;
}

std::uint64_t tp::_FastWcCapabilityToken::id() const noexcept
{
	return _id;
}
