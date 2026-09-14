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
    loong64, // new world
    mips64,  // n64 ABI
    sw64,
};

#ifdef LINYAPS_BOX_TARGET_ARCH_SW64
constexpr auto target_arch = arch::sw64;
#elif defined(LINYAPS_BOX_TARGET_ARCH_X86_64)
constexpr auto target_arch = arch::x86_64;
#elif defined(LINYAPS_BOX_TARGET_ARCH_AARCH64)
constexpr auto target_arch = arch::aarch64;
#elif defined(LINYAPS_BOX_TARGET_ARCH_RISCV64)
constexpr auto target_arch = arch::riscv64;
#elif defined(LINYAPS_BOX_TARGET_ARCH_LOONG64)
constexpr auto target_arch = arch::loong64;
#elif defined(LINYAPS_BOX_TARGET_ARCH_MIPS64)
constexpr auto target_arch = arch::mips64;
#else
#  error "unsupported target architecture"
#endif

// Fallback syscall numbers for syscalls added after Linux 4.19
template <arch A>
struct syscall_nr;

// From kernel syscall table:
//    https://github.com/torvalds/linux/blob/704340f1cd0dcef829eb62f5b48ae95a2ce17bdf/arch/x86/entry/syscalls/syscall_64.tbl
template <>
struct syscall_nr<arch::x86_64>
{
    static constexpr int pidfd_send_signal = 424;
    static constexpr int pidfd_open = 434;
    static constexpr int openat2 = 437;
    static constexpr int close_range = 436;
};

// AARCH64 use the asm-generic syscall:
//    https://github.com/torvalds/linux/blob/704340f1cd0dcef829eb62f5b48ae95a2ce17bdf/arch/arm64/include/uapi/asm/unistd.h
template <>
struct syscall_nr<arch::aarch64>
{
    static constexpr int pidfd_send_signal = 424;
    static constexpr int pidfd_open = 434;
    static constexpr int openat2 = 437;
    static constexpr int close_range = 436;
};

// RISCV64 use the asm-generic syscall:
//    https://github.com/torvalds/linux/blob/704340f1cd0dcef829eb62f5b48ae95a2ce17bdf/arch/riscv/include/uapi/asm/unistd.h#L20
template <>
struct syscall_nr<arch::riscv64>
{
    static constexpr int pidfd_send_signal = 424;
    static constexpr int pidfd_open = 434;
    static constexpr int openat2 = 437;
    static constexpr int close_range = 436;
};

// LOONG64 use the asm-generic syscall:
//   https://github.com/torvalds/linux/blob/704340f1cd0dcef829eb62f5b48ae95a2ce17bdf/arch/loongarch/include/uapi/asm/unistd.h
template <>
struct syscall_nr<arch::loong64>
{
    static constexpr int pidfd_send_signal = 424;
    static constexpr int pidfd_open = 434;
    static constexpr int openat2 = 437;
    static constexpr int close_range = 436;
};

// MIPS n64 numbers = __NR_Linux(5000) + kernel-table index.
// Index source:
//    https://github.com/torvalds/linux/blob/08df884136f1c1197bab2a27814404fd329d9aac/arch/mips/kernel/syscalls/syscall_n64.tbl
// Base (__NR_Linux 5000):
//    https://github.com/torvalds/linux/blob/704340f1cd0dcef829eb62f5b48ae95a2ce17bdf/arch/mips/include/uapi/asm/unistd.h
// Add offset:
//    https://github.com/torvalds/linux/blob/704340f1cd0dcef829eb62f5b48ae95a2ce17bdf/arch/mips/kernel/syscalls/Makefile#L12
template <>
struct syscall_nr<arch::mips64>
{
    static constexpr int pidfd_send_signal = 5424;
    static constexpr int pidfd_open = 5434;
    static constexpr int openat2 = 5437;
    static constexpr int close_range = 5436;
};

// SW64: NO upstream table exists (not in makernel, GCC, glibc, musl, LLVM, Go, or Rust).
// Just refer to:
//    https://github.com/orangeji11/sys/blob/5c2a576e6c78f1c938fa05b5f295b835603f39cd/unix/zsysnum_linux_sw64.go
template <>
struct syscall_nr<arch::sw64>
{
    static constexpr int pidfd_send_signal = 271;
    static constexpr int pidfd_open = 281;
    static constexpr int openat2 = 284;
    static constexpr int close_range = 283;
};

constexpr int nr_pidfd_send_signal =
#ifdef __NR_pidfd_send_signal
  __NR_pidfd_send_signal
#else
  syscall_nr<target_arch>::pidfd_send_signal
#endif
  ;

constexpr int nr_pidfd_open =
#ifdef __NR_pidfd_open
  __NR_pidfd_open
#else
  syscall_nr<target_arch>::pidfd_open
#endif
  ;

constexpr int nr_openat2 =
#ifdef __NR_openat2
  __NR_openat2
#else
  syscall_nr<target_arch>::openat2
#endif
  ;

constexpr int nr_close_range =
#ifdef __NR_close_range
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
