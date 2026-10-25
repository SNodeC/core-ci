#include "PrivateFile.h"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace coreci {

    std::string readPrivateFile(const std::string& path, std::size_t limit) {
        const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "open protected credential file");
        }

        struct stat info{};
        const bool owned = ::fstat(fd, &info) == 0 && S_ISREG(info.st_mode) && (info.st_mode & 0077) == 0 && info.st_uid == ::geteuid();
        std::string content(owned ? limit + 1 : 0, '\0');
        std::size_t offset = 0;
        ssize_t count = 1;
        while (owned && count > 0 && offset < content.size()) {
            count = ::read(fd, content.data() + offset, content.size() - offset);
            if (count > 0) {
                offset += static_cast<std::size_t>(count);
            }
        }
        ::close(fd);

        if (!owned || count < 0 || offset > limit) {
            throw std::runtime_error("credential file must be owned, private, regular and within its size limit");
        }
        content.resize(offset);
        return content;
    }

} // namespace coreci
