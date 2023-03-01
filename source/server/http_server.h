#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "http/httplib.h"

using namespace httplib;
extern SSLServer *svr;

static pthread_t http_server_thid;
extern int http_server_port;

namespace HttpServer
{
    void *ServerThread(void *argp);
    void Start();
    void Stop();
}

#endif