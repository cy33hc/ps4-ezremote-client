#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <orbis/Net.h>
#include "fs.h"
#include "lang.h"
#include "clients/nfsclient.h"
#include "windows.h"
#include "util.h"
#include "system.h"

#define BUF_SIZE 256*1024

NfsClient::NfsClient()
{
}

NfsClient::~NfsClient()
{
}

int NfsClient::Connect(const std::string &url, const std::string &user, const std::string &pass)
{
	nfs = nfs_init_context();
	if (nfs == nullptr)
	{
		sprintf(response, "%s", lang_strings[STR_FAIL_INIT_NFS_CONTEXT]);
		return 0;
	}

	struct nfs_url *nfsurl = nfs_parse_url_full(nfs, url.c_str());
	if (nfsurl == nullptr) {
		sprintf(response, "%s", nfs_get_error(nfs));
		nfs_destroy_context(nfs);
		return 0;
	}

	std::string export_path = std::string(nfsurl->path) + nfsurl->file;
	int ret = nfs_mount(nfs, nfsurl->server, export_path.c_str());
	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		nfs_destroy_url(nfsurl);
		nfs_destroy_context(nfs);
		nfs = nullptr;
		return 0;
	}
	nfs_destroy_url(nfsurl);

	connected = true;
	return 1;
}

/*
 * LastResponse - return a pointer to the last response received
 */
const char *NfsClient::LastResponse()
{
	return (const char *)response;
}

/*
 * IsConnected - return true if connected to remote
 */
bool NfsClient::IsConnected()
{
	return connected;
}

/*
 * Ping - return true if connected to remote
 */
bool NfsClient::Ping()
{
	return connected;
}

/*
 * Quit - disconnect from remote
 *
 * return 1 if successful, 0 otherwise
 */
int NfsClient::Quit()
{
	if (nfs != nullptr)
	{
		nfs_umount(nfs);
		nfs_destroy_context(nfs);
		nfs = nullptr;
	}
	connected = false;
	return 1;
}

/*
 * Mkdir - create a directory at server
 *
 * return 1 if successful, 0 otherwise
 */
int NfsClient::Mkdir(const std::string &ppath)
{
	int ret = nfs_mkdir(nfs, ppath.c_str());
	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}
	return 1;
}

/*
 * Rmdir - remove directory and all files under directory at remote
 *
 * return 1 if successful, 0 otherwise
 */
int NfsClient::_Rmdir(const std::string &ppath)
{
	int ret = nfs_rmdir(nfs, ppath.c_str());
	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}
	return 1;
}

/*
 * Rmdir - remove directory and all files under directory at remote
 *
 * return 1 if successful, 0 otherwise
 */
int NfsClient::Rmdir(const std::string &path, bool recursive)
{
	if (stop_activity)
		return 1;

	std::vector<DirEntry> list = ListDir(path);
	int ret;
	for (int i = 0; i < list.size(); i++)
	{
		if (stop_activity)
			return 1;

		if (list[i].isDir && recursive)
		{
			if (strcmp(list[i].name, "..") == 0)
				continue;
			ret = Rmdir(list[i].path, recursive);
			if (ret == 0)
			{
				sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DEL_DIR_MSG], list[i].path);
				return 0;
			}
		}
		else
		{
			sprintf(activity_message, "%s %s\n", lang_strings[STR_DELETING], list[i].path);
			ret = Delete(list[i].path);
			if (ret == 0)
			{
				sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DEL_FILE_MSG], list[i].path);
				return 0;
			}
		}
	}
	ret = _Rmdir(path);
	if (ret == 0)
	{
		sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DEL_DIR_MSG], path.c_str());
		return 0;
	}

	return 1;
}

/*
 * Get - issue a GET command and write received data to output
 *
 * return 1 if successful, 0 otherwise
 */

int NfsClient::Get(const std::string &outputfile, const std::string &ppath, uint64_t offset)
{
	if (!Size(ppath.c_str(), &bytes_to_download))
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}

	struct nfsfh *nfsfh = nullptr;
	int ret = nfs_open(nfs, ppath.c_str(), 0400, &nfsfh);
	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}

	FILE* out = FS::Create(outputfile);
	if (out == NULL)
	{
		sprintf(response, "%s", lang_strings[STR_FAILED]);
		return 0;
	}

	void *buff = malloc(BUF_SIZE);
	int count = 0;
	bytes_transfered = 0;
	while ((count = nfs_read(nfs, nfsfh, BUF_SIZE, buff)) > 0)
	{
		if (count < 0)
		{
			sprintf(response, "%s", nfs_get_error(nfs));
			FS::Close(out);
			nfs_close(nfs, nfsfh);
			free((void*)buff);
			return 0;
		}
		FS::Write(out, buff, count);
		bytes_transfered += count;
	}
	FS::Close(out);
	nfs_close(nfs, nfsfh);
	free((void*)buff);

	return 1;
}

int NfsClient::GetRange(const std::string &path, DataSink &sink, uint64_t size, uint64_t offset)
{
	struct nfsfh *nfsfh = nullptr;
	int ret = nfs_open(nfs, path.c_str(), 0400, &nfsfh);
	if (ret != 0)
	{
		return 0;
	}

	ret = this->GetRange((void *)nfsfh, sink, size, offset);
	nfs_close(nfs, nfsfh);

	return ret;
}

int NfsClient::GetRange(void *fp, DataSink &sink, uint64_t size, uint64_t offset)
{
	struct nfsfh *nfsfh = (struct nfsfh *)fp;

	int ret = nfs_lseek(nfs, nfsfh, offset, SEEK_SET, NULL);
	if (ret != 0)
	{
		return 0;
	}

	void *buff = malloc(BUF_SIZE);
    int count = 0;
    size_t bytes_remaining = size;
    do
    {
        size_t bytes_to_read = std::min<size_t>(BUF_SIZE, bytes_remaining);
        count = nfs_read(nfs, nfsfh, bytes_to_read, buff);
        if (count > 0)
        {
            bytes_remaining -= count;
            bool ok = sink.write((char*)buff, count);
			if (!ok)
			{
				free((void *)buff);
				return 0;
			}
        }
        else
        {
            break;
        }
    } while (1);

    free((void *)buff);
	return 1;
}

int NfsClient::GetRange(const std::string &ppath, void *buffer, uint64_t size, uint64_t offset)
{
	struct nfsfh *nfsfh = nullptr;
	int ret = nfs_open(nfs, ppath.c_str(), 0400, &nfsfh);
	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}

	ret = this->GetRange(nfsfh, buffer, size, offset);
	nfs_close(nfs, nfsfh);

	return ret;
}

int NfsClient::GetRange(void *fp, void *buffer, uint64_t size, uint64_t offset)
{
	struct nfsfh *nfsfh = (struct nfsfh *) fp;

	int ret = nfs_lseek(nfs, nfsfh, offset, SEEK_SET, NULL);
	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}

	int count = nfs_read(nfs, nfsfh, size, buffer);
	if (count != size)
		return 0;

	return 1;
}

int NfsClient::Copy(const std::string &ffrom, const std::string &tto)
{
	sprintf(response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
	return 0;
}

int NfsClient::Move(const std::string &ffrom, const std::string &tto)
{
	sprintf(response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
	return 0;
}

bool NfsClient::FileExists(const std::string &ppath)
{
	nfs_stat_64 st;
	int ret = nfs_stat64(nfs, ppath.c_str(), &st);
	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}
	return true;
}

/*
 * Put - issue a PUT command and send data from input
 *
 * return 1 if successful, 0 otherwise
 */
int NfsClient::Put(const std::string &inputfile, const std::string &ppath, uint64_t offset)
{
	bytes_to_download = FS::GetSize(inputfile);
	if (bytes_to_download < 0)
	{
		sprintf(response, "%s", lang_strings[STR_FAILED]);
		return 0;
	}

	FILE* in = FS::OpenRead(inputfile);
	if (in == NULL)
	{
		sprintf(response, "%s", lang_strings[STR_FAILED]);
		return 0;
	}
	
	struct nfsfh *nfsfh = nullptr;
	int ret;
	if (!FileExists(ppath))
		ret = nfs_creat(nfs, ppath.c_str(), 0660, &nfsfh);
	else
	{
		ret = nfs_open(nfs, ppath.c_str(), 0660, &nfsfh);
	}

	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}

	void* buff = malloc(BUF_SIZE);
	uint64_t count = 0;
	bytes_transfered = 0;
	while ((count = FS::Read(in, buff, BUF_SIZE)) > 0)
	{
		if (count < 0)
		{
			sprintf(response, "%s", lang_strings[STR_FAILED]);
			FS::Close(in);
			nfs_close(nfs, nfsfh);
			free(buff);
			return 0;
		}

		ret = nfs_write(nfs, nfsfh, count, buff);
		if (ret < 0)
		{
			sprintf(response, "%s", nfs_get_error(nfs));
			FS::Close(in);
			nfs_close(nfs, nfsfh);
			free(buff);
			return 0;
		}
		bytes_transfered += count;
	}
	FS::Close(in);
	nfs_close(nfs, nfsfh);
	free(buff);

	return 1;
}

int NfsClient::Rename(const std::string &src, const std::string &dst)
{
	int ret = nfs_rename(nfs, src.c_str(), dst.c_str());
	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}
	return 1;
}

int NfsClient::Delete(const std::string &ppath)
{
	int ret = nfs_unlink(nfs, ppath.c_str());
	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}
	return 1;
}

int NfsClient::Size(const std::string &ppath, int64_t *size)
{
	nfs_stat_64 st;
	int ret = nfs_stat64(nfs, ppath.c_str(), &st);
	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}
	*size = st.nfs_size;
	return 1;
}

std::vector<DirEntry> NfsClient::ListDir(const std::string &path)
{
	std::vector<DirEntry> out;
	DirEntry entry;
	Util::SetupPreviousFolder(path, &entry);
	out.push_back(entry);

	struct nfsdir *nfsdir;
	struct nfsdirent *nfsdirent;

	int ret = nfs_opendir(nfs, path.c_str(), &nfsdir);
	if (ret != 0) {
		sprintf(response, "%s", nfs_get_error(nfs));
		return out;
	}

	while ((nfsdirent = nfs_readdir(nfs, nfsdir)))
	{
		DirEntry entry;
		memset(&entry, 0, sizeof(entry));

		if (!show_hidden_files && nfsdirent->name[0] == '.')
			continue;

		entry.selectable = true;
		snprintf(entry.directory, 511, "%s", path.c_str());
		snprintf(entry.name, 255, "%s", nfsdirent->name);
		if (path.length() > 0 && path[path.length() - 1] == '/')
		{
			sprintf(entry.path, "%s%s", path.c_str(), nfsdirent->name);
		}
		else
		{
			sprintf(entry.path, "%s/%s", path.c_str(), nfsdirent->name);
		}

		entry.file_size = nfsdirent->size;
		struct tm tm = *localtime(&nfsdirent->mtime.tv_sec);
		OrbisDateTime gmt;
		OrbisDateTime lt;

		gmt.day = tm.tm_mday;
		gmt.month = tm.tm_mon + 1;
		gmt.year = tm.tm_year + 1900;
		gmt.hour = tm.tm_hour;
		gmt.minute = tm.tm_min;
		gmt.second = tm.tm_sec;

		convertUtcToLocalTime(&gmt, &lt);

		entry.modified.day = lt.day;
		entry.modified.month = lt.month;
		entry.modified.year = lt.year;
		entry.modified.hours = lt.hour;
		entry.modified.minutes = lt.minute;
		entry.modified.seconds = lt.second;

		switch (nfsdirent->mode & S_IFMT)
		{
		case S_IFLNK:
			entry.isLink = true;
			entry.file_size = 0;
			sprintf(entry.display_size, "%s", lang_strings[STR_LINK]);
			break;
		case S_IFREG:
			DirEntry::SetDisplaySize(&entry);
			break;
		case S_IFDIR:
			entry.isDir = true;
			entry.file_size = 0;
			sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);
			break;
		default:
			continue;
			break;
		}
		if (strcmp(entry.name, "..") != 0 && strcmp(entry.name, ".") != 0)
			out.push_back(entry);

	}
	nfs_closedir(nfs, nfsdir);

	return out;
}

std::string NfsClient::GetPath(std::string ppath1, std::string ppath2)
{
	std::string path1 = ppath1;
	std::string path2 = ppath2;
	path1 = Util::Trim(Util::Trim(path1, " "), "/");
	path2 = Util::Trim(Util::Trim(path2, " "), "/");
	path1 = "/" + path1 + "/" + path2;
	return path1;
}

int NfsClient::Head(const std::string &ppath, void *buffer, uint64_t len)
{
	if (!FileExists(ppath))
	{
		return 0;
	}

	struct nfsfh *nfsfh = nullptr;
	int ret = nfs_open(nfs, ppath.c_str(), 0400, &nfsfh);
	if (ret != 0)
	{
		sprintf(response, "%s", nfs_get_error(nfs));
		return 0;
	}

	int count = nfs_read(nfs, nfsfh, len, buffer);
	nfs_close(nfs, nfsfh);
	if (count != len)
		return 0;

	return 1;
}

void *NfsClient::Open(const std::string &path, int flags)
{
	struct nfsfh *nfsfh = nullptr;
    nfs_open(nfs, path.c_str(), 0400, &nfsfh);;
	return nfsfh;
}

void NfsClient::Close(void *fp)
{
    nfs_close(nfs, (struct nfsfh*)fp);
}

ClientType NfsClient::clientType()
{
	return CLIENT_TYPE_NFS;
}

uint32_t NfsClient::SupportedActions()
{
	return REMOTE_ACTION_ALL ^ REMOTE_ACTION_CUT ^ REMOTE_ACTION_COPY ^ REMOTE_ACTION_PASTE;
}
