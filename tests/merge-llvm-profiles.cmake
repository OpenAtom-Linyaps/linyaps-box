# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Helper script: merge all `llvm-*.profraw` files under PROFILE_DIR into a
# single PROF_OUT profile. Clang's profile runtime does NOT merge across
# processes, so each test process writes its own `%p`-suffixed file; this script
# folds them back into one profile.

file(GLOB _profraw_files "${PROFILE_DIR}/llvm-*.profraw")
list(SORT _profraw_files)

if(NOT _profraw_files)
    message(FATAL_ERROR "No llvm-*.profraw files found in ${PROFILE_DIR}")
endif()

execute_process(
    COMMAND "${LLVM_PROFDATA}" merge -sparse ${_profraw_files} -o "${PROF_OUT}"
    RESULT_VARIABLE _merge_result
    OUTPUT_VARIABLE _merge_stdout
    ERROR_VARIABLE _merge_stderr
)

if(NOT _merge_result EQUAL 0)
    message(
        FATAL_ERROR
        "llvm-profdata merge failed (${_merge_result}):\n${_merge_stderr}"
    )
endif()

file(REMOVE ${_profraw_files})
