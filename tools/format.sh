#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# tools/format.sh
# Format all C/C++ source files in the project using clang-format.
#
# Usage:
#   ./tools/format.sh                 # Use system default clang-format
#   ./tools/format.sh clang-format-17 # Use a specific clang-format binary
#
# The script automatically skips common build and third-party directories.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLANG_FORMAT="${1:-clang-format}"

# Check clang-format availability
if ! command -v "${CLANG_FORMAT}" >/dev/null 2>&1; then
    echo "Error: ${CLANG_FORMAT} not found. Please install clang-format." >&2
    exit 1
fi

# --- Project root validation ---
# Criteria for identifying a valid project root:
#   1. Has CMakeLists.txt, or
#   2. Has .clang-format, or
#   3. Contains app/, src/, or tests/ directories

VALID_ROOT=false
if [[ -f "${ROOT_DIR}/CMakeLists.txt" ]]; then
    VALID_ROOT=true
elif [[ -f "${ROOT_DIR}/.clang-format" ]]; then
    VALID_ROOT=true
else
    for d in app src tests; do
        if [ -d "${ROOT_DIR}/${d}" ]; then
            VALID_ROOT=true
            break
        fi
    done
fi

if [[ ${VALID_ROOT} != true ]]; then
    echo "Error: This script must be run inside a valid C++ project root."
    echo "Expected to find one of the following in ${ROOT_DIR}:"
    echo "  - CMakeLists.txt"
    echo "  - .clang-format or _clang-format"
    echo "  - app/, src/, or tests/ directories"
    exit 1
fi
# --- End project root validation ---

CLANG_FORMAT_VERSION=$("${CLANG_FORMAT}" --version | head -n 1 | cut -d ' ' -f 3)
echo "Using clang-format: ${CLANG_FORMAT_VERSION}"
echo "Project root: ${ROOT_DIR}"

# Only search for .cpp and .h files under app, src, and tests directories
dirs=()
for d in app src tests; do
    if [[ -d "${ROOT_DIR}/${d}" ]]; then
        dirs+=("${ROOT_DIR}/${d}")
    fi
done

if [[ ${#dirs[@]} -eq 0 ]]; then
    echo "Warning: No app/, src/, or tests/ directories found under ${ROOT_DIR}."
    exit 0
fi

find "${dirs[@]}" \
    \( -name "*.cpp" -o -name "*.h" \) \
    -type f -print |
    while read -r file; do
        echo "Formatting: ${file}"
        "${CLANG_FORMAT}" -i "${file}"
    done

echo "Formatting completed."
