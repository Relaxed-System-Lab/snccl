#!/bin/bash
# 功能：测量两台服务器之间的 TCP 吞吐量
# 用法：./measure_tcp_throughput.sh <服务器IP> <测试时长(秒)> <并行连接数>

# 参数检查
if [ $# -lt 1 ]; then
    echo "用法: $0 <服务器IP> [测试时长=10] [并行连接数=1]"
    exit 1
fi

SERVER_IP=$1
DURATION=${2:-10}  # 默认测试10秒
PARALLEL=${3:-1}   # 默认1个并行连接

# 检查 iperf3 是否安装
if ! command -v iperf3 &> /dev/null; then
    echo "正在安装 iperf3..."
    sudo apt-get update > /dev/null && sudo apt-get install -y iperf3  # Ubuntu/Debian
    # CentOS: sudo yum install -y iperf3
fi

# 启动服务端（在目标服务器手动运行）
echo "请在目标服务器运行: iperf3 -s"

# 客户端测试命令
echo "开始测试 TCP 吞吐量 (时长: ${DURATION}s, 并行数: ${PARALLEL})..."
iperf3 -c $SERVER_IP -t $DURATION -P $PARALLEL -J | jq '.end.sum_received.bits_per_second' | awk '{printf "吞吐量: %.2f Gbps\n", $1/1e9}'