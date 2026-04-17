#pragma once

#include <string>
#include <fstream>
#include <mutex>

enum class LogLevel
{
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger
{
public:
    static Logger &instance();

    void setLevel(LogLevel level);
    void setLevel(const std::string &level_str);
    void setLogFile(const std::string &path);

    void debug(const std::string &msg);
    void info(const std::string &msg);
    void warn(const std::string &msg);
    void error(const std::string &msg);

    static LogLevel levelFromString(const std::string &s);
    static std::string levelToString(LogLevel level);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void write(LogLevel level, const std::string &msg);

    LogLevel level_{LogLevel::INFO};
    std::ofstream file_sink_;
    std::mutex mutex_;
};

#define LOG_DEBUG(msg) Logger::instance().debug(msg)
#define LOG_INFO(msg) Logger::instance().info(msg)
#define LOG_WARN(msg) Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)
