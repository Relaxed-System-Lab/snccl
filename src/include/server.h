#ifndef NCCL_SERVER_H_
#define NCCL_SERVER_H_

#include "nccl.h"

ncclResult_t serverInit();
ncclResult_t clientInit();
ncclResult_t SendClientConncet();
ncclResult_t recvClientConncet();

#endif