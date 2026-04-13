#pragma once

#include <future>

namespace util {
	template <typename T>
	struct is_future_v : std::false_type {};
	template <typename T>
	struct is_future_v<std::future<T>> : std::true_type {};

	
	size_t intWidth(size_t n);
}

