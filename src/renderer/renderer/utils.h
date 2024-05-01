#pragma once

#include <vector>
#include <string>
#include <memory>
#include <sstream>
#include <iomanip>
#include <functional>
#include <array>
#include <numeric>
#include <map>
#include <chrono>
#include <stack>

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

    private:
        DebugUtils() {}
        std::vector<std::function<void(std::string)>> outputs;
    };

    struct FrameCounter
    {
        struct Sample
        {
            void Start()
            {
                startTime = std::chrono::high_resolution_clock::now();
            }

            void End()
            {
                auto endTime = std::chrono::high_resolution_clock::now();
                frameRates[i] = duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                i = (i + 1) % TotalFrames;
            }

            float GetAverageFrameTimeMs() const
            {
                return std::reduce(frameRates.begin(), frameRates.end()) / (float)TotalFrames;
            }

            static constexpr uint64_t TotalFrames = 100;
            std::array<uint64_t, TotalFrames> frameRates {};
            std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
            uint32_t i = 0;
        };

        static FrameCounter& GetInstance()
        {
            static FrameCounter frameCounter;
            return frameCounter;
        }

        void Start(std::string&& name)
        {
            samples[name].Start();
            lastName.push(name);
        }

        void End()
        {
            samples[lastName.top()].End();
            lastName.pop();
        }

        void GetPerformanceString(std::stringstream& ss)
        {
            for (const auto& sample : samples)
            {
                ss << sample.first << ": " << sample.second.GetAverageFrameTimeMs() << "\n";
            }
        }

        std::map<std::string, Sample> samples;
        std::stack<std::string> lastName;
    };
}

#define LOG(message) Utils::DebugUtils::GetInstance().Log(std::stringstream() << "File: " << __FILE__ << "|Line: " << __LINE__ << "|" << message << "\n")

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

#define CONCAT_INNER(a, b) a ## b
#define CONCAT(a, b) CONCAT_INNER(a, b)

#define NOT_FAILED(call, failureCode) \
   if ((call) == failureCode) \
   { \
        LOG("Failed with " << failureCode); \
        throw std::exception(); \
   } \

#define D3D_NOT_FAILED(call) \
   HRESULT CONCAT(result, __LINE__) = (call); \
   if (CONCAT(result, __LINE__) != S_OK) \
   { \
       LOG("Error: " << "0x" << std::hex << std::setw(8) << std::setfill('0') << CONCAT(result, __LINE__)); \
       throw std::exception(); \
   } \

#define ASSERT(call) \
    if (!(call)) throw std::exception("assert failed!");

//#define ENABLE_DETAILED_PERF_LOG

#ifdef ENABLE_DETAILED_PERF_LOG
#define PERF_START(sampleName) Utils::FrameCounter::GetInstance().Start(sampleName)
#define PERF_END() Utils::FrameCounter::GetInstance().End()
#else
#define PERF_START(sampleName)
#define PERF_END()
#endif