# Linux 应用外设驱动测试（步骤 1-2）

对应作业：

1. 在学习板子上做 TCP 服务器 Server，在 PC 虚拟机 Ubuntu 中做客户端 Client。
2. 用三个 Client 连接 Server。Server 对应创建 3 个线程进行具体通讯。

步骤 3-5（GPIO / I2C / SPI 驱动）的命令入口已经预留，当前只完成网络与多线程通讯。

## 整体结构

```
Ubuntu 虚拟机                          学习板
┌─────────────┐                     ┌──────────────────────┐
│ Client GPIO ├──── TCP 连接 1 ────►│ 线程 0 处理 GPIO     │
│ Client I2C  ├──── TCP 连接 2 ────►│ 线程 1 处理 I2C      │
│ Client SPI  ├──── TCP 连接 3 ────►│ 线程 2 处理 SPI      │
└─────────────┘                     │ 主线程: accept()     │
                                    └──────────────────────┘
```

- Server 监听 `0.0.0.0:8888`（可用参数改端口）。
- 每来一个 Client，主线程 `accept()` 后 `pthread_create()` 一个工作线程。
- 最多同时 3 个 Client；第 4 个会被拒绝（`ERR BUSY`）。
- 每个 Client 第一条消息必须是 `HELLO GPIO|I2C|SPI`，用来标明身份。

## 编译

板子和 Ubuntu 都可以用 gcc（板子若已有 Linux + gcc，直接本地编译即可）：

```bash
cd linux_peripheral_test
make
```

得到：

- `build/server`  放到学习板运行
- `build/client`  在 Ubuntu 虚拟机运行

交叉编译示例（按你的工具链改前缀）：

```bash
make CC=arm-linux-gnueabihf-gcc
```

## 网络准备

1. 板子和虚拟机要在同一网段（或虚拟机网卡使用桥接，不要只用 NAT）。
2. 在板子上查 IP：

```bash
ifconfig
# 或
ip addr
```

假设板子 IP 是 `192.168.1.20`。

3. 先在 Ubuntu 上 ping 通板子：

```bash
ping 192.168.1.20
```

## 运行步骤 1：启动 Server（学习板）

```bash
./build/server          # 默认 8888
# 或
./build/server 8888
```

看到类似输出即成功：

```
TCP Server 已启动（学习板端）
监听 0.0.0.0:8888 ，最多 3 个 Client
```

## 运行步骤 2：三个 Client 连上来（Ubuntu）

开 3 个终端，分别执行：

```bash
./build/client 192.168.1.20 GPIO
./build/client 192.168.1.20 I2C
./build/client 192.168.1.20 SPI
```

交互命令：

```
PING
ECHO hello
QUIT
```

一键同时拉起 3 个 Client（自动发 PING/ECHO，适合课堂演示）：

```bash
./build/client 192.168.1.20 --all --auto
```

Server 侧应能看到：

- 主线程为 3 个连接分别分配槽位 0/1/2
- `[线程 0] 注册成功：Client=GPIO`
- `[线程 1] 注册成功：Client=I2C`
- `[线程 2] 注册成功：Client=SPI`
- 每个 Client 的 PING 都由对应线程打印和应答

这就对应作业第 2 点：**三个 Client 连接，Server 创建 3 个线程分别通讯**。

## 不接板子时的本机自测

在 Ubuntu 上也可以先验证程序逻辑：

```bash
cd linux_peripheral_test
make test
```

或手动：

```bash
./build/server 8888
# 另开一个终端
./build/client 127.0.0.1 --all --auto
```

## 协议（方便接步骤 3-5）

一行一条命令，以换行结束。

| 方向 | 内容 | 含义 |
|------|------|------|
| C→S | `HELLO GPIO` | Client1 身份 |
| C→S | `HELLO I2C` | Client2 身份 |
| C→S | `HELLO SPI` | Client3 身份 |
| C→S | `PING` | 连通测试 |
| C→S | `ECHO <text>` | 回显测试 |
| C→S | `GPIO ON` / `GPIO OFF` | 预留步骤3 |
| C→S | `I2C READ ...` | 预留步骤4 |
| C→S | `SPI WRITE ...` | 预留步骤5 |
| C→S | `QUIT` | 断开 |
| S→C | `OK ...` / `ERR ...` | 应答 |

当前 GPIO/I2C/SPI 命令会返回 `ERR NOT_IMPLEMENTED`，等写好外设驱动后再接到对应线程里。

## 常见问题

- **连不上**：先 ping；检查板子防火墙；确认虚拟机是桥接模式；确认 Server 已启动。
- **第 4 个 Client 被拒**：设计就是最多 3 路，对应 3 个外设 Client。
- **端口被占用**：`./build/server 9999`，Client 同样带端口号。

## 源码说明

| 文件 | 作用 |
|------|------|
| `src/server.c` | 板端 TCP Server：accept + 每连接一个 pthread |
| `src/client.c` | Ubuntu 端 Client，支持单独启动或 `--all` |
| `include/protocol.h` | 文本协议定义 |
| `src/common.c` | 收发一行、日志、角色解析 |
