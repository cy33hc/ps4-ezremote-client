#ifndef EZREMOTE_LOADER_PAYLOAD_LOADER_H
#define EZREMOTE_LOADER_PAYLOAD_LOADER_H

#include <stdint.h>

int IsPortOpen(const char *ip, uint16_t port, int timeout_sec);
int StartEzRemoteServer(void);
void StopEzRemoteServer(void);

#endif
