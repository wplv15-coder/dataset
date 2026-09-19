#define _GNU_SOURCE

#include "common.h"
#include "protocol.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

void log_line(const char *fmt, ...)
{
    char timebuf[32];
    time_t now = time(NULL);
    struct tm tm_now;

    localtime_r(&now, &tm_now);
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm_now);

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s] ", timebuf);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

ssize_t send_line(int fd, const char *fmt, ...)
{
    char buf[BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);

    if (n < 0) {
        return -1;
    }
    if (n >= (int)sizeof(buf) - 1) {
        n = (int)sizeof(buf) - 2;
    }
    buf[n++] = '\n';

    size_t sent = 0;
    while (sent < (size_t)n) {
        ssize_t r = send(fd, buf + sent, (size_t)n - sent, 0);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (r == 0) {
            break;
        }
        sent += (size_t)r;
    }
    return (ssize_t)sent;
}

ssize_t recv_line(int fd, char *buf, size_t size)
{
    size_t n = 0;

    if (size == 0) {
        return -1;
    }

    while (n + 1 < size) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) {
            buf[n] = '\0';
            return n == 0 ? 0 : (ssize_t)n;
        }
        if (r < 0) {
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
        buf[n++] = c;
    }

    buf[n] = '\0';
    return (ssize_t)n;
}

int parse_role(const char *s)
{
    if (s == NULL) {
        return ROLE_UNKNOWN;
    }
    if (strcasecmp(s, ROLE_GPIO) == 0) {
        return ROLE_ID_GPIO;
    }
    if (strcasecmp(s, ROLE_I2C) == 0) {
        return ROLE_ID_I2C;
    }
    if (strcasecmp(s, ROLE_SPI) == 0) {
        return ROLE_ID_SPI;
    }
    return ROLE_UNKNOWN;
}

const char *role_name(int role)
{
    switch (role) {
    case ROLE_ID_GPIO:
        return ROLE_GPIO;
    case ROLE_ID_I2C:
        return ROLE_I2C;
    case ROLE_ID_SPI:
        return ROLE_SPI;
    default:
        return "UNKNOWN";
    }
}
