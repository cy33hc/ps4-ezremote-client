#ifndef EZ_HTTP_SERVER_H
#define EZ_HTTP_SERVER_H

#include "http/httplib.h"

using namespace httplib;
extern Server *svr;

static pthread_t http_server_thid;
extern int http_server_port;
extern int http_int_server_port;
extern char compressed_file_path[];

namespace HttpServer
{
    void *ServerThread(void *argp);
    // blocking: serve on the calling thread, so listen() blocks until the
    // server is stopped. Used by headless mode, which has no UI loop.
    // False when the server could not be started at all.
    bool Start(bool blocking = false);
    void Stop();
    bool AnyTransferRunning();
}

#endif