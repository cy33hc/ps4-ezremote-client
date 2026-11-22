#include <stddef.h>
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <stdbool.h>

#include "daemon.h"
#include "payload_loader.h"
#include "orbis_jbc.h"

int main(int argc, char *argv[])
{

    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE) < 0)
        return 1;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE) < 0)
        return 1;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0)
        return 1;
    if (sceNetInit() < 0)
        return 1;
    sceNetPoolCreate("ezremote_loader", 1 * 1024 * 1024, 0);

    if (!initialize_jbc())
        return 1;

    if (StartEzRemoteServer() < 0)
        Notify("Failed to start ezRemote Server");

    if (is_headless_mode())
    {
        int updated = update_assets("EZRC00001", "ezremote_client_webui");
        if (updated < 0)
        {
            Notify("Failed to update ezRemote Client browse mode assets");
        }
        else if (updated > 0)
        {
            Notify("ezRemote Client browse mode assets updated");
        }

        if (updated > 0 && is_daemon_running("EZRC00001"))
        {
            Notify("Restarting ezRemote Client browse mode");
            if (!stop_daemon("EZRC00001"))
                Notify("Failed to stop the ezRemote Client daemon");
        }

        if (launch_daemon("EZRC00001"))
        {
            terminate_jbc();
            sceSystemServiceLoadExec("exit", NULL);
            return 0;
        }
    }

    terminate_jbc();
    sceSystemServiceLoadExec("/app0/daemon/ezremote_client_nativeui.self", NULL);
    return 0;
}
