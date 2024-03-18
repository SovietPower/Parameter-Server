
# 编译与测试

**编译 - windows**

```
cmake .. -Wno-dev
make
```


**编译 - WSL**

```
cd /mnt/e/Study/ML/my-ps/build_wsl
cmake .. -DWSL=1 -Wno-dev
make
```

> wsl 下编译，需要加 -DWSL=1。
注意在不同平台编译时，需要手动修改 NetworkUtils.h 中的宏。

**测试**

在`build`下运行`make test`或`ctest`。

---
# 使用
**本地运行**

使用`local.py`在本地启动指定数量的节点和任务：
```
local.py [-h] -ns NS -nw NW -exec EXEC [-verbose VERBOSE] [-multi_customer]

options:
  -h, --help        show this help message and exit
  -ns NS            number of servers
  -nw NW            number of workers
  -exec EXEC        the program to be run
  -verbose VERBOSE  log level
  -multi_customer   whether to run all workers in one process. false in default
```

以 tests 下代码为例：

```
# 测试系统能否通信
python .\local.py -ns 1 -nw 1 -exec .\exe\test_connection.exe

# 测试 SimpleApp 能否多节点工作
python .\local.py -ns=2 -nw=5 -exec='.\exe\test_simple_app.exe'

# 测试 KVApp 能否多节点工作
python .\local.py -ns=2 -nw=5 -exec='.\exe\test_kv_app.exe' -verbose=1

# 测试在一个进程中启动多个 worker 节点（即多个 customer）
python .\local.py -ns=2 -nw=2 -exec='.\exe\test_kv_app_multi_workers.exe' -verbose=1 -multi_customer

# 测试执行时间
python .\local.py -ns=2 -nw=2 -exec='.\exe\test_kv_app_benchmark.exe' -verbose=1

# 任意测试
python .\local.py -ns=2 -nw=3 -exec='.\exe\test_my.exe' -verbose=1 -multi_customer
```


> 辅助：
结束程序：`taskkill /PID <PID> /F`
查找绑定或监听某端口的程序：`netstat -ano | findstr ":8000"`

---
# 配置变量

必须设置的变量：

- `PS_NUM_WORKER` : worker 数量。
- `PS_NUM_SERVER` : server 数量。
- `PS_ROLE` : 当前节点的身份。可选：`worker`, `server`, `scheduler`。
- `PS_SCHEDULER_URI` : scheduler 的 IP 或 host。
- `PS_SCHEDULER_PORT` : scheduler 所在端口。

可选变量：

- `PS_INTERFACE` : the network interface a node should use. in default choose automatically
- `PS_LOCAL` : runs in local machines if set any value, no network is needed.
- `PS_WATER_MARK`	: limit on the maximum number of outstanding messages
- `PS_VAN_TYPE` : Van 的类型，即底层通信方式。可选：`ibverbs`(RDMA), `zmq`(TCP，默认), `p3`(TCP with [priority based parameter propagation](https://anandj.in/wp-content/uploads/sysml.pdf))。
- `PS_RESEND_TIMEOUT`：消息超时时间（重发间隔）。如果设置，则如果消息在指定时间后未收到确认，则进行重发。默认为 0，即不重发。单位为毫秒。
- `PS_HEARTBEAT_TIMEOUT`：心跳超时时间。用途 TODO。默认为 0，即不会超时。单位为秒。
- `PS_HEARTBEAT_INTERVAL`：心跳间隔时间。节点每隔一次该时间，就向 scheduler 发送心跳信息。默认为 0，即不会发送。单位为毫秒。
- `PS_DROP_RATE`：收到消息后将其丢弃的概率。用于调试。
- `PS_VERBOSE`: 日志等级。默认为 0。

不确定：

- `PS_NODE_HOST`：节点 IP？
- `PS_PORT`：节点默认使用的端口？


---
# 其它

**更新 Message::Meta 结构**
需要修改并根据 meta.proto 生成对应源文件：`protoc -I=. --cpp_out=. ./meta.proto`
注意 protoc 的版本要与依赖的 protobuf 库版本一致，比如：3.21.x 库对应 protoc 21.x。


