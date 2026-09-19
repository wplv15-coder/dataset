#define _GNU_SOURCE

#include "common.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

typedef struct {
    int used;
    int sockfd;
    int slot;
    int role;
    pthread_t tid;
    char ip[INET_ADDRSTRLEN];
    int port;
} client_slot_t;

static client_slot_t g_slots[MAX_CLIENTS];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_listen_fd = -1;
static volatile sig_atomic_t g_running = 1;

static void on_signal(int signo)
{
    (void)signo;
    g_running = 0;
    if (g_listen_fd >= 0) {
        close(g_listen_fd);
        g_listen_fd = -1;
    }
}

static int alloc_slot(void)
{
    int slot = -1;

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_slots[i].used) {
            g_slots[i].used = 1;
            g_slots[i].sockfd = -1;
            g_slots[i].slot = i;
            g_slots[i].role = ROLE_UNKNOWN;
            slot = i;
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return slot;
}

static void free_slot(int slot)
{
    if (slot < 0 || slot >= MAX_CLIENTS) {
        return;
    }
    pthread_mutex_lock(&g_lock);
    if (g_slots[slot].sockfd >= 0) {
        close(g_slots[slot].sockfd);
        g_slots[slot].sockfd = -1;
    }
    g_slots[slot].used = 0;
    g_slots[slot].role = ROLE_UNKNOWN;
    pthread_mutex_unlock(&g_lock);
}

static int occupied_count(void)
{
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_slots[i].used) {
            n++;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return n;
}

static void handle_reserved_peripheral(int fd, int slot, int role,
                                       const char *cmd, const char *rest)
{
    const char *need = role_name(role);
    (void)rest;
    send_line(fd,
              "ERR NOT_IMPLEMENTED %s 命令已预留给后续外设步骤，当前只完成 TCP 多线程通讯",
              cmd);
    log_line("[线程 %d][%s] 收到预留命令 %s，步骤1-2 暂不执行外设操作",
             slot, need, cmd);
}

static void *client_thread(void *arg)
{
    client_slot_t *slot = arg;
    int fd = slot->sockfd;
    int id = slot->slot;
    char buf[BUF_SIZE];
    char cmd[64];
    char arg1[64];
    int role = ROLE_UNKNOWN;
    int hello_done = 0;

    log_line("[线程 %d] 启动 pthread=%lu，对端 %s:%d，等待 HELLO",
             id, (unsigned long)pthread_self(), slot->ip, slot->port);

    while (g_running) {
        ssize_t n = recv_line(fd, buf, sizeof(buf));
        if (n < 0) {
            log_line("[线程 %d] recv 失败: %s", id, strerror(errno));
            break;
        }
        if (n == 0) {
            log_line("[线程 %d] 客户端断开连接", id);
            break;
        }

        cmd[0] = '\0';
        arg1[0] = '\0';
        sscanf(buf, "%63s %63s", cmd, arg1);

        if (!hello_done) {
            if (strcasecmp(cmd, CMD_HELLO) != 0) {
                send_line(fd, "ERR NEED_HELLO 请先发送: HELLO GPIO|I2C|SPI");
                log_line("[线程 %d] 拒绝：第一条消息必须是 HELLO，收到 \"%s\"", id, buf);
                continue;
            }
            role = parse_role(arg1);
            if (role == ROLE_UNKNOWN) {
                send_line(fd, "ERR BAD_ROLE 角色必须是 GPIO / I2C / SPI");
                log_line("[线程 %d] 非法角色 \"%s\"", id, arg1);
                continue;
            }
            slot->role = role;
            hello_done = 1;
            send_line(fd, "OK HELLO role=%s slot=%d thread=%d",
                      role_name(role), id, id);
            log_line("[线程 %d] 注册成功：Client=%s，后续通讯由本线程独占处理",
                     id, role_name(role));
            continue;
        }

        log_line("[线程 %d][%s] 收到: %s", id, role_name(role), buf);

        if (strcasecmp(cmd, CMD_PING) == 0) {
            send_line(fd, "OK PONG slot=%d thread=%d role=%s",
                      id, id, role_name(role));
        } else if (strcasecmp(cmd, CMD_ECHO) == 0) {
            const char *text = buf;
            while (*text && *text != ' ') {
                text++;
            }
            while (*text == ' ') {
                text++;
            }
            send_line(fd, "OK ECHO %s", text[0] ? text : "");
        } else if (strcasecmp(cmd, CMD_GPIO) == 0 ||
                   strcasecmp(cmd, CMD_I2C) == 0 ||
                   strcasecmp(cmd, CMD_SPI) == 0) {
            handle_reserved_peripheral(fd, id, role, cmd, arg1);
        } else if (strcasecmp(cmd, CMD_QUIT) == 0) {
            send_line(fd, "OK BYE slot=%d", id);
            log_line("[线程 %d][%s] 客户端请求退出", id, role_name(role));
            break;
        } else {
            send_line(fd, "ERR UNKNOWN_CMD %s", cmd);
        }
    }

    log_line("[线程 %d] 退出，当前在线客户端 %d/3", id, occupied_count() - 1);
    close(fd);
    slot->sockfd = -1;
    free_slot(id);
    return NULL;
}

static int create_listen_socket(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, MAX_CLIENTS + 1) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char **argv)
{
    int port = DEFAULT_PORT;

    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "用法: %s [port]\n默认端口 %d\n", argv[0], DEFAULT_PORT);
            return 1;
        }
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    g_listen_fd = create_listen_socket(port);
    if (g_listen_fd < 0) {
        return 1;
    }

    log_line("====================================================");
    log_line(" TCP Server 已启动（学习板端）");
    log_line(" 监听 0.0.0.0:%d ，最多 %d 个 Client", port, MAX_CLIENTS);
    log_line(" 每个 Client 对应 1 个独立工作线程");
    log_line("====================================================");

    while (g_running) {
        struct sockaddr_in cli;
        socklen_t len = sizeof(cli);
        int cfd = accept(g_listen_fd, (struct sockaddr *)&cli, &len);
        if (cfd < 0) {
            if (!g_running || errno == EINTR || errno == EBADF) {
                break;
            }
            perror("accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
        int cport = ntohs(cli.sin_port);

        int slot = alloc_slot();
        if (slot < 0) {
            log_line("[主线程] 拒绝 %s:%d ，已有 %d 个 Client 在线",
                     ip, cport, MAX_CLIENTS);
            send_line(cfd, "ERR BUSY max_clients=%d", MAX_CLIENTS);
            close(cfd);
            continue;
        }

        g_slots[slot].sockfd = cfd;
        g_slots[slot].port = cport;
        snprintf(g_slots[slot].ip, sizeof(g_slots[slot].ip), "%s", ip);

        log_line("[主线程] 接受 %s:%d -> 分配槽位 %d，创建通讯线程（在线 %d/3）",
                 ip, cport, slot, occupied_count());

        int err = pthread_create(&g_slots[slot].tid, NULL, client_thread, &g_slots[slot]);
        if (err != 0) {
            log_line("[主线程] pthread_create 失败: %s", strerror(err));
            g_slots[slot].sockfd = -1;
            close(cfd);
            free_slot(slot);
            continue;
        }
        pthread_detach(g_slots[slot].tid);
    }

    log_line("Server 正在退出...");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_mutex_lock(&g_lock);
        int fd = g_slots[i].used ? g_slots[i].sockfd : -1;
        pthread_mutex_unlock(&g_lock);
        if (fd >= 0) {
            shutdown(fd, SHUT_RDWR);
        }
    }
    usleep(200 * 1000);
    return 0;
}
