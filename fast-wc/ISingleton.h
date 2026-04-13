#pragma once
#include <mutex>

template <class Derived> 
class ISingleton {
public:
	template <typename ...Args>
	static Derived* Instance(Args...args);

protected: 
	ISingleton();
	~ISingleton(); 

	static std::unique_ptr<Derived> _sInstance; 
	static std::mutex _sMutex;
	static std::once_flag _sInitFlag; 
};

template <class Derived>
std::unique_ptr<Derived> ISingleton<Derived>::_sInstance = nullptr;

template <class Derived>
std::mutex ISingleton<Derived>::_sMutex;

template <class Derived>
std::once_flag ISingleton<Derived>::_sInitFlag;

template <class Derived>
template <typename ...Args>
inline Derived* ISingleton<Derived>::Instance(Args ...args)
{
	std::call_once(_sInitFlag, [&]() {
		_sInstance.reset(new (std::nothrow) Derived(args...));
	});

	return _sInstance.get();
}

template <class Derived>
inline ISingleton<Derived>::ISingleton()
{
}

template <class Derived> 
inline ISingleton<Derived>::~ISingleton()
{
}