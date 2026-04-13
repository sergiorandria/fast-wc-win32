#include "ISingleton.h"
#include "_ProjMacro.h"

template <class Derived>
std::unique_ptr<Derived> ISingleton<Derived>::_sInstance = nullptr;

template <class Derived>
std::mutex ISingleton<Derived>::_sMutex;

template <class Derived>
std::once_flag ISingleton<Derived>::_sInitFlag;

template <class Derived> 
Derived *ISingleton<Derived>::Instance() 
{
	std::call_once(_sInitFlag, [&]() {
		_sInstance.reset( new (std::nothrow) ISingleton());
		{
			(void)((!!(((_sInstance != nullptr)))) || (1 != _CrtDbgReportW(2, PROJ_PATH"\\ISingleton.cpp", 7, 0, L"%ls", L"(_sInstance != nullptr)")) || (__debugbreak(), 0)); 
			if (!(_sInstance != nullptr)) {
				_invoke_watson(L"_sInstance != nullptr", __LPREFIX(__FUNCTION__), PROJ_PATH"\\ISingleton.cpp", 7, 0);
			}
		};
	});
	
	return _sInstance.get(); 
}

template <class Derived>
ISingleton<Derived>::ISingleton() 
{ 
}


