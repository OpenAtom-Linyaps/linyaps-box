// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/impl/json_printer.h"

#include "nlohmann/json.hpp"

#include <iostream>

void linyaps_box::impl::json_printer::print_statuses(const std::vector<container_status_t> &status)
{
    auto j = nlohmann::json::array();
    for (const auto &s : status) {
        j += status_to_json(s);
    }

    std::cout << j.dump(4) << std::endl;
}

void linyaps_box::impl::json_printer::print_status(const container_status_t &status)
{
    auto j = status_to_json(status);
    std::cout << j.dump(4) << std::endl;
}
