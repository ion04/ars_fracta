#include "utils/Logger.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ars::utils {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::setFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    filePath_ = path;
}

void Logger::info(const std::string& msg)  { write(LogLevel::Info, msg); }
void Logger::warn(const std::string& msg)  { write(LogLevel::Warn, msg); }
void Logger::error(const std::string& msg) { write(LogLevel::Error, msg); }

const std::vector<std::string>& Logger::entries() const { return entries_; }

void Logger::write(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* levelStr = "[INFO]";
    switch (level) {
        case LogLevel::Warn:  levelStr = "[WARN]";  break;
        case LogLevel::Error: levelStr = "[ERROR]"; break;
        default:              levelStr = "[INFO]";   break;
    }

    char timeBuf[32];
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", std::localtime(&tt));

    std::ostringstream os;
    os << timeBuf << ' ' << levelStr << ' ' << msg;

    entries_.push_back(os.str());
    if (entries_.size() > 200) entries_.erase(entries_.begin());

    std::cout << os.str() << '\n';

    if (!filePath_.empty()) {
        std::ofstream f(filePath_, std::ios::app);
        if (f) f << os.str() << '\n';
    }
}

} // namespace ars::utils