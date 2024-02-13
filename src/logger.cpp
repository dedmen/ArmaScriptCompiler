#include "logger.hpp"
#include <mutex>
#include <iostream>


std::mutex loggerMutex;

void LoggerBogger::Log(LogLevel level, const std::string& msg)
{
    if (level == LogLevel::Verbose && !m_EnableVerbose)
        return;

    std::lock_guard m(loggerMutex);

    std::cout << msg.c_str() << "\n";
}
