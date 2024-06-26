参考 ps-lite 实现的轻量级参数服务器。
仅供学习使用。

---
## 编译与测试

**编译 - windows**

```
cd build
cmake .. -Wno-dev
make
```

> 暂不支持在 WSL 编译。
理论上除了通过 vcpkg 管理的 ZeroMQ、protobuf 依赖外，没有跨平台的问题，简单修改 CMakeLists 就可在其它平台编译运行。
（此外非 Windows 要在 CMakeLists 设置 ON_WINDOWS 为 0，与 -DWSL 逻辑类似。它会在 NetworkUtils.h 中使用）

**单元测试**

在`build`下运行`make test`或`ctest`。

---
## 使用
**本地运行**

使用`local.py`在本地启动指定数量的节点和任务：
```
usage: local.py [-h] -ns NS -nw NW -exec EXEC [-lr] [-lr_normal] [-multi_customer] [-verbose VERBOSE]

options:
  -h, --help        show this help message and exit
  -ns NS            number of servers
  -nw NW            number of workers
  -exec EXEC        the program to be run
  -lr               whether to run lr_ps test
  -lr_normal        whether to run lr_normal test
  -multi_customer   whether to run all workers in one process. default: false
  -verbose VERBOSE  log level. default: 1
```

以 ./tests 下测试代码为例：

```
cd tests

# 测试系统能否通信
python .\local.py -ns 1 -nw 1 -exec .\exe\test_connection.exe -verbose=0

# 测试 SimpleApp 能否多节点工作
python .\local.py -ns=2 -nw=5 -exec='.\exe\test_simple_app.exe' -verbose=0

# 测试 KVApp 的多节点工作
python .\local.py -ns=2 -nw=5 -exec='.\exe\test_kv_app.exe' -verbose=0

# 测试在一个进程中启动多个 worker 节点（即多个 customer）
python .\local.py -ns=2 -nw=2 -exec='.\exe\test_kv_app_multi_workers.exe' -multi_customer

# 测试多节点工作的执行时间
python .\local.py -ns=2 -nw=2 -exec='.\exe\test_kv_app_benchmark.exe'

# 任意测试
python .\local.py -ns=2 -nw=3 -exec='.\exe\test_my.exe' -multi_customer
```

> 辅助命令：
> 结束程序：`taskkill /PID <PID> /F`
> 查找绑定或监听某端口的程序：`netstat -ano | findstr ":8000"`

---
## 配置变量

必须设置的变量：

- `PS_NUM_WORKER`：worker 数量。
- `PS_NUM_SERVER`：server 数量。
- `PS_ROLE`：当前节点的身份。可选：`worker`, `server`, `scheduler`。
- `PS_SCHEDULER_URI`：scheduler 的 IP 或 host。
- `PS_SCHEDULER_PORT`：scheduler 所在端口。

可选变量：

- `PS_INTERFACE`：the network interface a node should use. in default choose automatically
- `PS_LOCAL`：runs in local machines if set any value, no network is needed.
- `PS_WATER_MARK`	: limit on the maximum number of outstanding messages
- `PS_VAN_TYPE`：Van 的类型，即底层通信方式。可选：`ibverbs`(RDMA), `zmq`(TCP，默认), `p3`(TCP with [priority based parameter propagation](https://anandj.in/wp-content/uploads/sysml.pdf))。
- `PS_RESEND_TIMEOUT`：消息超时时间（重发间隔）。如果设置，则如果消息在指定时间后未收到确认，则进行重发。默认为 0，即不重发。单位为毫秒。
- `PS_HEARTBEAT_TIMEOUT`：心跳超时时间。用途 TODO。默认为 0，即不会超时。单位为秒。
- `PS_HEARTBEAT_INTERVAL`：心跳间隔时间。节点每隔一次该时间，就向 scheduler 发送心跳信息。默认为 0，即不会发送。单位为毫秒。
- `PS_DROP_RATE`：收到消息后将其丢弃的概率。用于调试。
- `PS_VERBOSE`: 日志等级。默认为 0。

ps-lite 中存在但是未使用：

- `PS_NODE_HOST`：节点 IP？
- `PS_PORT`：节点默认使用的端口？


---
## 其它

**Message::Meta**

如果要更新 Message::Meta 的成员，需要修改 meta.proto 并使用它生成对应源文件：`protoc -I=. --cpp_out=. ./meta.proto`

注意 protoc 的版本要与依赖的 protobuf 库版本一致，比如：3.21.x 库对应 protoc 21.x。


