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

static void timer_fn(void *arg) {
  struct mg_mgr *mgr = (struct mg_mgr *) arg;
  if (s_mqtt_conn == NULL) {
    struct mg_mqtt_opts opts = {.clean = true};
    s_mqtt_conn = mg_connect(c->mgr, mg_str("tcp://" DEST_IP ":" DEST_PORT).buf, NULL, NULL);
  }
}

ncclResult_t serverInit() {
  struct mg_mgr mgr;
  mg_mgr_init(&mgr);
  
  // 监听本地端口 8000（可修改）
  mg_listen(&mgr, "tcp://0.0.0.0:8000", ev_handler, NULL);
  mg_timer_add(&mgr, 3000, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, timer_fn, &mgr);
  
  // 事件循环
  while (true) mg_mgr_poll(&mgr, 50);
  
  mg_mgr_free(&mgr);
  return ncclSuccess;
}
