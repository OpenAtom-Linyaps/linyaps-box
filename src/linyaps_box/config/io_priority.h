// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>

namespace linyaps_box::config {

struct io_priority
{
    enum class class_t : uint8_t { rt, best_effort, idle };

    class_t class_;
    int priority;
};

LINYAPS_REGISTER_ENUM_TABLE(io_priority::class_t,
                            3,
                            { io_priority::class_t::rt, "IOPRIO_CLASS_RT" },
                            { io_priority::class_t::best_effort, "IOPRIO_CLASS_BE" },
                            { io_priority::class_t::idle, "IOPRIO_CLASS_IDLE" })

void from_json(const nlohmann::json &j, io_priority &v);

void validate(const io_priority &v);

} // namespace linyaps_box::config
