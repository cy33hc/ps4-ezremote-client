#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <orbis/libkernel.h>
#include <orbis/Bgft.h>
#include <orbis/AppInstUtil.h>
#include <orbis/UserService.h>
#include <curl/curl.h>
#include <request.hpp>
#include <urn.hpp>
#include "installer.h"
#include "util.h"
#include "config.h"
#include "windows.h"
#include "lang.h"
#include "rtc.h"
#include "fs.h"

#define BGFT_HEAP_SIZE (1 * 1024 * 1024)

static OrbisBgftInitParams s_bgft_init_params;

static bool s_bgft_initialized = false;

namespace INSTALLER
{
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

	int InstallRemotePkg(const char *filename, pkg_header *header)
	{
		std::string full_url = webdav_settings->server + std::string(filename);
		size_t scheme_pos = full_url.find("://");
		size_t root_pos = full_url.find("/", scheme_pos+3);
		std::string host = full_url.substr(0, root_pos);
		std::string path = full_url.substr(root_pos);

		WebDAV::Urn::Path uri(path);
		CURL *curl = curl_easy_init();
		path = uri.quote(curl);
		curl_easy_cleanup(curl);
		char url[2000];
		sprintf(url, "%s%s", host.c_str(), path.c_str());

		int ret;
		std::string cid = std::string((char *)header->pkg_content_id);
		cid = cid.substr(cid.find_first_of("-") + 1, 9);
		int user_id;
		ret = sceUserServiceGetForegroundUser(&user_id);
		const char *package_type;
		uint32_t content_type = BE32(header->pkg_content_type);
		uint32_t flags = BE32(header->pkg_content_flags);
		bool is_patch = false;

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

		OrbisBgftDownloadParam params;
		memset(&params, 0, sizeof(params));
		{
			params.userId = user_id;
			params.entitlementType = 5;
			params.id = (char *)header->pkg_content_id;
			params.contentUrl = url;
			params.contentName = cid.c_str();
			params.iconPath = "";
			params.playgoScenarioId = "0";
			params.option = ORBIS_BGFT_TASK_OPT_DISABLE_CDN_QUERY_PARAM;
			params.packageType = package_type;
			params.packageSubType = "";
			params.packageSize = BE64(header->pkg_size);
		}

		int task_id = -1;
		if (!is_patch)
			ret = sceBgftServiceIntDownloadRegisterTask(&params, &task_id);
		else
			ret = sceBgftServiceIntDebugDownloadRegisterPkg(&params, &task_id);
		if (ret)
		{
			goto err;
		}

		ret = sceBgftServiceDownloadStartTask(task_id);
		if (ret)
		{
			goto err;
		}

		Util::Notify("%s queued", cid.c_str());
		return 1;

	err:
		return 0;
	}

	int InstallLocalPkg(const char *filename, pkg_header *header, bool remove_after_install)
	{
		int ret;
		if (strncmp(filename, "/data/", 6) != 0 &&
			strncmp(filename, "/user/data/", 11) != 0 &&
			strncmp(filename, "/mnt/usb", 8) != 0)
			return -1;

		char filepath[1024];
		snprintf(filepath, 1023, "%s", filename);
		if (strncmp(filename, "/data/", 6) == 0)
			snprintf(filepath, 1023, "/user%s", filename);
		char titleId[18];
		memset(titleId, 0, sizeof(titleId));
		int is_app = -1;
		ret = sceAppInstUtilGetTitleIdFromPkg(filename, titleId, &is_app);
		if (ret)
		{
			return 0;
		}

		OrbisBgftTaskProgress progress_info;
		int prog = 0;
		OrbisBgftDownloadParamEx download_params;
		memset(&download_params, 0, sizeof(download_params));
		{
			download_params.params.entitlementType = 5;
			download_params.params.id = (char *)header->pkg_content_id;
			download_params.params.contentUrl = filepath;
			download_params.params.contentName = (char *)header->pkg_content_id;;
			download_params.params.iconPath = "";
			download_params.params.playgoScenarioId = "0";
			download_params.params.option = ORBIS_BGFT_TASK_OPT_FORCE_UPDATE;
			download_params.slot = 0;
		}

		int task_id = -1;
		ret = sceBgftServiceIntDownloadRegisterTaskByStorageEx(&download_params, &task_id);
		if (ret)
		{
			if (ret == 0x80990088)
				return -2;
			goto err;
		}

		ret = sceBgftServiceDownloadStartTask(task_id);
		if (ret)
		{
			goto err;
		}

		if (!remove_after_install)
		{
			Util::Notify("%s queued", titleId);
			return 1;
		}

		sprintf(activity_message, "%s", lang_strings[STR_WAIT_FOR_INSTALL_MSG]);
		bytes_to_download = 1;
		bytes_transfered = 0;
		while (prog < 99)
		{
			memset(&progress_info, 0, sizeof(progress_info));
			ret = sceBgftServiceDownloadGetProgress(task_id, &progress_info);
			if (ret || (progress_info.transferred > 0 && progress_info.errorResult != 0))
				return -3;
			prog = (uint32_t)(((float)progress_info.transferred / progress_info.length) * 100.f);
			bytes_to_download = progress_info.length;
			bytes_transfered = progress_info.transferred;
		}
		if (auto_delete_tmp_pkg)
			FS::Rm(filename);
		return 1;

	err:
		return 0;
	}
}