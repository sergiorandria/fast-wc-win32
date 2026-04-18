#include "_FastWcErrorDisplay.h"

void display_error(LPCTSTR lpszFunction)
// Routine Description:
// Retrieve and output the system error message for the last-error code
{
    const DWORD err = GetLastError();

    LPVOID lpMsgBuf = nullptr;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPTSTR>(&lpMsgBuf),
        0, nullptr
    );

    LPVOID lpDisplayBuf = LocalAlloc(
        LMEM_ZEROINIT,
        (lstrlen(static_cast<LPCTSTR>(lpMsgBuf))
            + lstrlen(lpszFunction) + 50) * sizeof(TCHAR)
    );

    StringCchPrintf(
        static_cast<LPTSTR>(lpDisplayBuf),
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %lu:\n%s"),
        lpszFunction, err, static_cast<LPCTSTR>(lpMsgBuf)
    );

    MessageBox(nullptr,
        static_cast<LPCTSTR>(lpDisplayBuf),
        TEXT("Error"),
        MB_OK | MB_ICONERROR);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
}