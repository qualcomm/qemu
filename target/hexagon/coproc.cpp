/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CONFIG_USER_ONLY

#pragma GCC diagnostic ignored "-Wundef"
#if !defined(__clang__)
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "rpc/client.h"
#include "rpc/rpc_error.h"

#include "coproc.h"

constexpr int COPROC_RPC_VERSION = 10;

#if defined(__unix__) || defined(__APPLE__)
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

constexpr const char *COPROC_EXECUTABLE_NAME = "coproc_rpc_remote";

typedef pid_t process_id;

struct pipe_t {
    union {
        struct {
            int read_end;
            int write_end;
        };
        int pipefd[2];
    };
};
#elif _WIN32
#include <windows.h>

constexpr const char *COPROC_EXECUTABLE_NAME = "coproc_rpc_remote.exe";

typedef DWORD process_id;

struct pipe_t {
    HANDLE read_end;
    HANDLE write_end;
};
#endif

class RemoteRPC
{
    std::unique_ptr<rpc::client> client;
    std::thread child_alive_checker;
    pipe_t coproc_pipe;

    void create_pipe()
    {
#if defined(__unix__) || defined(__APPLE__)
        if (-1 == pipe(coproc_pipe.pipefd)) {
            std::cerr << "Failed to create pipe: " << std::strerror(errno)
                      << std::endl;
            std::exit(EXIT_FAILURE);
        }
#elif _WIN32
        SECURITY_ATTRIBUTES sa = {
            sizeof(SECURITY_ATTRIBUTES),
            NULL,
            TRUE,
        };
        if (!CreatePipe(&coproc_pipe.read_end, &coproc_pipe.write_end, &sa,
                        0)) {
            std::cerr << "Failed to create pipe: " << GetLastError()
                      << std::endl;
            std::exit(EXIT_FAILURE);
        }
#endif
    }

    void start_child_alive_checker(process_id child_pid)
    {
        child_alive_checker = std::thread([=]() {
#if defined(__unix__) || defined(__APPLE__)
            for (;;) {
                int wstatus;
                if (-1 == waitpid(child_pid, &wstatus, 0)) {
                    std::cerr << "Waiting for child process failed: "
                              << std::strerror(errno) << std::endl;
                    std::exit(EXIT_FAILURE);
                }
                if (WIFEXITED(wstatus)) {
                    break;
                }
            }
#elif _WIN32
            HANDLE process =
                OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, child_pid);
            if (!process) {
                std::cerr << "Opening child process failed: "
                          << std::strerror(errno) << std::endl;
                std::exit(EXIT_FAILURE);
            }

            WaitForSingleObject(process, INFINITE);
            CloseHandle(process);
#endif
        });
    }

    unsigned short read_coproc_port() const
    {
        unsigned short coproc_port = 0;
#if defined(__unix__) || defined(__APPLE__)
        struct pollfd pfd = {coproc_pipe.read_end, POLLIN, 0};
        auto poll_ret = poll(&pfd, 1, 3000);
        if (poll_ret == 0) {
            std::cerr
            << "Timed out waiting for coproc rpc server port, possible version mismatch"
            << std::endl;
            std::exit(EXIT_FAILURE);
        } else if (poll_ret < 0) {
            std::cerr << "poll() failed waiting for coproc rpc server port: "
            << std::strerror(errno) << std::endl;
            std::exit(EXIT_FAILURE);
        }
        auto ret =
            read(coproc_pipe.read_end, &coproc_port, sizeof(coproc_port));
        if (-1 == ret) {
            std::cerr << "Failed to read coproc rpc server port: "
            << std::strerror(errno) << std::endl;
            std::exit(EXIT_FAILURE);
        }
#elif _WIN32
        auto wait_ret = WaitForSingleObject(coproc_pipe.read_end, 3000);
        if (wait_ret == WAIT_TIMEOUT) {
            std::cerr
            << "Timed out waiting for coproc rpc server port, possible version mismatch"
            << std::endl;
            std::exit(EXIT_FAILURE);
        } else if (wait_ret == WAIT_FAILED) {
            std::cerr << "Wait failed for coproc rpc server port: "
            << GetLastError() << std::endl;
            std::exit(EXIT_FAILURE);
        }
        DWORD bytes_read;
        auto ret = ReadFile(coproc_pipe.read_end, &coproc_port,
                            sizeof(coproc_port), &bytes_read, NULL);
        if (!ret) {
            std::cerr << "Failed to read coproc rpc server port: "
                      << GetLastError() << std::endl;
            std::exit(EXIT_FAILURE);
        }
        if (sizeof(coproc_port) != bytes_read) {
            std::cerr << "Server port bytes read don't match: "
                      << sizeof(coproc_port) << " != " << bytes_read
                      << std::endl;
            std::exit(EXIT_FAILURE);
        }
#endif
        return coproc_port;
    }

    process_id create_child(const std::string &exec_path)
    {
#if defined(__unix__) || defined(__APPLE__)
        auto child_pid = fork();
        if (child_pid == 0) {
            close(coproc_pipe.read_end);

            sigset_t sigset;
            sigfillset(&sigset);
            sigprocmask(SIG_SETMASK, &sigset, NULL);

            std::vector<const char *> argp{
                exec_path.c_str(),
                std::to_string(coproc_pipe.write_end).c_str(), 0
            };

            execv(exec_path.c_str(), const_cast<char **>(&argp[0]));

            std::cerr << "Failed to fork Coproc: " << std::strerror(errno)
                      << std::endl;
            std::exit(EXIT_FAILURE);
        } else if (-1 == child_pid) {
            std::cerr << "Failed to fork Coproc: " << std::strerror(errno)
                      << std::endl;
            std::exit(EXIT_FAILURE);
        }

        close(coproc_pipe.write_end);
#elif _WIN32
        STARTUPINFO si = { .cb = sizeof(si) };
        PROCESS_INFORMATION pi = {};

        auto command =
            exec_path + " " + std::to_string((uintptr_t)coproc_pipe.write_end);
        if (!CreateProcess(NULL, const_cast<char *>(command.c_str()), NULL,
                           NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            std::cerr << "Failed to fork Coproc: " << GetLastError()
                      << std::endl;
            std::exit(EXIT_FAILURE);
        }

        auto child_pid = pi.dwProcessId;
#endif

        return child_pid;
    }

  public:
    RemoteRPC(const std::string exec_path, const int hex_rev = 0)
    {
        if (exec_path.empty()) {
            std::cout << "Coproc path missing" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        create_pipe();

        auto child_pid = create_child(exec_path);
        start_child_alive_checker(child_pid);

        auto coproc_port = read_coproc_port();
        client = std::make_unique<rpc::client>("localhost", coproc_port);
        client->set_timeout(3000);

        try {
            int coproc_version =
                client->call("get_rpc_version").template as<int>();
            if (coproc_version != COPROC_RPC_VERSION) {
                std::cout << std::hex << "FATAL: coproc RPC version is 0x"
                          << coproc_version << " and this QEMU requires 0x"
                          << COPROC_RPC_VERSION << "." << std::endl;
                std::exit(EXIT_SUCCESS);
            }
        } catch (rpc::rpc_error &e) {
            std::cout << "FATAL: failed to query coproc RPC version.\n"
                      << "Your coproc binary might be too old." << std::endl;
            std::exit(EXIT_FAILURE);
        }

        if (!client->call("set_hexagon_revision", hex_rev).template as<int>()) {
            /* Error already reported. */
            std::exit(EXIT_FAILURE);
        }
    }

    ~RemoteRPC()
    {
        client->call("exit");

        if (child_alive_checker.joinable()) {
            child_alive_checker.join();
        }

#if defined(__unix__) || defined(__APPLE__)
        close(coproc_pipe.read_end);
#elif _WIN32
        CloseHandle(coproc_pipe.read_end);
#endif
    }

    void call_coproc(int32_t opcode, hwaddr vtcm_base, uint32_t vtcm_size,
                    uint32_t reg_usr, int32_t fd, int32_t page_size,
                    int32_t arg1, int32_t arg2)
    {
#if defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
        std::this_thread::sleep_for(std::chrono::nanoseconds(1));
#endif

        try {
            client->call("coproc", opcode, vtcm_base, vtcm_size, reg_usr, fd,
                         page_size, arg1, arg2);
        } catch (const std::exception &e) {
            std::cerr << "RPC call failed: " << e.what() << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }
};

static std::unique_ptr<RemoteRPC> local_rpc;

static std::filesystem::path qemu_executable()
{
    std::string location(PATH_MAX, '\0');

#if defined(__unix__) || defined(__APPLE__)
    static constexpr const char *SELF = "/proc/self/exe";

    auto nbytes = readlink(SELF, &location[0], location.size());
    if (-1 == nbytes) {
#elif _WIN32
    auto nbytes = GetModuleFileNameA(nullptr, &location[0], location.size());
    if (0 == nbytes) {
#endif
        std::cerr << "WARNING: Reading qemu location failed\n";
        return std::filesystem::path();
    }

    return std::filesystem::path(location);
}

int coproc_init(const char *coproc_location_user, int hex_rev)
{
    if (!local_rpc) {
        std::filesystem::path coproc_location =
            coproc_location_user ? std::filesystem::path(coproc_location_user) :
                                   qemu_executable().parent_path();

        auto coproc = coproc_location.append(COPROC_EXECUTABLE_NAME);
        if (!std::filesystem::exists(coproc)) {
            std::cerr << "ERROR: Hexagon COPROC not found: " << coproc << "\n";
            return COPROC_ERROR;
        }

        local_rpc = std::make_unique<RemoteRPC>(coproc.string(), hex_rev);
    }
    return COPROC_SUCCESS;
}

void coproc(const CoprocArgs *args)
{
    coproc_trace_op(args->opcode);
    if (local_rpc) {
        local_rpc->call_coproc(args->opcode, args->vtcm_base, args->vtcm_size,
                               args->reg_usr, args->subsystem_id,
                               args->page_size, args->arg1, args->arg2);
    }
}

#else

int coproc_init(const char *coproc_path, int hex_rev) { return COPROC_SUCCESS; }
void coproc(const CoprocArgs *args) {}

#endif
