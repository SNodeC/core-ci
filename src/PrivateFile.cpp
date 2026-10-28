#include "PrivateFile.h"

#include "Error.h"

#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace coreci {
    std::string readPrivateFile(const std::string& path, std::size_t limit) {
        struct File {
            int descriptor;
            ~File() {
                if (descriptor >= 0) {
                    ::close(descriptor);
                }
            }
        } file{::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)};
        struct stat metadata{};
        require(file.descriptor >= 0 && ::fstat(file.descriptor, &metadata) == 0 && S_ISREG(metadata.st_mode) &&
                    metadata.st_uid == ::geteuid() && (metadata.st_mode & 0077) == 0 && metadata.st_size >= 0 &&
                    static_cast<std::uint64_t>(metadata.st_size) <= limit,
                "credential file must be private, owned, regular and bounded");
        std::string result(limit + 1, '\0');
        std::size_t size = 0;
        ssize_t count = 1;
        while (count != 0 && size < result.size()) {
            count = ::read(file.descriptor, result.data() + size, result.size() - size);
            require(count >= 0 || errno == EINTR, "credential file read failed");
            if (count > 0) {
                size += static_cast<std::size_t>(count);
            }
        }
        require(size <= limit, "credential file exceeds configured bound");
        result.resize(size);
        return result;
    }
} // namespace coreci
