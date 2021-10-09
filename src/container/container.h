/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>

#include "util/oci-runtime.h"

namespace linglong {

struct Option;

class Container
{
public:
    explicit Container(const Runtime &r);

    ~Container();

    int start(const Option &opt);

    struct ContainerPrivate;

private:
    std::unique_ptr<ContainerPrivate> dd_ptr;
};

} // namespace linglong