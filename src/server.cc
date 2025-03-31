#include "mongoose.h"
#include "comm.h"
#include "nccl.h"

#define DEST_PORT "8001"
#define DEST_IP "127.0.0.1"

// MQTT connection event handler function
static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_ACCEPT) {
        // 客户端连接时，创建到目标服务器的连接
        struct mg_connection *dest = mg_connect(c->mgr, mg_str("tcp://" DEST_IP ":" DEST_PORT).buf, NULL, NULL);
        if (dest) {
            dest->fn_data = c;  // 绑定客户端与目标连接
            c->fn_data = dest;
        }
    } else if (ev == MG_EV_READ) {
        // 收到客户端数据时，转发到目标服务器
        struct mg_connection *dest = (struct mg_connection *)c->fn_data;
        if (dest && dest->is_connecting) {
            mg_send(dest, c->recv.buf, c->recv.len);
            c->recv.len = 0;  // 清空接收缓冲区
        }
    } else if (ev == MG_EV_CLOSE) {
        // 连接关闭时，断开双向连接
        struct mg_connection *dest = (struct mg_connection *)c->fn_data;
        if (dest) dest->is_closing = 1;
    }
}

void* ncclserverInit(){
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
  
    // 监听本地端口 8000（可修改）
    mg_listen(&mgr, "tcp://0.0.0.0:8000", ev_handler, NULL);
    // 事件循环
    while (true) mg_mgr_poll(&mgr, 50);
  
    mg_mgr_free(&mgr);
    return ncclSuccess;
}


static void fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_READ) {
        // 接收数据并打印日志（可替换为业务逻辑）
        printf("Received data: %.*s\n", (int)c->recv.len, c->recv.buf);
        c->recv.len = 0;  // 清空缓冲区
        // 可选：发送响应回客户端（需通过转发服务器）
        mg_send(c, "ACK\n", 4);
    }
}

void* ncclserver2Init() {
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_listen(&mgr, "tcp://0.0.0.0:8001", fn, NULL);  // 监听端口 8001
    while (true) mg_mgr_poll(&mgr, 50);
    mg_mgr_free(&mgr);
    return ncclSuccess;
}

static void client_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_CONNECT) {
        // 连接成功后发送目标地址头 + 数据
        const char *dest = "DEST:192.168.1.100:1234\n"; // 动态目标地址
        const char *payload = "Hello from client!";
        mg_send(c, dest, strlen(dest));
        mg_send(c, payload, strlen(payload));
    } else if (ev == MG_EV_READ) {
        // 处理目标服务器的响应（可选）
        printf("Response: %.*s\n", (int)c->recv.len, c->recv.buf);
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

pthread_t thread1 ;
pthread_t thread2 ;
ncclResult_t serverInit() {
  INFO(NCCL_INIT, "jiashu: serverInit");
  if (!thread1){
    PTHREADCHECK(pthread_create(&thread1, NULL, ncclserverInit, nullptr), "pthread_create");
    ncclSetThreadName(thread1, "NCCL Server1");
  }
  if (!thread2){
    PTHREADCHECK(pthread_create(&thread1, NULL, ncclserver2Init, nullptr), "pthread_create");
    ncclSetThreadName(thread1, "NCCL Server2");
  }
}