#ifndef FTPCLIENT_H
#define FTPCLIENT_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <string>
#include <vector>
#include "clients/remote_client.h"
#include "http/httplib.h"

#define FTP_CLIENT_MAX_FILENAME_LEN 128

typedef int (*FtpCallbackXfer)(int64_t xfered, void *arg);

struct ftphandle
{
	char *cput, *cget;
	int handle;
	int cavail, cleft;
	char *buf;
	int dir;
	ftphandle *ctrl;
	int cmode;
	int64_t xfered;
	int64_t xfered1;
	int64_t cbbytes;
	char response[512];
	int64_t offset;
	bool correctpasv;
	FtpCallbackXfer xfercb;
	void *cbarg;
	bool is_connected;
};

class FtpClient : public RemoteClient
{
public:
	enum accesstype
	{
		dir = 1,
		dirverbose,
		dirmlsd,
		fileread,
		filewrite,
		filereadappend,
		filewriteappend
	};

	enum transfermode
	{
		ascii = 'A',
		image = 'I'
	};

	enum connmode
	{
		pasv = 1,
		port
	};

	enum attributes
	{
		directory = 1,
		readonly = 2
	};

	FtpClient();
	~FtpClient();
	int Connect(const std::string &url, const std::string &user, const std::string &pass);
	void SetConnmode(connmode mode);
	int Site(const std::string &cmd);
	int Raw(const std::string &cmd);
	int SysType(char *buf, int max);
	int Mkdir(const std::string &path);
	int Chdir(const std::string &path);
	int Cdup();
	int Rmdir(const std::string &path);
	int Rmdir(const std::string &path, bool recursive);
	int Size(const std::string &path, int64_t *size);
	int Get(const std::string &outputfile, const std::string &path, uint64_t offset = 0);
	int GetRange(const std::string &path, void *buffer, uint64_t size, uint64_t offset);
	int GetRange(const std::string &path, DataSink &sink, uint64_t size, uint64_t offset);
	int Put(const std::string &inputfile, const std::string &path, uint64_t offset = 0);
	int Rename(const std::string &src, const std::string &dst);
	int Delete(const std::string &path);
    int Copy(const std::string &from, const std::string &to);
    int Move(const std::string &from, const std::string &to);
	int Head(const std::string &path, void *buffer, uint64_t len);
	std::vector<DirEntry> ListDir(const std::string &path);
	void SetCallbackXferFunction(FtpCallbackXfer pointer);
	void SetCallbackArg(void *arg);
	void SetCallbackBytes(int64_t bytes);
	bool Noop();
	bool Ping();
	bool FileExists(const std::string &path);
	bool IsConnected();
	char *LastResponse();
	long GetIdleTime();
	int Quit();
	std::string GetPath(std::string path1, std::string path2);
	ClientType clientType();
	uint32_t SupportedActions();

private:
	ftphandle *mp_ftphandle;
	struct tm cur_time;
	timeval tick;
	char server[128];
	int server_port;

	int FtpSendCmd(const std::string &cmd, char expected_resp, ftphandle *nControl);
	ftphandle *RawOpen(const std::string &path, accesstype type, transfermode mode);
	int RawClose(ftphandle *handle);
	int RawWrite(void *buf, int len, ftphandle *handle);
	int RawRead(void *buf, int max, ftphandle *handle);
	int ReadResponse(char c, ftphandle *nControl);
	int Readline(char *buf, int max, ftphandle *nControl);
	int Writeline(char *buf, int len, ftphandle *nData);
	void ClearHandle();
	int FtpOpenPasv(ftphandle *nControl, ftphandle **nData, transfermode mode, int dir, std::string &cmd);
	int FtpOpenPort(ftphandle *nControl, ftphandle **nData, transfermode mode, int dir, std::string &cmd);
	int FtpAcceptConnection(ftphandle *nData, ftphandle *nControl);
	int CorrectPasvResponse(int *v);
	int FtpAccess(const std::string &path, accesstype type, transfermode mode, ftphandle *nControl, ftphandle **nData);
	int FtpXfer(const std::string &localfile, const std::string &path, ftphandle *nControl, accesstype type, transfermode mode);
	int FtpWrite(void *buf, int len, ftphandle *nData);
	int FtpRead(void *buf, int max, ftphandle *nData);
	int FtpClose(ftphandle *nData);
	int ParseDirEntry(char *line, DirEntry *dirEntry);
	int ParseMLSDDirEntry(char *line, DirEntry *dirEntry);
};

#endif