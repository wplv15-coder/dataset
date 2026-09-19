/*
 * 学习板上的 TCP Server。
 *
 * 步骤 1：监听端口，接受 Ubuntu 虚拟机上的 Client 连接。
 * 步骤 2：每接受一个 Client，就创建一个独立 pthread 专门跟它通信。
 *
 * 三个 Client 同时连上来时，进程里会同时存在三条工作线程，互不阻塞。
 *
 * 编译（板子上或交叉编译）：
 *   make server
 * 运行：
 *   ./server [port]
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol.h"

typedef struct {
    int sockfd;
    int client_id;
    struct sockaddr_in addr;
} client_ctx_t;

static volatile sig_atomic_t g_running = 1;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_listen_fd = -1;

static void log_msg(const char *fmt, ...)
{
    va_list ap;
    pthread_mutex_lock(&g_log_lock);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    pthread_mutex_unlock(&g_log_lock);
}

static void on_signal(int sig)
{
    (void)sig;
    g_running = 0;
    if (g_listen_fd >= 0) {
        close(g_listen_fd);
        g_listen_fd = -1;
    }
}

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

/* 按行读取，兼容 "CMD\n" 和 "CMD\r\n"。 */
static int recv_line(int fd, char *out, size_t cap)
{
    size_t used = 0;

    while (used + 1 < cap) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0) {
            return 0; /* peer closed */
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
    return used > 0 ? (int)used : (used == 0 ? 1 : 0);
}

/*
 * 第 3~5 步会在这里真正操作 /dev 设备。
 * 当前先返回占位应答，保证步骤 1、2 的通信链路已经打通。
 */
static void handle_gpio_cmd(int fd, const char *args)
{
    if (args == NULL || args[0] == '\0') {
        send_line(fd, "ERR GPIO need ON|OFF|TOGGLE|READ");
        return;
    }
    send_line(fd, "OK GPIO stub cmd=%s (wire /dev driver in step 3)", args);
}

static void handle_i2c_cmd(int fd, const char *args)
{
    if (args == NULL || args[0] == '\0') {
        send_line(fd, "ERR I2C need READ <reg> or WRITE <reg> <val>");
        return;
    }
    send_line(fd, "OK I2C stub cmd=%s (wire /dev/i2c-* in step 4)", args);
}

static void handle_spi_cmd(int fd, const char *args)
{
    if (args == NULL || args[0] == '\0') {
        send_line(fd, "ERR SPI need READ or WRITE <val>");
        return;
    }
    send_line(fd, "OK SPI stub cmd=%s (wire /dev/spidev* in step 5)", args);
}

static void dispatch_command(int fd, int client_id, client_role_t role,
                             const char *line)
{
    char cmd[MAX_LINE];
    const char *args;
    char *space;

    snprintf(cmd, sizeof(cmd), "%s", line);
    space = strchr(cmd, ' ');
    if (space) {
        *space = '\0';
        args = space + 1;
        while (*args == ' ') {
            args++;
        }
    } else {
        args = "";
    }

    if (strcasecmp(cmd, "PING") == 0) {
        send_line(fd, "OK PONG client=%d role=%s thread=%lu",
                  client_id, role_name(role),
                  (unsigned long)pthread_self());
        return;
    }

    if (strcasecmp(cmd, "ECHO") == 0) {
        send_line(fd, "OK ECHO client=%d %s", client_id,
                  args[0] ? args : "");
        return;
    }

    if (strcasecmp(cmd, "QUIT") == 0) {
        send_line(fd, "BYE");
        return;
    }

    if (strcasecmp(cmd, "GPIO") == 0) {
        if (role != ROLE_GPIO) {
            send_line(fd, "ERR this thread is %s, not GPIO", role_name(role));
            return;
        }
        handle_gpio_cmd(fd, args);
        return;
    }

    if (strcasecmp(cmd, "I2C") == 0) {
        if (role != ROLE_I2C) {
            send_line(fd, "ERR this thread is %s, not I2C", role_name(role));
            return;
        }
        handle_i2c_cmd(fd, args);
        return;
    }

    if (strcasecmp(cmd, "SPI") == 0) {
        if (role != ROLE_SPI) {
            send_line(fd, "ERR this thread is %s, not SPI", role_name(role));
            return;
        }
        handle_spi_cmd(fd, args);
        return;
    }

    send_line(fd, "ERR unknown command: %s", cmd);
}

static void *client_thread(void *arg)
{
    client_ctx_t *ctx = arg;
    char ip[INET_ADDRSTRLEN];
    char line[MAX_LINE];
    client_role_t role = ROLE_UNKNOWN;
    unsigned long tid = (unsigned long)pthread_self();

    inet_ntop(AF_INET, &ctx->addr.sin_addr, ip, sizeof(ip));
    log_msg("[thread %lu] client #%d %s:%u connected, waiting HELLO\n",
            tid, ctx->client_id, ip, ntohs(ctx->addr.sin_port));

    while (g_running) {
        int n = recv_line(ctx->sockfd, line, sizeof(line));
        if (n <= 0) {
            break;
        }

        log_msg("[thread %lu] client #%d recv: %s\n",
                tid, ctx->client_id, line);

        if (role == ROLE_UNKNOWN) {
            char hello[8];
            char role_str[16];

            if (sscanf(line, "%7s %15s", hello, role_str) != 2 ||
                strcasecmp(hello, "HELLO") != 0) {
                send_line(ctx->sockfd, "ERR first command must be: HELLO GPIO|I2C|SPI");
                continue;
            }

            role = parse_role(role_str);
            if (role == ROLE_UNKNOWN) {
                send_line(ctx->sockfd, "ERR unknown role: %s", role_str);
                continue;
            }

            send_line(ctx->sockfd, "OK HELLO role=%s client=%d thread=%lu",
                      role_name(role), ctx->client_id, tid);
#ifdef __linux__
            {
                char tname[16];
                snprintf(tname, sizeof(tname), "%s-%d", role_name(role), ctx->client_id);
                pthread_setname_np(pthread_self(), tname);
            }
#endif
            log_msg("[thread %lu] client #%d registered as %s\n",
                    tid, ctx->client_id, role_name(role));
            continue;
        }

        dispatch_command(ctx->sockfd, ctx->client_id, role, line);
        if (strncasecmp(line, "QUIT", 4) == 0) {
            break;
        }
    }

    log_msg("[thread %lu] client #%d %s:%u disconnected\n",
            tid, ctx->client_id, ip, ntohs(ctx->addr.sin_port));
    close(ctx->sockfd);
    free(ctx);
    return NULL;
}

static int create_listen_socket(int port)
{
    int fd;
    int yes = 1;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, BACKLOG) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

int main(int argc, char **argv)
{
    int port = DEFAULT_PORT;
    int client_id = 0;
    struct sigaction sa;

    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "usage: %s [port]\n", argv[0]);
            return 1;
        }
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    g_listen_fd = create_listen_socket(port);
    if (g_listen_fd < 0) {
        return 1;
    }

    log_msg("[server] listening on 0.0.0.0:%d, waiting for 3 clients...\n", port);

    while (g_running) {
        client_ctx_t *ctx;
        pthread_t tid;
        socklen_t addrlen;
        int conn;

        ctx = calloc(1, sizeof(*ctx));
        if (!ctx) {
            perror("calloc");
            break;
        }

        addrlen = sizeof(ctx->addr);
        conn = accept(g_listen_fd, (struct sockaddr *)&ctx->addr, &addrlen);
        if (conn < 0) {
            free(ctx);
            if (!g_running || errno == EINTR || errno == EBADF) {
                break;
            }
            perror("accept");
            continue;
        }

        ctx->sockfd = conn;
        ctx->client_id = ++client_id;

        if (pthread_create(&tid, NULL, client_thread, ctx) != 0) {
            perror("pthread_create");
            close(conn);
            free(ctx);
            continue;
        }
        pthread_detach(tid);
        log_msg("[server] spawned worker thread %lu for client #%d\n",
                (unsigned long)tid, ctx->client_id);
    }

    if (g_listen_fd >= 0) {
        close(g_listen_fd);
    }
    log_msg("[server] shutdown\n");
    return 0;
}
