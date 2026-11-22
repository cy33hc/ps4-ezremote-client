#include <errno.h>
#include <string>
#include <cstring>
#include <cstdint>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <orbis/UserService.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <orbis/SystemService.h>

#include "config.h"
#include "daemon.h"
#include "fs.h"
#include "util.h"

extern "C" {
#include "orbis_jbc.h"
int sceLncUtilInitialize(void);
int sceLncUtilGetAppId(const char* TID);
int sceLncUtilKillApp(uint32_t appId);
}

// The PS4 launch API uses this 24-byte layout. The OpenOrbis header's
// LncAppParam is a different/larger shape and causes 0x80940005.
typedef struct
{
    uint32_t size;
    int32_t userId;
    uint32_t appAttr;
    uint32_t enableCrashReport;
    uint64_t checkFlag;
} DaemonLncAppParam;

int MD5_hash_compare(const std::string &file1, const std::string &file2)
{
	char md5_1[33] = {0};
	char md5_2[33] = {0};

	FILE* f1 = fopen(file1.c_str(), "rb");
	if (!f1)
	{
		return true;
	}

	FILE* f2 = fopen(file2.c_str(), "rb");
	if (!f2)
	{
		fclose(f1);
		return true;
	}

	size_t ret = fread(md5_1, 1, 32, f1);
	size_t ret2 = fread(md5_2, 1, 32, f2);
	fclose(f1);
	fclose(f2);

	if (ret != 32 || ret2 != 32)
		return true;

	return strncmp(md5_1, md5_2, 32) != 0;
}

bool is_daemon_outdated(const std::string &title_id, const std::string &eboot_name)
{
    std::string eboot_path = std::string(DAEMON_PATH) + "/" + title_id + "/eboot.bin";
    std::string src_md5_path = std::string("/mnt/sandbox/pfsmnt/") + APP_TITLE_ID + "-app0/daemon/" + eboot_name + ".md5";
    std::string dest_md5_path = std::string(DAEMON_PATH) + "/" + title_id + "/eboot.md5";
    bool res = true;

    if (FS::FileExists(dest_md5_path))
    {
        res = MD5_hash_compare(dest_md5_path, src_md5_path);
    }
    
    return res;
}

uint32_t launch_daemon(const std::string &title_id) {
    sceLncUtilInitialize();

    DaemonLncAppParam param;
    memset(&param, 0, sizeof(param));
    param.size = sizeof(DaemonLncAppParam);
    param.userId = -1; // current active user
    param.appAttr = 0;
    param.enableCrashReport = 0;
    param.checkFlag = 0;

    int32_t appid = sceLncUtilLaunchApp(title_id.c_str(), NULL,
                                        reinterpret_cast<LncAppParam *>(&param));
    return (uint32_t)appid;
}


static std::string read_version_file(const std::string &path)
{
    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr)
        return std::string();

    char buffer[128] = {0};
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    std::string value(buffer, len);
    while (!value.empty() && (value[value.size() - 1] == '\n' || value[value.size() - 1] == '\r'))
        value.erase(value.size() - 1);
    return value;
}

static bool copy_directory(const std::string &src, const std::string &dst)
{
    DIR *dir = opendir(src.c_str());
    if (dir == nullptr)
    {
        return false;
    }

    FS::MkDirs(dst);
    bool ok = true;
    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        std::string src_path = src + "/" + entry->d_name;
        std::string dst_path = dst + "/" + entry->d_name;
        struct stat st = {0};
        if (stat(src_path.c_str(), &st) != 0)
        {
            ok = false;
            break;
        }

        if (S_ISDIR(st.st_mode))
            ok = copy_directory(src_path, dst_path);
        else
            ok = FS::Copy(src_path, dst_path);

        if (!ok)
        {
            break;
        }
    }
    closedir(dir);
    return ok;
}

bool is_daemon_running(const std::string &title_id)
{
    uint32_t appid = (uint32_t)sceLncUtilGetAppId(title_id.c_str());
    return appid != 0 && (appid & ~0xFFFFFFu) == 0x60000000u;
}

bool stop_daemon(const std::string &title_id)
{
    if (!is_daemon_running(title_id))
        return true;

    uint32_t appid = (uint32_t)sceLncUtilGetAppId(title_id.c_str());
    if (sceLncUtilKillApp(appid) != 0)
    {
        return false;
    }

    // Lnc tears the app down asynchronously.
    for (int i = 0; i < 50; i++)
    {
        if (!is_daemon_running(title_id))
            return true;
        usleep(100000);
    }

    return false;
}

bool boot_daemon_services(const std::string &title_id, const std::string &eboot_name)
{
    const std::string app0 = std::string("/mnt/sandbox/pfsmnt/") + APP_TITLE_ID + "-app0";
    std::string daemon_path = std::string(DAEMON_PATH) + "/" + title_id;
    std::string daemon_sce_sys_path = daemon_path + "/sce_sys";
    std::string param_sfo = daemon_sce_sys_path + "/param.sfo";
    std::string src_eboot_path = app0 + "/daemon/" + eboot_name + ".self";
    std::string dest_eboot_path = daemon_path + "/eboot.bin";
    std::string src_eboot_md5_path = app0 + "/daemon/" + eboot_name + ".md5";
    std::string dest_eboot_md5_path = daemon_path + "/eboot.md5";
    std::string src_sfo_path = app0 + "/daemon/" + eboot_name + ".sfo";
    std::string src_assets_path = app0 + "/assets";
    std::string dest_assets_path = daemon_path + "/assets";
    std::string src_daemon_path = app0 + "/daemon";
    std::string dest_daemon_path = daemon_path + "/daemon";
    std::string src_version_path = app0 + "/version.txt";
    std::string dest_version_path = daemon_path + "/version.txt";

    bool daemon_missing = !FS::FileExists(dest_eboot_path) || is_daemon_outdated(title_id, eboot_name);
    bool assets_outdated = false;
    if (FS::FileExists(src_version_path))
    {
        const std::string src_version = read_version_file(src_version_path);
        const std::string dest_version = read_version_file(dest_version_path);
        assets_outdated = src_version != dest_version || !FS::FolderExists(dest_assets_path);
    }

    if (daemon_missing || assets_outdated)
    {
        stop_daemon(title_id);
        
        if (!!mount_large_fs("/dev/da0x4.crypt", "/system", "exfatfs", "511", MNT_UPDATE))
        {
            return false;
        }
        else
        {
            if (daemon_missing)
            {
                FS::RmRecursive(daemon_path);
                mkdir(daemon_path.c_str(), 0777);
                mkdir(daemon_sce_sys_path.c_str(), 0777);

                if (!FS::Copy(src_sfo_path, param_sfo))
                {
                    return false;
                }

                if (!FS::Copy(src_eboot_path, dest_eboot_path))
                {
                    return false;
                }

                if (!FS::Copy(src_eboot_md5_path, dest_eboot_md5_path))
                {
                    return false;
                }
            }

            if (daemon_missing || assets_outdated)
            {
                FS::RmRecursive(dest_assets_path);
                if (!copy_directory(src_assets_path, dest_assets_path))
                {
                    return false;
                }

                FS::RmRecursive(dest_daemon_path);
                if (!copy_directory(src_daemon_path, dest_daemon_path))
                {
                    return false;
                }

                if (!FS::Copy(src_version_path, dest_version_path))
                {
                    return false;
                }
            }
        }
    }

    uint32_t appid = (uint32_t)sceLncUtilGetAppId(title_id.c_str());
    if (!is_daemon_running(title_id)) {
        appid = launch_daemon(title_id);
    }

    return true;
}
/* *********************** daemon shit **********************/