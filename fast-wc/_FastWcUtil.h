#pragma once

#include <future>

namespace util {
	template <typename T>
	inline constexpr bool is_future_v = false;
	template <typename T>
	inline constexpr bool is_future_v<std::future<T>> = true;


	size_t intWidth(size_t n);
}

