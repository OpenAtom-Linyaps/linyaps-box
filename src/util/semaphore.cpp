/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "semaphore.h"

#include <sys/sem.h>

#include "util/logger.h"

namespace linglong {

union semun {
    int val;
    struct semid_ds *buf;
    ushort *array;
};

struct Semaphore::SemaphorePrivate {
    explicit SemaphorePrivate(Semaphore *parent = nullptr)
        : semaphore(parent)
    {
        (void)semaphore;
    }

    struct sembuf semLock = {0, -1, SEM_UNDO};

    struct sembuf semUnlock = {0, 1, SEM_UNDO};

    int semId = -1;

    Semaphore *semaphore;
};

Semaphore::Semaphore(int key)
    : semaphorePrivate(new SemaphorePrivate(this))
{
    semaphorePrivate->semId = semget(key, 1, IPC_CREAT | 0666);
    if (semaphorePrivate->semId < 0) {
        logErr() << "semget failed" << util::retErrString(semaphorePrivate->semId);
    }
}

Semaphore::~Semaphore() = default;

int Semaphore::init()
{
    union semun semUnion = {0};
    semUnion.val = 0;
    logDbg() << "semctl " << semaphorePrivate->semId;
    if (semctl(semaphorePrivate->semId, 0, SETVAL, semUnion) == -1) {
        logErr() << "semctl failed" << util::retErrString(-1);
    }
    return 0;
}

int Semaphore::minusOne()
{
    return semop(semaphorePrivate->semId, &semaphorePrivate->semLock, 1);
}

int Semaphore::plusOne()
{
    return semop(semaphorePrivate->semId, &semaphorePrivate->semUnlock, 1);
}

} // namespace linglong
