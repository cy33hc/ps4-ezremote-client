#undef main

#include <sstream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/SystemService.h>
#include <orbis/Pad.h>
#include <orbis/AudioOut.h>
#include <orbis/Net.h>
#include <dbglogger.h>

#include "server/http_server.h"
#include "clients/gdrive.h"
#include "config.h"
#include "lang.h"
#include "util.h"
#include "installer.h"
#include "system.h"

extern "C"
{
#include "orbis_jbc.h"
}

#define NET_HEAP_SIZE   (5 * 1024 * 1024)

static void terminate()
{
	terminate_jbc();
	sceSystemServiceLoadExec("exit", NULL);
}

int main()
{
	dbglogger_init();
	dbglogger_log("In daemon");
	int rc;

	// load common modules
	dbglogger_log("loading modules");
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_IME_DIALOG) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0 || sceNetInit() != 0) return 0;

	dbglogger_log("sceUserServiceInitialize");
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);

	sceNetPoolCreate("simple", NET_HEAP_SIZE, 0);

	dbglogger_log("initialize_jbc");
	if (!initialize_jbc())
	{
		terminate();
	}
	atexit(terminate);

	dbglogger_log("load_sys_modules");
	if (load_sys_modules() != 0)
		return 0;

	dbglogger_log("Exit daemon");
	return 0;
}
