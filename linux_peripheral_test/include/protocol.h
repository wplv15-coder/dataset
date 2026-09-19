#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
 * 简易文本协议（以 '\n' 结束一行）
 *
 * 客户端 -> 服务器:
 *   HELLO <GPIO|I2C|SPI>     注册身份，必须是第一条消息
 *   PING                     心跳 / 连通性测试
 *   ECHO <text>              回显测试
 *   GPIO ON|OFF              预留：步骤3 小灯控制
 *   I2C READ|WRITE ...       预留：步骤4
 *   SPI READ|WRITE ...       预留：步骤5
 *   QUIT                     主动断开
 *
 * 服务器 -> 客户端:
 *   OK HELLO role=<ROLE> slot=<n> thread=<n>
 *   OK PONG slot=<n> thread=<n>
 *   OK ECHO <text>
 *   ERR <reason>
 */

#define DEFAULT_PORT      8888
#define MAX_CLIENTS       3
#define BUF_SIZE          1024
#define ROLE_NAME_LEN     16

#define CMD_HELLO         "HELLO"
#define CMD_PING          "PING"
#define CMD_ECHO          "ECHO"
#define CMD_QUIT          "QUIT"
#define CMD_GPIO          "GPIO"
#define CMD_I2C           "I2C"
#define CMD_SPI           "SPI"

#define ROLE_GPIO         "GPIO"
#define ROLE_I2C          "I2C"
#define ROLE_SPI          "SPI"

#endif /* PROTOCOL_H */
