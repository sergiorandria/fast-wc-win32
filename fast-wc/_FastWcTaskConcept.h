#pragma once

#pragma once

#include <concepts>
#include <type_traits>

namespace tp {

	template<typename C, typename... Args>
	concept FastWcSubmittable = std::invocable<C, Args...>;

	template<typename C>
	concept FastWcCallable = std::invocable<C> && std::is_void_v<std::invoke_result_t<C>>;

	// TODO: Should add more security concept
}