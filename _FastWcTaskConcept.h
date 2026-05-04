#pragma once

#include <concepts>
#include <type_traits>
#include <string>

namespace tp {

	template<typename F, typename... Args>
	concept _FastWcSubmittable = std::invocable<F, Args...>;

	template<typename F>
	concept _FastWcCallable = std::invocable<F> && std::is_void_v<std::invoke_result_t<F>>;

	template<typename F>
	concept _FastWcCallableWithToken = std::invocable<F> && requires (F && f)
	{
		{ f.token } -> std::convertible_to<std::string>;
	};

	template <typename F, typename ...Args>
		requires (_FastWcSubmittable<F, Args...> && _FastWcCallable<F> && _FastWcCallableWithToken<F>)
	class _FastWcTaskConcept
	{

	};
}