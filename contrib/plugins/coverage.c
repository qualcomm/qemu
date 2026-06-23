/*
 * Copyright(c) 2021-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#if 0
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <inttypes.h>
#include <glib.h>
#include <qemu-plugin.h>

#define MAX_CPUS 32
#define TIME_ARRAY_ELT_COUNT 1024
#define BLOCK_ADDR_ELT_COUNT (16*1024*1024)

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;
static GArray *blocks_executed;
static GMutex lock;
static size_t target_ptr_size_bytes = 0;
int cov_fd = -1;

typedef uint32_t TargetAddr;

static void plugin_init(void)
{
    cov_fd = open("test.sancov", O_WRONLY|O_CREAT, S_IRWXU);
    if (cov_fd < 0) {
        perror("file open error: ");
        exit(3);
    }
    static const uint64_t SANCOV_MAGIC = 0xC0BFFFFFFFFFFF32;
    write(cov_fd, &SANCOV_MAGIC, sizeof(SANCOV_MAGIC));

    target_ptr_size_bytes = sizeof(TargetAddr); // FIXME: make this target-independent
    blocks_executed = g_array_sized_new(FALSE, TRUE, target_ptr_size_bytes,
            BLOCK_ADDR_ELT_COUNT);
}


static void vcpu_tb_executed(unsigned int cpu_index, void* udata)
{
    intptr_t vaddr_ =  (intptr_t) (udata);
    TargetAddr vaddr = (uint32_t)  (vaddr_);
    g_array_append_vals(blocks_executed, &vaddr, sizeof(vaddr)); // FIXME: make this target-independent
#if 0
    // TODO: track edges to/from blocks?
    g_mutex_lock(&lock);
    prev_tb_loc = vaddr;
    g_mutex_unlock(&lock);
#endif
}

static void plugin_exit(void *p)
{
    int i;
    int bytes_written;

    fprintf(stderr, "blocks len %d\n", blocks_executed->len);
    for (i = 0; i < blocks_executed->len; i++) {
        TargetAddr exec_address = g_array_index(blocks_executed, TargetAddr, i);
        if (exec_address == 0) continue;
        bytes_written = write(cov_fd, &exec_address, sizeof(exec_address));
        if (bytes_written < 0) {
            perror("file write error: ");
            exit(2);
        }
        //fprintf(stderr, "wrote some %d\n", bytes_written);
    }
    close(cov_fd);
}

static void vcpu_tb_trans(struct qemu_plugin_tb *tb, void *userdata)
{
    uint32_t pc = qemu_plugin_tb_vaddr(tb);
    qemu_plugin_register_vcpu_tb_exec_cb(tb,
            vcpu_tb_executed,
            QEMU_PLUGIN_CB_NO_REGS,
            (gpointer) pc);
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    int i,j;

    plugin_init();

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans, NULL);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    return 0;
}
