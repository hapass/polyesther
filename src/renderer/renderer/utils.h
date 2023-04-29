#pragma once

#include <Windows.h>

#include <stdint.h>
#include <vector>
#include <string>
#include <cassert>

void DebugOut(const wchar_t* fmt, ...);
void PrintError(HRESULT result);

#define CONCAT_INNER(a, b) a ## b
#define CONCAT(a, b) CONCAT_INNER(a, b)

#define NOT_FAILED(call, failureCode) \
    if ((call) == failureCode) \
    { \
         throw std::exception(); \
    } \

#define D3D_NOT_FAILED(call) \
    HRESULT CONCAT(result, __LINE__) = (call); \
    if (CONCAT(result, __LINE__) != S_OK) \
    { \
        PrintError(CONCAT(result, __LINE__)); \
        throw std::exception(); \
    } \
