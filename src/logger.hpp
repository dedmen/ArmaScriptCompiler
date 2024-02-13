#pragma once
#include <string>

enum class LogLevel
{
    Normal,
    Verbose
};

class LoggerBogger // Weird name because SQF-VM also has class Logger, without namespace (most of its stuff is in namespace, just not that)
{
public:
    //#TODO in-place formatting, vsnprintf
    //#TODO multithreading queue instead of locking

    //! This function might be hit by multiple threads
    void Log(LogLevel level, const std::string& msg);

    bool m_EnableVerbose = false;
};

inline LoggerBogger GLogger;