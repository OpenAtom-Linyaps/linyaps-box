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

namespace linglong {

class Semaphore
{
public:
    explicit Semaphore(int key);
    ~Semaphore();

    int init();

    //! passeren -1 to value
    //! if value < 0, block
    //! \return
    int passeren();

    //! passeren +1 to value
    //! if value <= 0, release process in queue
    //! \return
    int vrijgeven();

private:
    struct SemaphorePrivate;
    std::unique_ptr<SemaphorePrivate> dd_ptr;
};

} // namespace linglong
