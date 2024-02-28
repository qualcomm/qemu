/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "rpc/client.h"
#include "rpc/rpc_error.h"
#include "rpc/server.h"
#include "rpc/this_handler.h"
#include "rpc/this_server.h"
#include "rpc/this_session.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <poll.h>
#include <queue>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <utility>

#define RPC_VERSION 4
#define GS_Process_Server_Port "GS_Process_Server_Port"
#define GS_Process_Server_Port_Len 22
#define DECIMAL_PORT_NUM_STR_LEN 20
#define DECIMAL_PID_T_STR_LEN 20


class ProcAliveHandler {
  public:
    ProcAliveHandler()
        : stop_running(false), m_is_ppid_set{ false }, m_is_parent_setup_called{
              false
          }
    {
    }

    ~ProcAliveHandler()
    {
        close(m_sock_pair_fds[0]);
        close(m_sock_pair_fds[1]);
        stop_running = true;
        if (parent_alive_checker.joinable()) {
            parent_alive_checker.join();
        }
        if (child_waiter.joinable()) {
            child_waiter.join();
        }
    }

    void wait_child(pid_t cpid)
    {
        child_waiter = std::thread(
            [](pid_t cpid) {
                pid_t m_wpid;
                int wstatus;
                for (;;) {
                    m_wpid = waitpid(cpid, &wstatus, 0);
                    if (m_wpid == -1) {
                        exit(EXIT_FAILURE);
                    }
                    if (WIFEXITED(wstatus)) {
                        break;
                    }
                }
            },
            cpid);
    }

    inline void recv_sockpair_fds_from_remote(int fd0, int fd1)
    {
        m_sock_pair_fds[0] = fd0;
        m_sock_pair_fds[1] = fd1;
    }

    inline int get_sockpair_fd0() const
    {
        return m_sock_pair_fds[0];
    }

    inline int get_sockpair_fd1() const
    {
        return m_sock_pair_fds[1];
    }

    inline pid_t get_ppid() const
    {
        if (m_is_ppid_set) {
            return m_ppid;
        } else {
            return -1;
        }
    }

    inline void init_peer_conn_checker()
    {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, m_sock_pair_fds) == -1) {
            perror("ProcAliveHandler socketpair");
            std::exit(EXIT_FAILURE);
        }
    }

    inline void setup_parent_conn_checker()
    {
        m_is_parent_setup_called = true;
        close(m_sock_pair_fds[1]);
    }

    /*
     * same thread parent connected checker
     */
    inline void check_parent_conn_sth(std::function<void()> on_parent_exit)
    {
        _set_ppid();
        close(m_sock_pair_fds[0]);
        struct pollfd parent_connected_monitor;
        parent_connected_monitor.fd = m_sock_pair_fds[1];
        parent_connected_monitor.events = POLLIN;
        int ret = 0;
        while (!stop_running) {
            ret = poll(&parent_connected_monitor, 1, 500);
            if ((ret == -1 && errno == EINTR) || (ret == 0) /*timeout*/)
                continue;
            else if (parent_connected_monitor.revents &
                     (POLLHUP | POLLERR | POLLNVAL)) {
                on_parent_exit();
                exit(EXIT_SUCCESS);
            } else {
                perror("poll");
                exit(EXIT_FAILURE);
            }
        }
    }

    /*
     * new thread parent connected checker
     */
    void check_parent_conn_nth(std::function<void()> on_parent_exit)
    {
        parent_alive_checker = std::thread(
            &ProcAliveHandler::check_parent_conn_sth, this, on_parent_exit);
    }

  private:
    inline void _set_ppid()
    {
        m_ppid = getppid();
        m_is_ppid_set = true;
    }

  private:
    std::atomic_bool stop_running;
    pid_t m_ppid;
    std::thread child_waiter;
    std::thread parent_alive_checker;
    int m_sock_pair_fds[2];
    bool m_is_ppid_set;
    bool m_is_parent_setup_called;
};


typedef uint64_t hwaddr;

class RemoteRPC {
  public:
    RemoteRPC(std::string p_exec_path = "", int hex_rev = 0)
        : sport(0), cport(0), exec_path(p_exec_path), hexagon_revision(hex_rev),
          stop_server(false), cancel_waiting(false)
    {
    }

    void init()
    {
        server = new rpc::server(sport);
        server->suppress_exceptions(true);
        sport = server->port();

        if (cport == 0 && getenv((std::string(GS_Process_Server_Port) +
                                  std::to_string(getpid()))
                                     .c_str())) {
            cport = std::stoi(std::string(getenv(
                (std::string(GS_Process_Server_Port) + std::to_string(getpid()))
                    .c_str())));
        }

        server->bind("reg", [&](unsigned short port) {
            assert(cport == 0 && client == nullptr);
            cport = port;
            if (!client) {
                client = new rpc::client("localhost", cport);
            }
            std::lock_guard<std::mutex> lg(client_conncted_mut);
            is_client_connected.notify_one();
            client->async_call("sock_pair", pahandler.get_sockpair_fd0(),
                               pahandler.get_sockpair_fd1());
        });

        server->bind("exit", [&](int status) {
            rpc::this_session().post_exit();
            cancel_waiting = true;
            stop_server = true;
        });

        server->bind("sock_pair", [&](int sock_fd0, int sock_fd1) {
            pahandler.recv_sockpair_fds_from_remote(sock_fd0, sock_fd1);
            pahandler.check_parent_conn_nth([&]() {
                /*
                 * std::cout << "CLIENT: remote process (" << getpid()
                 *   << ") detected parent ("
                 *   << pahandler.get_ppid() << ") exit!" << std::endl;
                 */
            });
            return;
        });

        server_runner = std::thread([this]() {
            server->async_run();
            while (stop_server == false) {
                /* small delay before checking flag again */
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        if (cport) {
            if (!client) {
                client = new rpc::client("localhost", cport);
            }
            client->call("reg", sport);
        }

        if (!exec_path.empty()) {
            pahandler.init_peer_conn_checker();

            char val[DECIMAL_PORT_NUM_STR_LEN + 1];
            snprintf(val, DECIMAL_PORT_NUM_STR_LEN + 1, "%d", sport);

            std::vector<const char * > argp;
            argp.push_back(exec_path.c_str());
            argp.push_back(0);

            m_child_pid = fork();

            if (m_child_pid > 0) {
                pahandler.setup_parent_conn_checker();
                pahandler.wait_child(m_child_pid);

            } else if (m_child_pid == 0) {
                char
                    key[GS_Process_Server_Port_Len + DECIMAL_PID_T_STR_LEN + 1];
                snprintf(key,
                         GS_Process_Server_Port_Len + DECIMAL_PID_T_STR_LEN + 1,
                         "%s%d", GS_Process_Server_Port, getpid());
                setenv(key, val, 1);

                execv(exec_path.c_str(), const_cast<char * *>(&argp[0]));
                std::cout << "CLIENT: Unable to exec the "
                             "remote child process, error: "
                          << std::strerror(errno) << std::endl;
            } else {
                std::cout << "CLIENT: failed to fork remote process, error: "
                          << std::strerror(errno) << std::endl;
            }
        }

        /* Make sure by now the client is connected so we can send/recieve */
        std::unique_lock<std::mutex> ul(client_conncted_mut);
        is_client_connected.wait(
            ul, [&]() { return (cport > 0 || cancel_waiting); });
        ul.unlock();

        try {
            int coproc_version = client->call("get_rpc_version").template as<int>();
            if (coproc_version != RPC_VERSION) {
                std::cout << "FATAL: coproc RPC version is "
                          << coproc_version << " and this QEMU requires "
                          << RPC_VERSION << "." << std::endl;
                exit(1);
            }
        } catch (rpc::rpc_error &e) {
            std::cout << "FATAL: failed to query coproc RPC version.\n"
                      << "Your coproc binary might be too old." << std::endl;
            exit(1);
        }

        if (!client->call("set_hexagon_revision", hexagon_revision).template as<int>()) {
            /* Error already reported. */
            exit(1);
        }
    }

    void wait_server()
    {
        if (server_runner.joinable()) {
            server_runner.join();
        }
    }

    int call_coproc(int32_t opcode, hwaddr vtcm_base, uint32_t vtcm_size,
                    uint32_t reg_usr, int32_t fd, int32_t page_size,
                    int32_t arg1, int32_t arg2)
    {
        int result = -1;
        if (client) {
            result = client
                ->call("coproc", opcode, vtcm_base, vtcm_size, reg_usr,
                       fd, page_size, arg1, arg2).template as<int>();
        }
        return result;
    }

    ~RemoteRPC()
    {
        cancel_waiting = true;
        if (server) {
            server->close_sessions();
            server->stop();
            delete server;
            server = nullptr;
        }
        wait_server();
        if (client) {
            client->async_call("exit", 0);
            /* client->wait_all_responses();*/
            delete client;
            client = nullptr;
        }
        if (child_waiter.joinable()) {
            child_waiter.join();
        }
    }

  private:
    rpc::server * server = nullptr;
    rpc::client * client = nullptr;
    unsigned short sport;
    unsigned short cport;
    std::string exec_path;
    int hexagon_revision;
    pid_t m_child_pid;
    std::atomic_bool stop_server;
    std::atomic_bool cancel_waiting;
    std::condition_variable is_client_connected;
    std::mutex client_conncted_mut;
    std::thread child_waiter;
    std::thread server_runner;
    ProcAliveHandler pahandler;
    std::vector<int64_t> elapsed_v;
};
