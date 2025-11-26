#include "logging.hpp"
#include <cstdio>

void init_logging() {
}

void log_info(const char* msg) {
    std::printf("[INFO] %s\n", msg);
}
