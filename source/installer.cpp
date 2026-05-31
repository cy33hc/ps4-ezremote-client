#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <json-c/json.h>
#include <orbis/libkernel.h>
#include <orbis/Bgft.h>
#include <orbis/AppInstUtil.h>
#include <orbis/UserService.h>
#include <orbis/SystemService.h>
#include "clients/webdav.h"
#include "clients/remote_client.h"
#include "clients/smbclient.h"
#include "clients/sftpclient.h"
#include "clients/ftpclient.h"
#include "clients/github.h"
#include "clients/nfsclient.h"
#include "clients/webdav.h"
#include "clients/apache.h"
#include "clients/archiveorg.h"
#include "clients/iis.h"
#include "clients/github.h"
#include "clients/myrient.h"
#include "clients/nginx.h"
#include "clients/npxserve.h"
#include "clients/rclone.h"

#include "server/http_server.h"
#include "installer.h"
#include "util.h"
#include "config.h"
#include "windows.h"
#include "lang.h"
#include "system.h"
#include "fs.h"
#include "sfo.h"

#define SERVER_ELF_PATH "/mnt/sandbox/pfsmnt/RMTC00001-app0/daemon/daemon.elf"

#define BGFT_HEAP_SIZE (1 * 1024 * 1024)

struct BgProgressCheck
{
	ArchivePkgInstallData *archive_pkg_data;
	SplitPkgInstallData *split_pkg_data;
	int task_id;
	std::string hash;
};

static OrbisBgftInitParams s_bgft_init_params;

static bool s_bgft_initialized = false;

static std::map<std::string, ArchivePkgInstallData *> archive_pkg_install_data_list;
static std::map<std::string, SplitPkgInstallData *> split_pkg_install_data_list;

namespace INSTALLER
{
	static int FtpCallback(int64_t xfered, void *arg)
	{
		return 1;
	}

	int Init(void)
	{
		int ret;

		if (s_bgft_initialized)
		{
			goto done;
		}

		memset(&s_bgft_init_params, 0, sizeof(s_bgft_init_params));
		{
			s_bgft_init_params.heapSize = BGFT_HEAP_SIZE;
			s_bgft_init_params.heap = (uint8_t *)malloc(s_bgft_init_params.heapSize);
			if (!s_bgft_init_params.heap)
			{
				goto err;
			}
			memset(s_bgft_init_params.heap, 0, s_bgft_init_params.heapSize);
		}

		ret = sceBgftServiceIntInit(&s_bgft_init_params);
		if (ret)
		{
			goto err_bgft_heap_free;
		}

		s_bgft_initialized = true;

	done:
		return 0;

	err_bgft_heap_free:
		if (s_bgft_init_params.heap)
		{
			free(s_bgft_init_params.heap);
			s_bgft_init_params.heap = NULL;
		}

		memset(&s_bgft_init_params, 0, sizeof(s_bgft_init_params));

	err:
		s_bgft_initialized = false;

		return -1;
	}

	void Exit(void)
	{
		int ret;

		if (!s_bgft_initialized)
		{
			return;
		}

		ret = sceBgftServiceIntTerm();

		if (s_bgft_init_params.heap)
		{
			free(s_bgft_init_params.heap);
			s_bgft_init_params.heap = NULL;
		}

		memset(&s_bgft_init_params, 0, sizeof(s_bgft_init_params));

		s_bgft_initialized = false;
	}

	std::string GetRemotePkgTitle(RemoteClient *client, const std::string &path, pkg_header *header)
	{
		size_t entry_count = BE32(header->pkg_entry_count);
		uint32_t entry_table_offset = BE32(header->pkg_table_offset);
		uint64_t entry_table_size = entry_count * sizeof(pkg_table_entry);
		void *entry_table_data = malloc(entry_table_size);

		int ret = client->GetRange(path, entry_table_data, entry_table_size, entry_table_offset);
		if (ret == 0)
		{
			free(entry_table_data);
			return "";
		}

		pkg_table_entry *entries = (pkg_table_entry *)entry_table_data;
		void *param_sfo_data = nullptr;
		uint32_t param_sfo_offset = 0;
		uint32_t param_sfo_size = 0;
		for (size_t i = 0; i < entry_count; ++i)
		{
			if (BE32(entries[i].id) == PKG_ENTRY_ID__PARAM_SFO)
			{
				param_sfo_offset = BE32(entries[i].offset);
				param_sfo_size = BE32(entries[i].size);
				break;
			}
		}
		free(entry_table_data);

		std::string title;
		if (param_sfo_offset > 0 && param_sfo_size > 0)
		{
			param_sfo_data = malloc(param_sfo_size);
			int ret = client->GetRange(path, param_sfo_data, param_sfo_size, param_sfo_offset);
			if (ret)
			{
				const char *tmp_title = SFO::GetString((const char *)param_sfo_data, param_sfo_size, "TITLE");
				if (tmp_title != nullptr)
					title = std::string(tmp_title);
			}
			free(param_sfo_data);
		}

		return title;
	}

	std::string GetLocalPkgTitle(const std::string &path, pkg_header *header)
	{
		size_t entry_count = BE32(header->pkg_entry_count);
		uint32_t entry_table_offset = BE32(header->pkg_table_offset);
		uint64_t entry_table_size = entry_count * sizeof(pkg_table_entry);
		void *entry_table_data = malloc(entry_table_size);

		FILE *fd = FS::OpenRead(path);
		FS::Seek(fd, entry_table_offset);
		FS::Read(fd, entry_table_data, entry_table_size);

		pkg_table_entry *entries = (pkg_table_entry *)entry_table_data;
		void *param_sfo_data = NULL;
		uint32_t param_sfo_offset = 0;
		uint32_t param_sfo_size = 0;
		void *icon0_png_data = NULL;
		uint32_t icon0_png_offset = 0;
		uint32_t icon0_png_size = 0;
		for (size_t i = 0; i < entry_count; ++i)
		{
			if (BE32(entries[i].id) == PKG_ENTRY_ID__PARAM_SFO)
			{
				param_sfo_offset = BE32(entries[i].offset);
				param_sfo_size = BE32(entries[i].size);
				break;
			}
		}
		free(entry_table_data);

		std::string title;
		if (param_sfo_offset > 0 && param_sfo_size > 0)
		{
			param_sfo_data = malloc(param_sfo_size);
			FS::Seek(fd, param_sfo_offset);
			FS::Read(fd, param_sfo_data, param_sfo_size);
			const char *tmp_title = SFO::GetString((const char *)param_sfo_data, param_sfo_size, "TITLE");
			if (tmp_title != nullptr)
				title = std::string(tmp_title);
			free(param_sfo_data);
		}

		return title;
	}

	std::string StoreBgInstallHostData(RemoteSettings *settings, const std::string &path)
	{
		std::string hash = Util::UrlHash(settings->server + path + settings->username + settings->password + std::to_string(settings->type));
		json_object *history_item_obj = json_object_new_object();
		json_object_object_add(history_item_obj, "hash", json_object_new_string(hash.c_str()));
		json_object_object_add(history_item_obj, "url", json_object_new_string(settings->server));
		json_object_object_add(history_item_obj, "path", json_object_new_string(path.c_str()));
		json_object_object_add(history_item_obj, "username", json_object_new_string(settings->username));
		json_object_object_add(history_item_obj, "password", json_object_new_string(settings->password));
		json_object_object_add(history_item_obj, "type", json_object_new_int(settings->type));

		if (settings->type == CLIENT_TYPE_HTTP_SERVER)
		{
			json_object_object_add(history_item_obj, "http_server_type", json_object_new_string(settings->http_server_type));
		}

		const char *params_str = json_object_to_json_string(history_item_obj);
		
		httplib::Client client = httplib::Client(std::string("http://127.0.0.1:") + std::to_string(http_int_server_port));
		client.set_connection_timeout(1);

		if (auto res = client.Post("/store_bg_install_data", params_str, "application/json"))
		{
			if (HTTP_SUCCESS(res->status))
			{
	  		}
			else
			{
				return "";
			}
		}
		return hash;
	}

	std::string getRemoteUrl(const std::string path, bool encodeUrl)
	{
		if (remote_settings->type == CLIENT_TYPE_HTTP_SERVER && strcmp(remote_settings->http_server_type, HTTP_SERVER_GITHUB) == 0)
		{
			GithubClient *tmp_client = (GithubClient*) remoteclient;
			return tmp_client->GetDownloadUrl(path);
		}
		if ( strlen(remote_settings->username) == 0 &&
		     strlen(remote_settings->password) == 0 &&
			 (remote_settings->type == CLIENT_TYPE_WEBDAV ||
			   (remote_settings->type == CLIENT_TYPE_HTTP_SERVER && strcmp(remote_settings->http_server_type, HTTP_SERVER_ARCHIVEORG) == 0)
			  )
		   )
		{
			std::string full_url = WebDAVClient::GetHttpUrl(remote_settings->server + path);
			size_t scheme_pos = full_url.find("://");
			if (scheme_pos == std::string::npos)
				return "";
			size_t root_pos = full_url.find("/", scheme_pos + 3);
			std::string host = full_url.substr(0, root_pos);
			std::string path = full_url.substr(root_pos);

			if (encodeUrl)
			{
				path = httplib::detail::encode_url(path);
			}

			return host + path;
		}
		else
		{
			std::string hash = StoreBgInstallHostData(remote_settings, path);
			std::string full_url = std::string("http://127.0.0.1:") + std::to_string(http_int_server_port) + "/bg_install/" + hash;
			return full_url;
		}

		return "";
	}

	void *CheckBgInstallTaskThread(void *argp)
	{
		bool completed = false;
		OrbisBgftTaskProgress progress_info;
		BgProgressCheck *bg_check_data = (BgProgressCheck *)argp;
		int ret;

		while (!completed)
		{
			memset(&progress_info, 0, sizeof(progress_info));
			ret = sceBgftServiceDownloadGetProgress(bg_check_data->task_id, &progress_info);
			if (ret || (progress_info.transferred > 0 && progress_info.errorResult != 0))
			{
				goto finish;
			}
			if (progress_info.length > 0)
			{
				completed = progress_info.transferred == progress_info.length;
				bytes_to_download = progress_info.length;
				bytes_transfered = progress_info.transferred;
			}
			sceSystemServicePowerTick();
			sceKernelUsleep(500000);
		}
	finish:
		if (bg_check_data->archive_pkg_data != nullptr)
		{
			bg_check_data->archive_pkg_data->stop_write_thread = true;
			pthread_join(bg_check_data->archive_pkg_data->thread, NULL);
			delete (bg_check_data->archive_pkg_data->split_file);
			free(bg_check_data->archive_pkg_data);
			RemoveArchivePkgInstallData(bg_check_data->hash);
			free(bg_check_data);
		}
		else if (bg_check_data->split_pkg_data != nullptr)
		{
			bg_check_data->split_pkg_data->stop_write_thread = true;
			bg_check_data->split_pkg_data->split_file->Close();
			pthread_join(bg_check_data->split_pkg_data->thread, NULL);
			delete (bg_check_data->split_pkg_data->split_file);
			if (bg_check_data->split_pkg_data->delete_client)
				delete (bg_check_data->split_pkg_data->remote_client);
			free(bg_check_data->split_pkg_data);
			RemoveSplitPkgInstallData(bg_check_data->hash);
			free(bg_check_data);
		}
		activity_inprogess = false;
		file_transfering = false;
		Windows::SetModalMode(false);
		return nullptr;
	}

	bool canInstallRemotePkg(const std::string &url)
	{
		return true;
	}

	int InstallRemotePkg(const std::string &url, pkg_header *header, std::string title, bool prompt)
	{
		if (url.empty())
			return 0;

		int ret;
		std::string cid = std::string((char *)header->pkg_content_id);
		cid = cid.substr(cid.find_first_of("-") + 1, 9);
		std::string display_title = title.length() > 0 ? title : cid;
		int user_id;
		ret = sceUserServiceGetForegroundUser(&user_id);
		const char *package_type;
		uint32_t content_type = BE32(header->pkg_content_type);
		uint32_t flags = BE32(header->pkg_content_flags);
		bool is_patch = false;
		bool completed = false;

		switch (content_type)
		{
		case PKG_CONTENT_TYPE_GD:
			package_type = "PS4GD";
			break;
		case PKG_CONTENT_TYPE_AC:
			package_type = "PS4AC";
			break;
		case PKG_CONTENT_TYPE_AL:
			package_type = "PS4AL";
			break;
		case PKG_CONTENT_TYPE_DP:
			package_type = "PS4DP";
			break;
		default:
			package_type = NULL;
			return 0;
			break;
		}

		if (flags & PKG_CONTENT_FLAGS_FIRST_PATCH ||
			flags & PKG_CONTENT_FLAGS_SUBSEQUENT_PATCH ||
			flags & PKG_CONTENT_FLAGS_DELTA_PATCH ||
			flags & PKG_CONTENT_FLAGS_CUMULATIVE_PATCH)
		{
			is_patch = true;
		}

		OrbisBgftTaskProgress progress_info;
		OrbisBgftDownloadParam params;
		memset(&params, 0, sizeof(params));
		{
			params.userId = user_id;
			params.entitlementType = 5;
			params.id = (char *)header->pkg_content_id;
			params.contentUrl = url.c_str();
			params.contentName = display_title.c_str();
			params.iconPath = "";
			params.playgoScenarioId = "0";
			params.option = ORBIS_BGFT_TASK_OPT_DISABLE_CDN_QUERY_PARAM;
			params.packageType = package_type;
			params.packageSubType = "";
			params.packageSize = BE64(header->pkg_size);
		}

	retry:
		int task_id = -1;
		if (!is_patch)
			ret = sceBgftServiceIntDownloadRegisterTask(&params, &task_id);
		else
			ret = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &task_id);
		if (ret == 0x80990088 || ret == 0x80990015)
		{
			if (prompt)
			{
				sprintf(confirm_message, "%s - %s?", display_title.c_str(), lang_strings[STR_REINSTALL_CONFIRM_MSG]);
				confirm_state = CONFIRM_WAIT;
				action_to_take = selected_action;
				activity_inprogess = false;
				while (confirm_state == CONFIRM_WAIT)
				{
					sceKernelUsleep(100000);
				}
				activity_inprogess = true;
				selected_action = action_to_take;

				if (confirm_state == CONFIRM_YES)
				{
					ret = sceAppInstUtilAppUnInstall(cid.c_str());
					if (ret != 0)
					{
						goto err;
					}
					goto retry;
				}
			}
			else
			{
				ret = sceAppInstUtilAppUnInstall(cid.c_str());
				if (ret != 0)
					goto err;
				goto retry;
			}
		}
		else if (ret > 0)
		{
			goto err;
		}
		ret = sceBgftServiceDownloadStartTask(task_id);
		if (ret)
		{
			goto err;
		}

		Util::Notify("%s queued", display_title.c_str());
		if (prompt)
		{
			file_transfering = true;
			bytes_to_download = header->pkg_content_size;
			bytes_transfered = 0;
			sceRtcGetCurrentTick(&prev_tick);

			while (!completed)
			{
				memset(&progress_info, 0, sizeof(progress_info));
				ret = sceBgftServiceDownloadGetProgress(task_id, &progress_info);
				if (ret || (progress_info.transferred > 0 && progress_info.errorResult != 0))
					return 0;

				if (progress_info.length > 0)
				{
					completed = progress_info.transferred == progress_info.length;
					bytes_to_download = progress_info.length;
					bytes_transfered = progress_info.transferred;
				}
				sceSystemServicePowerTick();
			}
		}
		else
		{
			BgProgressCheck *bg_check_data = (BgProgressCheck *)malloc(sizeof(BgProgressCheck));
			memset(bg_check_data, 0, sizeof(BgProgressCheck));
			bg_check_data->archive_pkg_data = nullptr;
			bg_check_data->split_pkg_data = nullptr;
			bg_check_data->task_id = task_id;
			bg_check_data->hash = "";
			ret = pthread_create(&bk_install_thid, NULL, CheckBgInstallTaskThread, bg_check_data);
			return 1;
		}

		return 1;
	err:
		return 0;
	}

	int InstallLocalPkg(const std::string &path)
	{
		int ret;
		pkg_header header;
		bool completed = false;

		memset(&header, 0, sizeof(header));
		if (FS::Head(path.c_str(), (void *)&header, sizeof(header)) == 0)
			return 0;

		if (BE32(header.pkg_magic) != PKG_MAGIC)
			return 0;

		char filepath[1024];
		snprintf(filepath, 1023, "%s", path.c_str());
		if (strncmp(path.c_str(), "/data/", 6) == 0)
			snprintf(filepath, 1023, "/user%s", path.c_str());

		char titleId[18];
		memset(titleId, 0, sizeof(titleId));
		int is_app = -1;
		ret = sceAppInstUtilGetTitleIdFromPkg(path.c_str(), titleId, &is_app);
		if (ret)
		{
			return 0;
		}

		std::string title = GetLocalPkgTitle(path, &header);
		std::string display_title = title.length() > 0 ? title : std::string(titleId);

		OrbisBgftTaskProgress progress_info;
		OrbisBgftDownloadParamEx download_params;
		memset(&download_params, 0, sizeof(download_params));
		{
			download_params.params.entitlementType = 5;
			download_params.params.id = (char *)header.pkg_content_id;
			download_params.params.contentUrl = filepath;
			download_params.params.contentName = display_title.c_str();
			;
			download_params.params.iconPath = "";
			download_params.params.playgoScenarioId = "0";
			download_params.params.option = ORBIS_BGFT_TASK_OPT_FORCE_UPDATE;
			download_params.slot = 0;
		}

	retry:
		int task_id = -1;
		ret = sceBgftServiceIntDownloadRegisterTaskByStorageEx(&download_params, &task_id);
		if (ret == 0x80990088 || ret == 0x80990015)
		{
			ret = sceAppInstUtilAppUnInstall(titleId);
			if (ret != 0)
				return 0;
			goto retry;
		}
		else if (ret > 0)
			return 0;

		ret = sceBgftServiceDownloadStartTask(task_id);
		if (ret)
			return 0;

		Util::Notify("%s queued", display_title.c_str());

		file_transfering = true;
		bytes_to_download = header.pkg_content_size;
		bytes_transfered = 0;
		sceRtcGetCurrentTick(&prev_tick);

		while (!completed)
		{
			memset(&progress_info, 0, sizeof(progress_info));
			ret = sceBgftServiceDownloadGetProgress(task_id, &progress_info);
			if (ret || (progress_info.transferred > 0 && progress_info.errorResult != 0))
				return 0;

			if (progress_info.length > 0)
			{
				completed = progress_info.transferred == progress_info.length;
				bytes_to_download = progress_info.length;
				bytes_transfered = progress_info.transferred;
			}
			sceSystemServicePowerTick();
		}
		return 1;

	err:
		return 0;
	}

	int InstallLocalPkg(const std::string &path, pkg_header *header, bool remove_after_install)
	{
		int ret;
		bool completed = false;

		if (strncmp(path.c_str(), "/data/", 6) != 0 &&
			strncmp(path.c_str(), "/user/data/", 11) != 0 &&
			strncmp(path.c_str(), "/mnt/usb", 8) != 0)
			return -1;

		std::string filename = path.substr(path.find_last_of("/") + 1);
		char filepath[1024];
		snprintf(filepath, 1023, "%s", path.c_str());
		if (strncmp(path.c_str(), "/data/", 6) == 0)
			snprintf(filepath, 1023, "/user%s", path.c_str());
		char titleId[18];
		memset(titleId, 0, sizeof(titleId));
		int is_app = -1;
		ret = sceAppInstUtilGetTitleIdFromPkg(path.c_str(), titleId, &is_app);
		if (ret)
		{
			return 0;
		}

		std::string title = GetLocalPkgTitle(path, header);
		std::string display_title = title.length() > 0 ? title : std::string(titleId);

		OrbisBgftTaskProgress progress_info;
		int prog = 0;
		OrbisBgftDownloadParamEx download_params;
		memset(&download_params, 0, sizeof(download_params));
		{
			download_params.params.entitlementType = 5;
			download_params.params.id = (char *)header->pkg_content_id;
			download_params.params.contentUrl = filepath;
			download_params.params.contentName = display_title.c_str();
			;
			download_params.params.iconPath = "";
			download_params.params.playgoScenarioId = "0";
			download_params.params.option = ORBIS_BGFT_TASK_OPT_FORCE_UPDATE;
			download_params.slot = 0;
		}

	retry:
		int task_id = -1;
		ret = sceBgftServiceIntDownloadRegisterTaskByStorageEx(&download_params, &task_id);
		if (ret == 0x80990088 || ret == 0x80990015)
		{
			sprintf(confirm_message, "%s - %s?", display_title.c_str(), lang_strings[STR_REINSTALL_CONFIRM_MSG]);
			confirm_state = CONFIRM_WAIT;
			action_to_take = selected_action;
			activity_inprogess = false;
			while (confirm_state == CONFIRM_WAIT)
			{
				sceKernelUsleep(100000);
			}
			activity_inprogess = true;
			selected_action = action_to_take;

			if (confirm_state == CONFIRM_YES)
			{
				ret = sceAppInstUtilAppUnInstall(titleId);
				if (ret != 0)
					goto err;
				goto retry;
			}
			else
			{
				if (auto_delete_tmp_pkg)
					FS::Rm(path);
			}
		}
		else if (ret > 0)
			goto err;

		ret = sceBgftServiceDownloadStartTask(task_id);
		if (ret)
		{
			goto err;
		}

		if (!remove_after_install)
		{
			Util::Notify("%s queued", display_title.c_str());
			return 1;
		}

		sprintf(activity_message, "%s", lang_strings[STR_WAIT_FOR_INSTALL_MSG]);
		bytes_to_download = header->pkg_content_size;
		bytes_transfered = 0;
		sceRtcGetCurrentTick(&prev_tick);

		while (!completed)
		{
			memset(&progress_info, 0, sizeof(progress_info));
			ret = sceBgftServiceDownloadGetProgress(task_id, &progress_info);
			if (ret || (progress_info.transferred > 0 && progress_info.errorResult != 0))
				return -3;

			if (progress_info.length > 0)
			{
				completed = progress_info.transferred == progress_info.length;
				bytes_to_download = progress_info.length;
				bytes_transfered = progress_info.transferred;
			}
			sceSystemServicePowerTick();
		}
		if (auto_delete_tmp_pkg)
			FS::Rm(path);
		return 1;

	err:
		return 0;
	}

	bool ExtractLocalPkg(const std::string &path, const std::string sfo_path, const std::string icon_path)
	{
		pkg_header tmp_hdr;
		FS::Head(path, &tmp_hdr, sizeof(pkg_header));

		size_t entry_count = BE32(tmp_hdr.pkg_entry_count);
		uint32_t entry_table_offset = BE32(tmp_hdr.pkg_table_offset);
		uint64_t entry_table_size = entry_count * sizeof(pkg_table_entry);
		void *entry_table_data = malloc(entry_table_size);

		FILE *fd = FS::OpenRead(path);
		FS::Seek(fd, entry_table_offset);
		FS::Read(fd, entry_table_data, entry_table_size);

		pkg_table_entry *entries = (pkg_table_entry *)entry_table_data;
		void *param_sfo_data = NULL;
		uint32_t param_sfo_offset = 0;
		uint32_t param_sfo_size = 0;
		void *icon0_png_data = NULL;
		uint32_t icon0_png_offset = 0;
		uint32_t icon0_png_size = 0;
		short items = 0;
		for (size_t i = 0; i < entry_count; ++i)
		{
			switch (BE32(entries[i].id))
			{
			case PKG_ENTRY_ID__PARAM_SFO:
				param_sfo_offset = BE32(entries[i].offset);
				param_sfo_size = BE32(entries[i].size);
				items++;
				break;
			case PKG_ENTRY_ID__ICON0_PNG:
				icon0_png_offset = BE32(entries[i].offset);
				icon0_png_size = BE32(entries[i].size);
				items++;
				break;
			default:
				continue;
			}

			if (items == 2)
				break;
		}
		free(entry_table_data);

		if (param_sfo_offset > 0 && param_sfo_size > 0)
		{
			param_sfo_data = malloc(param_sfo_size);
			FILE *out = FS::Create(sfo_path);
			FS::Seek(fd, param_sfo_offset);
			FS::Read(fd, param_sfo_data, param_sfo_size);
			FS::Write(out, param_sfo_data, param_sfo_size);
			FS::Close(out);
			free(param_sfo_data);
		}

		if (icon0_png_offset > 0 && icon0_png_size > 0)
		{
			icon0_png_data = malloc(icon0_png_size);
			FILE *out = FS::Create(icon_path);
			FS::Seek(fd, icon0_png_offset);
			FS::Read(fd, icon0_png_data, icon0_png_size);
			FS::Write(out, icon0_png_data, icon0_png_size);
			FS::Close(out);
			free(icon0_png_data);
		}

		FS::Close(fd);
		return true;
	}

	bool ExtractRemotePkg(const std::string &path, const std::string sfo_path, const std::string icon_path)
	{
		pkg_header tmp_hdr;
		if (!remoteclient->Head(path, &tmp_hdr, sizeof(pkg_header)))
			return false;

		size_t entry_count = BE32(tmp_hdr.pkg_entry_count);
		uint32_t entry_table_offset = BE32(tmp_hdr.pkg_table_offset);
		uint64_t entry_table_size = entry_count * sizeof(pkg_table_entry);
		void *entry_table_data = malloc(entry_table_size);

		if (!remoteclient->GetRange(path, entry_table_data, entry_table_size, entry_table_offset))
			return false;

		pkg_table_entry *entries = (pkg_table_entry *)entry_table_data;
		void *param_sfo_data = NULL;
		uint32_t param_sfo_offset = 0;
		uint32_t param_sfo_size = 0;
		void *icon0_png_data = NULL;
		uint32_t icon0_png_offset = 0;
		uint32_t icon0_png_size = 0;
		short items = 0;
		for (size_t i = 0; i < entry_count; ++i)
		{
			switch (BE32(entries[i].id))
			{
			case PKG_ENTRY_ID__PARAM_SFO:
				param_sfo_offset = BE32(entries[i].offset);
				param_sfo_size = BE32(entries[i].size);
				items++;
				break;
			case PKG_ENTRY_ID__ICON0_PNG:
				icon0_png_offset = BE32(entries[i].offset);
				icon0_png_size = BE32(entries[i].size);
				items++;
				break;
			default:
				continue;
			}

			if (items == 2)
				break;
		}
		free(entry_table_data);

		if (param_sfo_offset > 0 && param_sfo_size > 0)
		{
			param_sfo_data = malloc(param_sfo_size);
			FILE *out = FS::Create(sfo_path);
			if (!remoteclient->GetRange(path, param_sfo_data, param_sfo_size, param_sfo_offset))
			{
				FS::Close(out);
				return false;
			}
			FS::Write(out, param_sfo_data, param_sfo_size);
			FS::Close(out);
			free(param_sfo_data);
		}

		if (icon0_png_offset > 0 && icon0_png_size > 0)
		{
			icon0_png_data = malloc(icon0_png_size);
			FILE *out = FS::Create(icon_path);
			if (!remoteclient->GetRange(path, icon0_png_data, icon0_png_size, icon0_png_offset))
			{
				FS::Close(out);
				return false;
			}
			FS::Write(out, icon0_png_data, icon0_png_size);
			FS::Close(out);
			free(icon0_png_data);
		}

		return true;
	}

	ArchivePkgInstallData *GetArchivePkgInstallData(const std::string &hash)
	{
		return archive_pkg_install_data_list[hash];
	}

	void AddArchivePkgInstallData(const std::string &hash, ArchivePkgInstallData *pkg_data)
	{
		std::pair<std::string, ArchivePkgInstallData *> pair = std::make_pair(hash, pkg_data);
		archive_pkg_install_data_list.erase(hash);
		archive_pkg_install_data_list.insert(pair);
	}

	void RemoveArchivePkgInstallData(const std::string &hash)
	{
		archive_pkg_install_data_list.erase(hash);
	}

	bool InstallArchivePkg(const std::string &path, ArchivePkgInstallData *pkg_data, bool bg)
	{
		pkg_header header;
		pkg_data->split_file->Read((char *)&header, sizeof(pkg_header), 0);

		int ret;
		std::string cid = std::string((char *)header.pkg_content_id);
		cid = cid.substr(cid.find_first_of("-") + 1, 9);
		std::string display_title = cid;
		int user_id;
		ret = sceUserServiceGetForegroundUser(&user_id);
		const char *package_type;
		uint32_t content_type = BE32(header.pkg_content_type);
		uint32_t flags = BE32(header.pkg_content_flags);
		bool is_patch = false;
		bool completed = false;

		switch (content_type)
		{
		case PKG_CONTENT_TYPE_GD:
			package_type = "PS4GD";
			break;
		case PKG_CONTENT_TYPE_AC:
			package_type = "PS4AC";
			break;
		case PKG_CONTENT_TYPE_AL:
			package_type = "PS4AL";
			break;
		case PKG_CONTENT_TYPE_DP:
			package_type = "PS4DP";
			break;
		default:
			package_type = NULL;
			return 0;
			break;
		}

		if (flags & PKG_CONTENT_FLAGS_FIRST_PATCH ||
			flags & PKG_CONTENT_FLAGS_SUBSEQUENT_PATCH ||
			flags & PKG_CONTENT_FLAGS_DELTA_PATCH ||
			flags & PKG_CONTENT_FLAGS_CUMULATIVE_PATCH)
		{
			is_patch = true;
		}

		std::string hash = Util::UrlHash(path);
		std::string full_url = std::string("http://127.0.0.1:") + std::to_string(http_server_port) + "/archive_inst/" + hash;
		AddArchivePkgInstallData(hash, pkg_data);

		OrbisBgftTaskProgress progress_info;
		OrbisBgftDownloadParam params;
		memset(&params, 0, sizeof(params));
		{
			params.userId = user_id;
			params.entitlementType = 5;
			params.id = (char *)header.pkg_content_id;
			params.contentUrl = full_url.c_str();
			params.contentName = display_title.c_str();
			params.iconPath = "";
			params.playgoScenarioId = "0";
			params.option = ORBIS_BGFT_TASK_OPT_DISABLE_CDN_QUERY_PARAM;
			params.packageType = package_type;
			params.packageSubType = "";
			params.packageSize = BE64(header.pkg_size);
		}

	retry:
		int task_id = -1;
		if (!is_patch)
			ret = sceBgftServiceIntDownloadRegisterTask(&params, &task_id);
		else
			ret = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &task_id);
		if (ret == 0x80990088 || ret == 0x80990015)
		{
			if (!bg)
			{
				sprintf(confirm_message, "%s - %s?", display_title.c_str(), lang_strings[STR_REINSTALL_CONFIRM_MSG]);
				confirm_state = CONFIRM_WAIT;
				action_to_take = selected_action;
				activity_inprogess = false;
				while (confirm_state == CONFIRM_WAIT)
				{
					sceKernelUsleep(100000);
				}
				activity_inprogess = true;
				selected_action = action_to_take;

				if (confirm_state == CONFIRM_YES)
				{
					ret = sceAppInstUtilAppUnInstall(cid.c_str());
					if (ret != 0)
					{
						ret = 0;
						goto finish;
					}
					goto retry;
				}
			}
			else
			{
				ret = sceAppInstUtilAppUnInstall(cid.c_str());
				if (ret != 0)
				{
					ret = 0;
					goto finish;
				}
				goto retry;
			}
		}
		else if (ret > 0)
		{
			ret = 0;
			goto finish;
		}

		ret = sceBgftServiceDownloadStartTask(task_id);
		if (ret)
		{
			ret = 0;
			goto finish;
		}

		Util::Notify("%s queued", display_title.c_str());

		if (!bg)
		{
			file_transfering = true;
			bytes_to_download = header.pkg_content_size;
			bytes_transfered = 0;
			sceRtcGetCurrentTick(&prev_tick);

			while (!completed)
			{
				memset(&progress_info, 0, sizeof(progress_info));
				ret = sceBgftServiceDownloadGetProgress(task_id, &progress_info);
				if (ret || (progress_info.transferred > 0 && progress_info.errorResult != 0))
				{
					ret = 0;
					goto finish;
				}
				if (progress_info.length > 0)
				{
					completed = progress_info.transferred == progress_info.length;
					bytes_to_download = progress_info.length;
					bytes_transfered = progress_info.transferred;
				}
				sceSystemServicePowerTick();
			}
		}
		else
		{
			BgProgressCheck *bg_check_data = (BgProgressCheck *)malloc(sizeof(BgProgressCheck));
			memset(bg_check_data, 0, sizeof(BgProgressCheck));
			bg_check_data->archive_pkg_data = pkg_data;
			bg_check_data->split_pkg_data = nullptr;
			bg_check_data->task_id = task_id;
			bg_check_data->hash = hash;
			ret = pthread_create(&bk_install_thid, NULL, CheckBgInstallTaskThread, bg_check_data);
			return 1;
		}
		ret = 1;
	finish:
		pkg_data->stop_write_thread = true;
		pthread_join(pkg_data->thread, NULL);
		delete (pkg_data->split_file);
		free(pkg_data);
		RemoveArchivePkgInstallData(hash);
		return ret;
	}

	SplitPkgInstallData *GetSplitPkgInstallData(const std::string &hash)
	{
		return split_pkg_install_data_list[hash];
	}

	void AddSplitPkgInstallData(const std::string &hash, SplitPkgInstallData *pkg_data)
	{
		std::pair<std::string, SplitPkgInstallData *> pair = std::make_pair(hash, pkg_data);
		split_pkg_install_data_list.erase(hash);
		split_pkg_install_data_list.insert(pair);
	}

	void RemoveSplitPkgInstallData(const std::string &hash)
	{
		split_pkg_install_data_list.erase(hash);
	}

	bool InstallSplitPkg(const std::string &path, SplitPkgInstallData *pkg_data, bool bg)
	{
		pkg_header header;
		pkg_data->split_file->Read((char *)&header, sizeof(pkg_header), 0);

		int ret;
		std::string cid = std::string((char *)header.pkg_content_id);
		cid = cid.substr(cid.find_first_of("-") + 1, 9);
		std::string display_title = cid;
		int user_id;
		ret = sceUserServiceGetForegroundUser(&user_id);
		const char *package_type;
		uint32_t content_type = BE32(header.pkg_content_type);
		uint32_t flags = BE32(header.pkg_content_flags);
		bool is_patch = false;
		bool completed = false;

		switch (content_type)
		{
		case PKG_CONTENT_TYPE_GD:
			package_type = "PS4GD";
			break;
		case PKG_CONTENT_TYPE_AC:
			package_type = "PS4AC";
			break;
		case PKG_CONTENT_TYPE_AL:
			package_type = "PS4AL";
			break;
		case PKG_CONTENT_TYPE_DP:
			package_type = "PS4DP";
			break;
		default:
			package_type = NULL;
			return 0;
			break;
		}

		if (flags & PKG_CONTENT_FLAGS_FIRST_PATCH ||
			flags & PKG_CONTENT_FLAGS_SUBSEQUENT_PATCH ||
			flags & PKG_CONTENT_FLAGS_DELTA_PATCH ||
			flags & PKG_CONTENT_FLAGS_CUMULATIVE_PATCH)
		{
			is_patch = true;
		}

		std::string hash = Util::UrlHash(path);
		std::string full_url = std::string("http://127.0.0.1:") + std::to_string(http_server_port) + "/split_inst/" + hash;
		AddSplitPkgInstallData(hash, pkg_data);

		OrbisBgftTaskProgress progress_info;
		OrbisBgftDownloadParam params;
		memset(&params, 0, sizeof(params));
		{
			params.userId = user_id;
			params.entitlementType = 5;
			params.id = (char *)header.pkg_content_id;
			params.contentUrl = full_url.c_str();
			params.contentName = display_title.c_str();
			params.iconPath = "";
			params.playgoScenarioId = "0";
			params.option = ORBIS_BGFT_TASK_OPT_DISABLE_CDN_QUERY_PARAM;
			params.packageType = package_type;
			params.packageSubType = "";
			params.packageSize = BE64(header.pkg_size);
		}

	retry:
		int task_id = -1;
		if (!is_patch)
			ret = sceBgftServiceIntDownloadRegisterTask(&params, &task_id);
		else
			ret = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &task_id);
		if (ret == 0x80990088 || ret == 0x80990015)
		{
			if (!bg)
			{
				sprintf(confirm_message, "%s - %s?", display_title.c_str(), lang_strings[STR_REINSTALL_CONFIRM_MSG]);
				confirm_state = CONFIRM_WAIT;
				action_to_take = selected_action;
				activity_inprogess = false;
				while (confirm_state == CONFIRM_WAIT)
				{
					sceKernelUsleep(100000);
				}
				activity_inprogess = true;
				selected_action = action_to_take;

				if (confirm_state == CONFIRM_YES)
				{
					ret = sceAppInstUtilAppUnInstall(cid.c_str());
					if (ret != 0)
					{
						ret = 0;
						goto finish;
					}
					goto retry;
				}
			}
			else
			{
				ret = sceAppInstUtilAppUnInstall(cid.c_str());
				if (ret != 0)
				{
					ret = 0;
					goto finish;
				}
				goto retry;
			}
		}
		else if (ret > 0)
		{
			ret = 0;
			goto finish;
		}

		ret = sceBgftServiceDownloadStartTask(task_id);
		if (ret)
		{
			ret = 0;
			goto finish;
		}

		Util::Notify("%s queued", display_title.c_str());

		if (!bg)
		{
			file_transfering = true;
			bytes_to_download = pkg_data->size;
			bytes_transfered = 0;
			sceRtcGetCurrentTick(&prev_tick);

			while (!completed)
			{
				memset(&progress_info, 0, sizeof(progress_info));
				ret = sceBgftServiceDownloadGetProgress(task_id, &progress_info);
				if (ret || (progress_info.transferred > 0 && progress_info.errorResult != 0))
				{
					ret = 0;
					goto finish;
				}
				if (progress_info.length > 0)
				{
					completed = progress_info.transferred == progress_info.length;
					bytes_to_download = progress_info.length;
					bytes_transfered = progress_info.transferred;
				}
				sceSystemServicePowerTick();
			}
		}
		else
		{
			BgProgressCheck *bg_check_data = (BgProgressCheck *)malloc(sizeof(BgProgressCheck));
			memset(bg_check_data, 0, sizeof(BgProgressCheck));
			bg_check_data->split_pkg_data = pkg_data;
			bg_check_data->archive_pkg_data = nullptr;
			bg_check_data->task_id = task_id;
			bg_check_data->hash = hash;
			ret = pthread_create(&bk_install_thid, NULL, CheckBgInstallTaskThread, bg_check_data);
			return 1;
		}
		ret = 1;
	finish:
		pkg_data->stop_write_thread = true;
		pkg_data->split_file->Close();
		pthread_join(pkg_data->thread, NULL);
		delete (pkg_data->split_file);
		if (pkg_data->delete_client)
			delete (pkg_data->remote_client);
		free(pkg_data);
		RemoveSplitPkgInstallData(hash);
		activity_inprogess = false;
		file_transfering = false;
		Windows::SetModalMode(false);
		return ret;
	}

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

	int StartEzRemoteServer()
	{
		char buffer[8192];
		in_addr_t in_addr;
		in_addr_t server_addr;
		int filefd;
		int sockfd;
		ssize_t i;
		ssize_t read_return;
		struct hostent *hostent;
		struct sockaddr_in sockaddr_in;
		unsigned short server_port = 9090;

		if (IsPortOpen("127.0.0.1", 6701, 1))
		{
			return 0;
		}

		filefd = open(SERVER_ELF_PATH, O_RDONLY);
		if (filefd == -1)
		{
			return -1;
		}

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd == -1)
		{
			return -1;
		}

		hostent = gethostbyname("127.0.0.1");
		if (hostent == NULL)
		{
			return -1;
		}

		in_addr = inet_addr(inet_ntoa(*(struct in_addr *)*(hostent->h_addr_list)));
		if (in_addr == (in_addr_t)-1)
		{
			return -1;
		}

		sockaddr_in.sin_addr.s_addr = in_addr;
		sockaddr_in.sin_family = AF_INET;
		sockaddr_in.sin_port = htons(server_port);

		if (connect(sockfd, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in)) == -1)
		{
			return -1;
		}

		while (1)
		{
			read_return = read(filefd, buffer, 8192);
			if (read_return == 0)
				break;
			if (read_return == -1)
			{
				return -1;
			}
			if (write(sockfd, buffer, read_return) == -1)
			{
				return -1;
			}
		}

		close(filefd);
		close(sockfd);

		return 0;
	}

	void StopEzRemoteServer()
    {
        httplib::Client tmp_client = httplib::Client("http://127.0.0.1:6701");
        tmp_client.Get("/stop");
    }

	RemoteClient *GetRemoteClient(int site_idx)
	{
		RemoteClient *tmp_client = nullptr;
		RemoteSettings *tmp_settings = &site_settings[sites[site_idx]];

		return GetRemoteClient(tmp_settings);
	}

	RemoteClient *GetRemoteClient(RemoteSettings *settings)
	{
		RemoteClient *tmp_client = nullptr;

		if (settings->type == CLIENT_TYPE_HTTP_SERVER)
		{
			if (strcmp(remote_settings->http_server_type, HTTP_SERVER_APACHE) == 0)
				tmp_client = new ApacheClient();
			else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_MS_IIS) == 0)
				tmp_client = new IISClient();
			else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_NGINX) == 0)
				tmp_client = new NginxClient();
			else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_NPX_SERVE) == 0)
				tmp_client = new NpxServeClient();
			else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_RCLONE) == 0)
				tmp_client = new RCloneClient();
			else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_ARCHIVEORG) == 0)
				tmp_client = new ArchiveOrgClient();
			else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_GITHUB) == 0)
				tmp_client = new GithubClient();
			else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_MYRIENT) == 0)
				tmp_client = new MyrientClient();
		}
		else if (settings->type == CLIENT_TYPE_WEBDAV)
		{
			tmp_client = new WebDAVClient();
		}
		else if (settings->type == CLIENT_TYPE_SMB)
		{
			tmp_client = new SmbClient();
		}
		else if (settings->type == CLIENT_TYPE_SFTP)
		{
			tmp_client = new SFTPClient();
		}
		else if (settings->type == CLIENT_TYPE_FTP)
		{
			tmp_client = new FtpClient();
			FtpClient *ftp_client = (FtpClient *)tmp_client;
			ftp_client->SetCallbackXferFunction(FtpCallback);
		}
		else if (settings->type == CLIENT_TYPE_NFS)
		{
			tmp_client = new NfsClient();
		}

		tmp_client->Connect(settings->server, settings->username, settings->password, false);

		return tmp_client;
	}
}
