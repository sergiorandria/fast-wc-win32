#pragma once

#pragma once

#include <concepts>
#include <type_traits>

namespace tp {

	template<typename F, typename... Args>
	concept _FastWcSubmittable = std::invocable<F, Args...>;

	template<typename F>
	concept _FastWcCallable = std::invocable<F> && std::is_void_v<std::invoke_result_t<F>>;

	template<typename F>
	concept _FastWcCallableWithToken = std::invocable<F> && requires (F && f)
	{
		std::is_convertible_v<decltype(f.token), std::string>;
	};

	template <typename F, typename ...Args>
		requires (_FastWcSubmittable<F, Args...>&& _FastWcCallable<F>&& _FastWcCallableWithToken<F>)
	class _FastWcTaskConcept
	{
	public:
		template<class Concept>
		constexpr bool _is_concept_callable()
		{
			static_assert(_FastWcCallable<Concept>, "Mismatching concept!\n");
			return true;
		}

		template<class Concept>
		constexpr bool _is_concept_submittable()
		{
			static_assert(_FastWcSubmittable<Concept>, "Mismatching concept!\n");
			return true;
		}

		template<class Concept>
		constexpr bool _is_concept_callable_with_token()
		{
			static_assert(_FastWcCallableWithToken<Concept>, "Mismatching concept!\n");
			return true;
		}
	};
}