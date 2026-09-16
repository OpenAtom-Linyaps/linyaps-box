#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# tools/analyzer.sh
# Run the Clang Static Analyzer over the project.
#
# Usage:
#   ./tools/analyzer.sh
#
# Configures with the "analyzer" preset, then replays the compilation database
# it exports through analyze-build. The HTML report is written under
# build-analyzer/scan-build-report/. Exits non-zero when a bug is found or when
# the analysis itself fails.
#
# Set ANALYZE_BUILD to use a specific analyze-build binary.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PRESET="analyzer"
CDB="${ROOT_DIR}/build-analyzer/compile_commands.json"
REPORT_DIR="${ROOT_DIR}/build-analyzer/scan-build-report"

die() {
  echo "Error: $*" >&2
  exit 1
}

resolve_tool() {
  local tool="$1" dir candidate

  if command -v "${tool}" > /dev/null 2>&1; then
    command -v "${tool}"
    return 0
  fi

  local IFS=:
  for dir in ${PATH}; do
    for candidate in "${dir}/${tool}" "${dir}/${tool}"-*; do
      if [[ -f "${candidate}" && -x "${candidate}" ]]; then
        printf '%s\n' "${candidate}"
        return 0
      fi
    done
  done

  return 1
}

if [[ -z "${ANALYZE_BUILD:-}" ]]; then
  ANALYZE_BUILD="$(resolve_tool analyze-build)" ||
    die "analyze-build not found. Please install clang-tools."
fi

for tool in cmake clang++; do
  if ! command -v "${tool}" > /dev/null 2>&1; then
    die "${tool} not found. Please install cmake and clang."
  fi
done

if [[ ! -f "${ROOT_DIR}/CMakeLists.txt" ]]; then
  die "${ROOT_DIR} does not look like the project root."
fi

cd "${ROOT_DIR}"

echo "Configuring (preset: ${PRESET})..."
cmake --preset="${PRESET}"

if [[ ! -s "${CDB}" ]]; then
  die "compilation database ${CDB} is missing or empty; nothing to analyze."
fi

rm -rf "${REPORT_DIR}"

echo "Running the Clang Static Analyzer (${ANALYZE_BUILD})..."
status=0
"${ANALYZE_BUILD}" --cdb "${CDB}" --status-bugs -o "${REPORT_DIR}" ||
  status=$?

report="$(find "${REPORT_DIR}" -maxdepth 2 -name index.html 2> /dev/null | head -n 1 || true)"
if [[ -n "${report}" ]]; then
  echo
  echo "Report: ${report}"
  echo "Clang Static Analyzer reported issues (exit ${status})." >&2
  exit "${status}"
fi

if [[ "${status}" -ne 0 ]]; then
  die "analysis failed (exit ${status}) without producing a report."
fi

echo
echo "No findings."
