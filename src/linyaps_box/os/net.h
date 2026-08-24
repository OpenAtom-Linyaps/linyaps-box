// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/io.h"
#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/enum_formatter.h"
#include "linyaps_box/utils/file_describer.h"

#include <cstring>
#include <vector>

#include <sys/socket.h>

namespace linyaps_box::os {

namespace sys {

enum class send_flag : uint16_t {
    none = 0,
    confirm = MSG_CONFIRM,
    dontroute = MSG_DONTROUTE,
    dontwait = MSG_DONTWAIT,
    eor = MSG_EOR,
    more = MSG_MORE,
    nosignal = MSG_NOSIGNAL,
    oob = MSG_OOB,
    LINYAPS_MARK_AS_BITMASK_ENUM(more),
};
LINYAPS_REGISTER_ENUM(send_flag,
                      { send_flag::none, "NONE" },
                      { send_flag::confirm, "MSG_CONFIRM" },
                      { send_flag::dontroute, "MSG_DONTROUTE" },
                      { send_flag::dontwait, "MSG_DONTWAIT" },
                      { send_flag::eor, "MSG_EOR" },
                      { send_flag::more, "MSG_MORE" },
                      { send_flag::nosignal, "MSG_NOSIGNAL" },
                      { send_flag::oob, "MSG_OOB" })

enum class recv_flag : uint32_t {
    none = 0,
    cmsg_cloexec = MSG_CMSG_CLOEXEC,
    dontwait = MSG_DONTWAIT,
    errqueue = MSG_ERRQUEUE,
    oob = MSG_OOB,
    peek = MSG_PEEK,
    trunc = MSG_TRUNC,
    waitall = MSG_WAITALL,
    LINYAPS_MARK_AS_BITMASK_ENUM(cmsg_cloexec),
};
LINYAPS_REGISTER_ENUM(recv_flag,
                      { recv_flag::none, "NONE" },
                      { recv_flag::cmsg_cloexec, "MSG_CMSG_CLOEXEC" },
                      { recv_flag::dontwait, "MSG_DONTWAIT" },
                      { recv_flag::errqueue, "MSG_ERRQUEUE" },
                      { recv_flag::oob, "MSG_OOB" },
                      { recv_flag::peek, "MSG_PEEK" },
                      { recv_flag::trunc, "MSG_TRUNC" },
                      { recv_flag::waitall, "MSG_WAITALL" })

enum class return_flag : uint32_t {
    none = 0,
    oob = MSG_OOB,
    eor = MSG_EOR,
    trunc = MSG_TRUNC,
    ctrunc = MSG_CTRUNC,
    errqueue = MSG_ERRQUEUE,
    cmsg_cloexec = MSG_CMSG_CLOEXEC,
    LINYAPS_MARK_AS_BITMASK_ENUM(cmsg_cloexec),
};
LINYAPS_REGISTER_ENUM(return_flag,
                      { return_flag::none, "NONE" },
                      { return_flag::oob, "MSG_OOB" },
                      { return_flag::eor, "MSG_EOR" },
                      { return_flag::trunc, "MSG_TRUNC" },
                      { return_flag::ctrunc, "MSG_CTRUNC" },
                      { return_flag::errqueue, "MSG_ERRQUEUE" },
                      { return_flag::cmsg_cloexec, "MSG_CMSG_CLOEXEC" })

enum class address_family : uint8_t { unspecified = AF_UNSPEC, unix = AF_UNIX };
LINYAPS_REGISTER_ENUM(address_family,
                      { address_family::unspecified, "AF_UNSPEC" },
                      { address_family::unix, "AF_UNIX" })

enum class socket_type : uint8_t {
    stream = SOCK_STREAM,
    datagram = SOCK_DGRAM,
    raw = SOCK_RAW,
    rdm = SOCK_RDM,
    seqpacket = SOCK_SEQPACKET
};
LINYAPS_REGISTER_ENUM(socket_type,
                      { socket_type::stream, "SOCK_STREAM" },
                      { socket_type::datagram, "SOCK_DGRAM" },
                      { socket_type::raw, "SOCK_RAW" },
                      { socket_type::rdm, "SOCK_RDM" },
                      { socket_type::seqpacket, "SOCK_SEQPACKET" })

enum class socket_flag : uint32_t {
    none = 0,
    nonblock = SOCK_NONBLOCK,
    cloexec = SOCK_CLOEXEC,
    LINYAPS_MARK_AS_BITMASK_ENUM(cloexec),
};
LINYAPS_REGISTER_ENUM(socket_flag,
                      { socket_flag::none, "NONE" },
                      { socket_flag::nonblock, "SOCK_NONBLOCK" },
                      { socket_flag::cloexec, "SOCK_CLOEXEC" })

namespace cmsg {

struct rights
{
    static constexpr auto level = SOL_SOCKET;
    static constexpr auto type = SCM_RIGHTS;

    std::size_t count{ 1 };

    using value_type = utils::span<const int>;

    [[nodiscard]] constexpr std::size_t payload_size() const noexcept
    {
        return sizeof(int) * count;
    }

    [[nodiscard]] static value_type parse(utils::span<const std::byte> raw) noexcept
    {
        return { reinterpret_cast<const int *>(raw.data()), raw.size() / sizeof(int) };
    }
};

struct credentials
{
    static constexpr int level = SOL_SOCKET;
    static constexpr int type = SCM_CREDENTIALS;

    using value_type = struct ucred;

    [[nodiscard]] constexpr std::size_t payload_size() const noexcept { return sizeof(::ucred); }

    [[nodiscard]] static std::optional<value_type> parse(utils::span<const std::byte> raw) noexcept
    {
        constexpr auto data_size = sizeof(value_type);
        if (raw.size() < data_size) {
            return std::nullopt;
        }

        value_type cred{ };
        std::memcpy(&cred, raw.data(), data_size);
        return cred;
    }
};

template <typename... Tags>
constexpr std::size_t space(Tags... tags) noexcept
{
    return (CMSG_SPACE(tags.payload_size()) + ...);
}

// Total buffer size needed to hold the given cmsg messages plus extra room for
// the internal cmsghdr alignment adjustment done by ancillary_buffer /
// ancillary_buffer_writer.  Callers allocate a std::array<std::byte,
// buffer_size(...)> (no alignas needed); the buffer types align internally.
template <typename... Tags>
constexpr std::size_t buffer_size(Tags... tags) noexcept
{
    return space(tags...) + (alignof(struct cmsghdr) - 1);
}

} // namespace cmsg
} // namespace sys

namespace detail {

// Aligns the start of `buffer` up to the next `cmsghdr` alignment boundary.
// The caller must allocate enough slack (see cmsg::buffer_size) so that the
// aligned sub-span does not run past the end of the buffer.
inline auto align_for_cmsghdr(utils::span<std::byte> buffer) noexcept -> utils::span<std::byte>
{
    const auto align = alignof(struct cmsghdr);
    auto addr = reinterpret_cast<std::uintptr_t>(buffer.data());
    const auto adjusted = (addr + (align - 1)) & ~static_cast<std::uintptr_t>(align - 1);
    const auto skip = adjusted - addr;
    if (skip > buffer.size()) {
        return { };
    }

    return buffer.subspan(skip);
}

} // namespace detail

class endpoint
{
public:
    constexpr endpoint() noexcept { storage_.ss_family = AF_UNSPEC; }

    endpoint(const sockaddr *addr, socklen_t len) noexcept;

    static auto from_path(const std::filesystem::path &path) noexcept -> os::Result<endpoint>;

    [[nodiscard]] auto data() const noexcept -> const sockaddr *
    {
        return reinterpret_cast<const ::sockaddr *>(&storage_);
    }

    [[nodiscard]] auto data_mut() noexcept -> sockaddr *
    {
        return reinterpret_cast<::sockaddr *>(&storage_);
    }

    [[nodiscard]] auto size() const noexcept -> socklen_t { return len_; }

    [[nodiscard]] auto size_ptr() noexcept -> socklen_t * { return &len_; }

    [[nodiscard]] static constexpr auto capacity() noexcept -> socklen_t
    {
        return sizeof(sockaddr_storage);
    }

    [[nodiscard]] auto family() const noexcept -> sa_family_t { return storage_.ss_family; }

private:
    sockaddr_storage storage_{ };
    socklen_t len_{ 0 };
};

class ancillary_buffer_writer
{
public:
    using value_type = std::byte;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = std::byte *;
    using const_pointer = const std::byte *;

    explicit ancillary_buffer_writer(utils::span<std::byte> buffer) noexcept
        : buffer_(detail::align_for_cmsghdr(buffer))
    {
    }

    ancillary_buffer_writer(const ancillary_buffer_writer &) = delete;
    ancillary_buffer_writer &operator=(const ancillary_buffer_writer &) = delete;
    ancillary_buffer_writer(ancillary_buffer_writer &&) noexcept = default;
    ancillary_buffer_writer &operator=(ancillary_buffer_writer &&) noexcept = default;

    ~ancillary_buffer_writer() noexcept = default;

    template <typename Tag, typename T, typename = std::enable_if_t<!utils::is_span_v<T>>>
    [[nodiscard]] auto try_push_back(const T &val) noexcept -> bool
    {
        if constexpr (std::is_same_v<Tag, sys::cmsg::rights>) {
            int raw_fd = static_cast<int>(val);
            return try_push_raw(Tag::level, Tag::type, &raw_fd, sizeof(int));
        } else {
            return try_push_raw(Tag::level, Tag::type, std::addressof(val), sizeof(T));
        }
    }

    template <typename Tag,
              typename Fd,
              typename = std::enable_if_t<
                std::is_same_v<Tag, sys::cmsg::rights>
                && (std::is_same_v<std::decay_t<Fd>, int>
                    || std::is_same_v<std::decay_t<Fd>, utils::file_descriptor_ref>)>>
    [[nodiscard]] auto try_push_back(utils::span<const Fd> fds) noexcept -> bool
    {
        if (fds.empty()) {
            return true;
        }

        static_assert(sizeof(Fd) == sizeof(int));
        static_assert(alignof(Fd) == alignof(int));
        return try_push_raw(Tag::level, Tag::type, fds.data(), fds.size_bytes());
    }

    template <typename Tag, typename = std::enable_if_t<std::is_same_v<Tag, sys::cmsg::rights>>>
    auto try_push_back(utils::file_descriptor &&) noexcept = delete;

    template <typename Tag, typename = std::enable_if_t<std::is_same_v<Tag, sys::cmsg::rights>>>
    auto try_push_back(utils::span<utils::file_descriptor> &&) noexcept = delete;

    template <typename Tag>
    [[nodiscard]] auto try_push_bytes(utils::span<const std::byte> bytes) noexcept -> bool
    {
        return try_push_raw(Tag::level, Tag::type, bytes.data(), bytes.size());
    }

    [[nodiscard]] auto try_push_bytes(int level,
                                      int type,
                                      utils::span<const std::byte> bytes) noexcept -> bool
    {
        return try_push_raw(level, type, bytes.data(), bytes.size());
    }

    [[nodiscard]] const_pointer data() const noexcept { return buffer_.data(); }

    [[nodiscard]] pointer data() noexcept { return buffer_.data(); }

    [[nodiscard]] size_type size() const noexcept { return size_; }

    [[nodiscard]] size_type capacity() const noexcept { return buffer_.size(); }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] utils::span<const std::byte> as_span() const noexcept
    {
        return buffer_.subspan(0, size_);
    }

    operator utils::span<const std::byte>() const noexcept { return as_span(); }

    auto clear() noexcept { size_ = 0; }

private:
    auto try_push_raw(int level, int type, const void *data, size_type len) noexcept -> bool;

    utils::span<std::byte> buffer_;
    size_type size_{ 0 };
};

class ancillary_buffer
{
public:
    explicit ancillary_buffer(utils::span<std::byte> buffer) noexcept
        : buffer_(detail::align_for_cmsghdr(buffer))
    {
    }

    ancillary_buffer(const ancillary_buffer &) = delete;
    ancillary_buffer &operator=(const ancillary_buffer &) = delete;
    ancillary_buffer(ancillary_buffer &&) noexcept = default;
    ancillary_buffer &operator=(ancillary_buffer &&) noexcept = default;

    ~ancillary_buffer() noexcept = default;

    // Buffer written by the kernel on recvmsg; aligned to cmsghdr.
    [[nodiscard]] auto data() const noexcept -> std::byte * { return buffer_.data(); }

    [[nodiscard]] auto size() const noexcept -> std::size_t { return buffer_.size(); }

private:
    utils::span<std::byte> buffer_;
};

class ancillary_message_view
{
public:
    explicit ancillary_message_view(const struct cmsghdr &cmsg) noexcept
        : cmsg_(&cmsg)
    {
        assert(cmsg_ != nullptr);
        assert(cmsg_->cmsg_len >= CMSG_LEN(0) && "malformed cmsg_len");
    }

    [[nodiscard]] auto level() const noexcept -> int
    {
        return cmsg_ != nullptr ? cmsg_->cmsg_level : 0;
    }

    [[nodiscard]] auto type() const noexcept -> int
    {
        return cmsg_ != nullptr ? cmsg_->cmsg_type : 0;
    }

    // Returns empty span for both legitimate zero-payload cmsg and malformed
    // cmsg_len (< CMSG_LEN(0)).  os layer does not distinguish; callers
    // needing to detect corruption should check `RecvMsg.flags &
    // return_flag::ctrunc` (primary) or inspect `raw_header()->cmsg_len`
    // directly (fallback).
    [[nodiscard]] auto raw_data() const noexcept -> utils::span<const std::byte>;

    template <typename Tag>
    [[nodiscard]] bool is() const noexcept
    {
        return level() == Tag::level && type() == Tag::type;
    }

    template <typename Tag>
    [[nodiscard]] auto get() const noexcept
    {
        if (!is<Tag>()) {
            return decltype(Tag::parse(raw_data())){ };
        }

        return Tag::parse(raw_data());
    }

    [[nodiscard]] const struct cmsghdr *raw_header() const noexcept { return cmsg_; }

    [[nodiscard]] explicit operator bool() const noexcept { return cmsg_ != nullptr; }

private:
    const struct cmsghdr *cmsg_{ nullptr };
};

class ancillary_buffer_view
{
public:
    using value_type = ancillary_message_view;
    using size_type = std::size_t;

    constexpr ancillary_buffer_view() noexcept = default;

    explicit ancillary_buffer_view(utils::span<const std::byte> buffer) noexcept
        : buffer_(buffer)
    {
    }

    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = ancillary_message_view;
        using difference_type = std::ptrdiff_t;
        using pointer = const ancillary_message_view *;
        using reference = ancillary_message_view;

        iterator() noexcept = default;

        iterator(const std::byte *data, std::size_t size) noexcept
        {
            if (data != nullptr && size >= sizeof(struct cmsghdr)) {
                msg_.msg_control = const_cast<void *>(static_cast<const void *>(data));
                msg_.msg_controllen = size;
                current_ = CMSG_FIRSTHDR(&msg_);
            }
        }

        reference operator*() const noexcept { return ancillary_message_view{ *current_ }; }

        iterator &operator++() noexcept
        {
            if (current_ != nullptr) {
                current_ = CMSG_NXTHDR(&msg_, const_cast<struct cmsghdr *>(current_));
            }

            return *this;
        }

        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator &other) const noexcept { return current_ == other.current_; }

        bool operator!=(const iterator &other) const noexcept { return !(*this == other); }

    private:
        struct msghdr msg_{ };
        const struct cmsghdr *current_{ nullptr };
    };

    using const_iterator = iterator;

    [[nodiscard]] auto begin() const noexcept -> const_iterator
    {
        return iterator{ buffer_.data(), buffer_.size() };
    };

    [[nodiscard]] auto end() const noexcept -> const_iterator { return iterator{ }; }

    [[nodiscard]] auto cbegin() const noexcept -> const_iterator { return begin(); }

    [[nodiscard]] auto cend() const noexcept -> const_iterator { return end(); }

    [[nodiscard]] auto empty() const noexcept -> bool { return begin() == end(); }

    [[nodiscard]] auto size_bytes() const noexcept -> size_type { return buffer_.size(); }

    [[nodiscard]] auto as_span() const noexcept -> utils::span<const std::byte> { return buffer_; }

    // Returns the raw file descriptors from SCM_RIGHTS messages, or an empty
    // vector if none were received.  The returned fds are not owned by this
    // view; callers must wrap them (e.g. in file_descriptor) to take ownership.
    //
    // TODO: once file_descriptor is moved into the os layer (see the TODO in
    // utils/file_describer.h), change the return type to
    // std::vector<file_descriptor> so the owning fds are produced directly and
    // callers no longer wrap the raw ints by hand.
    [[nodiscard]] auto rights() const noexcept -> std::vector<int>;

private:
    utils::span<const std::byte> buffer_;
};

// The result of a recvmsg call.  `.control` is a non-owning view pointing into
// the caller-supplied control_buf — the caller must keep that buffer alive
// until done with this result.  `.flags & return_flag::ctrunc` signals that the
// kernel truncated the control buffer (the primary corruption signal);
// recipients should treat this as an error before iterating `.control`.
struct RecvMsg
{
    std::size_t bytes{ 0 };
    sys::return_flag flags{ sys::return_flag::none };
    std::optional<endpoint> address;
    ancillary_buffer_view control;
};

auto send(utils::file_descriptor_ref fd,
          utils::span<const std::byte> buf,
          sys::send_flag flags = sys::send_flag::none) noexcept -> Result<std::size_t>;

// sendmsg/recvmsg deliberately take a single iovec rather than a span of them.
// If that ever changes, add a span<io_slice> overload back rather than changing this one.
auto sendmsg(utils::file_descriptor_ref fd,
             const io_slice &iov,
             ancillary_buffer_writer &control,
             sys::send_flag flags = sys::send_flag::none) noexcept -> Result<std::size_t>;

auto recv(utils::file_descriptor_ref fd,
          utils::span<std::byte> buf,
          sys::recv_flag flags = sys::recv_flag::none) noexcept -> Result<std::size_t>;

auto recvmsg(utils::file_descriptor_ref fd,
             const mutable_io_slice &iov,
             ancillary_buffer &control,
             sys::recv_flag flags = sys::recv_flag::none) noexcept -> Result<RecvMsg>;

auto connect(utils::file_descriptor_ref fd, const endpoint &ep) noexcept -> Result<void>;

// currently we don't need other protocols, so we only provide a 'int' type for protocol, and we
// don't provide a wrapper class for it. If we need to support more protocols in the future, we can
// add a wrapper class for protocol.
auto socket(sys::address_family domain,
            sys::socket_type type,
            sys::socket_flag flag = sys::socket_flag::none,
            int protocol = 0) noexcept -> Result<utils::file_descriptor>;

auto socketpair(sys::address_family domain,
                sys::socket_type type,
                sys::socket_flag flag = sys::socket_flag::none,
                int protocol = 0) noexcept
  -> Result<std::pair<utils::file_descriptor, utils::file_descriptor>>;

} // namespace linyaps_box::os
