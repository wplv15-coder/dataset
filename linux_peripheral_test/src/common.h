#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <sys/types.h>

void log_line(const char *fmt, ...);
ssize_t send_line(int fd, const char *fmt, ...);
ssize_t recv_line(int fd, char *buf, size_t size);
int parse_role(const char *s);
const char *role_name(int role);

enum {
    ROLE_UNKNOWN = 0,
    ROLE_ID_GPIO = 1,
    ROLE_ID_I2C  = 2,
    ROLE_ID_SPI  = 3
};

#endif /* COMMON_H */
