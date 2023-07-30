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

#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080
#define NET_HEAP_SIZE   (5 * 1024 * 1024)

static void terminate()
{
	INSTALLER::Exit();
	terminate_jbc();
	sceSystemServiceLoadExec("exit", NULL);
}

int main()
{
	dbglogger_init();
	dbglogger_log("If you see this you've set up dbglogger correctly.");
	int rc;
	// No buffering
	setvbuf(stdout, NULL, _IONBF, 0);

	// load common modules
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_IME_DIALOG) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_PAD) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT) < 0 || sceAudioOutInit() != 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0 || sceNetInit() != 0) return 0;

	sceNetPoolCreate("simple", NET_HEAP_SIZE, 0);

	if (INSTALLER::Init() < 0)
		return 0;

	CONFIG::LoadConfig();

	HttpServer::ServerThread(nullptr);
	if (load_sys_modules() != 0)
		return 0;

	atexit(terminate);

	return 0;
}
