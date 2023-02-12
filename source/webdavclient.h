#ifndef WEBDAVCLIENT_H
#define WEBDAVCLIENT_H

#include <time.h>
#include <string>
#include <vector>
#include "webdav/client.hpp"
#include "common.h"
#include "remote_client.h"

namespace WebDAV
{
	class WebDavClient : public RemoteClient
	{
	public:
		WebDavClient();
		int Connect(const std::string &url, const std::string &user, const std::string &pass);
		int Connect(const std::string &url, const std::string &user, const std::string &pass, bool check_enabled);
		int Mkdir(const std::string &path);
		int Rmdir(const std::string &path, bool recursive);
		int Size(const std::string &path, int64_t *size);
		int Get(const std::string &outputfile, const std::string &path, uint64_t offset=0);
		int Put(const std::string &inputfile, const std::string &path, uint64_t offset=0);
		int Rename(const std::string &src, const std::string &dst);
		int Delete(const std::string &path);
		int Copy(const std::string &from, const std::string &to);
		int Move(const std::string &from, const std::string &to);
		bool FileExists(const std::string &path);
		std::vector<DirEntry> ListDir(const std::string &path);
		bool IsConnected();
		bool Ping();
		const char *LastResponse();
		int Quit();
		std::string GetPath(std::string path1, std::string path2);
		int Head(const std::string &path, void *buffer, int64_t len);
		bool GetHeaders(const std::string &path, dict_t *headers);
		WebDAV::Client *GetClient();
		ClientType clientType();

	private:
		int _Rmdir(const std::string &path);
		WebDAV::Client *client;
		char response[1024];
		bool connected = false;
	};

}
#endif