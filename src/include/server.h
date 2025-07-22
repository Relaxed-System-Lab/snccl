#ifndef NCCL_SERVER_H_
#define NCCL_SERVER_H_

#include "nccl.h"

#define SERVER1_ADDR "172.27.109.125:8080"
#define SERVER2_ADDR "172.20.93.148:8080"

ncclResult_t serverInit();
ncclResult_t clientInit();
ncclResult_t SendClientConncet();
ncclResult_t recvClientConncet();

#endif