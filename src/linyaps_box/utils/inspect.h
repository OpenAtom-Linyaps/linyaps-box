#pragma once

#include <string>

namespace linyaps_box::utils {

std::string inspect_fcntl_or_open_flags(int flags);
std::string inspect_fd(int fd);
std::string inspect_fds();

} // namespace linyaps_box::utils
