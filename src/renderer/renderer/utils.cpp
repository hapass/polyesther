#include <renderer/utils.h>

void DebugOut(const wchar_t* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    wchar_t dbg_out[4096];
    vswprintf_s(dbg_out, fmt, argp);
    va_end(argp);
    OutputDebugString(dbg_out);
}

void PrintError(HRESULT result)
{
    DebugOut(L"D3D Result: 0x%08X \n", result);
}
