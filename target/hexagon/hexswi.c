/*
 * Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "cpu_helper.h"
#include "exec/helper-proto.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"
#include "arch.h"
#include "internal.h"
#include "macros.h"
#include "sys_macros.h"
#include "tcg/tcg-op.h"
#include "hex_mmu.h"
#include "hexswi.h"

#ifdef CONFIG_USER_ONLY
#error "This file is only used in system emulation"
#endif

#include "semihosting/common-semi.h"
#include "semihosting/syscalls.h"
#include "semihosting/guestfd.h"

/* non-arm-compatible semihosting calls */
#define HEXAGON_SPECIFIC_SWI_FLAGS \
    DEF_SWI_FLAG(OPEN,             0x01) \
    DEF_SWI_FLAG(WRITEC,           0x03) \
    DEF_SWI_FLAG(WRITE0,           0x04) \
    DEF_SWI_FLAG(ISTTY,            0x09) \
    DEF_SWI_FLAG(HEAPINFO,         0x16) \
    DEF_SWI_FLAG(EXCEPTION,        0x18) \
    DEF_SWI_FLAG(SEEK,             0x0A) \
    DEF_SWI_FLAG(READ_CYCLES,      0x40) \
    DEF_SWI_FLAG(PROF_ON,          0x41) \
    DEF_SWI_FLAG(PROF_OFF,         0x42) \
    DEF_SWI_FLAG(WRITECREG,        0x43) \
    DEF_SWI_FLAG(READ_TCYCLES,     0x44) \
    DEF_SWI_FLAG(LOG_EVENT,        0x45) \
    DEF_SWI_FLAG(REDRAW,           0x46) \
    DEF_SWI_FLAG(READ_ICOUNT,      0x47) \
    DEF_SWI_FLAG(PROF_STATSRESET,  0x48) \
    DEF_SWI_FLAG(DUMP_PMU_STATS,   0x4a) \
    DEF_SWI_FLAG(READ_PCYCLES,     0x52) \
    DEF_SWI_FLAG(COREDUMP,         0xCD) \
    DEF_SWI_FLAG(FTELL,            0x100) \
    DEF_SWI_FLAG(FSTAT,            0x101) \
    DEF_SWI_FLAG(STAT,             0x103) \
    DEF_SWI_FLAG(GETCWD,           0x104) \
    DEF_SWI_FLAG(ACCESS,           0x105) \
    DEF_SWI_FLAG(OPENDIR,          0x180) \
    DEF_SWI_FLAG(CLOSEDIR,         0x181) \
    DEF_SWI_FLAG(READDIR,          0x182) \
    DEF_SWI_FLAG(EXEC,             0x185) \
    DEF_SWI_FLAG(FTRUNC,           0x186)

/*
 * We use the arm-compatible semihosting routines for these ones, but we do
 * need some hexagon-specific preprocessing.
 */
#define HEX_SYS_WRITE       0x05
#define HEX_SYS_READ        0x06
#define HEX_SYS_READC       0x07

#define DEF_SWI_FLAG(name, val) HEX_SYS_ ##name = val,
enum hex_swi_flag {
    HEXAGON_SPECIFIC_SWI_FLAGS
};
#undef DEF_SWI_FLAG

#define DEF_SWI_FLAG(_, val) case val:
static inline bool is_hexagon_specific_swi_flag(enum hex_swi_flag what_swi)
{
    switch (what_swi) {
    HEXAGON_SPECIFIC_SWI_FLAGS
        return true;
    }
    return false;
}
#undef DEF_SWI_FLAG

static void do_preload(CPUHexagonState *env, target_ulong swi_info, bool load)
{
    uint32_t addr, count;
    uintptr_t retaddr = 0;
    hexagon_read_memory(env, swi_info + 4, 4, &addr, retaddr);
    hexagon_read_memory(env, swi_info + 8, 4, &count, retaddr);
    hexagon_touch_memory(env, addr, count, load ? MMU_DATA_LOAD : MMU_DATA_STORE);
}

/* We start from 1 as 0 is used to signal an error from opendir() */
static const int DIR_INDEX_OFFSET = 1;

/* Hexagon semihosting errno values */
enum HexagonErrno {
    HEX_EPERM           = 1,
    HEX_ENOENT          = 2,
    HEX_EINTR           = 4,
    HEX_EIO             = 5,
    HEX_ENXIO           = 6,
    HEX_EBADF           = 9,
    HEX_EAGAIN          = 11,
    HEX_ENOMEM          = 12,
    HEX_EACCES          = 13,
    HEX_EFAULT          = 14,
    HEX_EBUSY           = 16,
    HEX_EEXIST          = 17,
    HEX_EXDEV           = 18,
    HEX_ENODEV          = 19,
    HEX_ENOTDIR         = 20,
    HEX_EISDIR          = 21,
    HEX_EINVAL          = 22,
    HEX_ENFILE          = 23,
    HEX_EMFILE          = 24,
    HEX_ENOTTY          = 25,
    HEX_ETXTBSY         = 26,
    HEX_EFBIG           = 27,
    HEX_ENOSPC          = 28,
    HEX_ESPIPE          = 29,
    HEX_EROFS           = 30,
    HEX_EMLINK          = 31,
    HEX_EPIPE           = 32,
    HEX_ERANGE          = 34,
    HEX_ENAMETOOLONG    = 36,
    HEX_ENOSYS          = 38,
    HEX_ELOOP           = 40,
    HEX_EOVERFLOW       = 75,
};

/* Map host errno to hexagon semihosting errno */
static void semi_cb(CPUState *cs, uint64_t ret, int err)
{
#define E(N) case E##N: err = HEX_E##N; break

    switch (err) {
    case 0:
        break;
    E(PERM);
    E(NOENT);
    E(INTR);
    E(IO);
    E(NXIO);
    E(BADF);
    E(AGAIN);
    E(NOMEM);
    E(ACCES);
    E(FAULT);
    E(BUSY);
    E(EXIST);
    E(XDEV);
    E(NODEV);
    E(NOTDIR);
    E(ISDIR);
    E(INVAL);
    E(NFILE);
    E(MFILE);
    E(NOTTY);
    E(TXTBSY);
    E(FBIG);
    E(NOSPC);
    E(SPIPE);
    E(ROFS);
    E(MLINK);
    E(PIPE);
    E(RANGE);
    E(NAMETOOLONG);
    E(NOSYS);
    E(LOOP);
    E(OVERFLOW);
    default:
        err = HEX_EINVAL;
        break;
    }

#undef E

    common_semi_cb(cs, ret, err);
}

static void common_semi_ftell_cb(CPUState *cs, uint64_t ret, int err)
{
    if (err) {
        ret = -1;
    }
    semi_cb(cs, ret, err);
}

static void coredump(CPUHexagonState *env)
{
    uint32_t ssr = arch_get_system_reg(env, HEX_SREG_SSR);
    printf("CRASH!\n");
    printf("I think the exception was: ");
    switch (GET_SSR_FIELD(SSR_CAUSE, ssr)) {
    case 0x43:
        printf("0x43, NMI");
        break;
    case 0x42:
        printf("0x42, Data abort");
        break;
    case 0x44:
        printf("0x44, Multi TLB match");
        break;
    case HEX_CAUSE_BIU_PRECISE:
        printf("0x%x, Bus Error (Precise BIU error)",
               HEX_CAUSE_BIU_PRECISE);
        break;
    case HEX_CAUSE_DOUBLE_EXCEPT:
        printf("0x%x, Exception observed when EX = 1 (double exception)",
               HEX_CAUSE_DOUBLE_EXCEPT);
        break;
    case HEX_CAUSE_FETCH_NO_XPAGE:
        printf("0x%x, Privilege violation: User/Guest mode execute"
               " to page with no execute permissions",
               HEX_CAUSE_FETCH_NO_XPAGE);
        break;
    case HEX_CAUSE_FETCH_NO_UPAGE:
        printf("0x%x, Privilege violation: "
               "User mode exececute to page with no user permissions",
               HEX_CAUSE_FETCH_NO_UPAGE);
        break;
    case HEX_CAUSE_INVALID_PACKET:
        printf("0x%x, Invalid packet",
               HEX_CAUSE_INVALID_PACKET);
        break;
    case HEX_CAUSE_PRIV_USER_NO_GINSN:
        printf("0x%x, Privilege violation: guest mode insn in user mode",
               HEX_CAUSE_PRIV_USER_NO_GINSN);
        break;
    case HEX_CAUSE_PRIV_USER_NO_SINSN:
        printf("0x%x, Privilege violation: "
               "monitor mode insn ins user/guest mode",
               HEX_CAUSE_PRIV_USER_NO_SINSN);
        break;
    case HEX_CAUSE_REG_WRITE_CONFLICT:
        printf("0x%x, Multiple writes to same register",
               HEX_CAUSE_REG_WRITE_CONFLICT);
        break;
    case HEX_CAUSE_PC_NOT_ALIGNED:
        printf("0x%x, PC not aligned",
               HEX_CAUSE_PC_NOT_ALIGNED);
        break;
    case HEX_CAUSE_MISALIGNED_LOAD:
        printf("0x%x, Misaligned Load @ 0x%x",
               HEX_CAUSE_MISALIGNED_LOAD,
               arch_get_system_reg(env, HEX_SREG_BADVA));
        break;
    case HEX_CAUSE_MISALIGNED_STORE:
        printf("0x%x, Misaligned Store @ 0x%x",
               HEX_CAUSE_MISALIGNED_STORE,
               arch_get_system_reg(env, HEX_SREG_BADVA));
        break;
    case HEX_CAUSE_PRIV_NO_READ:
        printf("0x%x, Privilege violation: "
            "user/guest read permission @ 0x%x",
            HEX_CAUSE_PRIV_NO_READ,
            arch_get_system_reg(env, HEX_SREG_BADVA));
        break;
    case HEX_CAUSE_PRIV_NO_WRITE:
        printf("0x%x, Privilege violation: "
            "user/guest write permission @ 0x%x",
            HEX_CAUSE_PRIV_NO_WRITE,
            arch_get_system_reg(env, HEX_SREG_BADVA));
        break;
    case HEX_CAUSE_PRIV_NO_UREAD:
        printf("0x%x, Privilege violation: user read permission @ 0x%x",
               HEX_CAUSE_PRIV_NO_UREAD,
               arch_get_system_reg(env, HEX_SREG_BADVA));
        break;
    case HEX_CAUSE_PRIV_NO_UWRITE:
        printf("0x%x, Privilege violation: user write permission @ 0x%x",
               HEX_CAUSE_PRIV_NO_UWRITE,
               arch_get_system_reg(env, HEX_SREG_BADVA));
        break;
    case HEX_CAUSE_COPROC_LDST:
        printf("0x%x, Coprocessor VMEM address error. @ 0x%x",
               HEX_CAUSE_COPROC_LDST,
               arch_get_system_reg(env, HEX_SREG_BADVA));
        break;
    case HEX_CAUSE_STACK_LIMIT:
        printf("0x%x, Stack limit check error", HEX_CAUSE_STACK_LIMIT);
        break;
    case HEX_CAUSE_FPTRAP_CAUSE_BADFLOAT:
        printf("0x%X, Floating-Point: Execution of Floating-Point "
               "instruction resulted in exception",
               HEX_CAUSE_FPTRAP_CAUSE_BADFLOAT);
        break;
    case HEX_CAUSE_NO_COPROC_ENABLE:
        printf("0x%x, Illegal Execution of Coprocessor Instruction",
               HEX_CAUSE_NO_COPROC_ENABLE);
        break;
    case HEX_CAUSE_NO_COPROC2_ENABLE:
        printf("0x%x, "
               "Illegal Execution of Secondary Coprocessor Instruction",
               HEX_CAUSE_NO_COPROC2_ENABLE);
        break;
    case HEX_CAUSE_UNSUPORTED_HVX_64B:
        printf("0x%x, "
               "Unsuported Execution of Coprocessor Instruction with "
               "64bits Mode On",
               HEX_CAUSE_UNSUPORTED_HVX_64B);
        break;
    case HEX_CAUSE_VWCTRL_WINDOW_MISS:
        printf("0x%x, "
               "Thread accessing a region outside VWCTRL window",
               HEX_CAUSE_VWCTRL_WINDOW_MISS);
        break;
    default:
        printf("Don't know");
        break;
    }
    printf("\nRegister Dump:\n");
    hexagon_dump(env, stdout, 0);
}

static void sim_handle_trap0(CPUHexagonState *env)
{
    g_assert(bql_locked());
    target_ulong what_swi = arch_get_thread_reg(env, HEX_REG_R00);
    target_ulong swi_info = arch_get_thread_reg(env, HEX_REG_R01);
    uintptr_t retaddr = 0;
    CPUState *cs = env_cpu(env);

    if (!is_hexagon_specific_swi_flag(what_swi)) {
        if (what_swi == HEX_SYS_READ || what_swi == HEX_SYS_READC ||
            what_swi == HEX_SYS_WRITE) {
            /*
             * Avoid page faults if the buffer is not in memory yet.
             * NOTE: Counterintuitive, but a WRITE must be able to LOAD from
             * the input address. The contents of that buffer will be
             * directed to the SWI interface.
             */
            do_preload(env, swi_info, (what_swi == HEX_SYS_WRITE));
        }
        do_common_semihosting(cs);
        return;
    }

#ifndef _WIN32
    switch (what_swi) {

    case HEX_SYS_OPEN:
    {
        char filename[BUFSIZ];
        target_ulong physicalFilenameAddr;

        unsigned int filemode;
        int length;
        int real_openmode;
        int ret, err = 0;
        static const unsigned int mode_table[] = {
            O_RDONLY,
            O_RDONLY | O_BINARY,
            O_RDWR,
            O_RDWR | O_BINARY,
            O_WRONLY | O_CREAT | O_TRUNC,
            O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
            O_RDWR | O_CREAT | O_TRUNC,
            O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
            O_WRONLY | O_APPEND | O_CREAT,
            O_WRONLY | O_APPEND | O_CREAT | O_BINARY,
            O_RDWR | O_APPEND | O_CREAT,
            O_RDWR | O_APPEND | O_CREAT | O_BINARY,
            O_RDWR | O_CREAT,
            O_RDWR | O_CREAT | O_EXCL
        };


        hexagon_read_memory(env, swi_info, 4, &physicalFilenameAddr, retaddr);
        hexagon_read_memory(env, swi_info + 4, 4, &filemode, retaddr);
        hexagon_read_memory(env, swi_info + 8, 4, &length, retaddr);

        if (length >= BUFSIZ) {
            printf("%s:%d: ERROR: filename too large\n",
                    __func__, __LINE__);
        }

        int i = 0;
        do {
            hexagon_read_memory(env, physicalFilenameAddr + i, 1, &filename[i],
                                retaddr);
            i++;
        } while (filename[i - 1]);

        /* convert ARM ANGEL filemode into host filemode */
        if (filemode < 14)
            real_openmode = mode_table[filemode];
        else {
            real_openmode = 0;
            printf("%s:%d: ERROR: invalid OPEN mode: %d\n",
                   __func__, __LINE__, filemode);
        }

        ret = open(filename, real_openmode | O_BINARY, 0644);

        if (ret == -1) {
            err = errno;
            HexagonCPU *cpu = env_archcpu(env);
            if (cpu->usefs && g_strrstr(filename, ".so") != NULL
                && errno == ENOENT) {
                /*
                 * Didn't find it, so now we also try to open in the
                 * 'search dir':
                 */
                GString *lib_filename_str = g_string_new(cpu->usefs);

                g_string_append_printf(lib_filename_str, "/%s", filename);
                gchar *lib_filename = g_string_free(lib_filename_str, false);
                ret = open(lib_filename, real_openmode, 0644);
                if (ret == -1) {
                    err = errno;
                }
                g_free(lib_filename);
            }
        }

        if (ret != -1) {
            int guestfd = alloc_guestfd();
            associate_guestfd(guestfd, ret);
            ret = guestfd;
        }
        semi_cb(cs, ret, err);
    }
    break;

    case HEX_SYS_EXCEPTION:
        arch_set_system_reg(env, HEX_SREG_MODECTL, 0);
        exit(arch_get_thread_reg(env, HEX_REG_R02));
        break;

    /* We override arm-compatible version to print at stdout, not console. */
    case HEX_SYS_WRITEC:
    {
        FILE *fp = stdout;
        char c;
        rcu_read_lock();
        hexagon_read_memory(env, swi_info, 1, &c, retaddr);
        fprintf(fp, "%c", c);
        fflush(fp);
        rcu_read_unlock();
    }
    break;

    case HEX_SYS_WRITECREG:
        fprintf(stdout, "%c", swi_info);
        fflush(stdout);
        semi_cb(cs, 0, 0);
        break;

    /* We override arm-compatible version to print at stdout, not console. */
    case HEX_SYS_WRITE0:
    {
        FILE *fp = stdout;
        char c;
        int i = 0;
        rcu_read_lock();
        do {
            hexagon_read_memory(env, swi_info + i, 1, &c, retaddr);
            fprintf(fp, "%c", c);
            i++;
        } while (c);
        fflush(fp);
        rcu_read_unlock();
        break;
    }

    /*
     * Hexagon's SYS_ISTTY is a bit different than arm's: we do not return -1
     * on error, neither errno. So we override with out own implementation.
     */
    case HEX_SYS_ISTTY:
    {
        int fd;
        hexagon_read_memory(env, swi_info, 4, &fd, retaddr);
        semi_cb(cs, isatty(fd), 0);
    }
    break;

    case HEX_SYS_SEEK:
    {
        int fd;
        target_ulong off;
        hexagon_read_memory(env, swi_info, 4, &fd, retaddr);
        hexagon_read_memory(env, swi_info + 4, 4, &off, retaddr);
        semihost_sys_lseek(env_cpu(env), common_semi_ftell_cb, fd, off,
                          GDB_SEEK_SET);
    }
    break;

    case HEX_SYS_STAT:
    case HEX_SYS_FSTAT:
    {
        /*
         * This must match the caller's definition, it would be in the
         * caller's angel.h or equivalent header.
         */
        struct __SYS_STAT {
            uint64_t dev;
            uint64_t ino;
            uint32_t mode;
            uint32_t nlink;
            uint64_t rdev;
            uint32_t size;
            uint32_t __pad1;
            uint32_t atime;
            uint32_t mtime;
            uint32_t ctime;
            uint32_t __pad2;
        } sys_stat;
        struct stat st_buf;
        uint8_t *st_bufptr = (uint8_t *)&sys_stat;
        int rc, err = 0;
        char filename[BUFSIZ];
        target_ulong physicalFilenameAddr;
        target_ulong statBufferAddr;
        hexagon_read_memory(env, swi_info, 4, &physicalFilenameAddr, retaddr);

        if (what_swi == HEX_SYS_STAT) {
            int i = 0;
            do {
                hexagon_read_memory(env, physicalFilenameAddr + i, 1,
                                    &filename[i], retaddr);
                i++;
            } while ((i < BUFSIZ) && filename[i - 1]);
            rc = stat(filename, &st_buf);
            err = errno;
        } else{
            int fd = physicalFilenameAddr;
            GuestFD *gf = get_guestfd(fd);
            if (gf->type != GuestFDHost) {
                fprintf(stderr,
                        "fstat semihosting only implemented for native mode.\n");
                g_assert_not_reached();
            }
            rc = fstat(gf->hostfd, &st_buf);
            err = errno;
        }
        if (rc == 0) {
            sys_stat.dev   = st_buf.st_dev;
            sys_stat.ino   = st_buf.st_ino;
            sys_stat.mode  = st_buf.st_mode;
            sys_stat.nlink = (uint32_t) st_buf.st_nlink;
            sys_stat.rdev  = st_buf.st_rdev;
            sys_stat.size  = (uint32_t) st_buf.st_size;
#if defined(__linux__)
            sys_stat.atime = (uint32_t) st_buf.st_atim.tv_sec;
            sys_stat.mtime = (uint32_t) st_buf.st_mtim.tv_sec;
            sys_stat.ctime = (uint32_t) st_buf.st_ctim.tv_sec;
#elif defined(_WIN32)
            sys_stat.atime = st_buf.st_atime;
            sys_stat.mtime = st_buf.st_mtime;
            sys_stat.ctime = st_buf.st_ctime;
#endif
        }
        hexagon_read_memory(env, swi_info + 4, 4, &statBufferAddr, retaddr);

        for (int i = 0; i < sizeof(sys_stat); i++) {
            hexagon_write_memory(env, statBufferAddr + i, 1, st_bufptr[i],
                                 retaddr);
        }
        semi_cb(cs, rc, rc == 0 ? 0 : err);
    }
    break;

    case HEX_SYS_FTRUNC:
    {
        int fd;
        off_t size_limit;
        hexagon_read_memory(env, swi_info, 4, &fd, retaddr);
        hexagon_read_memory(env, swi_info + 4, 8, &size_limit, retaddr);
        semihost_sys_ftruncate(cs, semi_cb, fd, size_limit);
    }
    break;

    case HEX_SYS_ACCESS:
    {
        char filename[BUFSIZ];
        uint32_t FileNameAddr;
        uint32_t BufferMode;
        int rc;

        int i = 0;

        hexagon_read_memory(env, swi_info, 4, &FileNameAddr, retaddr);
        do {
            hexagon_read_memory(env, FileNameAddr + i, 1, &filename[i],
                                retaddr);
            i++;
        } while ((i < BUFSIZ) && (filename[i - 1]));
        filename[i] = 0;

        hexagon_read_memory(env, swi_info + 4, 4, &BufferMode, retaddr);

        rc = access(filename, BufferMode);
        semi_cb(cs, rc,  rc == 0 ? 0 : errno);
    }
    break;

    case HEX_SYS_GETCWD:
    {
        char cwdPtr[PATH_MAX];
        uint32_t BufferAddr;
        uint32_t BufferSize;
        uint32_t rc = 0, err = 0;

        hexagon_read_memory(env, swi_info, 4, &BufferAddr, retaddr);
        hexagon_read_memory(env, swi_info + 4, 4, &BufferSize, retaddr);

        if (!getcwd(cwdPtr, PATH_MAX)) {
            err = errno;
        } else {
            size_t cwd_size = strlen(cwdPtr);
            if (cwd_size > BufferSize) {
                err = ERANGE;
            } else {
                for (int i = 0; i < cwd_size; i++) {
                    hexagon_write_memory(env, BufferAddr + i, 1,
                                         (uint64_t)cwdPtr[i], retaddr);
                }
                rc = BufferAddr;
            }
        }
        semi_cb(cs, rc, rc != 0 ? 0 : err);
        break;
    }

    case HEX_SYS_EXEC:
    {
        qemu_log_mask(LOG_UNIMP, "SYS_EXEC is deprecated\n");
        semi_cb(cs, -1, ENOSYS);
    }
    break;

    case HEX_SYS_OPENDIR:
    {
        DIR *dir;
        char buf[BUFSIZ];
        int rc = 0, err = 0;

        int i = 0;
        do {
            hexagon_read_memory(env, swi_info + i, 1, &buf[i], retaddr);
            i++;
        } while (buf[i - 1]);

        dir = opendir(buf);
        if (dir != NULL) {
            env->dir_list = g_list_append(env->dir_list, dir);
            rc = g_list_index(env->dir_list, dir) + DIR_INDEX_OFFSET;
        } else {
            err = errno;
        }
        semi_cb(cs, rc, rc != 0 ? 0 : err);
        break;
    }

    case HEX_SYS_READDIR:
    {
        struct dirent *host_dir_entry = NULL;
        int dir_index = swi_info - DIR_INDEX_OFFSET;
        DIR *dir = g_list_nth_data(env->dir_list, dir_index);
        uint32_t rc = 0, err = 0;

        if (dir) {
            errno = 0;
            host_dir_entry = readdir(dir);
            if (host_dir_entry == NULL) {
                err = errno;
            }
        } else {
            err = EBADF;
        }

        if (host_dir_entry) {
            uint32_t guest_dir_entry = arch_get_thread_reg(env, HEX_REG_R02);
            hexagon_write_memory(env, guest_dir_entry, 4, host_dir_entry->d_ino,
                                 retaddr);
            for (int i = 0; i < sizeof(host_dir_entry->d_name); i++) {
                hexagon_write_memory(env, guest_dir_entry + 4 + i, 1,
                                     host_dir_entry->d_name[i], retaddr);
                if (!host_dir_entry->d_name[i]) {
                    break;
                }
            }
            rc = guest_dir_entry;
        }
        semi_cb(cs, rc, err);
        break;
    }

    case HEX_SYS_CLOSEDIR:
    {
        DIR *dir;
        int ret = -1, err;
        int dir_index = swi_info - DIR_INDEX_OFFSET;

        dir = g_list_nth_data(env->dir_list, dir_index);
        if (dir != NULL) {
            ret = closedir(dir);
            if (ret != 0) {
                err = errno;
            }
        } else {
            err = EBADF;
        }
        semi_cb(cs, ret, ret == 0 ? 0 : err);
        break;
    }

    case HEX_SYS_COREDUMP:
        coredump(env);
        break;

    case HEX_SYS_FTELL:
    {
        int fd;
        hexagon_read_memory(env, swi_info, 4, &fd, retaddr);
        semihost_sys_lseek(cs, common_semi_ftell_cb, fd, 0, GDB_SEEK_CUR);
    }
    break;

    case HEX_SYS_READ_CYCLES:
    case HEX_SYS_READ_TCYCLES:
    case HEX_SYS_READ_ICOUNT:
    {
        arch_set_thread_reg(env, HEX_REG_R00, 0);
        arch_set_thread_reg(env, HEX_REG_R01, 0);
        break;
    }

    case HEX_SYS_READ_PCYCLES:
    {
        arch_set_thread_reg(env, HEX_REG_R00,
            arch_get_system_reg(env, HEX_SREG_PCYCLELO));
        arch_set_thread_reg(env, HEX_REG_R01,
            arch_get_system_reg(env, HEX_SREG_PCYCLEHI));
        break;
    }

    case HEX_SYS_PROF_ON:
    case HEX_SYS_PROF_OFF:
    case HEX_SYS_PROF_STATSRESET:
    case HEX_SYS_DUMP_PMU_STATS:
    case HEX_SYS_HEAPINFO:
        semi_cb(cs, -1, ENOSYS);
        qemu_log_mask(LOG_UNIMP, "SWI call %x is unimplemented in QEMU\n",
                      what_swi);
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "unknown swi request: 0x%x\n", what_swi);
        cpu_abort(env_cpu(env), "Hexagon Unsupported swi call 0x%x\n", what_swi);
    }
#else
    (void) retaddr;
    qemu_log_mask(LOG_UNIMP, "SWI call %x is unimplemented in QEMU\n",
                      what_swi);
#endif /* _WIN32 */

}

static void set_addresses(CPUHexagonState *env, target_ulong pc_offset,
                          target_ulong exception_index)

{
    HexagonCPU *cpu = env_archcpu(env);
    uint32_t evb = cpu->globalregs ?
        arch_get_system_reg(env, HEX_SREG_EVB) :
        cpu->boot_addr;
    arch_set_system_reg(env, HEX_SREG_ELR,
                        arch_get_thread_reg(env, HEX_REG_PC) + pc_offset);
    arch_set_thread_reg(env, HEX_REG_PC, evb | (exception_index << 2));
}

static const char *event_name[] = {
    [HEX_EVENT_RESET] = "HEX_EVENT_RESET",
    [HEX_EVENT_IMPRECISE] = "HEX_EVENT_IMPRECISE",
    [HEX_EVENT_TLB_MISS_X] = "HEX_EVENT_TLB_MISS_X",
    [HEX_EVENT_TLB_MISS_RW] = "HEX_EVENT_TLB_MISS_RW",
    [HEX_EVENT_TRAP0] = "HEX_EVENT_TRAP0",
    [HEX_EVENT_TRAP1] = "HEX_EVENT_TRAP1",
    [HEX_EVENT_FPTRAP] = "HEX_EVENT_FPTRAP",
    [HEX_EVENT_DEBUG] = "HEX_EVENT_DEBUG",
    [HEX_EVENT_INT0] = "HEX_EVENT_INT0",
    [HEX_EVENT_INT1] = "HEX_EVENT_INT1",
    [HEX_EVENT_INT2] = "HEX_EVENT_INT2",
    [HEX_EVENT_INT3] = "HEX_EVENT_INT3",
    [HEX_EVENT_INT4] = "HEX_EVENT_INT4",
    [HEX_EVENT_INT5] = "HEX_EVENT_INT5",
    [HEX_EVENT_INT6] = "HEX_EVENT_INT6",
    [HEX_EVENT_INT7] = "HEX_EVENT_INT7",
    [HEX_EVENT_INT8] = "HEX_EVENT_INT8",
    [HEX_EVENT_INT9] = "HEX_EVENT_INT9",
    [HEX_EVENT_INTA] = "HEX_EVENT_INTA",
    [HEX_EVENT_INTB] = "HEX_EVENT_INTB",
    [HEX_EVENT_INTC] = "HEX_EVENT_INTC",
    [HEX_EVENT_INTD] = "HEX_EVENT_INTD",
    [HEX_EVENT_INTE] = "HEX_EVENT_INTE",
    [HEX_EVENT_INTF] = "HEX_EVENT_INTF"
};

void hexagon_cpu_do_interrupt(CPUState *cs)

{
    CPUHexagonState *env = cpu_env(cs);
    uint32_t ssr;

    BQL_LOCK_GUARD();

    qemu_log_mask(CPU_LOG_INT,
                  "\t%s: event 0x%" PRIx32 ":%s, cause 0x" TARGET_FMT_lx
                  "(%" PRId32 ")\n",
                  __func__, cs->exception_index,
                  event_name[cs->exception_index], env->cause_code,
                  (int32_t)env->cause_code);

    env->llsc_addr = ~0;

    ssr = arch_get_system_reg(env, HEX_SREG_SSR);
    if (GET_SSR_FIELD(SSR_EX, ssr) == 1) {
        arch_set_system_reg(env, HEX_SREG_DIAG, env->cause_code);
        env->cause_code = HEX_CAUSE_DOUBLE_EXCEPT;
        cs->exception_index = HEX_EVENT_PRECISE;
    }

    switch (cs->exception_index) {
    case HEX_EVENT_TRAP0:
        if (env->cause_code == 0) {
            sim_handle_trap0(env);
        }

        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 4, cs->exception_index);
        break;

    case HEX_EVENT_TRAP1:
        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 4, cs->exception_index);
        break;

    case HEX_EVENT_TLB_MISS_X:
        switch (env->cause_code) {
        case HEX_CAUSE_TLBMISSX_CAUSE_NORMAL:
        case HEX_CAUSE_TLBMISSX_CAUSE_NEXTPAGE:
            qemu_log_mask(CPU_LOG_MMU,
                          "TLB miss EX exception (0x%x) caught: "
                          "Cause code (0x%x) "
                          "TID = 0x%" PRIx32 ", PC = 0x%" PRIx32
                          ", BADVA = 0x%" PRIx32 "\n",
                          cs->exception_index, env->cause_code, env->threadId,
                          arch_get_thread_reg(env, HEX_REG_PC),
                          arch_get_system_reg(env, HEX_SREG_BADVA));

            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        default:
            cpu_abort(cs,
                      "1:Hexagon exception %d/0x%x: "
                      "Unknown cause code %d/0x%x\n",
                      cs->exception_index, cs->exception_index, env->cause_code,
                      env->cause_code);
            break;
        }
        break;

    case HEX_EVENT_TLB_MISS_RW:
        switch (env->cause_code) {
        case HEX_CAUSE_TLBMISSRW_CAUSE_READ:
        case HEX_CAUSE_TLBMISSRW_CAUSE_WRITE:
            qemu_log_mask(CPU_LOG_MMU,
                          "TLB miss RW exception (0x%x) caught: "
                          "Cause code (0x%x) "
                          "TID = 0x%" PRIx32 ", PC = 0x%" PRIx32
                          ", BADVA = 0x%" PRIx32 "\n",
                          cs->exception_index, env->cause_code, env->threadId,
                          env->gpr[HEX_REG_PC],
                          arch_get_system_reg(env, HEX_SREG_BADVA));

            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            /* env->sreg[HEX_SREG_BADVA] is set when the exception is raised */
            break;

        default:
            cpu_abort(cs,
                      "2:Hexagon exception %d/0x%x: "
                      "Unknown cause code %d/0x%x\n",
                      cs->exception_index, cs->exception_index, env->cause_code,
                      env->cause_code);
            break;
        }
        break;

    case HEX_EVENT_FPTRAP:
        hexagon_ssr_set_cause(env, env->cause_code);
        arch_set_thread_reg(env, HEX_REG_PC,
                            arch_get_system_reg(env, HEX_SREG_EVB) |
                                (cs->exception_index << 2));
        break;

    case HEX_EVENT_DEBUG:
        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 0, cs->exception_index);
        env->ss_pending = false;
        break;

    case HEX_EVENT_PRECISE:
        switch (env->cause_code) {
        case HEX_CAUSE_FETCH_NO_XPAGE:
        case HEX_CAUSE_FETCH_NO_UPAGE:
        case HEX_CAUSE_PRIV_NO_READ:
        case HEX_CAUSE_PRIV_NO_UREAD:
        case HEX_CAUSE_PRIV_NO_WRITE:
        case HEX_CAUSE_PRIV_NO_UWRITE:
        case HEX_CAUSE_MISALIGNED_LOAD:
        case HEX_CAUSE_MISALIGNED_STORE:
        case HEX_CAUSE_PC_NOT_ALIGNED:
            qemu_log_mask(CPU_LOG_MMU,
                          "MMU permission exception (0x%x) caught: "
                          "Cause code (0x%x) "
                          "TID = 0x%" PRIx32 ", PC = 0x%" PRIx32
                          ", BADVA = 0x%" PRIx32 "\n",
                          cs->exception_index, env->cause_code, env->threadId,
                          env->gpr[HEX_REG_PC],
                          arch_get_system_reg(env, HEX_SREG_BADVA));


            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            /* env->sreg[HEX_SREG_BADVA] is set when the exception is raised */
            break;

        case HEX_CAUSE_DOUBLE_EXCEPT:
        case HEX_CAUSE_PRIV_USER_NO_SINSN:
        case HEX_CAUSE_PRIV_USER_NO_GINSN:
        case HEX_CAUSE_INVALID_OPCODE:
        case HEX_CAUSE_NO_COPROC_ENABLE:
        case HEX_CAUSE_NO_COPROC2_ENABLE:
        case HEX_CAUSE_UNSUPORTED_HVX_64B:
        case HEX_CAUSE_REG_WRITE_CONFLICT:
        case HEX_CAUSE_VWCTRL_WINDOW_MISS:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        case HEX_CAUSE_COPROC_LDST:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        case HEX_CAUSE_STACK_LIMIT:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        default:
            cpu_abort(cs,
                      "3:Hexagon exception %d/0x%x: "
                      "Unknown cause code %d/0x%x\n",
                      cs->exception_index, cs->exception_index, env->cause_code,
                      env->cause_code);
            break;
        }
        break;

    case HEX_EVENT_IMPRECISE:
        switch (env->cause_code) {
        case HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 4, cs->exception_index);
            arch_set_system_reg(env, HEX_SREG_DIAG,
                (0x4 << 4) |
                    (arch_get_system_reg(env, HEX_SREG_HTID) & 0xF));
            break;

        case HEX_CAUSE_IMPRECISE_NMI:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 4, cs->exception_index);
            arch_set_system_reg(env, HEX_SREG_DIAG,
                (0x3 << 4) |
                    (arch_get_system_reg(env, HEX_SREG_DIAG)));
            break;

        default:
            cpu_abort(cs,
                      "4:Hexagon exception %d/0x%x: "
                      "Unknown cause code %d/0x%x\n",
                      cs->exception_index, cs->exception_index,
                      env->cause_code, env->cause_code);
            break;
        }
        break;

    default:
        qemu_log_mask(LOG_UNIMP,
                "Hexagon Unsupported exception 0x%x/0x%x\n",
                  cs->exception_index, env->cause_code);
        break;
    }

    cs->exception_index = HEX_EVENT_NONE;
}

void register_trap_exception(CPUHexagonState *env, int traptype, int imm,
                             target_ulong PC)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = (traptype == 0) ? HEX_EVENT_TRAP0 : HEX_EVENT_TRAP1;
    ASSERT_DIRECT_TO_GUEST_UNSET(env, cs->exception_index);

    env->cause_code = imm;
    env->gpr[HEX_REG_PC] = PC;
    cpu_loop_exit(cs);
}
