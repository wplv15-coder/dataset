#define _GNU_SOURCE

#include "common.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/wait.h>

static void print_usage(const char *argv0)
{
    fprintf(stderr,
            "用法:\n"
            "  %s <server_ip> [port] <GPIO|I2C|SPI> [--auto [次数]]\n"
            "  %s <server_ip> [port] --all [--auto [次数]]\n"
            "\n"
            "示例:\n"
            "  %s 192.168.1.20 GPIO\n"
            "  %s 192.168.1.20 8888 I2C --auto 3\n"
            "  %s 192.168.1.20 --all --auto\n",
            argv0, argv0, argv0, argv0, argv0);
}

static int connect_server(const char *ip, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        log_line("[%s] 非法 IP 地址", ip);
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_line("连接 %s:%d 失败: %s", ip, port, strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

static int run_auto_session(int fd, const char *role, int rounds)
{
    char buf[BUF_SIZE];

    if (send_line(fd, "HELLO %s", role) < 0) {
        return 1;
    }
    if (recv_line(fd, buf, sizeof(buf)) <= 0) {
        log_line("[%s] 未收到 HELLO 应答", role);
        return 1;
    }
    log_line("[%s] <- %s", role, buf);
    if (strncmp(buf, "OK", 2) != 0) {
        return 1;
    }

    for (int i = 1; i <= rounds; i++) {
        send_line(fd, "PING");
        if (recv_line(fd, buf, sizeof(buf)) <= 0) {
            log_line("[%s] 连接已断开", role);
            return 1;
        }
        log_line("[%s] PING #%d <- %s", role, i, buf);

        send_line(fd, "ECHO hello-from-%s-%d", role, i);
        if (recv_line(fd, buf, sizeof(buf)) <= 0) {
            log_line("[%s] 连接已断开", role);
            return 1;
        }
        log_line("[%s] ECHO #%d <- %s", role, i, buf);
        sleep(1);
    }

    send_line(fd, "QUIT");
    recv_line(fd, buf, sizeof(buf));
    log_line("[%s] <- %s", role, buf[0] ? buf : "(closed)");
    return 0;
}

static int run_interactive_session(int fd, const char *role)
{
    char buf[BUF_SIZE];
    char line[BUF_SIZE];

    if (send_line(fd, "HELLO %s", role) < 0) {
        return 1;
    }
    if (recv_line(fd, buf, sizeof(buf)) <= 0) {
        log_line("[%s] 未收到 HELLO 应答", role);
        return 1;
    }
    log_line("[%s] <- %s", role, buf);
    if (strncmp(buf, "OK", 2) != 0) {
        return 1;
    }

    fprintf(stdout,
            "\n[%s] 已连接。可输入命令: PING / ECHO <text> / QUIT\n"
            "后续步骤可在此发送 GPIO / I2C / SPI 命令。\n\n",
            role);
    fflush(stdout);

    while (1) {
        fprintf(stdout, "[%s]> ", role);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) {
            continue;
        }

        if (send_line(fd, "%s", line) < 0) {
            log_line("[%s] 发送失败", role);
            return 1;
        }
        if (recv_line(fd, buf, sizeof(buf)) <= 0) {
            log_line("[%s] 服务器已关闭连接", role);
            return 1;
        }
        log_line("[%s] <- %s", role, buf);

        if (strcasecmp(line, "QUIT") == 0) {
            break;
        }
    }
    return 0;
}

static int run_one_client(const char *ip, int port, const char *role,
                          int auto_mode, int rounds)
{
    log_line("[%s] 正在连接 %s:%d ...", role, ip, port);
    int fd = connect_server(ip, port);
    if (fd < 0) {
        return 1;
    }
    log_line("[%s] 已建立 TCP 连接", role);

    int rc = auto_mode ? run_auto_session(fd, role, rounds)
                       : run_interactive_session(fd, role);
    close(fd);
    log_line("[%s] 已退出", role);
    return rc;
}

static int run_all_clients(const char *ip, int port, int auto_mode, int rounds)
{
    const char *roles[MAX_CLIENTS] = { ROLE_GPIO, ROLE_I2C, ROLE_SPI };
    pid_t pids[MAX_CLIENTS];
    int fail = 0;

    if (!auto_mode) {
        log_line("--all 默认使用 --auto，避免三个进程抢同一个键盘");
        auto_mode = 1;
    }

    log_line("同时拉起 3 个 Client: GPIO / I2C / SPI");

    for (int i = 0; i < MAX_CLIENTS; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            int rc = run_one_client(ip, port, roles[i], auto_mode, rounds);
            _exit(rc);
        }
        pids[i] = pid;
        usleep(150 * 1000);
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        int status = 0;
        if (waitpid(pids[i], &status, 0) < 0) {
            fail = 1;
            continue;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fail = 1;
        }
    }
    return fail;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int port = DEFAULT_PORT;
    int idx = 2;
    int all = 0;
    int auto_mode = 0;
    int rounds = 3;
    const char *role = NULL;

    if (idx < argc && argv[idx][0] >= '0' && argv[idx][0] <= '9') {
        port = atoi(argv[idx]);
        if (port <= 0 || port > 65535) {
            print_usage(argv[0]);
            return 1;
        }
        idx++;
    }

    if (idx >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[idx], "--all") == 0) {
        all = 1;
        idx++;
    } else {
        role = argv[idx];
        if (parse_role(role) == ROLE_UNKNOWN) {
            fprintf(stderr, "未知角色: %s（必须是 GPIO / I2C / SPI）\n", role);
            return 1;
        }
        idx++;
    }

    if (idx < argc && strcmp(argv[idx], "--auto") == 0) {
        auto_mode = 1;
        idx++;
        if (idx < argc) {
            rounds = atoi(argv[idx]);
            if (rounds <= 0) {
                rounds = 3;
            }
        }
    }

    if (all) {
        return run_all_clients(ip, port, auto_mode, rounds);
    }
    return run_one_client(ip, port, role, auto_mode, rounds);
}
