#include "payload_loader.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define SERVER_ELF_PATH "/mnt/sandbox/pfsmnt/RMTC00001-app0/daemon/ezremote-server.elf"
#define SERVER_PORT 6701
#define PAYLOAD_PORT 9090

int IsPortOpen(const char *ip, uint16_t port, int timeout_sec)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return 0;

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    int res = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (res < 0)
    {
        if (errno == EINPROGRESS)
        {
            fd_set write_fds;
            struct timeval timeout;
            FD_ZERO(&write_fds);
            FD_SET(sockfd, &write_fds);
            timeout.tv_sec = timeout_sec;
            timeout.tv_usec = 0;

            res = select(sockfd + 1, NULL, &write_fds, NULL, &timeout);
            if (res > 0)
            {
                int so_error = 0;
                socklen_t len = sizeof(so_error);
                getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
                if (so_error == 0)
                {
                    close(sockfd);
                    return 1;
                }
            }
        }
    }
    else
    {
        close(sockfd);
        return 1;
    }

    close(sockfd);
    return 0;
}

static int LoadPayload(const char *elf_path)
{
    char buffer[8192];
    int filefd = open(elf_path, O_RDONLY);
    if (filefd < 0)
        return -1;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        close(filefd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PAYLOAD_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(filefd);
        close(sockfd);
        return -1;
    }

    for (;;)
    {
        ssize_t read_len = read(filefd, buffer, sizeof(buffer));
        if (read_len == 0)
            break;
        if (read_len < 0)
        {
            close(filefd);
            close(sockfd);
            return -1;
        }

        ssize_t written = 0;
        while (written < read_len)
        {
            ssize_t n = write(sockfd, buffer + written, (size_t)(read_len - written));
            if (n <= 0)
            {
                close(filefd);
                close(sockfd);
                return -1;
            }
            written += n;
        }
    }

    close(filefd);
    close(sockfd);
    return 0;
}

int StartEzRemoteServer(void)
{
    if (IsPortOpen("127.0.0.1", SERVER_PORT, 1))
        return 0;

    int ret = LoadPayload(SERVER_ELF_PATH);
    if (!ret)
        sleep(3);

    return ret;
}

void StopEzRemoteServer(void)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
    {
        const char request[] = "GET /stop HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n";
        ssize_t ignored = write(sockfd, request, sizeof(request) - 1);
        (void)ignored;
    }

    close(sockfd);
}
