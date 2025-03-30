#include "mongoose.h"

static const char *s_mqtt_url = "localhost:80088";
static struct mg_connection *s_mqtt_conn = NULL;

// MQTT connection event handler function
static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_OPEN) {
    MG_INFO(("%lu created, connecting to %s ...", c->id, s_mqtt_url));
  } else if (ev == MG_EV_MQTT_OPEN) {
    struct mg_mqtt_opts opts = {.qos = 1, .topic = mg_str("device1/rx")};
    mg_mqtt_sub(c, &opts);
    MG_INFO(("%lu connected, subscribing to %s", c->id, opts.topic.buf));
  } else if (ev == MG_EV_MQTT_MSG) {
    char response[100];
    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    struct mg_mqtt_opts opts = {.qos = 1, .topic = mg_str("device1/tx")};
    mg_snprintf(response, sizeof(response), "Received [%.*s] / [%.*s]",
                mm->topic.len, mm->topic.buf, mm->data.len, mm->data.buf);
    opts.message = mg_str(response);
    mg_mqtt_pub(c, &opts);
  } else if (ev == MG_EV_CLOSE) {
    MG_INFO(("%u closing", c->id));
    s_mqtt_conn = NULL;
  }
}

// Reconnection timer function. If we get disconnected, reconnect again
static void timer_fn(void *arg) {
  struct mg_mgr *mgr = (struct mg_mgr *) arg;
  if (s_mqtt_conn == NULL) {
    struct mg_mqtt_opts opts = {.clean = true};
    s_mqtt_conn = mg_mqtt_connect(mgr, s_mqtt_url, &opts, ev_handler, NULL);
  }
}

void server_init() {
  struct mg_mgr mgr;  // Mongoose event manager. Holds all connections
  mg_mgr_init(&mgr);  // Initialise event manager
  mg_timer_add(&mgr, 3000, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, timer_fn, &mgr);
  for (;;) {
    mg_mgr_poll(&mgr, 1000);  // Infinite event loop
  }
}