#ifndef SMBCLIENT_H
#define SMBCLIENT_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <string>
#include <vector>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include "clients/remote_client.h"
#include "common.h"

#define SMB_CLIENT_MAX_FILENAME_LEN 256

class SmbClient : public RemoteClient
{
public:
	SmbClient();
	~SmbClient();
	int Connect(const std::string &url, const std::string &user, const std::string &pass);
	int Mkdir(const std::string &path);
	int Rmdir(const std::string &path, bool recursive);
	int Size(const std::string &path, int64_t *size);
	int Get(const std::string &outputfile, const std::string &path, uint64_t offset=0);
	int GetRange(const std::string &path, void *buffer, uint64_t size, uint64_t offset);
	int Put(const std::string &inputfile, const std::string &path, uint64_t offset=0);
	int Rename(const std::string &src, const std::string &dst);
	int Delete(const std::string &path);
	bool FileExists(const std::string &path);
    int Copy(const std::string &from, const std::string &to);
    int Move(const std::string &from, const std::string &to);
	int CopyToSocket(const std::string &path, int socket_fd);
	std::vector<DirEntry> ListDir(const std::string &path);
	bool IsConnected();
	bool Ping();
	const char *LastResponse();
	int Quit();
	std::string GetPath(std::string ppath1, std::string ppath2);
	int Head(const std::string &path, void *buffer, uint64_t len);
	ClientType clientType();
	uint32_t SupportedActions();

private:
	int _Rmdir(const std::string &path);
	struct smb2_context *smb2;
	char response[1024];
	bool connected = false;
	uint32_t max_read_size = 0;
	uint32_t max_write_size = 0;
};

#endif