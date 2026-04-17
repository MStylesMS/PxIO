#include "logger.h"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>

// ─── singleton ───────────────────────────────────────────────────────────────

Logger &Logger::instance()
{
    static Logger inst;
    return inst;
}

Logger::~Logger()
{
    if (file_sink_.is_open())
        file_sink_.close();
}

// ─── configuration ───────────────────────────────────────────────────────────

void Logger::setLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lk(mutex_);
    level_ = level;
}

void Logger::setLevel(const std::string &level_str)
{
    setLevel(levelFromString(level_str));
}

void Logger::setLogFile(const std::string &path)
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (file_sink_.is_open())
        file_sink_.close();
    file_sink_.open(path, std::ios::app);
    if (!file_sink_.is_open())
        std::cerr << "[ERROR] Logger: cannot open log file: " << path << "\n";
}

// ─── public interface ────────────────────────────────────────────────────────

void Logger::debug(const std::string &msg) { write(LogLevel::DEBUG, msg); }
void Logger::info(const std::string &msg) { write(LogLevel::INFO, msg); }
void Logger::warn(const std::string &msg) { write(LogLevel::WARN, msg); }
void Logger::error(const std::string &msg) { write(LogLevel::ERROR, msg); }

// ─── static helpers ──────────────────────────────────────────────────────────

LogLevel Logger::levelFromString(const std::string &s)
{
    std::string up = s;
    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
    if (up == "DEBUG")
        return LogLevel::DEBUG;
    if (up == "WARN")
        return LogLevel::WARN;
    if (up == "ERROR")
        return LogLevel::ERROR;
    return LogLevel::INFO;
}

std::string Logger::levelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    default:
        return "INFO";
    }
}

// ─── internal write ──────────────────────────────────────────────────────────

static std::string now_iso8601()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return ss.str();
}

void Logger::write(LogLevel level, const std::string &msg)
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (level < level_)
        return;

    std::string line = "[" + levelToString(level) + "] " + now_iso8601() + " " + msg + "\n";

    if (level >= LogLevel::WARN)
        std::cerr << line;
    else
        std::cout << line;

    // File sink: WARN and ERROR only
    if (file_sink_.is_open() && level >= LogLevel::WARN)
    {
        file_sink_ << line;
        file_sink_.flush();
    }
}
