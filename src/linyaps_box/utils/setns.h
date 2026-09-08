// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/oci_config.h"
#include "linyaps_box/utils/file_describer.h"

#include <sys/types.h>

namespace linyaps_box::utils {

auto open_namespace_fd(pid_t target_pid, config::ns::type ns_type) -> file_descriptor;

auto setns(const file_descriptor &ns_fd, config::ns::type ns_type) -> void;

auto join_namespace(const file_descriptor &ns_fd, config::ns::type ns_type) -> void;

auto join_container_namespaces(pid_t target_pid, const config::linux &linux_config) -> void;

// Verifies, via /proc/self/fd/<N> readlink ("type:[id]" prefix match), that
// `fd` refers to a namespace of the expected type.  The check is pinned to the
// very descriptor that will be passed to setns(2) — no check/use gap.
auto verify_namespace_fd(const file_descriptor &fd, config::ns::type type) -> void;

// Joins every namespace entry that carries a `path` (runc/crun/youki
// semantics: a namespace with a path is joined, not created).  All descriptors
// are opened and type-verified first (TOCTOU-safe), then setns is performed
// with the USER namespace joined first.
auto join_namespaces_with_path(const std::vector<config::ns> &namespaces) -> void;

} // namespace linyaps_box::utils
