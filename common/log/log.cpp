#include "log.h"

#include <ctime>
#include <chrono>
#include <cstdarg>
#include <stdio.h>

namespace debug_log {
const char *NAME = "";

void set_name(const char *name) {
    NAME = name;
}

void info(const char *format, ...) {
    auto now = std::time(nullptr);
    printf("[%ld] [%s] ", now, NAME);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
} // namespace debug_log
