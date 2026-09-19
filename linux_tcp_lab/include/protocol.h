#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <strings.h>

/*
 * 学习板 TCP 实验共用协议。
 *
 * 第 1、2 步只需要完成：建连、身份登记、按行收发。
 * 第 3~5 步再把 GPIO / I2C / SPI 命令接到对应线程里。
 *
 * 文本行协议（每条命令以 '\n' 结束）：
 *
 *   Client -> Server
 *     HELLO GPIO|I2C|SPI     登记角色，必须是连接后的第一条命令
 *     PING                   心跳，验证线程在工作
 *     ECHO <text>            回显，用来观察三路并发
 *     GPIO ON|OFF|TOGGLE|READ
 *     I2C READ <reg>
 *     I2C WRITE <reg> <val>
 *     SPI READ
 *     SPI WRITE <val>
 *     QUIT                   主动断开
 *
 *   Server -> Client
 *     OK <payload>
 *     ERR <reason>
 *     BYE
 */

#define DEFAULT_PORT        8888
#define MAX_LINE            512
#define MAX_CLIENTS         16
#define BACKLOG             8
#define RECV_TIMEOUT_SEC    0   /* 0 表示不启用超时 */

typedef enum {
    ROLE_UNKNOWN = 0,
    ROLE_GPIO,
    ROLE_I2C,
    ROLE_SPI
} client_role_t;

static inline const char *role_name(client_role_t role)
{
    switch (role) {
    case ROLE_GPIO: return "GPIO";
    case ROLE_I2C:  return "I2C";
    case ROLE_SPI:  return "SPI";
    default:        return "UNKNOWN";
    }
}

static inline client_role_t parse_role(const char *s)
{
    if (s == NULL) {
        return ROLE_UNKNOWN;
    }
    if (strcasecmp(s, "GPIO") == 0) {
        return ROLE_GPIO;
    }
    if (strcasecmp(s, "I2C") == 0) {
        return ROLE_I2C;
    }
    if (strcasecmp(s, "SPI") == 0) {
        return ROLE_SPI;
    }
    return ROLE_UNKNOWN;
}

#endif /* PROTOCOL_H */
