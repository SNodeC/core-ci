#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

namespace {
    long long offset() {
        long long result = 0;
        const auto* path = std::getenv("CORECI_TEST_CLOCK_FILE");
        if (path) {
            const int file = open(path, O_RDONLY);
            if (file >= 0) {
                char value[32]{};
                if (read(file, value, sizeof(value) - 1) > 0) {
                    result = std::strtoll(value, nullptr, 10);
                }
                close(file);
            }
        }
        return result;
    }
} // namespace

extern "C" int clock_gettime(clockid_t id, timespec* value) noexcept {
    static const auto original = reinterpret_cast<int (*)(clockid_t, timespec*)>(dlsym(RTLD_NEXT, "clock_gettime"));
    const int result = original(id, value);
    const int saved = errno;
    if (result == 0 && (id == CLOCK_REALTIME || id == CLOCK_MONOTONIC || id == CLOCK_REALTIME_COARSE || id == CLOCK_MONOTONIC_COARSE)) {
        value->tv_sec += static_cast<decltype(value->tv_sec)>(offset());
    }
    errno = saved;
    return result;
}

extern "C" int gettimeofday(timeval* value, void*) noexcept {
    timespec now{};
    const int result = clock_gettime(CLOCK_REALTIME, &now);
    if (result == 0) {
        value->tv_sec = now.tv_sec;
        value->tv_usec = static_cast<decltype(value->tv_usec)>(now.tv_nsec / 1000);
    }
    return result;
}

extern "C" time_t time(time_t* destination) noexcept {
    timespec now{};
    time_t result = -1;
    if (clock_gettime(CLOCK_REALTIME, &now) == 0) {
        result = now.tv_sec;
        if (destination) {
            *destination = result;
        }
    }
    return result;
}
