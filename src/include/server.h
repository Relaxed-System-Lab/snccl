#ifndef NCCL_SERVER_H_
#define NCCL_SERVER_H_

#include "nccl.h"

#define DEST_PORT "8001"
#define DEST_IP "192.168.1.148"
#define SERVER1_ADDR "tcp://192.168.1.148:8000"
#define SERVER2_ADDR "tcp://192.168.1.148:8001"

ncclResult_t serverInit();
ncclResult_t clientInit();
ncclResult_t SendClientConncet();
ncclResult_t recvClientConncet();

#endif