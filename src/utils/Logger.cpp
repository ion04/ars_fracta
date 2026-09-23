#include "utils/Logger.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>

namespace ars::utils {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::info(const std::string& msg)  { write("[INFO]", msg); }
void Logger::error(const std::string& msg) { write("[ERROR]", msg); }

std::vector<std::string> Logger::entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

void Logger::write(const std::string& level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    char timeBuf[32];
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", std::localtime(&tt));

    std::ostringstream os;
    os << timeBuf << ' ' << level << ' ' << msg;

    entries_.push_back(os.str());
    if (entries_.size() > 200) entries_.erase(entries_.begin());

    std::cout << os.str() << '\n';
}

} // namespace ars::utils