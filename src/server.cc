#include "mongoose.h"
#include "comm.h"
#include "nccl.h"

#define DEST_PORT "8001"
#define DEST_IP "192.168.1.148"
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
    } else if (ev == MG_EV_READ) {
        // 1. 解析 client1 数据包中的目标地址
        struct mg_str *data = &c->recv;
        if (data->len < 4) return; // 等待完整包头
        
        uint16_t addr_len = mg_ntohs(*(uint16_t *)data->buf);
        char *target_addr = (char *)data->buf + 2;
        uint16_t payload_len = mg_ntohs(*(uint16_t *)(target_addr + addr_len));
        
        // 2. 动态连接 server2（若未连接）
        struct mg_connection *server2_conn = mg_connect(c->mgr, SERVER2_ADDR, NULL, NULL);
        if (server2_conn) {
            // 3. 封装目标地址和数据，转发至 server2
            mg_send(server2_conn, data->buf, data->len);
            c->recv.len = 0; // 清空接收缓冲区
        }
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
    mg_listen(&mgr, "tcp://192.168.1.148:8000", ev_handler, NULL);
    // 事件循环
    while (true) mg_mgr_poll(&mgr, 50);
  
    mg_mgr_free(&mgr);
    return NULL;
}


static void fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_READ) {
        // 1. 解析 server1 转发的数据包
        struct mg_str *data = &c->recv;
        uint16_t addr_len = mg_ntohs(*(uint16_t *)data->buf);
        char *target_addr = (char *)data->buf + 2;
        uint16_t payload_len = mg_ntohs(*(uint16_t *)(target_addr + addr_len));
        char *payload = target_addr + addr_len + 2;

        // 2. 查找目标 client2
        ClientEntry *entry;
        HASH_FIND_STR(clients, target_addr, entry);
        if (entry && entry->conn->is_connected) {
            // 3. 转发数据到 client2
            mg_send(entry->conn, payload, payload_len);
        }
        c->recv.len = 0; // 清空接收缓冲区
    } else if (ev == MG_EV_ACCEPT) {
        // 新 client2 连接时注册地址
        ClientEntry *entry = (ClientEntry *)calloc(1, sizeof(ClientEntry));
        snprintf(entry->addr, sizeof(entry->addr), "%s:%d", c->peer.ip, c->peer.port);
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
    mg_listen(&mgr, "tcp://192.168.1.148:8001", fn, NULL);  // 监听端口 8001
    while (true) mg_mgr_poll(&mgr, 50);
    mg_mgr_free(&mgr);
    return NULL;
}

static void client2_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if (ev == MG_EV_CONNECT) {
        // 连接成功后发送注册信息（如自身地址）
        const char *reg_msg = "REGISTER|192.168.1.2:8000";
        mg_send(c, reg_msg, strlen(reg_msg));
    } else if (ev == MG_EV_READ) {
        // 接收 server2 转发的数据
        printf("Client2 received: %.*s\n", (int)c->recv.len, c->recv.buf);
        c->recv.len = 0;
    }
}

ncclResult_t clientConncet() {
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_connect(&mgr, "tcp://转发服务器IP:8000", client_handler, NULL); // 连接转发服务器
    while (true) mg_mgr_poll(&mgr, 50);
    mg_mgr_free(&mgr);
    return ncclSuccess;
}

ncclResult_t serverInit() {
  INFO(NCCL_INIT, "jiashu: serverInit");
  pthread_t thread1;
  pthread_t thread2;
  PTHREADCHECK(pthread_create(&thread1, NULL, ncclserverInit, nullptr), "pthread_create");
  ncclSetThreadName(thread1, "NCCL Server1");
  INFO(NCCL_INIT, "jiashu: serverInit success");
  PTHREADCHECK(pthread_create(&thread2, NULL, ncclserver2Init, nullptr), "pthread_create");
  ncclSetThreadName(thread2, "NCCL Server2");
  INFO(NCCL_INIT, "jiashu: server2Init success");
  return ncclSuccess;
}