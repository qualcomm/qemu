/*
 * libqemu
 *
 * Copyright (c) 2019 Luc Michel <luc.michel@greensocs.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "libqemu/run-on-iothread.h"
#include "qemu/main-loop.h"

typedef struct IOThreadWorkerParams IOThreadWorkerParams;

struct IOThreadWorkerParams {
    void (*worker)(void *);
    void *params;
    bool done;
};


static void iothread_worker(void *opaque)
{
    IOThreadWorkerParams *params;

    params = (IOThreadWorkerParams *) opaque;

    params->worker(params->params);

    qatomic_set(&params->done, true);
}

void run_on_iothread(void (*worker)(void *), void *opaque)
{
    IOThreadWorkerParams params = {
        .worker = worker,
        .params = opaque,
        .done = false,
    };

    assert(!qemu_mutex_iothread_locked());

    QEMUBH *bh = qemu_bh_new(iothread_worker, &params);
    qemu_bh_schedule(bh);

    while(!qatomic_read(&params.done)) {
        cpu_relax();
    }

    qemu_bh_delete(bh);
}
