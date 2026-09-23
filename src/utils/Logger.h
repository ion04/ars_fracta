#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace ars::utils {

/**
 * @brief Простой логгер с ринг-буфером последних записей.
 *
 * Записи отображаются в статусную строку GUI и дублируются в stdout.
 */
class Logger {
public:
    static Logger& instance();

    void info(const std::string& msg);
    void error(const std::string& msg);

    /// Копия последних записей (потокобезопасно).
    std::vector<std::string> entries() const;

private:
    Logger() = default;
    void write(const std::string& level, const std::string& msg);

    mutable std::mutex mutex_;
    std::vector<std::string> entries_;
};

} // namespace ars::utils