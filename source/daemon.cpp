#include <orbis/SystemService.h>
#include "system.h"
#include "fs.h"
#include "dbglogger.h"

extern "C"
{
#include "orbis_jbc.h"
    int sceLncUtilGetAppId(const char *tid);
}

#define EZREMOTE_SERVER_TITLEID "EZSR00001"
#define EZREMOTE_CLIENT_TITLEID "RMTC00001"

#define DAEMON_PATH "/system/vsh/app/" EZREMOTE_SERVER_TITLEID
#define DAEMON_SRC_PATH "/mnt/sandbox/pfsmnt/" EZREMOTE_CLIENT_TITLEID "-app0/daemon"

namespace Daemon
{
    int copyFile(const char *sourcefile, const char *destfile)
    {
        int src = sceKernelOpen(sourcefile, 0x0000, 0);
        if (src > 0)
        {
            int out = sceKernelOpen(destfile, 0x0001 | 0x0200 | 0x0400, 0777);
            if (out > 0)
            {
                size_t bytes;
                char *buffer = (char *)malloc(65536);
                if (buffer != NULL)
                {
                    while (0 < (bytes = sceKernelRead(src, buffer, 65536)))
                        sceKernelWrite(out, buffer, bytes);
                    free(buffer);
                }
                sceKernelClose(out);
            }
            else
                return -1;

            sceKernelClose(src);
            return 0;
        }

        dbglogger_log("[ELFLOADER] fuxking error");
        dbglogger_log("[Itemz-loader:%s:%i] ----- src fd = %i---", __FUNCTION__, __LINE__, src);

        return -1;
    }

    int MD5_hash_compare(const std::string &file1, const std::string &file2)
    {
        std::vector<char> file1_content = FS::Load(file1);
        std::vector<char> file2_content = FS::Load(file2);

        std::string str1 = std::string(file1_content.data(), file1_content.size());
        std::string str2 = std::string(file1_content.data(), file1_content.size());

        return str1.compare(str2);
    }

    bool IsDaemonOutdated(void)
    {
        bool res = true;
        if (FS::FileExists(DAEMON_PATH "/eboot.md5"))
        {
            res = MD5_hash_compare(DAEMON_PATH "/daemon.md5", DAEMON_SRC_PATH "/deamon.md5");
            dbglogger_log("Daemon Is Outdated?: %s", res ? "Yes" : "No");
        }

        return res;
    }

    uint32_t LaunchDaemon(const char *TITLE_ID)
    {
        uint32_t userId = -1;

        LncAppParam param;
        param.size = sizeof(LncAppParam);
        param.user_id = userId;
        param.app_opt = 0;
        param.crash_report = 0;
        param.LaunchAppCheck_flag = LaunchApp_SkipSystemUpdate;

        return sceLncUtilLaunchApp(TITLE_ID, NULL, &param);

        return 0;
    }

    bool BootDaemonServices()
    {
        dbglogger_log("Booting Daemon Services");

        if (!FS::FolderExists(DAEMON_PATH) || IsDaemonOutdated())
        {
            if (mount_large_fs("/dev/da0x4.crypt", "/system", "exfatfs", "511", MNT_UPDATE) != 0)
            {
                dbglogger_log("mounting /system failed with %s.", strerror(errno));
                return false;
            }
            else
            {

                dbglogger_log("Remount Successful");
                // Delete the folder and all its files
                FS::RmRecursive(DAEMON_PATH);
                FS::MkDirs(DAEMON_PATH);
                FS::MkDirs(DAEMON_PATH "/sce_sys");

                if (copyFile(DAEMON_SRC_PATH "/param", DAEMON_PATH "/sce_sys/param.sfo") != -1)
                {
                    if (copyFile(DAEMON_SRC_PATH "/daemon.self", DAEMON_PATH "/eboot.bin") != 0 ||
                        copyFile(DAEMON_SRC_PATH "/daemon.md5", DAEMON_PATH "/daemon.md5") != 0)
                    {
                        dbglogger_log("Creating the Daemon eboot failed to create: %s", strerror(errno));
                        return false;
                    }
                }
                else
                {
                    dbglogger_log("Copying Daemon files failed");
                    return false;
                }
            }
        }

        int32_t appid = sceLncUtilGetAppId(EZREMOTE_SERVER_TITLEID);
        // Launch Daemon with silent
        if ((appid & ~0xFFFFFF) != 0x60000000)
        {
            appid = LaunchDaemon(EZREMOTE_SERVER_TITLEID);
            dbglogger_log("Launched Daemon AppId: %x", appid);
        }
        else
            dbglogger_log("Found Daemon AppId: %x", appid);

        return true;
    }
}