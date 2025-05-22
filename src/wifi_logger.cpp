#include "wifi_logger.hpp"
#include <iostream>
#include <ctime>
#include <iomanip>

namespace wificpp {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel = level;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLevel) return;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    const char* levelStr;
    switch (level) {
        case LogLevel::DEBUG:   levelStr = "DEBUG"; break;
        case LogLevel::INFO:    levelStr = "INFO"; break;
        case LogLevel::WARNING: levelStr = "WARN"; break;
        case LogLevel::ERROR:   levelStr = "ERROR"; break;
        default:               levelStr = "UNKNOWN";
    }    std::cout << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
              << " [" << levelStr << "] "
              << message << std::endl;
}

} // namespace wificpp
