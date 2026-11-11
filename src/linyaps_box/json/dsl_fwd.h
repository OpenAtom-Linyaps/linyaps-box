// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

// Model headers only declare their ADL registration:
//   auto ops_of(json::object_tag<Model>) -> const json::frame_ops &;

namespace linyaps_box::json {

struct frame_ops;

template <typename T>
struct object_tag;

} // namespace linyaps_box::json
