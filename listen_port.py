import socket
import threading
import time
import psutil
from collections import defaultdict

# 配置参数
LISTEN_IP = "0.0.0.0"  # 监听所有网络接口
LISTEN_PORT = 8080
STAT_INTERVAL = 2  # 吞吐量统计间隔（秒）
TARGET_IP = "172.27.109.125"  # 仅统计流向此IP的流量（可选）

class ThroughputMonitor:
    def __init__(self):
        self.running = True
        self.traffic_stats = defaultdict(lambda: {"bytes_sent": 0, "bytes_recv": 0})
        self.lock = threading.Lock()

    def start_monitor(self):
        """启动后台流量统计线程"""
        threading.Thread(target=self._track_throughput, daemon=True).start()

    def _track_throughput(self):
        """定期计算吞吐量"""
        prev_stats = psutil.net_io_counters(pernic=True)
        while self.running:
            time.sleep(STAT_INTERVAL)
            curr_stats = psutil.net_io_counters(pernic=True)
            for intf in prev_stats:
                bytes_sent = curr_stats[intf].bytes_sent - prev_stats[intf].bytes_sent
                bytes_recv = curr_stats[intf].bytes_recv - prev_stats[intf].bytes_recv
                with self.lock:
                    self.traffic_stats[intf]["bytes_sent"] = bytes_sent
                    self.traffic_stats[intf]["bytes_recv"] = bytes_recv
            prev_stats = curr_stats

    def get_throughput(self):
        """返回当前吞吐量统计结果（MB/s）"""
        with self.lock:
            return {
                intf: {
                    "send_mbps": stats["bytes_sent"] / (STAT_INTERVAL * 1e6) * 8,  # 转换为 Mbps
                    "recv_mbps": stats["bytes_recv"] / (STAT_INTERVAL * 1e6) * 8
                }
                for intf, stats in self.traffic_stats.items()
            }

def start_server(monitor):
    """启动TCP监听服务"""
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((LISTEN_IP, LISTEN_PORT))
    server_socket.listen(5)
    print(f"监听中: {LISTEN_IP}:{LISTEN_PORT} | 统计间隔: {STAT_INTERVAL}秒")

    while monitor.running:
        try:
            client_socket, addr = server_socket.accept()
            client_ip = addr[0]
            # 仅统计目标IP的流量（若未配置TARGET_IP则统计所有）
            if not TARGET_IP or client_ip == TARGET_IP:
                print(f"新连接: {client_ip}:{addr[1]}")
            client_socket.close()  # 立即关闭连接（仅统计连接行为）
        except Exception as e:
            print(f"监听异常: {e}")

if __name__ == "__main__":
    monitor = ThroughputMonitor()
    monitor.start_monitor()
    
    # 启动服务器监听线程
    server_thread = threading.Thread(target=start_server, args=(monitor,))
    server_thread.daemon = True
    server_thread.start()

    try:
        while True:
            throughput = monitor.get_throughput()
            for intf, stats in throughput.items():
                print(
                    f"[{intf}] 发送: {stats['send_mbps']:.2f} Mbps | "
                    f"接收: {stats['recv_mbps']:.2f} Mbps"
                )
            print("-" * 50)
            time.sleep(STAT_INTERVAL)
    except KeyboardInterrupt:
        monitor.running = False
        print("\n监控已停止")