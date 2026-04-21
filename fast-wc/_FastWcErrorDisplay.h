#pragma once
//#define NOMINMAX
#include <Windows.h>

#include <thread>
#include <strsafe.h>
#include <tchar.h>

void _FastWcErrorDisplay(LPCTSTR lpszFunction);

#pragma weak __FastWcErrorDisplay
#pragma weak ___FastWcErrorDisplay 

