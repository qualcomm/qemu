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
#include "qemu/main-loop.h"
#include "qemu/rcu.h"
#include "qemu/thread.h"
#include "sysemu/sysemu.h"
#include "tcg/tcg.h"

#include "callbacks.h"
#include "ctors.h"
#include "fill.h"
#include "libqemu.h"
#include "wrappers/libqemu.h"

/* The QEMU main function */
int main(int argc, const char *const argv[], char **envp);

typedef struct LibQemuContext LibQemuContext;

struct LibQemuContext {
    LibQemuExports exports;
    QemuThread iothread;
    GMainContext *iothread_context;
    int argc;
    char **argv;

    struct {
        LibQemuCpuEndOfLoopFn cb;
        void *opaque;
    } cpu_end_of_loop_cb;

    struct {
        LibQemuCpuKickFn cb;
        void *opaque;
    } cpu_kick_cb;
};

/* Since QEMU has a implicit state, there is no use in returning an explicit
 * one to the user. Calling the init function multiple times is not allowed
 * anyway. So we keep a global context here.
 */
static LibQemuContext context;

/* This is the entry function for the thread which call QEMU constructors and
 * main function. The main function calls the main loop so this thread becomes
 * the iothread.
 */
static void *iothread_entry(void *arg)
{
    LibQemuContext *context = (LibQemuContext *)arg;

    g_main_context_push_thread_default(context->iothread_context);

    libqemu_call_ctors();

    qemu_init(context->argc, context->argv);
    qemu_main_loop();
    qemu_cleanup();

    g_main_context_pop_thread_default(context->iothread_context);

    return NULL;
}


/* v5.1.0:
 * Wait for the main function to ends initialization and call the main loop.
 * This is tricky to do without being intrusive. The trick used here could
 * change from version to version.
 * We wait for the qmp_dispatcher_bh global variable in monitor.c to be
 * initialized. Once it is, we know for sure the BQL has been created and is
 * currently hold by the iothread, or else the iothread has finished
 * initialization.
 *
 * v5.2.0:
 * Replace qmp_dispatcher_bh with qmp_dispatcher_co since qmp_dispatcher_bh no
 * longer exists in v5.2.0
 */

/* monitor.c */
extern Coroutine *qmp_dispatcher_co;

static void wait_for_iothread_startup(void)
{
    while (qatomic_read(&qmp_dispatcher_co) == NULL) {
        cpu_relax();
    }

    /* From here, we can lock the iothread mutex */
    qemu_mutex_lock_iothread();

    /* Once acquired, we know for sure initialization is done */
    qemu_mutex_unlock_iothread();
}

static void start_iothread(int argc, char **argv)
{
    context.argc = argc;
    context.argv = argv;
    context.iothread_context = g_main_context_new();

    qemu_thread_create(&context.iothread, "qemu-iothread", iothread_entry,
                       &context, QEMU_THREAD_JOINABLE);
    wait_for_iothread_startup();
}

LibQemuExports *LIBQEMU_INIT_SYM(int argc, char **argv)
{
    libqemu_exports_fill(&context.exports);
    start_iothread(argc, argv);

    return &context.exports;
}

void libqemu_set_cpu_end_of_loop_cb(LibQemuCpuEndOfLoopFn cb, void *opaque)
{
    context.cpu_end_of_loop_cb.cb = cb;
    context.cpu_end_of_loop_cb.opaque = opaque;
}

void libqemu_cpu_end_of_loop_cb(CPUState *cpu)
{
    LibQemuCpuEndOfLoopFn cb = context.cpu_end_of_loop_cb.cb;
    void *opaque = context.cpu_end_of_loop_cb.opaque;

    if (cb) {
        cb((QemuObject *)cpu, opaque);
    }
}

void libqemu_set_cpu_kick_cb(LibQemuCpuKickFn cb, void *opaque)
{
    context.cpu_kick_cb.cb = cb;
    context.cpu_kick_cb.opaque = opaque;
}

void libqemu_cpu_kick_cb(CPUState *cpu)
{
    LibQemuCpuKickFn cb = context.cpu_kick_cb.cb;
    void *opaque = context.cpu_kick_cb.opaque;

    if (cb) {
        cb((QemuObject *)cpu, opaque);
    }
}

void libqemu_enable_opengl()
{
    display_opengl = true;
}
