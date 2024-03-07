


# environment variables

The variables must be set for starting

- `PS_NUM_WORKER` : the number of workers
- `PS_NUM_SERVER` : the number of servers
- `PS_ROLE` : the role of the current node, can be `worker`, `server`, or `scheduler`
- `PS_SCHEDULER_URI` : the ip or hostname of the scheduler node
- `PS_SCHEDULER_PORT` : the port that the scheduler node is listening

additional variables:

- `PS_INTERFACE` : the network interface a node should use. in default choose automatically
- `PS_LOCAL` : runs in local machines, no network is needed
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