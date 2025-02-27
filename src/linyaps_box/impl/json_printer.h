#pragma once

#include "linyaps_box/container_status.h"
#include "linyaps_box/printer.h"

namespace linyaps_box::impl {

class json_printer : public virtual linyaps_box::printer
{
public:
    void print_status(const container_status_t &status);
    void print_statuses(const std::vector<container_status_t> &status);
};

static_assert(!std::is_abstract_v<json_printer>);

} // namespace linyaps_box::impl
