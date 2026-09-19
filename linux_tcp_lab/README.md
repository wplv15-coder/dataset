# Linux 应用外设驱动测试：步骤 1、2

作业要求：

1. 学习板上做 TCP Server，PC 虚拟机 Ubuntu 上做 Client。
2. 三个 Client 连接 Server，Server 为每个连接创建一条线程通信。

本目录给出可直接编译运行的实现。GPIO / I2C / SPI 驱动是第 3~5 步，当前只留了命令入口。

## 整体结构

```
Ubuntu 虚拟机                         学习板
┌─────────────┐                      ┌──────────────────────────┐
│ client GPIO │──┐                   │  accept 循环             │
│ client I2C  │──┼── TCP :8888 ───► │   ├─ pthread: GPIO 通信  │
│ client SPI  │──┘                   │   ├─ pthread: I2C 通信   │
└─────────────┘                      │   └─ pthread: SPI 通信   │
                                     └──────────────────────────┘
```

关键点：`accept()` 放在主线程，每来一个连接就 `pthread_create()` 一条工作线程。三个 Client 同时在线时，板上会有三条独立收发循环，互不阻塞。

## 编译

在 Ubuntu 上编 Client（本机也可先把 Server 编出来做联调）：

```bash
cd linux_tcp_lab
make
```

交叉编译到 ARM 学习板（工具链按你的板子改）：

```bash
make CC=arm-linux-gnueabihf-gcc
```

把 `bin/server` 拷到板子上，`bin/client` 留在 Ubuntu。

## 步骤 1：先打通一条 TCP 连接

1. 板子和虚拟机要在同一网段。虚拟机网卡建议用桥接，不要只用 NAT。
2. 板子上查 IP：

```bash
ifconfig
# 或
ip addr
```

3. Ubuntu 先 ping 通板子：

```bash
ping 192.168.1.10
```

4. 板子启动 Server：

```bash
./server 8888
```

5. Ubuntu 启动一个 Client：

```bash
./client 192.168.1.10 8888 GPIO
```

看到 `connected` 后输入：

```
PING
ECHO hello
QUIT
```

Server 端应打印该 Client 的线程号和收到的命令。这一步只验证 socket 能通。

## 步骤 2：三个 Client 对应三条线程

开三个终端，同时连：

```bash
./client 192.168.1.10 8888 GPIO --demo
./client 192.168.1.10 8888 I2C --demo
./client 192.168.1.10 8888 SPI --demo
```

或本机自测（不插板子也能验证线程模型）：

```bash
make test
```

Server 日志里应出现类似：

```
[server] spawned worker thread ... for client #1
[thread ...] client #1 registered as GPIO
[server] spawned worker thread ... for client #2
[thread ...] client #2 registered as I2C
[server] spawned worker thread ... for client #3
[thread ...] client #3 registered as SPI
```

三个 `thread` 地址不同，说明是三条线程在分别通信，而不是一个循环轮询三个连接。

## 协议

连接后第一条必须是角色登记：

```
HELLO GPIO
HELLO I2C
HELLO SPI
```

之后可发：

| 命令 | 含义 |
| --- | --- |
| `PING` | 心跳，回 `PONG` 和线程号 |
| `ECHO <text>` | 回显，方便看三路是否并发 |
| `GPIO ON\|OFF\|TOGGLE\|READ` | 第 3 步接 LED 驱动 |
| `I2C READ <reg>` / `I2C WRITE <reg> <val>` | 第 4 步接 I2C 驱动 |
| `SPI READ` / `SPI WRITE <val>` | 第 5 步接 SPI 驱动 |
| `QUIT` | 断开 |

角色和命令不匹配会被拒绝，例如 GPIO 线程收到 `I2C READ` 会返回错误。这样后面三个 Client 不会抢同一路外设。

## 代码对应关系

| 文件 | 作用 |
| --- | --- |
| `src/server.c` | 板上 Server：`socket/bind/listen/accept` + 每连接一个 `pthread` |
| `src/client.c` | Ubuntu Client：`connect` 后按行发命令 |
| `include/protocol.h` | 端口、角色、命令约定 |
| `scripts/run_three_clients.sh` | 本机三 Client 联调 |

Server 工作线程入口是 `client_thread()`：先等 `HELLO`，再进入 `dispatch_command()`。第 3~5 步只需要改 `handle_gpio_cmd()` / `handle_i2c_cmd()` / `handle_spi_cmd()`，打开对应 `/dev` 节点即可，不用再改网络部分。

## 常见问题

- **连不上**：先 ping。虚拟机如果是 NAT，Ubuntu 能访问板子，但板子不一定能被访问；改桥接或做端口转发。
- **bind: Address already in use**：换端口 `./server 9999`，Client 跟着改。
- **板子没有 pthread**：链接时加 `-pthread`（Makefile 已加）。
- **防火墙**：Ubuntu `sudo ufw status`，板子侧一般不用开入站限制，但 PC 防火墙不要拦 8888。
