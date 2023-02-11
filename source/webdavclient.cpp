#include <errno.h>
#include <unistd.h>
#include <cstring>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include "lang.h"
#include "webdav/client.hpp"
#include "webdavclient.h"
#include "windows.h"
#include "util.h"
#include "rtc.h"

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static const char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

namespace WebDAV
{
	static int DownloadCallback(void *context, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
	{
		int64_t *bytes_transfered = (int64_t *)context;
		*bytes_transfered = reinterpret_cast<int64_t>(dlnow);
		return CURLE_OK;
	}

	static int UploadCallback(void *context, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
	{
		int64_t *bytes_transfered = (int64_t *)context;
		*bytes_transfered = reinterpret_cast<int64_t>(ulnow);
		return CURLE_OK;
	}

	int WebDavClient::Connect(const std::string &host, const std::string &user, const std::string &pass)
	{
		return Connect(host, user, pass, true);
	}

	WebDavClient::WebDavClient() {};

	int WebDavClient::Connect(const std::string &host, const std::string &user, const std::string &pass, bool check_enabled)
	{
		std::string url = std::string(host);
		std::size_t scheme_pos = url.find_first_of("://");
		std::string root_folder = "/";
		if (scheme_pos != std::string::npos)
		{
			std::size_t root_folder_pos = url.find_first_of("/", scheme_pos + 3);
			if (root_folder_pos != std::string::npos)
			{
				root_folder = url.substr(root_folder_pos);
				url = url.substr(0, root_folder_pos);
			}
		}
		WebDAV::dict_t options = {
			{"webdav_hostname", url},
			{"webdav_root", root_folder},
			{"webdav_username", user},
			{"webdav_password", pass},
			{"check_enabled", check_enabled ? "1" : "0"}};
		client = new WebDAV::Client(options);
		connected = true;

		return 1;
	}

	/*
	 * LastResponse - return a pointer to the last response received
	 */
	const char *WebDavClient::LastResponse()
	{
		return (const char *)response;
	}

	/*
	 * IsConnected - return true if connected to remote
	 */
	bool WebDavClient::IsConnected()
	{
		return connected;
	}

	/*
	 * Ping - return true if connected to remote
	 */
	bool WebDavClient::Ping()
	{
		connected = client->check();
		sprintf(response, "Http Code %ld", client->status_code());
		return connected;
	}

	/*
	 * Quit - disconnect from remote
	 *
	 * return 1 if successful, 0 otherwise
	 */
	int WebDavClient::Quit()
	{
		if (client != NULL)
			delete (client);
		client = NULL;
		connected = false;
		return 1;
	}

	/*
	 * Mkdir - create a directory at server
	 *
	 * return 1 if successful, 0 otherwise
	 */
	int WebDavClient::Mkdir(const std::string &ppath)
	{
		bool ret = client->create_directory(ppath);
		sprintf(response, "Http Code %ld", client->status_code());
		return ret;
	}

	/*
	 * Rmdir - remove directory and all files under directory at remote
	 *
	 * return 1 if successful, 0 otherwise
	 */
	int WebDavClient::_Rmdir(const std::string &ppath)
	{
		bool ret = client->clean(ppath);
		sprintf(response, "Http Code %ld", client->status_code());
		return ret;
	}

	/*
	 * Rmdir - remove directory and all files under directory at remote
	 *
	 * return 1 if successful, 0 otherwise
	 */
	int WebDavClient::Rmdir(const std::string &path, bool recursive)
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

	int WebDavClient::Get(const std::string &outputfile, const std::string &ppath, uint64_t offset)
	{
		bool ret = client->download(ppath, outputfile, &bytes_transfered, DownloadCallback);
		sprintf(response, "Http Code %ld", client->status_code());
		return ret;
	}

	bool WebDavClient::FileExists(const std::string &ppath)
	{
		std::string path = ppath;
		path = Util::Ltrim(path, "/");
		bool ret = client->check(path);
		sprintf(response, "Http Code %ld", client->status_code());
		return ret;
	}

	/*
	 * Put - issue a PUT command and send data from input
	 *
	 * return 1 if successful, 0 otherwise
	 */
	int WebDavClient::Put(const std::string &inputfile, const std::string &ppath, uint64_t offset)
	{
		bool ret = client->upload(ppath, inputfile, &bytes_transfered, UploadCallback);
		sprintf(response, "Http Code %ld", client->status_code());
		return ret;
	}

	int WebDavClient::Rename(const std::string &src, const std::string &dst)
	{
		bool ret = client->move(src, dst);
		sprintf(response, "Http Code %ld", client->status_code());
		return ret;
	}

	int WebDavClient::Delete(const std::string &ppath)
	{
		bool ret = client->clean(ppath);
		sprintf(response, "Http Code %ld", client->status_code());
		return ret;
	}

	int WebDavClient::Size(const std::string &ppath, int64_t *size)
	{
		WebDAV::dict_t file_info = client->info(ppath);
		std::string file_size = WebDAV::get(file_info, "size");
		if (file_size.empty())
			return 0;
		*size = std::stoll(file_size);
		return 1;
	}

	std::vector<DirEntry> WebDavClient::ListDir(const std::string &path)
	{
		std::vector<DirEntry> out;
		DirEntry entry;
		memset(&entry, 0, sizeof(DirEntry));
		if (path.length() > 1 && path[path.length() - 1] == '/')
		{
			strlcpy(entry.directory, path.c_str(), path.length() - 1);
		}
		else
		{
			sprintf(entry.directory, "%s", path.c_str());
		}
		sprintf(entry.name, "..");
		sprintf(entry.path, "%s", entry.directory);
		sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);
		entry.file_size = 0;
		entry.isDir = true;
		entry.selectable = false;
		out.push_back(entry);

		WebDAV::dict_items_t files = client->list(path);
		for (int i = 0; i < files.size(); i++)
		{
			DirEntry entry;
			memset(&entry, 0, sizeof(entry));
			entry.selectable = true;
			sprintf(entry.directory, "%s", path.c_str());
			sprintf(entry.name, "%s", WebDAV::get(files[i], "name").c_str());

			if (path.length() == 1 and path[0] == '/')
			{
				sprintf(entry.path, "%s%s", path.c_str(), WebDAV::get(files[i], "name").c_str());
			}
			else
			{
				sprintf(entry.path, "%s/%s", path.c_str(), WebDAV::get(files[i], "name").c_str());
			}

			std::string resource_type = WebDAV::get(files[i], "type");
			entry.isDir = resource_type.find("collection") != std::string::npos;
			entry.file_size = 0;
			if (!entry.isDir)
			{
				entry.file_size = std::stoll(WebDAV::get(files[i], "size"));
				if (entry.file_size < 1024)
				{
					sprintf(entry.display_size, "%luB", entry.file_size);
				}
				else if (entry.file_size < 1024 * 1024)
				{
					sprintf(entry.display_size, "%.2fKB", entry.file_size * 1.0f / 1024);
				}
				else if (entry.file_size < 1024 * 1024 * 1024)
				{
					sprintf(entry.display_size, "%.2fMB", entry.file_size * 1.0f / (1024 * 1024));
				}
				else
				{
					sprintf(entry.display_size, "%.2fGB", entry.file_size * 1.0f / (1024 * 1024 * 1024));
				}
			}
			else
			{
				sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);
			}

			char modified_date[32];
			char *p_char = NULL;
			sprintf(modified_date, "%s", WebDAV::get(files[i], "modified").c_str());
			p_char = strchr(modified_date, ' ');
			if (p_char)
			{
				OrbisDateTime gmt;
				OrbisDateTime lt;
				char month[5];
				sscanf(p_char, "%hd %s %hd %hd:%hd:%hd", &gmt.day, month, &gmt.year, &gmt.hour, &gmt.minute, &gmt.second);
				for (int k = 0; k < 12; k++)
				{
					if (strcmp(month, months[k]) == 0)
					{
						gmt.month = k + 1;
						break;
					}
				}
				convertUtcToLocalTime(&gmt, &lt);
				entry.modified.day = lt.day;
				entry.modified.month = lt.month;
				entry.modified.year = lt.year;
				entry.modified.hours = lt.hour;
				entry.modified.minutes = lt.minute;
				entry.modified.seconds = lt.second;
			}
			out.push_back(entry);
		}

		return out;
	}

	std::string WebDavClient::GetPath(std::string ppath1, std::string ppath2)
	{
		std::string path1 = ppath1;
		std::string path2 = ppath2;
		path1 = Util::Rtrim(Util::Trim(path1, " "), "/");
		path2 = Util::Rtrim(Util::Trim(path2, " "), "/");
		path1 = path1 + "/" + path2;
		return path1;
	}

	int WebDavClient::Head(const std::string &path, void *buffer, int64_t len)
	{
		char *buffer_ptr = nullptr;
		unsigned long long buffer_size = 0;

		bool ret = client->download_range_to(path, buffer_ptr, buffer_size, 0, len - 1);
		sprintf(response, "Http Code %ld", client->status_code());
		if (buffer_size != len)
		{
			return 0;
		}
		memcpy(buffer, buffer_ptr, len);
		return 1;
	}

	bool WebDavClient::GetHeaders(const std::string &path, dict_t *headers)
	{
		return client->head(path, headers);
	}

	WebDAV::Client *WebDavClient::GetClient()
	{
		return this->client;
	}

	ClientType WebDavClient::clientType()
	{
		return CLIENT_TYPE_WEBDAV;
	}
}