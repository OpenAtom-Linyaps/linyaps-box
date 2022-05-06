/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_CONTAINER_CONTAINER_H_
#define LINGLONG_BOX_SRC_CONTAINER_CONTAINER_H_

#include <memory>

#include "util/oci_runtime.h"
#include "util/message_reader.h"

namespace linglong {

struct Option;
struct ContainerPrivate;

class Container
{
public:
    explicit Container(const Runtime &r, std::unique_ptr<util::MessageReader> reader);

    ~Container();

    int Start(const Option &opt);

private:
    std::unique_ptr<ContainerPrivate> dd_ptr;
};

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_CONTAINER_CONTAINER_H_ */
