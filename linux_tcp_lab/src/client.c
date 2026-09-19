/*
 * PC 虚拟机 Ubuntu 上的 TCP Client。
 *
 * 用法：
 *   ./client <server_ip> [port] [GPIO|I2C|SPI] [--demo]
 *
 * 交互模式：连上后从 stdin 读命令发给 Server。
 * --demo 模式：自动发几条 PING/ECHO，用来一次拉起三个 Client 做步骤 2 验证。
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol.h"

static int send_line(int fd, const char *fmt, ...)
{
    char buf[MAX_LINE];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    if (n < 0) {
        return -1;
    }
    if (n > (int)sizeof(buf) - 2) {
        n = (int)sizeof(buf) - 2;
    }
    buf[n++] = '\n';
    buf[n] = '\0';
    return (int)send(fd, buf, (size_t)n, 0);
}

static int recv_line(int fd, char *out, size_t cap)
{
    size_t used = 0;

    while (used + 1 < cap) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0) {
            return 0;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (c == '\n') {
            break;
        }
        if (c == '\r') {
            continue;
        }
        out[used++] = c;
    }
    out[used] = '\0';
    return used > 0 ? (int)used : 1;
}

static int connect_server(const char *ip, int port)
{
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid ip: %s\n", ip);
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

static int exchange(int fd, const char *cmd)
{
    char resp[MAX_LINE];
    int n;

    printf(">> %s\n", cmd);
    if (send_line(fd, "%s", cmd) < 0) {
        perror("send");
        return -1;
    }
    n = recv_line(fd, resp, sizeof(resp));
    if (n <= 0) {
        printf("<< (server closed)\n");
        return -1;
    }
    printf("<< %s\n", resp);
    return 0;
}

static int run_demo(int fd, const char *role)
{
    char hello[64];
    char echo[MAX_LINE];

    snprintf(hello, sizeof(hello), "HELLO %s", role);
    if (exchange(fd, hello) < 0) {
        return 1;
    }
    if (exchange(fd, "PING") < 0) {
        return 1;
    }

    snprintf(echo, sizeof(echo), "ECHO hello-from-%s-pid-%d",
             role, (int)getpid());
    if (exchange(fd, echo) < 0) {
        return 1;
    }

    if (strcasecmp(role, "GPIO") == 0) {
        exchange(fd, "GPIO ON");
    } else if (strcasecmp(role, "I2C") == 0) {
        exchange(fd, "I2C READ 0x00");
    } else if (strcasecmp(role, "SPI") == 0) {
        exchange(fd, "SPI WRITE 0x5A");
    }

    /* 稍等一会，方便三个 Client 同时在线时在 Server 日志里看到三条线程并存。 */
    usleep(300000);
    exchange(fd, "QUIT");
    return 0;
}

static int run_interactive(int fd, const char *hello)
{
    char line[MAX_LINE];

    if (hello && hello[0]) {
        if (exchange(fd, hello) < 0) {
            return 1;
        }
    }

    printf("connected. type commands (PING / ECHO text / QUIT), Ctrl-D to exit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) {
            continue;
        }
        if (exchange(fd, line) < 0) {
            return 1;
        }
        if (strcasecmp(line, "QUIT") == 0) {
            break;
        }
    }
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s <server_ip> [port] [GPIO|I2C|SPI] [--demo]\n"
            "  step 1: ./client 192.168.1.10\n"
            "  step 2: start three terminals:\n"
            "          ./client 192.168.1.10 8888 GPIO --demo\n"
            "          ./client 192.168.1.10 8888 I2C --demo\n"
            "          ./client 192.168.1.10 8888 SPI --demo\n",
            argv0);
}

int main(int argc, char **argv)
{
    const char *ip;
    int port = DEFAULT_PORT;
    const char *role = NULL;
    int demo = 0;
    int fd;
    char hello[64];
    int i;
    int rc;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    ip = argv[1];
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--demo") == 0) {
            demo = 1;
        } else if (argv[i][0] >= '0' && argv[i][0] <= '9') {
            port = atoi(argv[i]);
        } else {
            role = argv[i];
        }
    }

    if (port <= 0 || port > 65535) {
        usage(argv[0]);
        return 1;
    }

    fd = connect_server(ip, port);
    if (fd < 0) {
        return 1;
    }

    printf("[client] connected to %s:%d pid=%d\n", ip, port, (int)getpid());

    if (role) {
        snprintf(hello, sizeof(hello), "HELLO %s", role);
    } else {
        hello[0] = '\0';
    }

    if (demo) {
        if (!role) {
            role = "GPIO";
        }
        rc = run_demo(fd, role);
    } else {
        rc = run_interactive(fd, hello);
    }

    close(fd);
    return rc;
}
