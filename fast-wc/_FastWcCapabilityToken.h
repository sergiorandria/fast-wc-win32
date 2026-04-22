#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstdint>
#include <stdexcept>

namespace tp {

	enum class _FastWcPrivilegeTier : std::uint8_t { Low = 0, High = 1 };

	class _FastWcCapabilityToken {
		friend class _FastWcTokenAuthority;

		std::uint64_t                     _id;
		_FastWcPrivilegeTier              _tier;
		std::shared_ptr<std::atomic_bool> _valid;

		_FastWcCapabilityToken(std::uint64_t id, _FastWcPrivilegeTier tier, std::shared_ptr<std::atomic_bool> valid)
			: _id(id), _tier(tier), _valid(std::move(valid)) 
		{
		}

	public:
		_FastWcCapabilityToken() = delete;

		bool isValid() const noexcept;

		_FastWcPrivilegeTier tier() const noexcept; 
		std::uint64_t        id()   const noexcept; 
	};

	class _FastWcTokenAuthority {
		std::atomic<uint64_t>                                     _nextId{ 0 };
		std::mutex                                                _mutex;
		std::unordered_map<uint64_t, std::weak_ptr<std::atomic_bool>> _issued;

	public:
		[[nodiscard]] _FastWcCapabilityToken issue(_FastWcPrivilegeTier tier);

		void revoke(uint64_t tokenId);

		void revokeAll();
	};

}