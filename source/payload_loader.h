#pragma once

#include <stdint.h>
#include <string>

namespace PAYLOAD
{
    std::string EzRemoteServerVersion();
    int StartEzRemoteServer();
    void StopEzRemoteServer();
    bool IsPortOpen(const char *ip, uint16_t port, int timeout_sec);
}
