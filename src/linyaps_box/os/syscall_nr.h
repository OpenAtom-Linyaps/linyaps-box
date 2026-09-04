// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <sys/syscall.h> // for the toolchain header's __NR_* definitions

#include <cstdint>

namespace linyaps_box::os {

// Target architectures.
enum class arch : uint8_t {
    x86_64,
    aarch64,
    riscv64,
    loong64,
    mips64, // n64 ABI
    sw64,
};

#ifdef __x86_64__
inline constexpr auto target_arch = arch::x86_64;
#elif defined(__aarch64__)
inline constexpr auto target_arch = arch::aarch64;
#elif defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 64
inline constexpr auto target_arch = arch::riscv64;
#elif defined(__loongarch64)
inline constexpr auto target_arch = arch::loong64;
#elif defined(__mips__) && defined(__LP64__) // mips64 n64 (n32 is ILP32)
inline constexpr auto target_arch = arch::mips64;
#elif defined(__sw_64__)
inline constexpr auto target_arch = arch::sw64;
#else
#  error "unsupported target architecture: add a row to os/syscall_nr.h"
#endif

// Fallback syscall numbers for syscalls added after Linux 4.19
template <arch A>
struct syscall_nr;

template <>
struct syscall_nr<arch::x86_64>
{
    static constexpr int pidfd_send_signal = 424;
    static constexpr int pidfd_open = 434;
    static constexpr int openat2 = 437;
    static constexpr int close_range = 436;
};

template <>
struct syscall_nr<arch::aarch64>
{
    static constexpr int pidfd_send_signal = 424;
    static constexpr int pidfd_open = 434;
    static constexpr int openat2 = 437;
    static constexpr int close_range = 436;
};

template <>
struct syscall_nr<arch::riscv64>
{
    static constexpr int pidfd_send_signal = 424;
    static constexpr int pidfd_open = 434;
    static constexpr int openat2 = 437;
    static constexpr int close_range = 436;
};

template <>
struct syscall_nr<arch::loong64>
{
    static constexpr int pidfd_send_signal = 424;
    static constexpr int pidfd_open = 434;
    static constexpr int openat2 = 437;
    static constexpr int close_range = 436;
};

template <>
struct syscall_nr<arch::mips64>
{
    static constexpr int pidfd_send_signal = 424;
    static constexpr int pidfd_open = 434;
    static constexpr int openat2 = 437;
    static constexpr int close_range = 436;
};

template <>
struct syscall_nr<arch::sw64>
{
    static constexpr int pidfd_send_signal = 271;
    static constexpr int pidfd_open = 281;
    static constexpr int openat2 = 284;
    static constexpr int close_range = 283;
};

inline constexpr int nr_pidfd_send_signal =
#ifdef __NR_pidfd_send_signal
  __NR_pidfd_send_signal
#else
  syscall_nr<target_arch>::pidfd_send_signal
#endif
  ;

inline constexpr int nr_pidfd_open =
#ifdef __NR_pidfd_open
  __NR_pidfd_open
#else
  syscall_nr<target_arch>::pidfd_open
#endif
  ;

inline constexpr int nr_openat2 =
#if defined(__NR_openat2)
  __NR_openat2
#else
  syscall_nr<target_arch>::openat2
#endif
  ;

inline constexpr int nr_close_range =
#if defined(__NR_close_range)
  __NR_close_range
#else
  syscall_nr<target_arch>::close_range
#endif
  ;

#ifdef __NR_pidfd_send_signal
static_assert(syscall_nr<target_arch>::pidfd_send_signal == __NR_pidfd_send_signal,
              "syscall_nr fallback table out of date");
#endif

#ifdef __NR_pidfd_open
static_assert(syscall_nr<target_arch>::pidfd_open == __NR_pidfd_open,
              "syscall_nr fallback table out of date");
#endif

#ifdef __NR_openat2
static_assert(syscall_nr<target_arch>::openat2 == __NR_openat2,
              "syscall_nr fallback table out of date");
#endif

#ifdef __NR_close_range
static_assert(syscall_nr<target_arch>::close_range == __NR_close_range,
              "syscall_nr fallback table out of date");
#endif

} // namespace linyaps_box::os
