#pragma once

#include <vector>
#include <string>
#include <memory>
#include <sstream>
#include <iomanip>
#include <functional>

namespace Utils
{
    struct DebugUtils
    {
        static DebugUtils& GetInstance()
        {
            static DebugUtils utils;
            return utils;
        }

        DebugUtils& AddOutput(std::function<void(std::string)> func)
        {
            outputs.push_back(func);
            return *this;
        }

        DebugUtils& RemoveOutputs()
        {
            outputs.clear();
            return *this;
        }

        void Log(const std::stringstream& ss)
        {
            std::string message = ss.str();
            for (const auto& output : outputs)
            {
                output(message);
            }
        }

        void LogError(long error)
        {
            Log(std::stringstream() << "Error: " << "0x" << std::hex << std::setw(8) << std::setfill('0') << error);
        }

    private:
        DebugUtils() {}
        std::vector<std::function<void(std::string)>> outputs;
    };
}

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
       Utils::DebugUtils::GetInstance().LogError(CONCAT(result, __LINE__)); \
       throw std::exception(); \
   } \

#define LOG(message) Utils::DebugUtils::GetInstance().Log(std::stringstream() << "File: " << __FILE__ << "|Line: " << __LINE__ << "|" << message)

#define REPORT_ERROR() LOG("Error."); return false

#define REPORT_ERROR_IF_FALSE(value) \
    if (value) \
    { \
        return true; \
    } \
    else \
    { \
        REPORT_ERROR(); \
    } \
