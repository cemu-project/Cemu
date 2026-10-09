#pragma once
#include <cstdarg>

namespace drcb {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

void log_set_level(LogLevel level);
void log_set_tag(const char* tag); // printed on every line, e.g. "bridge", "mock-pad"
[[gnu::format(printf, 2, 3)]] void log(LogLevel level, const char* fmt, ...);

} // namespace drcb

#define LOGD(...) ::drcb::log(::drcb::LogLevel::Debug, __VA_ARGS__)
#define LOGI(...) ::drcb::log(::drcb::LogLevel::Info, __VA_ARGS__)
#define LOGW(...) ::drcb::log(::drcb::LogLevel::Warn, __VA_ARGS__)
#define LOGE(...) ::drcb::log(::drcb::LogLevel::Error, __VA_ARGS__)
