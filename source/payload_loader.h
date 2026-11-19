#pragma once

#include <stdint.h>
#include <string>

namespace PAYLOAD
{
    std::string EzRemoteServerVersion();
    int StartEzRemoteServer();
    int StartEzRemoteClient();
    void StopEzRemoteServer();
    void StopEzRemoteClient();
    bool IsEzremoteClientStarted();
    bool IsPortOpen(const char *ip, uint16_t port, int timeout_sec);
}
