#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace ars::utils {

enum class LogLevel : int {
    Info = 0,
    Warn = 1,
    Error = 2,
};

/**
 * @brief Простой логгер с ринг-буфером последних записей.
 *
 * Записи отображаются в статусную строку GUI, а также могут
 * выводиться в файл (через setFile).
 */
class Logger {
public:
    static Logger& instance();

    void setFile(const std::string& path);

    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);

    const std::vector<std::string>& entries() const;

private:
    Logger() = default;
    void write(LogLevel level, const std::string& msg);

    mutable std::mutex mutex_;
    std::vector<std::string> entries_;
    std::string filePath_;
};

} // namespace ars::utils