
# 使用

**编译**

```
wsl
cd /mnt/e/Study/ML/my-ps/build_wsl
cmake .. -DWSL=1 -Wno-dev
make
```

如果源码有改动，需要重新 make；如果 CML 有改动，需要重新 cmake 生成 makefile（如果 vsc 有插件则会自动生成）。

> wsl 下编译，需要加 -DWSL=1。
注意在不同平台编译时，需要手动修改 NetworkUtils.h 中的宏。（似乎只能在 WSL 下编译它？）

**测试**

使用 GoogleTest 并启用 ctest：

- 在 vscode 的 cmake - 项目大纲 可看到创建的测试目标，可直接运行。
- 或在测试所在的项目构建目录（比如`build`或`build/subproj`）运行`ctest`，进行该目录下所有测试。
- 或在测试所在的项目构建目录，运行生成的 exe，手动、单个测试。


**TODO**
```
/usr/bin/ld: cannot find -lws2_32: No such file or directory
/usr/bin/ld: cannot find -ladvapi32: No such file or directory
/usr/bin/ld: cannot find -lrpcrt4: No such file or directory
/usr/bin/ld: cannot find -liphlpapi: No such file or directory
/usr/bin/ld: cannot find -llibzmq-static: No such file or directory
collect2: error: ld returned 1 exit status
make[2]: *** [CMakeFiles/SVectorTest.dir/build.make:201: SVectorTest] Error 1
make[1]: *** [CMakeFiles/Makefile2:924: CMakeFiles/SVectorTest.dir/all] Error 2
make: *** [Makefile:146: all] Error 2
```


# environment variables

The variables must be set for starting

- `PS_NUM_WORKER` : the number of workers
- `PS_NUM_SERVER` : the number of servers
- `PS_ROLE` : the role of the current node, can be `worker`, `server`, or `scheduler`
- `PS_SCHEDULER_URI` : the ip or hostname of the scheduler node
- `PS_SCHEDULER_PORT` : the port that the scheduler node is listening

additional variables:

- `PS_INTERFACE` : the network interface a node should use. in default choose automatically
- `PS_LOCAL` : runs in local machines if set any value, no network is needed.
- `PS_WATER_MARK`	: limit on the maximum number of outstanding messages
- `PS_VAN_TYPE` : the type of the Van for transport, can be `ibverbs` for RDMA, `zmq` for TCP, `p3` for TCP with [priority based parameter propagation](https://anandj.in/wp-content/uploads/sysml.pdf).
- `PS_RESEND_TIMEOUT`：消息超时时间（重发间隔）。如果设置，则如果消息在指定时间后未收到确认，则进行重发。默认为 0，即不重发。单位为毫秒。
- `PS_HEARTBEAT_TIMEOUT`：心跳超时时间。用途 TODO。默认为 0，即不会超时。单位为秒。
- `PS_HEARTBEAT_INTERVAL`：心跳间隔时间。节点每隔一次该时间，就向 scheduler 发送心跳信息。默认为 0，即不会发送。单位为毫秒。
- `PS_DROP_RATE`：收到消息后将其丢弃的概率。用于调试。
- `PS_VAN_TYPE`：van 的类型，默认为 ZMQ。

不确定：

- `PS_NODE_HOST`：节点 IP？
- `PS_PORT`：节点默认使用的端口？


---
# 其它

根据 meta.proto 生成对应源文件：`protoc -I=. --cpp_out=. ./meta.proto`
注意 protoc 的版本要与依赖的 protobuf 库版本一致，比如：3.21.x 库对应 protoc 21.x。


