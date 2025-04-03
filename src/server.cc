#include "mongoose.h"
#include "comm.h"
#include "nccl.h"
#include "uthash.h"

#include <arpa/inet.h>  // Linux/macOS
// 或 #include <ws2tcpip.h>  // Windows

const char *mg_ntop(const struct mg_addr *addr, char *buf, size_t len) {
    if (addr->is_ip6) {
        return inet_ntop(AF_INET6, &addr->ip, buf, len);
    } else {
        return inet_ntop(AF_INET, &addr->ip, buf, len);
    }
}

typedef struct {
    char addr[64];          // 客户端地址
    struct mg_connection *conn; // 对应的连接句柄
    UT_hash_handle hh;      // 哈希表句柄
} ClientEntry;

typedef struct {
    uint16_t info_len;  // 目标信息长度
    char info[32];       // 目标信息
    // char data[];     // 实际数据（柔性数组）
} custom_packet;

static ClientEntry *clients = NULL;

#define DEST_PORT "8001"
#define DEST_IP "192.168.1.148"
#define SERVER1_ADDR "tcp://192.168.1.148:8000"
#define SERVER2_ADDR "tcp://192.168.1.148:8001"

// MQTT connection event handler function
static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_ACCEPT) {
        // 客户端连接时，创建到目标服务器的连接
        struct mg_connection *dest = mg_connect(c->mgr, SERVER2_ADDR, NULL, NULL);
        if (dest) {
            dest->fn_data = c;  // 绑定客户端与目标连接
            c->fn_data = dest;
        }
        INFO(NCCL_INIT, "jiashu: connecting to server2");
    } else if (ev == MG_EV_READ) {
        // 1. 解析 client1 数据包中的目标地址
        struct mg_iobuf *recv_buf = &c->recv;
        struct mg_str data = mg_str_n((const char *)recv_buf->buf, recv_buf->len);
        if (data.len < 4) return; // 等待完整包头
        
        // 2. 动态连接 server2（若未连接）
        struct mg_connection *server2_conn = mg_connect(c->mgr, SERVER2_ADDR, NULL, NULL);
        if (server2_conn) {
            // 3. 封装目标地址和数据，转发至 server2
            mg_send(server2_conn, data.buf, data.len);
            c->recv.len = 0; // 清空接收缓冲区
        }

        INFO(NCCL_INIT, "jiashu:send to server2");
    } else if (ev == MG_EV_CLOSE) {
        // 连接关闭时，断开双向连接
        struct mg_connection *dest = (struct mg_connection *)c->fn_data;
        if (dest) dest->is_closing = 1;
    }
}

void* ncclserverInit(void* args){
    struct mg_mgr mgr;

    INFO(NCCL_INIT, "mg_mgr_init");
    mg_mgr_init(&mgr);
  
    // 监听本地端口 8000（可修改）
    INFO(NCCL_INIT, "mg_mgr_init success");
    mg_listen(&mgr, SERVER1_ADDR, ev_handler, NULL);
    // 事件循环
    while (true) mg_mgr_poll(&mgr, 50);
  
    mg_mgr_free(&mgr);
    return NULL;
}


static void fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_READ) {
        // 1. 解析 server1 转发的数据包
        struct mg_iobuf *recv_buf = &c->recv;
        struct mg_str data = mg_str_n((const char *)recv_buf->buf, recv_buf->len);

        custom_packet *pkt = (custom_packet *)data.buf;
        uint16_t addr_len = ntohs(pkt->info_len);
        char *target_addr = pkt->info;
        char *payload = pkt->info + addr_len;
        uint16_t payload_len = mg_ntohs(*(uint16_t *)(target_addr + addr_len));

        // 2. 查找目标 client2
        ClientEntry *entry;
        HASH_FIND_STR(clients, target_addr, entry);
        if (entry && entry->conn->is_connecting) {
            // 3. 转发数据到 client2
            mg_send(entry->conn, payload, payload_len);
        }
        c->recv.len = 0; // 清空接收缓冲区
    } else if (ev == MG_EV_ACCEPT) {
        // 新 client2 连接时注册地址
        ClientEntry *entry = (ClientEntry *)calloc(1, sizeof(ClientEntry));
        char ip_str[46]; // 足够存储 IPv6 地址
        mg_ntop(&c->rem, ip_str, sizeof(ip_str));
        snprintf(entry->addr, sizeof(entry->addr), "%s:%hu", ip_str, mg_ntohs(c->rem.port));
        entry->conn = c;
        HASH_ADD_STR(clients, addr, entry);
    } else if (ev == MG_EV_CLOSE) {
        // client2 断开时移除记录
        ClientEntry *entry;
        HASH_FIND_PTR(clients, &c, entry);
        if (entry) {
            HASH_DEL(clients, entry);
            free(entry);
        }
    }
}

void* ncclserver2Init(void* args) {
    struct mg_mgr mgr;

    INFO(NCCL_INIT, "mg_mgr_init");
    mg_mgr_init(&mgr);

    INFO(NCCL_INIT, "mg_mgr_init success");
    mg_listen(&mgr, SERVER2_ADDR, fn, NULL);  // 监听端口 8001
    while (true) mg_mgr_poll(&mgr, 50);
    mg_mgr_free(&mgr);
    return NULL;
}

static void client1_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_CONNECT) {
        // 连接 Server1 成功后发送测试数据
        const char *target_info = SERVER2_ADDR;
        const char *data = "Hello from Client1";
        uint16_t info_len = htons(strlen(target_info));

        // 构造自定义报文
        size_t total_len = sizeof(custom_packet) + strlen(target_info) + strlen(data);
        custom_packet *pkt = (custom_packet *)malloc(total_len);
        pkt->info_len = info_len;
        memcpy(pkt->info, target_info, strlen(target_info));
        memcpy(pkt->info + strlen(target_info), data, strlen(data));

        mg_send(c, pkt, total_len);  // 发送报文
        free(pkt);
    } else if (ev == MG_EV_READ) {
        // 接收 Server1 返回的响应（如转发结果）
        printf("Client2 received: %.*s\n", (int)c->recv.len, c->recv.buf);
        c->recv.len = 0;
    } else if (ev == MG_EV_CLOSE) {
        printf("Connection closed\n");
    }
}

static void client2_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_CONNECT) {
        // 连接成功后发送注册信息（如自身地址）
        const char *reg_msg = SERVER1_ADDR;
        mg_send(c, reg_msg, strlen(reg_msg));
    } else if (ev == MG_EV_READ) {
        // 接收 server2 转发的数据
        printf("Client2 received: %.*s\n", (int)c->recv.len, c->recv.buf);
        c->recv.len = 0;
    } else if (ev == MG_EV_CLOSE) {
        printf("Connection closed\n");
    }
}

ncclResult_t recvClientConncet() {
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_connect(&mgr, SERVER2_ADDR, client2_handler, NULL); // 连接转发服务器
    while (true) mg_mgr_poll(&mgr, 50);
    mg_mgr_free(&mgr);
    return ncclSuccess;
}


ncclResult_t SendClientConncet(char* target) {
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_connect(&mgr, SERVER1_ADDR, client1_handler, NULL); // 连接转发服务器
    while (true) mg_mgr_poll(&mgr, 50);
    mg_mgr_free(&mgr);
    return ncclSuccess;
}

ncclResult_t serverInit() {
  pthread_t thread1;
  pthread_t thread2;
  PTHREADCHECK(pthread_create(&thread1, NULL, ncclserverInit, nullptr), "pthread_create");
  ncclSetThreadName(thread1, "NCCL Server1");
  PTHREADCHECK(pthread_create(&thread2, NULL, ncclserver2Init, nullptr), "pthread_create");
  ncclSetThreadName(thread2, "NCCL Server2");
  INFO(NCCL_INIT, "Forwarding Server Init Succsss");
  return ncclSuccess;
}