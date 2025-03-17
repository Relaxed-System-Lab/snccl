import socket
import sys

# 配置参数
HOST = '127.0.0.1'  # 本地环回地址
PORT = 8888         # 监听端口
BUFFER_SIZE = 8192  # 接收缓冲区大小（建议与内核参数对齐[2,3](@ref)）

def start_tcp_host():
    # 创建TCP套接字
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            # 设置接收缓冲区大小（需系统内核支持[2](@ref)）
            s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 262144)  # 256KB
            
            # 绑定端口并监听
            s.bind((HOST, PORT))
            s.listen(5)  # 允许5个待处理连接
            print(f"Listening on {HOST}:{PORT}...")

            while True:
                # 接受客户端连接
                conn, addr = s.accept()
                print(f"Connected by {addr}")

                with conn:
                    while True:
                        data = conn.recv(BUFFER_SIZE)
                        if not data:
                            break  # 连接关闭
                        print(f"Received {len(data)} bytes: {data[:20]}...")  # 显示前20字节
                        
                        # 此处可添加数据处理逻辑，如写入文件/数据库

        except Exception as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)

if __name__ == "__main__":
    start_tcp_host()