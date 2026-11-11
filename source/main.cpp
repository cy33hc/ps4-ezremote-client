#undef main

#include <sstream>
#include <atomic>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/SystemService.h>
#include <orbis/Pad.h>
#include <orbis/AudioOut.h>
#include <orbis/Net.h>
#include <dbglogger.h>

#include "imgui.h"
#include "SDL2/SDL.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "server/http_server.h"
#include "clients/gdrive.h"
#include "config.h"
#include "lang.h"
#include "gui.h"
#include "util.h"
#include "installer.h"
#include "system.h"
#include "textures.h"
#include "fonts.h"
#include "activity.h"
#include "windows.h"

extern "C"
{
#include "orbis_jbc.h"
}

#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080
#define NET_HEAP_SIZE   (5 * 1024 * 1024)

// SDL window and software renderer
SDL_Window *window;
SDL_Renderer *renderer;

static std::atomic<bool> g_shutdown_started{false};
static bool g_http_started = false;

ImVec4 ColorFromBytes(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
	return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f);
};

void InitImgui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	int32_t lang_idx = 0;
	sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_LANG, &lang_idx);

	Fonts::Load();

	Lang::SetSystemLangIndex(lang_idx);
	Lang::SetTranslation(lang_idx);

	auto &style = ImGui::GetStyle();
	style.AntiAliasedLinesUseTex = false;
	style.AntiAliasedLines = true;
	style.AntiAliasedFill = true;
	style.WindowRounding = 8.0f;
	style.FrameRounding = 5.0f;
	style.GrabRounding = 5.0f;

	ImVec4* colors = style.Colors;
	const ImVec4 bgColor           = ColorFromBytes(37, 37, 38);
	const ImVec4 bgColorBlur       = ColorFromBytes(37, 37, 38, 170);
	const ImVec4 lightBgColor      = ColorFromBytes(82, 82, 85);
	const ImVec4 veryLightBgColor  = ColorFromBytes(90, 90, 95);

	const ImVec4 titleColor        = ColorFromBytes(10, 100, 142);
	const ImVec4 panelColor        = ColorFromBytes(51, 51, 55);
	const ImVec4 panelHoverColor   = ColorFromBytes(29, 151, 236);
	const ImVec4 panelActiveColor  = ColorFromBytes(0, 119, 200);

	const ImVec4 textColor         = ColorFromBytes(255, 255, 255);
	const ImVec4 textDisabledColor = ColorFromBytes(151, 151, 151);
	const ImVec4 borderColor       = ColorFromBytes(78, 78, 78);

	colors[ImGuiCol_Text]                 = textColor;
	colors[ImGuiCol_TextDisabled]         = textDisabledColor;
	colors[ImGuiCol_TextSelectedBg]       = panelActiveColor;
	colors[ImGuiCol_WindowBg]             = bgColor;
	colors[ImGuiCol_ChildBg]              = panelColor;
	colors[ImGuiCol_PopupBg]              = bgColor;
	colors[ImGuiCol_Border]               = borderColor;
	colors[ImGuiCol_BorderShadow]         = borderColor;
	colors[ImGuiCol_FrameBg]              = panelColor;
	colors[ImGuiCol_FrameBgHovered]       = panelHoverColor;
	colors[ImGuiCol_FrameBgActive]        = panelActiveColor;
	colors[ImGuiCol_TitleBg]              = titleColor;
	colors[ImGuiCol_TitleBgActive]        = titleColor;
	colors[ImGuiCol_TitleBgCollapsed]     = titleColor;
	colors[ImGuiCol_MenuBarBg]            = panelColor;
	colors[ImGuiCol_ScrollbarBg]          = panelColor;
	colors[ImGuiCol_ScrollbarGrab]        = lightBgColor;
	colors[ImGuiCol_ScrollbarGrabHovered] = veryLightBgColor;
	colors[ImGuiCol_ScrollbarGrabActive]  = veryLightBgColor;
	colors[ImGuiCol_CheckMark]            = panelActiveColor;
	colors[ImGuiCol_SliderGrab]           = panelHoverColor;
	colors[ImGuiCol_SliderGrabActive]     = panelActiveColor;
	colors[ImGuiCol_Button]               = panelColor;
	colors[ImGuiCol_ButtonHovered]        = panelHoverColor;
	colors[ImGuiCol_ButtonActive]         = panelHoverColor;
	colors[ImGuiCol_Header]               = panelColor;
	colors[ImGuiCol_HeaderHovered]        = panelHoverColor;
	colors[ImGuiCol_HeaderActive]         = panelActiveColor;
	colors[ImGuiCol_Separator]            = borderColor;
	colors[ImGuiCol_SeparatorHovered]     = borderColor;
	colors[ImGuiCol_SeparatorActive]      = borderColor;
	colors[ImGuiCol_ResizeGrip]           = bgColor;
	colors[ImGuiCol_ResizeGripHovered]    = panelColor;
	colors[ImGuiCol_ResizeGripActive]     = lightBgColor;
	colors[ImGuiCol_PlotLines]            = panelActiveColor;
	colors[ImGuiCol_PlotLinesHovered]     = panelHoverColor;
	colors[ImGuiCol_PlotHistogram]        = panelActiveColor;
	colors[ImGuiCol_PlotHistogramHovered] = panelHoverColor;
	colors[ImGuiCol_ModalWindowDimBg]     = bgColorBlur;
	colors[ImGuiCol_DragDropTarget]       = bgColor;
	colors[ImGuiCol_NavHighlight]         = titleColor;
	colors[ImGuiCol_Tab]                  = bgColor;
	colors[ImGuiCol_TabActive]            = panelActiveColor;
	colors[ImGuiCol_TabUnfocused]         = bgColor;
	colors[ImGuiCol_TabUnfocusedActive]   = panelActiveColor;
	colors[ImGuiCol_TabHovered]           = panelHoverColor;
}

static int instance_lock_fd = -1;

static void Shutdown()
{
	// Idempotent: the render loop, /stop and atexit can all reach this path.
	if (g_shutdown_started.exchange(true))
		return;

	done = true;

	HttpServer::Stop();
	if (g_http_started)
	{
		pthread_join(http_server_thid, NULL);
		g_http_started = false;
	}

	Activities::Shutdown();
	// Give cancellable workers a moment to notice and unwind.
	for (int i = 0; i < 50 && Activities::AnyRunning(); i++)
		usleep(100000);

	INSTALLER::Exit();
	GDriveClient::StopRefreshToken();

	if (ImGui::GetCurrentContext() != nullptr)
	{
		ImGui_ImplSDLRenderer_Shutdown();
		ImGui_ImplSDL2_Shutdown();
	}

	if (renderer != nullptr)
	{
		Textures::Exit();
		SDL_DestroyRenderer(renderer);
		renderer = nullptr;
	}
	if (window != nullptr)
	{
		SDL_DestroyWindow(window);
		window = nullptr;
	}
	if (ImGui::GetCurrentContext() != nullptr)
		ImGui::DestroyContext();
	SDL_Quit();

	terminate_jbc();
	sceNetTerm();

	if (instance_lock_fd >= 0)
	{
		flock(instance_lock_fd, LOCK_UN);
		close(instance_lock_fd);
		instance_lock_fd = -1;
	}
}

static void terminate()
{
	Shutdown();
	sceSystemServiceLoadExec("exit", NULL);
}

int main()
{
	dbglogger_init();
	dbglogger_log("If you see this you've set up dbglogger correctly.");

	int rc;
	// No buffering
	setvbuf(stdout, NULL, _IONBF, 0);
	signal(SIGPIPE, SIG_IGN);
	
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
	{
		return 0;
	}

	// load common modules
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_IME_DIALOG) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_PAD) < 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT) < 0 || sceAudioOutInit() != 0) return 0;
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0 || sceNetInit() != 0) return 0;

	sceNetPoolCreate("ezremote_client", NET_HEAP_SIZE, 0);

	if (INSTALLER::Init() < 0)
		return 0;

	CONFIG::LoadConfig();

	// Create a window context
	window = SDL_CreateWindow("main", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, FRAME_WIDTH, FRAME_HEIGHT, 0);
	if (window == NULL)
		return 0;

	renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL)
		return 0;

	Textures::Init(renderer);
	InitImgui();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer_Init(renderer);
	ImGui_ImplSDLRenderer_CreateFontsTexture();

	if (!initialize_jbc())
		terminate();
	if (load_sys_modules() != 0)
	{
		Shutdown();
		return 0;
	}
	atexit(terminate);

	HttpServer::Start();
	g_http_started = true;

	if (INSTALLER::StartEzRemoteServer() < 0)
	{
		Util::Notify("Cloud not load ezRemote Server. It is needed for background install and download. Please ensure Goldhen payload server is enabled");
	}

	GUI::RenderLoop(renderer);

	Shutdown();
	return 0;
}
