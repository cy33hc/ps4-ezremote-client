#include <orbis/libkernel.h>
#include <orbis/SystemService.h>
#include "config.h"
#include "fs.h"
#include "md5.h"
#include "daemon.h"
extern "C"
{
#include "orbis_jbc.h"
    int sceLncUtilGetAppId(const char *title_id);
}
#include "dbglogger.h"

uint32_t LaunchDaemon(const char *title_id)
{
    uint32_t userId = -1;

    int libcmi = sceKernelLoadStartModule("/system/common/lib/libSceSystemService.sprx", 0, NULL, 0, 0, 0);
    if (libcmi > 0)
    {

        LncAppParam param;
        param.size = sizeof(LncAppParam);
        param.user_id = userId;
        param.app_opt = 0;
        param.crash_report = 0;
        param.LaunchAppCheck_flag = LaunchApp_SkipSystemUpdate;

        dbglogger_log("Calling sceLncUtilLaunchApp");
        return sceLncUtilLaunchApp(title_id, NULL, &param);
    }

    return 0;
}

bool IsDaemonOutdated()
{
    bool res = true;
    dbglogger_log("In IsDaemonOutdated");
    if (FS::FileExists(DAEMON_PATH "/eboot.bin"))
    {
        res = md5FileCompare(DAEMON_PATH "/eboot.bin", "/mnt/sandbox/pfsmnt/RMTC00001-app0/daemon/daemon.self") == 0;
        dbglogger_log("Daemon Is Outdated?: %s", res ? "Yes" : "No");
    }

    return res;
}

bool BootDaemonService()
{
    dbglogger_log("Booting daemon");
    if (!FS::FolderExists(DAEMON_PATH) || IsDaemonOutdated())
    {
        dbglogger_log("mounting /system");
        if (mount_large_fs("/dev/da0x4.crypt", "/system", "exfatfs", "511", MNT_UPDATE) < 0)
        {
            dbglogger_log("mounting /system failed with %s", strerror(errno));
        }
        else
        {
            FS::MkDirs(DAEMON_PATH);
            FS::MkDirs(DAEMON_PATH "/sce_sys");
            dbglogger_log("copying param.sfo");
            if (FS::Copy("/system/vsh/app/NPXS21007/sce_sys/param.sfo", DAEMON_PATH "/sce_sys/param.sfo"))
            {
                dbglogger_log("copying daemon.self");
                if (!FS::Copy("/mnt/sandbox/pfsmnt/RMTC00001-app0/daemon/daemon.self", DAEMON_PATH "/eboot.bin"))
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    }

    dbglogger_log("Calling sceLncUtilGetAppId");
    uint32_t appid = sceLncUtilGetAppId("RMTC00002");
    if ((appid & ~0xFFFFFF) != 0x60000000)
    {
        dbglogger_log("Start launching RMTC00002");
        uint32_t appid = LaunchDaemon("RMTC00002");
        dbglogger_log("Launched Daemon AppId: %x", appid);
    }
    else
        dbglogger_log("Found Daemon AppId: %x", appid);

    return true;
}