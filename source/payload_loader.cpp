#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <string>

#include <json-c/json.h>

#include "http/httplib.h"
#include "server/http_server.h"
#include "common.h"
#include "config.h"
#include "payload_loader.h"
#include "daemon.h"
#include "util.h"

namespace PAYLOAD
{
	std::string EzRemoteServerVersion()
	{
		if (!IsPortOpen("127.0.0.1", 6701, 1))
			return "";

		httplib::Client tmp_client = httplib::Client("http://127.0.0.1:6701");

		tmp_client.set_connection_timeout(1);

		if (auto res = tmp_client.Get("/version"))
		{
			if (HTTP_SUCCESS(res->status))
			{
				return res->body;
			}
		}

		return "";
	}

	bool IsPortOpen(const char *ip, uint16_t port, int timeout_sec)
	{
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0)
			return 0;

		// Set socket to non-blocking mode
		int flags = fcntl(sockfd, F_GETFL, 0);
		fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, ip, &addr.sin_addr);

		// Attempt connection
		int res = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));

		if (res < 0)
		{
			if (errno == EINPROGRESS)
			{
				// Wait for connection completion using select()
				fd_set write_fds;
				struct timeval timeout;

				FD_ZERO(&write_fds);
				FD_SET(sockfd, &write_fds);

				timeout.tv_sec = timeout_sec;
				timeout.tv_usec = 0;

				res = select(sockfd + 1, NULL, &write_fds, NULL, &timeout);

				if (res > 0)
				{
					// Check if connection succeeded or failed with an error
					int so_error;
					socklen_t len = sizeof(so_error);
					getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

					if (so_error == 0)
					{
						// Port is open! Clean up and return success
						close(sockfd);
						return 1;
					}
				}
			}
		}
		else
		{
			// Connected instantly (e.g., local loopback)
			close(sockfd);
			return 1;
		}

		close(sockfd);
		return 0; // Port is closed or timed out
	}

	// Streams an ELF to the GoldHEN payload loader on port 9090.
	static int LoadPayload(const char *elf_path)
	{
		char buffer[8192];
		in_addr_t in_addr;
		int filefd = -1;
		int sockfd = -1;
		ssize_t read_return;
		struct hostent *hostent;
		struct sockaddr_in sockaddr_in;
		unsigned short loader_port = 9090;

		filefd = open(elf_path, O_RDONLY);
		if (filefd == -1)
		{
			return -1;
		}

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd == -1)
		{
			close(filefd);
			return -1;
		}

		hostent = gethostbyname("127.0.0.1");
		if (hostent == NULL)
		{
			close(filefd);
			close(sockfd);
			return -1;
		}

		in_addr = inet_addr(inet_ntoa(*(struct in_addr *)*(hostent->h_addr_list)));
		if (in_addr == (in_addr_t)-1)
		{
			close(filefd);
			close(sockfd);
			return -1;
		}

		memset(&sockaddr_in, 0, sizeof(sockaddr_in));
		sockaddr_in.sin_addr.s_addr = in_addr;
		sockaddr_in.sin_family = AF_INET;
		sockaddr_in.sin_port = htons(loader_port);

		if (connect(sockfd, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in)) == -1)
		{
			close(filefd);
			close(sockfd);
			return -1;
		}

		while (1)
		{
			read_return = read(filefd, buffer, 8192);
			if (read_return == 0)
				break;
			if (read_return == -1)
			{
				close(filefd);
				close(sockfd);
				return -1;
			}
			if (write(sockfd, buffer, read_return) == -1)
			{
				close(filefd);
				close(sockfd);
				return -1;
			}
		}

		close(filefd);
		close(sockfd);

		return 0;
	}

	int StartEzRemoteServer()
	{
		// Already serving on its port; nothing to do.
		if (IsPortOpen("127.0.0.1", 6701, 1))
		{
			return 0;
		}

		return boot_daemon_services(EZREMOTE_SERVER_DAEMON_TITLE_ID, EZREMOTE_SERVER_DAEMON_SELF) ? 0 : -1;
	}

	void StopEzRemoteServer()
    {
        stop_daemon(EZREMOTE_SERVER_DAEMON_TITLE_ID);
    }

}
