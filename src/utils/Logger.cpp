#include "utils/Logger.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>

namespace ars::utils {

static std::tm safe_localtime(const std::time_t* t) {
    std::tm tm{};
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
    localtime_s(&tm, t);
#else
    localtime_r(t, &tm);
#endif
    return tm;
}

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
    const std::tm tm = safe_localtime(&tt);
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm);

    std::ostringstream os;
    os << timeBuf << ' ' << level << ' ' << msg;

    entries_.push_back(os.str());
    if (entries_.size() > 200) entries_.erase(entries_.begin());

    std::cout << os.str() << '\n';
}

} // namespace ars::utils