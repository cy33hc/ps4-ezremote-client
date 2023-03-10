#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include "common.h"
#include "clients/remote_client.h"
#include "clients/sftpclient.h"
#include "fs.h"
#include "lang.h"
#include "util.h"
#include "windows.h"
#include "system.h"

#define FTP_CLIENT_BUFSIZ 1048576

SFTPClient::SFTPClient()
{
    session = nullptr;
    sftp_session = nullptr;
    sock = 0;
};

SFTPClient::~SFTPClient(){};

int SFTPClient::Connect(const std::string &url, const std::string &username, const std::string &password)
{
    int port = 22;
    std::string host = url.substr(7);
    size_t colon_pos = host.find(":");
    if (colon_pos != std::string::npos)
    {
        port = std::atoi(host.substr(colon_pos + 1).c_str());
        host = host.substr(0, colon_pos);
    }

    struct hostent *he;
    struct in_addr **addr_list;
    char ip[20];
    int i;

    if (strcmp(host.c_str(), "localhost") == 0)
    {
        sprintf(ip, "%s", "127.0.0.1");
    }
    else
    {
        if ((he = gethostbyname(host.c_str())) == NULL)
        {
            sprintf(this->response, "%s", lang_strings[STR_COULD_NOT_RESOLVE_HOST]);
            return 0;
        }

        addr_list = (struct in_addr **)he->h_addr_list;
        for (i = 0; addr_list[i] != NULL; i++)
        {
            strcpy(ip, inet_ntoa(*addr_list[i]));
            break;
        }
    }

    in_addr dst_addr;
    sockaddr_in server_addr;
    int on = 1;
    int32_t retval;

    memset(&server_addr, 0, sizeof(server_addr));
    inet_pton(AF_INET, ip, (void *)&dst_addr);
    server_addr.sin_addr = dst_addr;
    server_addr.sin_port = htons(port);
    server_addr.sin_family = AF_INET;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&on, sizeof(on));
    int const size = FTP_CLIENT_BUFSIZ;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == -1)
    {
        close(sock);
        return 0;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == -1)
    {
        close(sock);
        return 0;
    }

    if (connect(sock, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr_in)) != 0)
    {
        sprintf(this->response, "%s", "Failed to connect!");
        return 0;
    }
    /* Create a session instance
     */
    session = libssh2_session_init();
    libssh2_session_set_blocking(session, 1);
    libssh2_keepalive_config(session, 1, 5);

    if (!session)
    {
        sprintf(this->response, "%s", lang_strings[STR_FAILED]);
        return 0;
    }

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    sceKernelUsleep(100000);
    int rc = libssh2_session_handshake(session, sock);
    if (rc)
    {
        sprintf(this->response, "Failed SSL handshake %d", rc);
        return 0;
    }

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    const char *fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);

    /* check what authentication methods are available */
    char *userauthlist = libssh2_userauth_list(session, username.c_str(), username.length());

    int auth_pw = 0;
    if (strstr(userauthlist, "password") != NULL)
    {
        auth_pw |= 1;
    }
    if (strstr(userauthlist, "keyboard-interactive") != NULL)
    {
        auth_pw |= 2;
    }
    if (strstr(userauthlist, "publickey") != NULL)
    {
        auth_pw |= 4;
    }

    bool use_identity = password.find("file://") != std::string::npos;
    if (auth_pw & 1 && !use_identity)
    {
        /* We could authenticate via password */
        if (libssh2_userauth_password(session, username.c_str(), password.c_str()))
        {
            sprintf(this->response, "%s", "Authentication by password failed!");
            goto shutdown;
        }
    }
    else if (auth_pw & 4 && use_identity)
    {
        /* Or by public key */
        std::string publickey = password.substr(7) + "/id_rsa.pub";
        std::string privatekey = password.substr(7) + "/id_rsa";
        if (!FS::FileExists(publickey.c_str()))
        {
            sprintf(response, "SSH public key %s is not found", publickey.c_str());
            goto shutdown;
        }
        if (!FS::FileExists(privatekey.c_str()))
        {
            sprintf(response, "SSH private key %s is not found", privatekey.c_str());
            goto shutdown;
        }
        if (libssh2_userauth_publickey_fromfile(session, username.c_str(), publickey.c_str(), privatekey.c_str(), ""))
        {
            sprintf(this->response, "%s", "Authentication by public key failed!");
            goto shutdown;
        }
    }
    else
    {
        sprintf(this->response, "%s", "No supported authentication methods found!");
        goto shutdown;
    }

    sftp_session = libssh2_sftp_init(session);
    this->connected = true;
    return 1;

shutdown:
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    close(sock);
    libssh2_exit();
    session = nullptr;
    sock = 0;
    return 0;
}

int SFTPClient::Mkdir(const std::string &path)
{
    int rc = libssh2_sftp_mkdir(sftp_session, path.c_str(),
                                LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP | LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH);
    if (rc)
    {
        return 0;
    }
    return 1;
}

int SFTPClient::Rmdir(const std::string &path, bool recursive)
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
    ret = Rmdir(path);
    return 1;
}

int SFTPClient::Rmdir(const std::string &path)
{
    int rc = libssh2_sftp_rmdir(sftp_session, path.c_str());
    if (rc)
    {
        return 0;
    }
    return 1;
}

int SFTPClient::Size(const std::string &path, int64_t *size)
{
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session, path.c_str(), &attrs);
    if (rc)
    {
        return 0;
    }
    *size = attrs.filesize;
    return 1;
}

int SFTPClient::Get(const std::string &outputfile, const std::string &path, uint64_t offset)
{
    if (!Size(path, &bytes_to_download))
    {
        return 0;
    }

    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_open(sftp_session, path.c_str(), LIBSSH2_FXF_READ, 0);
    if (!sftp_handle)
    {
        sprintf(response, "Unable to open file with SFTP: %ld", libssh2_sftp_last_error(sftp_session));
        return 0;
    }

    FILE *out = FS::Create(outputfile);
    if (out == NULL)
    {
        sprintf(response, "%s", lang_strings[STR_FAILED]);
        return 0;
    }

    char *buff = (char *)malloc(FTP_CLIENT_BUFSIZ);
    int rc, count = 0;
    bytes_transfered = 0;
    do
    {
        rc = libssh2_sftp_read(sftp_handle, buff, FTP_CLIENT_BUFSIZ);
        if (rc > 0)
        {
            bytes_transfered += rc;
            FS::Write(out, buff, rc);
        }
        else
        {
            break;
        }
    } while (1);
    FS::Close(out);
    libssh2_sftp_close(sftp_handle);

    return 1;
}

int SFTPClient::Put(const std::string &inputfile, const std::string &path, uint64_t offset)
{
    char *ptr, *buff;
    int rc;

    bytes_to_download = FS::GetSize(inputfile);
    if (bytes_to_download < 0)
    {
        sprintf(response, "%s", lang_strings[STR_FAILED]);
        return 0;
    }

    FILE *in = FS::OpenRead(inputfile);
    if (in == NULL)
    {
        sprintf(response, "%s", lang_strings[STR_FAILED]);
        return 0;
    }

    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_open(sftp_session, path.c_str(),
                                                         LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
                                                         LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR |
                                                             LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);

    if (!sftp_handle)
    {
        sprintf(response, "%s", "Unable to open file with SFTP");
        return 0;
    }

    buff = (char *)malloc(FTP_CLIENT_BUFSIZ);
    int nread, count = 0;
    bytes_transfered = 0;
    do
    {
        nread = FS::Read(in, buff, FTP_CLIENT_BUFSIZ);
        if (nread <= 0)
        {
            /* end of file */
            break;
        }
        ptr = buff;

        do
        {
            /* write data in a loop until we block */
            rc = libssh2_sftp_write(sftp_handle, ptr, nread);

            if (rc < 0)
                break;
            ptr += rc;
            nread -= rc;
            bytes_transfered += rc;
        } while (nread);
    } while (rc > 0);

    libssh2_sftp_close(sftp_handle);
    FS::Close(in);
    free(buff);
    return 1;
}

int SFTPClient::Rename(const std::string &src, const std::string &dst)
{
    int rc = libssh2_sftp_rename_ex(sftp_session, src.c_str(), src.length(),
                                    dst.c_str(), dst.length(), LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE);
    if (rc)
    {
        return 0;
    }
    return 1;
}

int SFTPClient::Delete(const std::string &path)
{
    int rc = libssh2_sftp_unlink(sftp_session, path.c_str());
    if (rc)
    {
        return 0;
    }
    return 1;
}

int SFTPClient::Copy(const std::string &from, const std::string &to)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int SFTPClient::Move(const std::string &from, const std::string &to)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int SFTPClient::Head(const std::string &path, void *buffer, uint64_t len)
{
	if (!Size(path.c_str(), &bytes_to_download))
	{
		return 0;
	}

    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_open(sftp_session, path.c_str(), LIBSSH2_FXF_READ, 0);
    if (!sftp_handle)
    {
        return 0;
    }

	int count = libssh2_sftp_read(sftp_handle, (char*)buffer, len);
	libssh2_sftp_close(sftp_handle);
	if (count != len)
		return 0;

	return 1;
}

bool SFTPClient::FileExists(const std::string &path)
{
    int64_t file_size;
    return Size(path, &file_size);
}

std::vector<DirEntry> SFTPClient::ListDir(const std::string &path)
{
    std::vector<DirEntry> out;
    DirEntry entry;
    Util::SetupPreviousFolder(path, &entry);
    out.push_back(entry);

    /* Request a dir listing via SFTP */
    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_opendir(sftp_session, path.c_str());
    if (!sftp_handle)
    {
        return out;
    }

    do
    {
        char mem[512];
        LIBSSH2_SFTP_ATTRIBUTES attrs;
        DirEntry entry;
        memset(&entry, 0, sizeof(entry));
        entry.selectable = true;

        /* loop until we fail */
        int rc = libssh2_sftp_readdir(sftp_handle, mem, sizeof(mem), &attrs);
        if (rc > 0)
        {
            std::string new_path = std::string(mem, rc);
            if (new_path.compare(".") == 0 || new_path.compare("..") == 0)
                continue;
            ;

            sprintf(entry.name, "%s", new_path.c_str());
            sprintf(entry.directory, "%s", path.c_str());
            if (path.length() > 0 && path[path.length() - 1] == '/')
            {
                sprintf(entry.path, "%s%s", path.c_str(), entry.name);
            }
            else
            {
                sprintf(entry.path, "%s/%s", path.c_str(), entry.name);
            }

            if (LIBSSH2_SFTP_S_ISDIR(attrs.permissions))
            {
                entry.isDir = true;
                sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);
            }
            else if (LIBSSH2_SFTP_S_ISREG(attrs.permissions))
            {
                entry.file_size = attrs.filesize;
                DirEntry::SetDisplaySize(&entry);
                entry.isDir = false;
            }
            else if (LIBSSH2_SFTP_S_ISLNK(attrs.permissions))
            {
                entry.file_size = 0;
                sprintf(entry.display_size, "%s", lang_strings[STR_LINK]);
                entry.isDir = false;
                entry.isLink = true;
                entry.selectable = false;
            }
            else // skip any files that aren't regular files/directory
            {
                entry.file_size = attrs.filesize;
                DirEntry::SetDisplaySize(&entry);
                entry.isDir = false;
                entry.selectable = false;
            }

            struct tm tm = *localtime((const time_t*)&attrs.mtime);
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

            out.push_back(entry);
        }
        else
            break;

    } while (1);

    return out;
}

std::string SFTPClient::GetPath(std::string ppath1, std::string ppath2)
{
    std::string path1 = ppath1;
    std::string path2 = ppath2;
    path1 = Util::Rtrim(Util::Trim(path1, " "), "/");
    path2 = Util::Rtrim(Util::Trim(path2, " "), "/");
    path1 = path1 + "/" + path2;
    return path1;
}

bool SFTPClient::IsConnected()
{
    return this->connected;
}

bool SFTPClient::Ping()
{
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session, "/", &attrs);
    if (rc)
    {
        this->connected = false;
        return false;
    }

    return true;
}

const char *SFTPClient::LastResponse()
{
    return this->response;
}

int SFTPClient::Quit()
{
    if (sftp_session != nullptr)
        libssh2_sftp_shutdown(sftp_session);
    if (session != nullptr)
    {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        close(sock);
        libssh2_exit();
    }
    session = nullptr;
    sftp_session = nullptr;
    sock = 0;
    return 1;
}

ClientType SFTPClient::clientType()
{
    return CLIENT_TYPE_FTP;
}

uint32_t SFTPClient::SupportedActions()
{
    return REMOTE_ACTION_ALL ^ REMOTE_ACTION_CUT ^ REMOTE_ACTION_COPY ^ REMOTE_ACTION_PASTE;
}