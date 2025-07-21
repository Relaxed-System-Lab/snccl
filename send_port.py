import socket
import time
import threading
import argparse

class ThroughputSender:
    def __init__(self, target_ip, target_port, packet_size=1024, interval=0.1, duration=60):
        self.target_ip = target_ip
        self.target_port = target_port
        self.packet_size = packet_size  # 单次发送数据包大小（字节）
        self.interval = interval        # 发送间隔（秒）
        self.duration = duration        # 总测试时长（秒）
        self.sent_bytes = 0
        self.running = True

    def _send_data(self):
        """持续发送数据到目标服务器"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.connect((self.target_ip, self.target_port))
            # 生成测试数据（填充随机字节）
            data = b'A' * self.packet_size
            start_time = time.time()
            
            while self.running and (time.time() - start_time < self.duration):
                sock.sendall(data)
                self.sent_bytes += self.packet_size
                time.sleep(self.interval)
        except Exception as e:
            print(f"发送失败: {e}")
        finally:
            sock.close()

    def start(self):
        """启动发送线程和吞吐量统计"""
        sender_thread = threading.Thread(target=self._send_data, daemon=True)
        sender_thread.start()
        print(f"开始向 {self.target_ip}:{self.target_port} 发送数据 | 包大小: {self.packet_size}字节 | 间隔: {self.interval}秒")
        
        # 实时计算发送端吞吐量
        start_time = time.time()
        while time.time() - start_time < self.duration:
            elapsed = time.time() - start_time
            throughput_mbps = (self.sent_bytes * 8) / (elapsed * 1e6)  # 转换为 Mbps
            print(f"发送端吞吐量: {throughput_mbps:.2f} Mbps | 已发送: {self.sent_bytes / 1e6:.2f} MB")
            time.sleep(1)
        
        self.running = False
        print("测试结束")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="TCP 吞吐量测试工具")
    parser.add_argument("--ip", default="127.0.0.1", help="目标服务器IP（默认：127.0.0.1）")
    parser.add_argument("--port", type=int, default=8080, help="目标端口（默认：8080）")
    parser.add_argument("--size", type=int, default=4096, help="数据包大小（字节，默认：4096）")
    parser.add_argument("--interval", type=float, default=0.01, help="发送间隔（秒，默认：0.01）")
    parser.add_argument("--duration", type=int, default=30, help="测试时长（秒，默认：30）")
    args = parser.parse_args()

    sender = ThroughputSender(
        target_ip=args.ip,
        target_port=args.port,
        packet_size=args.size,
        interval=args.interval,
        duration=args.duration
    )
    sender.start()