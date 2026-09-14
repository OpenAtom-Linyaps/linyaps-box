include(CheckCXXSourceCompiles)

# Detect target 64-bit architecture and return result via output variable.
#
# out_var: The variable name in caller scope to store the result string.
function(detect_architecture out_var)

  set(CMAKE_REQUIRED_QUIET ON)

  # sw64
  # Private ISA
  check_cxx_source_compiles(
    "
#if !defined(__sw_64__) && !defined(__sw_64)
#  error not sw64
#endif
int main() { return 0; }"
    _ARCH_IS_SW64)

  if(_ARCH_IS_SW64)
    set(${out_var} "SW64" PARENT_SCOPE)
    return()
  endif()

  # x86_64
  # __x86_64__ / __x86_64 / __amd64__
  # GCC/Clang predefine all three on x86-64
  check_cxx_source_compiles(
    "
  #if !defined(__x86_64__) && !defined(__x86_64) && !defined(__amd64__)
  #  error not x86_64
  #endif
  int main() { return 0; }"
    _ARCH_IS_X86_64)

  if (_ARCH_IS_X86_64)
    set(${out_var} "X86_64" PARENT_SCOPE)
    return()
  endif()

  # aarch64
  # __aarch64__ / __arm64__
  # GCC/Clang predefine __aarch64__; __arm64__ covers Apple/Solaris-style
  check_cxx_source_compiles(
    "
#if !defined(__aarch64__)
#  error not aarch64
#endif
int main() { return 0; }"
    _ARCH_IS_AARCH64)

  if (_ARCH_IS_AARCH64)
    set(${out_var} "AARCH64" PARENT_SCOPE)
    return()
  endif()

  # mips n64
  # https://github.com/torvalds/linux/blob/704340f1cd0dcef829eb62f5b48ae95a2ce17bdf/arch/mips/include/uapi/asm/unistd.h
  check_cxx_source_compiles(
    "
#if !defined(__mips__) || !defined(_MIPS_SIM) || !defined(_ABI64)
#  error not mips64 n64
#endif
#if _MIPS_SIM != _ABI64
#  error not mips64 n64
#endif
int main() { return 0; }"
    _ARCH_IS_MIPS64)

  if (_ARCH_IS_MIPS64)
    set(${out_var} "MIPS64" PARENT_SCOPE)
    return()
  endif()

  # riscv64
  # https://github.com/riscv-non-isa/riscv-c-api-doc/blob/b70a8eab9db1cb97873ab274c50fb02b89f1b312/src/c-api.adoc
  check_cxx_source_compiles(
    "
#if !defined(__riscv) || !defined(__riscv_xlen) || __riscv_xlen != 64
#  error not riscv64
#endif
int main() { return 0; }"
    _ARCH_IS_RISCV64)

  if (_ARCH_IS_RISCV64)
    set(${out_var} "RISCV64" PARENT_SCOPE)
    return()
  endif()

  # loong64
  # from gcc:
  # https://github.com/gcc-mirror/gcc/blob/b76fde4b175ff5f8e8ed8affede662bacb5775a4/gcc/config/loongarch/loongarch-c.cc
  # and also llvm:
  # https://github.com/llvm/llvm-project/blob/a92db5feb9eb78ae7bf37d969391d9f94f181afb/clang/lib/Basic/Targets/LoongArch.cpp
  # official documentation:
  # https://github.com/loongson/LoongArch-Documentation/blob/e0d6592229d9e00e512bd28b688a7f20b171714f/docs/LoongArch-toolchain-conventions-EN.adoc
  check_cxx_source_compiles(
    "
#if !defined(__loongarch__) || !defined(__loongarch_lp64)
#  error not loong64
#endif
int main() { return 0; }"
    _ARCH_IS_LOONG64)

  if (_ARCH_IS_LOONG64)
    set(${out_var} "LOONG64" PARENT_SCOPE)
    return()
  endif()

  message(FATAL_ERROR "Unsupported 64-bit target architecture")
endfunction()
