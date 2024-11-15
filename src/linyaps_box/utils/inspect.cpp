#include "linyaps_box/utils/inspect.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <fcntl.h>

namespace linyaps_box::utils {

std::string inspect_fcntl_or_open_flags(int flags)
{
    std::stringstream ss;

    ss << "[";
    if ((flags & O_WRONLY) == 0) {
        ss << " O_RDONLY";
    }
    if (flags & O_WRONLY) {
        ss << " O_WRONLY";
    }
    if (flags & O_RDWR) {
        ss << " O_RDWR";
    }
    if (flags & O_CREAT) {
        ss << " O_CREAT";
    }
    if (flags & O_EXCL) {
        ss << " O_EXCL";
    }
    if (flags & O_NOCTTY) {
        ss << " O_NOCTTY";
    }
    if (flags & O_TRUNC) {
        ss << " O_TRUNC";
    }
    if (flags & O_APPEND) {
        ss << " O_APPEND";
    }
    if (flags & O_NONBLOCK) {
        ss << " O_NONBLOCK";
    }
    if (flags & O_NDELAY) {
        ss << " O_SYNC";
    }
    if (flags & O_SYNC) {
        ss << " O_SYNC";
    }
    if (flags & O_ASYNC) {
        ss << " O_ASYNC";
    }
    if (flags & O_LARGEFILE) {
        ss << " O_LARGEFILE";
    }
    if (flags & O_DIRECTORY) {
        ss << " O_DIRECTORY";
    }
    if (flags & O_NOFOLLOW) {
        ss << " O_NOFOLLOW";
    }
    if (flags & O_CLOEXEC) {
        ss << " O_CLOEXEC";
    }
    if (flags & O_DIRECT) {
        ss << " O_DIRECT";
    }
    if (flags & O_NOATIME) {
        ss << " O_NOATIME";
    }
    if (flags & O_PATH) {
        ss << " O_PATH";
    }
    if (flags & O_DSYNC) {
        ss << " O_DSYNC";
    }
    if ((flags & O_TMPFILE) == O_TMPFILE) {
        ss << " O_TMPFILE";
    }
    ss << " ]";
    return ss.str();
}

std::string inspect_fd(const std::filesystem::path &fdinfo_path)
{
    std::stringstream ss;

    ss << std::filesystem::read_symlink("/proc/self/fd/" + fdinfo_path.stem().string());

    std::ifstream fdinfo(fdinfo_path);
    assert(fdinfo.is_open());

    std::string key;

    while (fdinfo >> key) {
        ss << " " << key << " ";
        if (key != "flags:") {
            std::string value;
            fdinfo >> value;
            ss << value;
            continue;
        }

        int value;

        fdinfo >> std::oct >> value;
        ss << inspect_fcntl_or_open_flags(value);
    }

    return ss.str();
}

std::string inspect_fd(int fd)
{
    return inspect_fd("/proc/self/fdinfo/" + std::to_string(fd));
}

std::string inspect_fds()
{
    std::stringstream ss;

    bool first_line = true;

    for (const auto &entry : std::filesystem::directory_iterator("/proc/self/fdinfo")) {
        if (entry.path() == "/proc/self/fdinfo/0" || entry.path() == "/proc/self/fdinfo/1"
            || entry.path() == "/proc/self/fdinfo/2") {
            continue;
        }
        if (!first_line) {
            ss << std::endl;
        }
        first_line = false;
        ss << entry.path() << " " << inspect_fd(entry.path());
    }

    return ss.str();
}

} // namespace linyaps_box::utils
