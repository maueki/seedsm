#pragma once

#include <cstdio>
#include <cstdarg>
#include <cstring>

#ifndef SEEDS_LOG_HANDLER
#define SEEDS_LOG_HANDLER(fmt, arg) \
    {                               \
        vfprintf(stderr, fmt, arg); \
        fprintf(stderr, "\n");      \
    }
#endif

namespace seeds {

__attribute__((format(printf, 1, 2)))
static void log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    SEEDS_LOG_HANDLER(fmt, ap);
    va_end(ap);
}

}  // seeds

