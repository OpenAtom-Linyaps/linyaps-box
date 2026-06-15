// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config.h"
#include "linyaps_box/utils/file_describer.h"

#include <sys/types.h>

namespace linyaps_box::utils {

auto open_namespace_fd(pid_t target_pid, oci_config::linux_t::namespace_t::type ns_type)
  -> file_descriptor;

auto setns(const file_descriptor &ns_fd, oci_config::linux_t::namespace_t::type ns_type) -> void;

auto join_namespace(const file_descriptor &ns_fd, oci_config::linux_t::namespace_t::type ns_type)
  -> void;

auto join_container_namespaces(pid_t target_pid, const oci_config::linux_t &linux_config) -> void;

} // namespace linyaps_box::utils
