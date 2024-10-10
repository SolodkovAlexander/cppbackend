#pragma once

#include <chrono>
#include <cassert>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>

#include <iostream>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    auto GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        return std::put_time(std::localtime(&t_c), "%F %T");
    }

    // Для имени файла возьмите дату с форматом "%Y_%m_%d"
    std::string GetFileTimeStamp() const {
        const auto now = std::chrono::system_clock::now();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        char time_str[100];
        assert(std::strftime(time_str, sizeof(time_str), "%Y_%m_%d", std::localtime(&t_c)) > 0);
        return "/var/log/sample_log_"s + std::string(std::move(time_str)) + ".log"s;
    }

    // Для имени файла возьмите дату с форматом "%Y_%m_%d"
    void OpenFile() {
        const auto file_name = GetFileTimeStamp();

        // Если имена файлов совпадают: файл открыт: выход
        if (file_name_ == file_name) {
            assert(log_file_.is_open());
            return;
        }

        if (log_file_.is_open()) {
            log_file_.close();
        }

        log_file_.open(file_name, std::ios::out | std::ios_base::app);
        assert(log_file_.is_open());
        file_name_ = file_name;
    }

    Logger() = default;
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    // Функция для вывода одного и более значений
    template <typename T0, typename... Ts>
    void LogImpl(const T0& arg0, const Ts&... args) {
        log_file_ << arg0;

        if constexpr (sizeof...(args) != 0) {
            LogImpl(args...);  // Рекурсивно выводим остальные параметры
        }
    }

    // Выведите в поток все аргументы.
    template<class... Ts>
    void Log(const Ts&... args) {
        if constexpr (sizeof...(args) == 0) {
            return;
        }

        // Выводим аргументы функции, если они не пустые
        std::lock_guard lock(m_);

        OpenFile();

        log_file_ << GetTimeStamp() << ": "sv;
        LogImpl(args...);
        log_file_ << std::endl;
    }

    // Установите manual_ts_. Учтите, что эта операция может выполняться
    // параллельно с выводом в поток, вам нужно предусмотреть 
    // синхронизацию.
    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard lock(m_);

        manual_ts_ = ts;
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    std::string file_name_;
    std::ofstream log_file_;
    std::mutex m_;
};
