#include <SDL2/SDL.h>
#include <orbis/SystemService.h>
#include <stdio.h>
#include <algorithm>
#include <set>
#include "clients/gdrive.h"
#include "server/http_server.h"
#include "imgui.h"
#include "windows.h"
#include "fs.h"
#include "config.h"
#include "gui.h"
#include "actions.h"
#include "util.h"
#include "lang.h"
#include "ime_dialog.h"
#include "IconsFontAwesome6.h" 
#include "OpenFontIcons.h"
#include "textures.h"
#include "sfo.h"
#include "system.h"

#define MAX_IMAGE_HEIGHT 980
#define MAX_IMAGE_WIDTH 1820

extern "C"
{
#include "inifile.h"
}

bool paused = false;
int view_mode;
static float scroll_direction = 0.0f;
static int selected_local_position = -1;
static int selected_remote_position = -1;

static ime_callback_t ime_callback = nullptr;
static ime_callback_t ime_after_update = nullptr;
static ime_callback_t ime_before_update = nullptr;
static ime_callback_t ime_cancelled = nullptr;
static std::vector<std::string> *ime_multi_field;
static char *ime_single_field;
static int ime_field_size;

static char txt_http_server_port[6];

bool handle_updates = false;
int64_t bytes_transfered;
int64_t bytes_to_download;
OrbisTick prev_tick;

std::vector<DirEntry> local_files;
std::vector<DirEntry> remote_files;
std::set<DirEntry> multi_selected_local_files;
std::set<DirEntry> multi_selected_remote_files;
std::vector<DirEntry> local_paste_files;
std::vector<DirEntry> remote_paste_files;
DirEntry selected_local_file;
DirEntry selected_remote_file;
ACTIONS selected_action;
ACTIONS paste_action;
char status_message[1024];
char local_file_to_select[256];
char remote_file_to_select[256];
char local_filter[32];
char remote_filter[32];
char dialog_editor_text[1024];
char activity_message[1024];
int selected_browser = 0;
int saved_selected_browser;
bool activity_inprogess = false;
bool stop_activity = false;
bool file_transfering = false;
bool set_focus_to_local = false;
bool set_focus_to_remote = false;
bool select_url_inprogress = false;
int favorite_url_idx = 0;
char extract_zip_folder[256];
char zip_file_path[384];
bool show_settings = false;

// Editor variables
std::vector<std::string> edit_buffer;
bool editor_inprogress = false;
char edit_line[1024];
int edit_line_num = 0;
char label[256];
bool editor_modified = false;
char edit_file[256];
int edit_line_to_select = -1;
std::string copy_text;

// Images varaibles
bool view_image= false;
Tex texture;

bool show_pkg_info = false;
std::map<std::string, std::string> sfo_params;

// Overwrite dialog variables
bool dont_prompt_overwrite = false;
bool dont_prompt_overwrite_cb = false;
int confirm_transfer_state = -1;
int overwrite_type = OVERWRITE_PROMPT;

int confirm_state = CONFIRM_NONE;
char confirm_message[256];
ACTIONS action_to_take = ACTION_NONE;

bool prev_down[21] = {false, false, false, false, false, false, false, false, false, false,
                      false, false, false, false, false, false, false, false, false, false, false};
bool cur_down[21] = {false, false, false, false, false, false, false, false, false, false,
                     false, false, false, false, false, false, false, false, false, false, false};
namespace Windows
{

    void Init()
    {
        remoteclient = nullptr;

        sprintf(local_file_to_select, "..");
        sprintf(remote_file_to_select, "..");
        sprintf(status_message, "");
        sprintf(local_filter, "");
        sprintf(remote_filter, "");
        sprintf(txt_http_server_port, "%d", http_server_port);
        dont_prompt_overwrite = false;
        confirm_transfer_state = -1;
        dont_prompt_overwrite_cb = false;
        overwrite_type = OVERWRITE_PROMPT;
        local_paste_files.clear();
        remote_paste_files.clear();

        Actions::RefreshLocalFiles(false);
    }

    void HandleWindowInput()
    {
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        SDL_GameController *game_controller = SDL_GameControllerOpen(0);
        cur_down[SDL_CONTROLLER_BUTTON_X] = SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_X);
        cur_down[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        cur_down[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        cur_down[SDL_CONTROLLER_BUTTON_BACK] = SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_BACK);

        if (prev_down[SDL_CONTROLLER_BUTTON_X] && !cur_down[SDL_CONTROLLER_BUTTON_X] && !paused)
        {
            if (selected_browser & LOCAL_BROWSER && strcmp(selected_local_file.name, "..") != 0)
            {
                auto search_item = multi_selected_local_files.find(selected_local_file);
                if (search_item != multi_selected_local_files.end())
                {
                    multi_selected_local_files.erase(search_item);
                }
                else
                {
                    multi_selected_local_files.insert(selected_local_file);
                }
            }
            if (selected_browser & REMOTE_BROWSER && strcmp(selected_remote_file.name, "..") != 0)
            {
                auto search_item = multi_selected_remote_files.find(selected_remote_file);
                if (search_item != multi_selected_remote_files.end())
                {
                    multi_selected_remote_files.erase(search_item);
                }
                else
                {
                    multi_selected_remote_files.insert(selected_remote_file);
                }
            }
        }

        if (prev_down[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] && !cur_down[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] && !paused)
        {
            set_focus_to_remote = true;
        }

        if (prev_down[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] && !cur_down[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] && !paused)
        {
            set_focus_to_local = true;
        }

        if (prev_down[SDL_CONTROLLER_BUTTON_BACK] && !cur_down[SDL_CONTROLLER_BUTTON_BACK] && !paused)
        {
            selected_action = ACTION_DISCONNECT_AND_EXIT;
        }

        prev_down[SDL_CONTROLLER_BUTTON_X] = cur_down[SDL_CONTROLLER_BUTTON_X];
        prev_down[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = cur_down[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER];
        prev_down[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = cur_down[SDL_CONTROLLER_BUTTON_LEFTSHOULDER];
        prev_down[SDL_CONTROLLER_BUTTON_BACK] = cur_down[SDL_CONTROLLER_BUTTON_BACK];
    }

    void SetModalMode(bool modal)
    {
        paused = modal;
    }

    std::string getUniqueZipFilename()
    {
        std::string zipfolder;
        std::string zipname;
        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        if (strncmp(files.begin()->directory, "/data", 5) != 0 &&
            strncmp(files.begin()->directory, "/mnt/usb", 8) != 0)
        {
            zipfolder = "/data";
        }
        else
        {
            zipfolder = files.begin()->directory;
        }

        if (files.size() == 1)
        {
            zipname = files.begin()->name;
        }
        else if (strcmp(files.begin()->directory, "/") == 0)
        {
            zipname = "new_zip";
        }
        else
        {
            zipname = std::string(files.begin()->directory);
            zipname = zipname.substr(zipname.find_last_of("/") + 1);
        }

        std::string zip_path;
        zip_path = zipfolder + "/" + zipname;
        int i = 0;
        while (true)
        {
            std::string temp_path;
            i > 0 ? temp_path = zip_path + "(" + std::to_string(i) + ").zip" : temp_path = zip_path + ".zip";
            if (!FS::FileExists(temp_path))
                return temp_path;
            i++;
        }
    }

    std::string getExtractFolder()
    {
        std::string zipfolder;
        std::vector<DirEntry> files;
        bool local_browser_selected = saved_selected_browser & LOCAL_BROWSER;
        bool remote_browser_selected = saved_selected_browser & REMOTE_BROWSER;

        if (local_browser_selected)
        {
            if (multi_selected_local_files.size() > 0)
                std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_local_file);
        }
        else
        {
            if (multi_selected_remote_files.size() > 0)
                std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_remote_file);
        }

        if (strncmp(local_directory, "/data", 5) != 0 &&
            strncmp(local_directory, "/mnt/usb", 8) != 0 &&
            strncmp(local_directory, "/user/data", 10) != 0)
        {
            zipfolder = "/data";
        }
        else if (files.size() > 1)
        {
            zipfolder = local_directory;
        }
        else
        {
            std::string filename = std::string(files.begin()->name);
            size_t dot_pos = filename.find_last_of(".");
            zipfolder = std::string(local_directory) + "/" + filename.substr(0, dot_pos);
        }
        return zipfolder;
    }

    void ConnectionPanel()
    {
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        static char title[64];
        sprintf(title, "ezRemote %s", lang_strings[STR_CONNECTION_SETTINGS]);
        BeginGroupPanel(title, ImVec2(1905, 100));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        char id[256];
        std::string hidden_password = (strlen(remote_settings->password) > 0) ? std::string("*******") : "";
        ImVec2 pos;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
        bool is_connected = remoteclient != nullptr && remoteclient->IsConnected();

        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetItemDefaultFocus();
        }
        sprintf(id, "%s###connectbutton", is_connected ? lang_strings[STR_DISCONNECT] : lang_strings[STR_CONNECT]);
        if (ImGui::Button(id, ImVec2(200, 0)))
        {
            selected_action = is_connected ? ACTION_DISCONNECT : ACTION_CONNECT;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", is_connected ? lang_strings[STR_DISCONNECT] : lang_strings[STR_CONNECT]);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();

        ImGui::SetNextItemWidth(120);
        if (is_connected)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.3f);
        }
        if (ImGui::BeginCombo("##Site", display_site, ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLargest | ImGuiComboFlags_NoArrowButton))
        {
            static char site_id[64];
            static char site_display[512];
            for (int n = 0; n < sites.size(); n++)
            {
                const bool is_selected = strcmp(sites[n].c_str(), last_site) == 0;
                sprintf(site_id, "%s %d", lang_strings[STR_SITE], n + 1);
                sprintf(site_display, "%s %d    %s", lang_strings[STR_SITE], n + 1, site_settings[sites[n]].server);
                if (ImGui::Selectable(site_display, is_selected))
                {
                    sprintf(last_site, "%s", sites[n].c_str());
                    sprintf(display_site, "%s", site_id);
                    remote_settings = &site_settings[sites[n]];
                    sprintf(remote_directory, "%s", remote_settings->default_directory);
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (is_connected)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        sprintf(id, "%s##server", remote_settings->server);
        int width = 550;
        if (remote_settings->type == CLIENT_TYPE_HTTP_SERVER)
            width = 500;
        else if (remote_settings->type == CLIENT_TYPE_GOOGLE)
            width = 600;
        else if (remote_settings->type == CLIENT_TYPE_NFS)
            width = 900;
        pos = ImGui::GetCursorPos();
        if (ImGui::Button(id, ImVec2(width, 0)))
        {
            ime_single_field = remote_settings->server;
            ResetImeCallbacks();
            ime_field_size = 255;
            ime_callback = SingleValueImeCallback;
            ime_after_update = AferServerChangeCallback;
            Dialog::initImeDialog(lang_strings[STR_SERVER], remote_settings->server, 255, ORBIS_TYPE_TYPE_URL, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        }
        ImGui::SameLine();

        if (remote_settings->type == CLIENT_TYPE_HTTP_SERVER)
        {
            ImGui::SetNextItemWidth(140);
            if (ImGui::BeginCombo("##HttpServer", remote_settings->http_server_type, ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLargest | ImGuiComboFlags_NoArrowButton))
            {
                for (int n = 0; n < http_servers.size(); n++)
                {
                    const bool is_selected = strcmp(http_servers[n].c_str(), remote_settings->http_server_type) == 0;
                    if (ImGui::Selectable(http_servers[n].c_str(), is_selected))
                    {
                        sprintf(remote_settings->http_server_type, "%s", http_servers[n].c_str());
                    }
                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
        }

        if (remote_settings->type != CLIENT_TYPE_NFS)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_USERNAME]);
            ImGui::SameLine();

            width = 180;
            if (remote_settings->type == CLIENT_TYPE_GOOGLE)
                width = 480;
            sprintf(id, "%s##username", remote_settings->username);
            pos = ImGui::GetCursorPos();
            if (ImGui::Button(id, ImVec2(width, 0)))
            {
                ime_single_field = remote_settings->username;
                ResetImeCallbacks();
                ime_field_size = 32;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_USERNAME], remote_settings->username, 32, ORBIS_TYPE_BASIC_LATIN, pos.x, pos.y);
                gui_mode = GUI_MODE_IME;
            }
            ImGui::SameLine();
        }
        
        if (remote_settings->type != CLIENT_TYPE_NFS && remote_settings->type != CLIENT_TYPE_GOOGLE)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_PASSWORD]);
            ImGui::SameLine();

            sprintf(id, "%s##password", hidden_password.c_str());
            pos = ImGui::GetCursorPos();
            if (ImGui::Button(id, ImVec2(100, 0)))
            {
                ime_single_field = remote_settings->password;
                ResetImeCallbacks();
                ime_field_size = 127;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_PASSWORD], remote_settings->password, 127, ORBIS_TYPE_BASIC_LATIN, pos.x, pos.y);
                gui_mode = GUI_MODE_IME;
            }
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_ENABLE_RPI]);
        ImGui::SameLine();

        if (ImGui::Checkbox("###enable_rpi", &remote_settings->enable_rpi))
        {
            CONFIG::SaveConfig();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetNextWindowSize(ImVec2(450, 110));
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 440);
            ImGui::Text("%s", lang_strings[STR_ENABLE_RPI_FTP_SMB_MSG]);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_ENABLE_DISK_CACHE]);
        ImGui::SameLine();

        if (ImGui::Checkbox("###enable_disk_cache", &remote_settings->enable_disk_cache))
        {
            CONFIG::SaveConfig();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetNextWindowSize(ImVec2(550, 110));
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 540);
            ImGui::Text("%s", lang_strings[STR_ENABLE_DISC_CACHE_MSG]);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();

        ImGui::SameLine();
        ImGui::SetCursorPosX(1870);
        if (ImGui::Button(ICON_FA_GEAR, ImVec2(35, 0)))
        {
            show_settings = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_SETTINGS]);
            ImGui::EndTooltip();
        }

        ImGui::Dummy(ImVec2(0, 10));
        EndGroupPanel();
    }

    void BrowserPanel()
    {
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        selected_browser = 0;
        ImVec2 pos;

        ImGui::Dummy(ImVec2(0, 5));
        BeginGroupPanel(lang_strings[STR_LOCAL], ImVec2(948, 720));
        ImGui::Dummy(ImVec2(0, 10));

        float posX = ImGui::GetCursorPosX();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_DIRECTORY]);
        ImGui::SameLine();
        ImVec2 size = ImGui::CalcTextSize(local_directory);
        ImGui::SetCursorPosX(posX + 180);
        ImGui::PushID("local_directory##local");
        pos = ImGui::GetCursorPos();
        if (ImGui::Button(local_directory, ImVec2(569, 0)))
        {
            ime_single_field = local_directory;
            ResetImeCallbacks();
            ime_field_size = 255;
            ime_after_update = AfterLocalFileChangesCallback;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_DIRECTORY], local_directory, 256, ORBIS_TYPE_BASIC_LATIN, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        }
        ImGui::PopID();
        ImGui::PopStyleVar();
        if (size.x > 560 && ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", local_directory);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();

        ImGui::PushID("refresh##local");
        if (ImGui::Button(lang_strings[STR_REFRESH], ImVec2(155, 0)))
        {
            selected_action = ACTION_REFRESH_LOCAL_FILES;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_REFRESH]);
            ImGui::EndTooltip();
        }
        ImGui::PopID();

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_FILTER]);
        ImGui::SameLine();
        ImGui::SetCursorPosX(posX + 180);
        ImGui::PushID("local_filter##local");
        if (ImGui::Button(local_filter, ImVec2(569, 0)))
        {
            ime_single_field = local_filter;
            ResetImeCallbacks();
            ime_field_size = 31;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_FILTER], local_filter, 31, ORBIS_TYPE_BASIC_LATIN, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        }
        ImGui::PopID();
        ImGui::PopStyleVar();
        ImGui::SameLine();

        ImGui::PushID("search##local");
        if (ImGui::Button(lang_strings[STR_SEARCH], ImVec2(155, 0)))
        {
            selected_action = ACTION_APPLY_LOCAL_FILTER;
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_SEARCH]);
            ImGui::EndTooltip();
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);

        ImGui::BeginChild("Local##ChildWindow", ImVec2(919, 720));
        ImGui::Separator();
        ImGui::Columns(2, "Local##Columns", true);
        int i = 0;
        if (set_focus_to_local)
        {
            set_focus_to_local = false;
            ImGui::SetWindowFocus();
        }
        for (int j = 0; j < local_files.size(); j++)
        {
            DirEntry item = local_files[j];
            ImGui::SetColumnWidth(-1, 740);
            ImGui::PushID(i);
            auto search_item = multi_selected_local_files.find(item);
            if (search_item != multi_selected_local_files.end())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            }
            if (ImGui::Selectable(item.name, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(919, 0)))
            {
                selected_local_file = item;
                if (item.isDir)
                {
                    selected_action = ACTION_CHANGE_LOCAL_DIRECTORY;
                }
                else
                {
                    std::string filename = Util::ToLower(selected_local_file.name);
                    size_t dot_pos = filename.find_last_of(".");
                    if (dot_pos != std::string::npos)
                    {
                        std::string ext = filename.substr(dot_pos);
                        if (image_file_extensions.find(ext) != image_file_extensions.end())
                        {
                            selected_action = ACTION_VIEW_LOCAL_IMAGE;
                        }
                        else if (text_file_extensions.find(ext) != text_file_extensions.end())
                        {
                            selected_action = ACTION_LOCAL_EDIT;
                        }
                        else if (ext.compare(".pkg") == 0)
                        {
                            selected_action = ACTION_VIEW_LOCAL_PKG;
                        }
                    }
                }
            }
            ImGui::PopID();
            if (ImGui::IsItemFocused())
            {
                selected_local_file = item;
            }
            if (ImGui::IsItemHovered())
            {
                if (ImGui::CalcTextSize(item.name).x > 740)
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", item.name);
                    ImGui::EndTooltip();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) && !paused)
                {
                    if (j == 0)
                    {
                        selected_local_position = local_files.size()-1;
                        scroll_direction = 0.0f;
                    }
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) && !paused)
                {
                    if (j == local_files.size()-1)
                    {
                        selected_local_position = 0;
                        scroll_direction = 1.0f;
                    }
                }
            }
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            {
                if (strcmp(local_file_to_select, item.name) == 0)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(0.5f);
                    sprintf(local_file_to_select, "");
                }
                if (selected_local_position == j && !paused)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(scroll_direction);
                    selected_local_position = -1;
                }
                selected_browser |= LOCAL_BROWSER;
            }
            ImGui::NextColumn();
            ImGui::SetColumnWidth(-1, 150);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(item.display_size).x - ImGui::GetScrollX() - ImGui::GetStyle().ItemSpacing.x);
            ImGui::Text("%s", item.display_size);
            if (search_item != multi_selected_local_files.end())
            {
                ImGui::PopStyleColor();
            }
            ImGui::NextColumn();
            ImGui::Separator();
            i++;
        }
        ImGui::Columns(1);
        ImGui::EndChild();
        EndGroupPanel();
        ImGui::SameLine();

        BeginGroupPanel(lang_strings[STR_REMOTE], ImVec2(948, 720));
        ImGui::Dummy(ImVec2(0, 10));
        posX = ImGui::GetCursorPosX();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_DIRECTORY]);
        ImGui::SameLine();
        size = ImGui::CalcTextSize(remote_directory);
        ImGui::SetCursorPosX(posX + 180);
        ImGui::PushID("remote_directory##remote");
        pos = ImGui::GetCursorPos();
        if (ImGui::Button(remote_directory, ImVec2(569, 0)))
        {
            ime_single_field = remote_directory;
            ResetImeCallbacks();
            ime_field_size = 255;
            ime_after_update = AfterRemoteFileChangesCallback;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_DIRECTORY], remote_directory, 256, ORBIS_TYPE_BASIC_LATIN, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        }
        ImGui::PopID();
        ImGui::PopStyleVar();
        if (size.x > 560 && ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", remote_directory);
            ImGui::EndTooltip();
        }

        ImGui::SameLine();
        ImGui::PushID("refresh##remote");
        if (ImGui::Button(lang_strings[STR_REFRESH], ImVec2(155, 0)))
        {
            selected_action = ACTION_REFRESH_REMOTE_FILES;
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_REFRESH]);
            ImGui::EndTooltip();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_FILTER]);
        ImGui::SameLine();
        ImGui::SetCursorPosX(posX + 180);
        ImGui::PushID("remote_filter##remote");
        pos = ImGui::GetCursorPos();
        if (ImGui::Button(remote_filter, ImVec2(569, 0)))
        {
            ime_single_field = remote_filter;
            ResetImeCallbacks();
            ime_field_size = 31;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_FILTER], remote_filter, 31, ORBIS_TYPE_BASIC_LATIN, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        };
        ImGui::PopID();
        ImGui::PopStyleVar();
        ImGui::SameLine();

        ImGui::PushID("search##remote");
        if (ImGui::Button(lang_strings[STR_SEARCH], ImVec2(155, 0)))
        {
            selected_action = ACTION_APPLY_REMOTE_FILTER;
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_SEARCH]);
            ImGui::EndTooltip();
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        ImGui::BeginChild(ImGui::GetID("Remote##ChildWindow"), ImVec2(919, 720));
        if (set_focus_to_remote)
        {
            set_focus_to_remote = false;
            ImGui::SetWindowFocus();
        }
        ImGui::Separator();
        ImGui::Columns(2, "Remote##Columns", true);
        i = 99999;
        for (int j = 0; j < remote_files.size(); j++)
        {
            DirEntry item = remote_files[j];

            ImGui::SetColumnWidth(-1, 740);
            auto search_item = multi_selected_remote_files.find(item);
            if (search_item != multi_selected_remote_files.end())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            }
            ImGui::PushID(i);
            if (ImGui::Selectable(item.name, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(919, 0)))
            {
                selected_remote_file = item;
                if (item.isDir)
                {
                    selected_action = ACTION_CHANGE_REMOTE_DIRECTORY;
                }
                else
                {
                    std::string filename = Util::ToLower(selected_remote_file.name);
                    size_t dot_pos = filename.find_last_of(".");
                    if (dot_pos != std::string::npos)
                    {
                        std::string ext = filename.substr(dot_pos);
                        if (image_file_extensions.find(ext) != image_file_extensions.end())
                        {
                            selected_action = ACTION_VIEW_REMOTE_IMAGE;
                        }
                        else if (text_file_extensions.find(ext) != text_file_extensions.end())
                        {
                            selected_action = ACTION_REMOTE_EDIT;
                        }
                        else if (ext.compare(".pkg") == 0)
                        {
                            selected_action = ACTION_VIEW_REMOTE_PKG;
                        }
                    }
                }
            }
            if (ImGui::IsItemFocused())
            {
                selected_remote_file = item;
            }
            if (ImGui::IsItemHovered())
            {
                if (ImGui::CalcTextSize(item.name).x > 740)
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", item.name);
                    ImGui::EndTooltip();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) && !paused)
                {
                    if (j == 0)
                    {
                        selected_remote_position = remote_files.size()-1;
                        scroll_direction = 0.0f;
                    }
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) && !paused)
                {
                    if (j == remote_files.size()-1)
                    {
                        selected_remote_position = 0;
                        scroll_direction = 1.0f;
                    }
                }
            }
            ImGui::PopID();
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            {
                if (strcmp(remote_file_to_select, item.name) == 0)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(0.5f);
                    sprintf(remote_file_to_select, "");
                }
                if (selected_remote_position == j && !paused)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(scroll_direction);
                    selected_remote_position = -1;
                }
                selected_browser |= REMOTE_BROWSER;
            }
            ImGui::NextColumn();
            ImGui::SetColumnWidth(-1, 150);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(item.display_size).x - ImGui::GetScrollX() - ImGui::GetStyle().ItemSpacing.x);
            ImGui::Text("%s", item.display_size);
            if (search_item != multi_selected_remote_files.end())
            {
                ImGui::PopStyleColor();
            }
            ImGui::NextColumn();
            ImGui::Separator();
            i++;
        }
        ImGui::Columns(1);
        ImGui::EndChild();
        EndGroupPanel();

        if (ImGui::IsKeyPressed(ImGuiKey_C) && !paused)
        {
            if (selected_browser & LOCAL_BROWSER)
            {
                selected_local_file = local_files[0];
                selected_action = ACTION_CHANGE_LOCAL_DIRECTORY;
            }
            else if (selected_browser & REMOTE_BROWSER)
            {
                if (remoteclient != nullptr && remote_files.size() > 0)
                {
                    selected_remote_file = remote_files[0];
                    selected_action = ACTION_CHANGE_REMOTE_DIRECTORY;
                }
            }
        }
    }

    void StatusPanel()
    {
        ImGui::Dummy(ImVec2(0, 5));
        BeginGroupPanel(lang_strings[STR_MESSAGES], ImVec2(1905, 100));
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::Dummy(ImVec2(1880, 30));
        ImGui::SetCursorPos(pos);
        ImGui::SetCursorPosX(pos.x + 10);
        ImGui::PushTextWrapPos(1870);
        if (strncmp(status_message, "4", 1) == 0 || strncmp(status_message, "3", 1) == 0)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", status_message);
        }
        else
        {
            ImGui::Text("%s", status_message);
        }
        ImGui::PopTextWrapPos();
        ImGui::SameLine();
        EndGroupPanel();
    }

    int getSelectableFlag(uint32_t remote_action)
    {
        int flag = ImGuiSelectableFlags_Disabled;
        bool local_browser_selected = saved_selected_browser & LOCAL_BROWSER;
        bool remote_browser_selected = saved_selected_browser & REMOTE_BROWSER;

        if ((local_browser_selected && selected_local_file.selectable) ||
            (remote_browser_selected && selected_remote_file.selectable &&
             remoteclient != nullptr && (remoteclient->SupportedActions() & remote_action)))
        {
            flag = ImGuiSelectableFlags_None;
        }
        return flag;
    }

    void ShowActionsDialog()
    {
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        int flags;

        if (ImGui::IsKeyDown(ImGuiKey_GamepadFaceUp) && !paused)
        {
            if (!paused)
                saved_selected_browser = selected_browser;

            if (saved_selected_browser > 0)
            {
                SetModalMode(true);
                ImGui::OpenPopup(lang_strings[STR_ACTIONS]);
            }
        }

        bool local_browser_selected = saved_selected_browser & LOCAL_BROWSER;
        bool remote_browser_selected = saved_selected_browser & REMOTE_BROWSER;
        if (local_browser_selected)
        {
            ImGui::SetNextWindowPos(ImVec2(410, 250));
        }
        else if (remote_browser_selected)
        {
            ImGui::SetNextWindowPos(ImVec2(1330, 250));
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(230, 150), ImVec2(230, 660), NULL, NULL);
        if (ImGui::BeginPopupModal(lang_strings[STR_ACTIONS], NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::PushID("Select All##settings");
            if (ImGui::Selectable(lang_strings[STR_SELECT_ALL], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                SetModalMode(false);
                if (local_browser_selected)
                    selected_action = ACTION_LOCAL_SELECT_ALL;
                else if (remote_browser_selected)
                    selected_action = ACTION_REMOTE_SELECT_ALL;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Clear All##settings");
            if (ImGui::Selectable(lang_strings[STR_CLEAR_ALL], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                SetModalMode(false);
                if (local_browser_selected)
                    selected_action = ACTION_LOCAL_CLEAR_ALL;
                else if (remote_browser_selected)
                    selected_action = ACTION_REMOTE_CLEAR_ALL;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Cut##settings");
            if (ImGui::Selectable(lang_strings[STR_CUT], false, getSelectableFlag(REMOTE_ACTION_CUT) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                selected_action = local_browser_selected ? ACTION_LOCAL_CUT : ACTION_REMOTE_CUT;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Copy##settings");
            if (ImGui::Selectable(lang_strings[STR_COPY], false, getSelectableFlag(REMOTE_ACTION_COPY) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                selected_action = local_browser_selected ? ACTION_LOCAL_COPY : ACTION_REMOTE_COPY;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Paste##settings");
            flags = ImGuiSelectableFlags_Disabled;
            if ((local_browser_selected && local_paste_files.size() > 0) ||
                (remote_browser_selected && remote_paste_files.size() > 0 &&
                 remoteclient != nullptr && (remoteclient->SupportedActions() | REMOTE_ACTION_PASTE)))
                flags = ImGuiSelectableFlags_None;
            if (ImGui::Selectable(lang_strings[STR_PASTE], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                SetModalMode(false);
                selected_action = local_browser_selected ? ACTION_LOCAL_PASTE : ACTION_REMOTE_PASTE;
                file_transfering = true;
                confirm_transfer_state = 0;
                dont_prompt_overwrite_cb = dont_prompt_overwrite;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
            {
                int height = local_browser_selected ? (local_paste_files.size() * 30) + 42 : (remote_paste_files.size() * 30) + 42;
                ImGui::SetNextWindowSize(ImVec2(500, height));
                ImGui::BeginTooltip();
                int text_width = ImGui::CalcTextSize(lang_strings[STR_FILES]).x;
                int file_pos = ImGui::GetCursorPosX() + text_width + 15;
                ImGui::Text("%s: %s", lang_strings[STR_TYPE], (paste_action == ACTION_LOCAL_CUT | paste_action == ACTION_REMOTE_CUT) ? lang_strings[STR_CUT] : lang_strings[STR_COPY]);
                ImGui::Text("%s:", lang_strings[STR_FILES]);
                ImGui::SameLine();
                std::vector<DirEntry> files = (local_browser_selected) ? local_paste_files : remote_paste_files;
                for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
                {
                    ImGui::SetCursorPosX(file_pos);
                    ImGui::Text("%s", it->path);
                }
                ImGui::EndTooltip();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Delete##settings");
            if (ImGui::Selectable(lang_strings[STR_DELETE], false, getSelectableFlag(REMOTE_ACTION_DELETE) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                confirm_state = CONFIRM_WAIT;
                sprintf(confirm_message, "%s", lang_strings[STR_DEL_CONFIRM_MSG]);
                if (local_browser_selected)
                    action_to_take = ACTION_DELETE_LOCAL;
                else if (remote_browser_selected)
                    action_to_take = ACTION_DELETE_REMOTE;
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Rename##settings");
            flags = getSelectableFlag(REMOTE_ACTION_RENAME);
            if ((local_browser_selected && multi_selected_local_files.size() > 1) ||
                (remote_browser_selected && multi_selected_remote_files.size() > 1))
                flags = ImGuiSelectableFlags_Disabled;
            if (ImGui::Selectable(lang_strings[STR_RENAME], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_RENAME_LOCAL;
                else if (remote_browser_selected)
                    selected_action = ACTION_RENAME_REMOTE;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("setdefaultfolder##settings");
            if (ImGui::Selectable(lang_strings[STR_SET_DEFAULT_DIRECTORY], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_SET_DEFAULT_LOCAL_FOLDER;
                else if (remote_browser_selected)
                    selected_action = ACTION_SET_DEFAULT_REMOTE_FOLDER;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("New Folder##settings");
            flags = ImGuiSelectableFlags_None;
            if (remote_browser_selected && remoteclient != nullptr && !(remoteclient->SupportedActions() & REMOTE_ACTION_NEW_FOLDER))
            {
                flags = ImGuiSelectableFlags_Disabled;
            }
            if (ImGui::Selectable(lang_strings[STR_NEW_FOLDER], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_NEW_LOCAL_FOLDER;
                else if (remote_browser_selected)
                    selected_action = ACTION_NEW_REMOTE_FOLDER;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("New File##settings");
            flags = ImGuiSelectableFlags_None;
            if (remote_browser_selected && remoteclient != nullptr && !(remoteclient->SupportedActions() & REMOTE_ACTION_NEW_FILE))
            {
                flags = ImGuiSelectableFlags_Disabled;
            }
            if (ImGui::Selectable(lang_strings[STR_NEW_FILE], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_NEW_LOCAL_FILE;
                else if (remote_browser_selected)
                    selected_action = ACTION_NEW_REMOTE_FILE;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Edit##settings");
            flags = ImGuiSelectableFlags_None;
            if ((remote_browser_selected && remoteclient != nullptr && (!(remoteclient->SupportedActions() & REMOTE_ACTION_EDIT) || selected_remote_file.isDir)) ||
                (local_browser_selected && selected_local_file.isDir))
            {
                flags = ImGuiSelectableFlags_Disabled;
            }
            if (ImGui::Selectable(lang_strings[STR_EDIT], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                if (local_browser_selected)
                {
                    selected_action = ACTION_LOCAL_EDIT;
                }
                else
                {
                    selected_action = ACTION_REMOTE_EDIT;
                }
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Extract##settings");
            if (ImGui::Selectable(lang_strings[STR_EXTRACT], false, getSelectableFlag(REMOTE_ACTION_EXTRACT) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                ResetImeCallbacks();
                sprintf(extract_zip_folder, "%s", getExtractFolder().c_str());
                ime_single_field = extract_zip_folder;
                ime_field_size = 255;
                ime_callback = SingleValueImeCallback;
                if (local_browser_selected)
                    ime_after_update = AfterExtractFolderCallback;
                else
                    ime_after_update = AfterExtractRemoteFolderCallback;
                Dialog::initImeDialog(lang_strings[STR_EXTRACT_LOCATION], extract_zip_folder, 255, ORBIS_TYPE_BASIC_LATIN, 600, 350);
                gui_mode = GUI_MODE_IME;
                file_transfering = false;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            if (local_browser_selected)
            {
                ImGui::PushID("Compress##settings");
                if (ImGui::Selectable(lang_strings[STR_COMPRESS], false, getSelectableFlag(REMOTE_ACTION_NONE) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    std::string zipname;
                    std::string zipfolder;

                    ResetImeCallbacks();
                    sprintf(zip_file_path, "%s", getUniqueZipFilename().c_str());
                    ime_single_field = zip_file_path;
                    ime_field_size = 383;
                    ime_callback = SingleValueImeCallback;
                    ime_after_update = AfterZipFileCallback;
                    Dialog::initImeDialog(lang_strings[STR_ZIP_FILE_PATH], zip_file_path, 383, ORBIS_TYPE_BASIC_LATIN, 600, 350);
                    gui_mode = GUI_MODE_IME;
                    file_transfering = true;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();

                flags = getSelectableFlag(REMOTE_ACTION_UPLOAD);
                if (local_browser_selected && remoteclient != nullptr && !(remoteclient->SupportedActions() & REMOTE_ACTION_UPLOAD))
                {
                    flags = ImGuiSelectableFlags_Disabled;
                }
                ImGui::PushID("Upload##settings");
                if (ImGui::Selectable(lang_strings[STR_UPLOAD], false, flags | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    SetModalMode(false);
                    selected_action = ACTION_UPLOAD;
                    file_transfering = true;
                    confirm_transfer_state = 0;
                    dont_prompt_overwrite_cb = dont_prompt_overwrite;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();

                ImGui::PushID("Install##local");
                if (ImGui::Selectable(lang_strings[STR_INSTALL], false, getSelectableFlag(REMOTE_ACTION_INSTALL) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    SetModalMode(false);
                    selected_action = ACTION_INSTALL_LOCAL_PKG;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();
            }

            if (remote_browser_selected)
            {
                ImGui::PushID("Download##settings");
                if (ImGui::Selectable(lang_strings[STR_DOWNLOAD], false, getSelectableFlag(REMOTE_ACTION_DOWNLOAD) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    SetModalMode(false);
                    selected_action = ACTION_DOWNLOAD;
                    file_transfering = true;
                    confirm_transfer_state = 0;
                    dont_prompt_overwrite_cb = dont_prompt_overwrite;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();

                ImGui::PushID("Install##remote");
                if (ImGui::Selectable(lang_strings[STR_INSTALL], false, getSelectableFlag(REMOTE_ACTION_INSTALL) | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    SetModalMode(false);
                    selected_action = ACTION_INSTALL_REMOTE_PKG;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();
            }

            ImGui::PushID("InstallFromUrl##both");
            if (ImGui::Selectable(lang_strings[STR_INSTALL_FROM_URL], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                select_url_inprogress = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Properties##settings");
            if (ImGui::Selectable(lang_strings[STR_PROPERTIES], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                if (local_browser_selected)
                    selected_action = ACTION_SHOW_LOCAL_PROPERTIES;
                else if (remote_browser_selected)
                    selected_action = ACTION_SHOW_REMOTE_PROPERTIES;
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushID("Cancel##settings");
            if (ImGui::Selectable(lang_strings[STR_CANCEL], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
            {
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetItemDefaultFocus();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))
            {
                SetModalMode(false);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (confirm_state == CONFIRM_WAIT)
        {
            ImGui::OpenPopup(lang_strings[STR_CONFIRM]);
            ImGui::SetNextWindowPos(ImVec2(680, 350));
            ImGui::SetNextWindowSizeConstraints(ImVec2(640, 100), ImVec2(640, 200), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_CONFIRM], NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 620);
                ImGui::Text("%s", confirm_message);
                ImGui::PopTextWrapPos();
                ImGui::NewLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 220);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
                if (ImGui::Button(lang_strings[STR_NO], ImVec2(100, 0)))
                {
                    confirm_state = CONFIRM_NO;
                    selected_action = ACTION_NONE;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                };
                ImGui::SameLine();
                if (ImGui::Button(lang_strings[STR_YES], ImVec2(100, 0)))
                {
                    confirm_state = CONFIRM_YES;
                    selected_action = action_to_take;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        if (confirm_transfer_state == 0)
        {
            ImGui::OpenPopup(lang_strings[STR_OVERWRITE_OPTIONS]);
            ImGui::SetNextWindowPos(ImVec2(680, 400));
            ImGui::SetNextWindowSizeConstraints(ImVec2(640, 100), ImVec2(640, 400), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_OVERWRITE_OPTIONS], NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::RadioButton(lang_strings[STR_DONT_OVERWRITE], &overwrite_type, 0);
                ImGui::RadioButton(lang_strings[STR_ASK_FOR_CONFIRM], &overwrite_type, 1);
                ImGui::RadioButton(lang_strings[STR_DONT_ASK_CONFIRM], &overwrite_type, 2);
                ImGui::Separator();
                ImGui::Checkbox("##AlwaysUseOption", &dont_prompt_overwrite_cb);
                ImGui::SameLine();
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 625);
                ImGui::Text("%s", lang_strings[STR_ALLWAYS_USE_OPTION]);
                ImGui::PopTextWrapPos();
                ImGui::Separator();

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 170);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
                if (ImGui::Button(lang_strings[STR_CANCEL], ImVec2(150, 0)))
                {
                    confirm_transfer_state = 2;
                    dont_prompt_overwrite_cb = dont_prompt_overwrite;
                    selected_action = ACTION_NONE;
                    ImGui::CloseCurrentPopup();
                };
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::SameLine();
                if (ImGui::Button(lang_strings[STR_CONTINUE], ImVec2(150, 0)))
                {
                    confirm_transfer_state = 1;
                    dont_prompt_overwrite = dont_prompt_overwrite_cb;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    void ShowPropertiesDialog(DirEntry item)
    {
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        ImGuiStyle *style = &ImGui::GetStyle();
        ImVec4 *colors = style->Colors;
        SetModalMode(true);
        ImGui::OpenPopup(lang_strings[STR_PROPERTIES]);

        ImGui::SetNextWindowPos(ImVec2(610, 400));
        ImGui::SetNextWindowSizeConstraints(ImVec2(700, 80), ImVec2(700, 250), NULL, NULL);
        if (ImGui::BeginPopupModal(lang_strings[STR_PROPERTIES], NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_TYPE]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(150);
            ImGui::Text("%s", item.isDir ? lang_strings[STR_FOLDER] : lang_strings[STR_FILE]);
            ImGui::Separator();

            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_NAME]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(150);
            ImGui::TextWrapped("%s", item.name);
            ImGui::Separator();

            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_SIZE]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(150);
            ImGui::Text("%ld   (%s)", item.file_size, item.display_size);
            ImGui::Separator();

            ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_DATE]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(150);
            ImGui::Text("%02d/%02d/%d %02d:%02d:%02d", item.modified.day, item.modified.month, item.modified.year,
                        item.modified.hours, item.modified.minutes, item.modified.seconds);
            ImGui::Separator();

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 300);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
            if (ImGui::Button(lang_strings[STR_CLOSE], ImVec2(100, 0)))
            {
                SetModalMode(false);
                selected_action = ACTION_NONE;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void ShowProgressDialog()
    {
        if (activity_inprogess)
        {
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            ImGuiStyle *style = &ImGui::GetStyle();
            ImVec4 *colors = style->Colors;

            SetModalMode(true);
            ImGui::OpenPopup(lang_strings[STR_PROGRESS]);

            ImGui::SetNextWindowPos(ImVec2(680, 350));
            ImGui::SetNextWindowSizeConstraints(ImVec2(640, 80), ImVec2(640, 400), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_PROGRESS], NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImVec2 cur_pos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(cur_pos);
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 620);
                ImGui::Text("%s", activity_message);
                ImGui::PopTextWrapPos();
                ImGui::SetCursorPosY(cur_pos.y + 95);

                if (file_transfering)
                {
                    static float progress = 0.0f;
                    static double transfer_speed = 0.0f;
                    static char progress_text[32];
                    static OrbisTick cur_tick;
                    static double tick_delta;
                   
                    sceRtcGetCurrentTick(&cur_tick);
                    tick_delta = (cur_tick.mytick - prev_tick.mytick) * 1.0f / 1000000.0f;

                    progress = bytes_transfered * 1.0f / (float)bytes_to_download;
                    transfer_speed = (bytes_transfered * 1.0f / tick_delta) / 1048576.0f;

                    sprintf(progress_text, "%.2f MB/s", transfer_speed);
                    ImGui::ProgressBar(progress, ImVec2(625, 0), progress_text);
                }

                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 245);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
                if (ImGui::Button(lang_strings[STR_CANCEL], ImVec2(150, 0)))
                {
                    stop_activity = true;
                    SetModalMode(false);
                }
                if (stop_activity)
                {
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 620);
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", lang_strings[STR_CANCEL_ACTION_MSG]);
                    ImGui::PopTextWrapPos();
                }
                ImGui::EndPopup();
                sceSystemServicePowerTick();
            }
        }
    }

    void ShowFavoriteUrlsDialog()
    {
        if (select_url_inprogress)
        {
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            ImGuiStyle *style = &ImGui::GetStyle();
            ImVec4 *colors = style->Colors;

            SetModalMode(true);
            ImGui::OpenPopup(lang_strings[STR_FAVORITE_URLS]);

            ImGui::SetNextWindowPos(ImVec2(420, 320));
            ImGui::SetNextWindowSizeConstraints(ImVec2(1080, 80), ImVec2(1080, 500), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_FAVORITE_URLS], NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImVec2 cur_pos = ImGui::GetCursorPos();
                char id[128];
                
                ImGui::Checkbox("##enable_alldebrid_install_uril", &install_pkg_url.enable_alldebrid);
                ImGui::SameLine();
                ImGui::Text("%s", lang_strings[STR_ENABLE_ALLDEBRID_MSG]);

                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX()+40);
                ImGui::Checkbox("##enable_realdebrid_install_uril", &install_pkg_url.enable_realdebrid);
                ImGui::SameLine();
                ImGui::Text("%s", lang_strings[STR_ENABLE_REALDEBRID_MSG]);

                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX()+40);
                ImGui::Checkbox("##enable_rpi_install_url", &install_pkg_url.enable_rpi);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetNextWindowSize(ImVec2(550, 110));
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 540);
                    ImGui::Text("%s", lang_strings[STR_ENABLE_RPI_WEBDAV_MSG]);
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                ImGui::Text("%s", lang_strings[STR_ENABLE_RPI]);

                if (install_pkg_url.enable_rpi)
                {
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+40);
                    ImGui::Checkbox("##enable_diskcache_install_uril", &install_pkg_url.enable_disk_cache);
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetNextWindowSize(ImVec2(550, 110));
                        ImGui::BeginTooltip();
                        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 540);
                        ImGui::Text("%s", lang_strings[STR_ENABLE_DISC_CACHE_MSG]);
                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", lang_strings[STR_ENABLE_DISKCACHE_DESC]);
                }

                ImGui::Separator();
                if (ImGui::Button(lang_strings[STR_ONETIME_URL], ImVec2(1070, 0)))
                {
                    ResetImeCallbacks();
                    sprintf(install_pkg_url.url, "%s", "");
                    ime_single_field = install_pkg_url.url;
                    ime_field_size = 511;
                    ime_after_update = AfterPackageUrlCallback;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog("URL", install_pkg_url.url, 511, ORBIS_TYPE_BASIC_LATIN, 600, 340);
                    gui_mode = GUI_MODE_IME;
                    select_url_inprogress = false;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }

                ImGui::Separator();
                for (int j = 0; j < MAX_FAVORITE_URLS; j++)
                {
                    ImGui::Text("%s %d:", lang_strings[STR_SLOT], j);
                    ImGui::SameLine();
                    sprintf(id, "%d##saveslot", j);
                    ImGui::PushID(id);
                    ImGui::SetCursorPosX(cur_pos.x + 100);
                    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
                    if (ImGui::Button(favorite_urls[j], ImVec2(875, 0)))
                    {
                        sprintf(install_pkg_url.url, "%s", favorite_urls[j]);
                        memset(install_pkg_url.username, 0, sizeof(install_pkg_url.username));
                        memset(install_pkg_url.password, 0, sizeof(install_pkg_url.password));
                        selected_action = ACTION_INSTALL_URL_PKG;
                        SetModalMode(false);
                        select_url_inprogress = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopID();
                    if (ImGui::IsItemHovered())
                    {
                        if (ImGui::CalcTextSize(favorite_urls[j]).x > 870)
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("%s", favorite_urls[j]);
                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::SameLine();
                    sprintf(id, "%s##%d", lang_strings[STR_EDIT], j);
                    if (ImGui::Button(id, ImVec2(70, 0)))
                    {
                        ResetImeCallbacks();
                        favorite_url_idx = j;
                        ime_single_field = favorite_urls[j];
                        ime_field_size = 511;
                        ime_after_update = AfterFavoriteUrlCallback;
                        ime_callback = SingleValueImeCallback;
                        Dialog::initImeDialog("URL", favorite_urls[j], 511, ORBIS_TYPE_BASIC_LATIN, 600, 340);
                        gui_mode = GUI_MODE_IME;
                    }
                }

                if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))
                {
                    select_url_inprogress = false;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }
    }

    void ShowEditorDialog()
    {
        if (editor_inprogress)
        {
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            ImGuiStyle *style = &ImGui::GetStyle();
            ImVec4 *colors = style->Colors;

            SetModalMode(true);
            ImGui::OpenPopup(lang_strings[STR_EDITOR]);

            ImGui::SetNextWindowPos(ImVec2(320, 115));
            ImGui::SetNextWindowSizeConstraints(ImVec2(1280, 80), ImVec2(1280, 850), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_EDITOR], NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImVec2 cur_pos = ImGui::GetCursorPos();
                char id[128];
                sprintf(id, "%s##editor", lang_strings[STR_CLOSE]);
                if (ImGui::Button(id, ImVec2(635, 0)))
                {
                    editor_inprogress = false;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                sprintf(id, "%s##editor", lang_strings[STR_SAVE]);
                if (ImGui::Button(id, ImVec2(635, 0)))
                {
                    bool local_browser_selected = saved_selected_browser & LOCAL_BROWSER;
                    bool remote_browser_selected = saved_selected_browser & REMOTE_BROWSER;
                    if (local_browser_selected)
                    {
                        FS::SaveText(&edit_buffer, selected_local_file.path);
                        selected_action = ACTION_REFRESH_LOCAL_FILES;
                    }
                    else
                    {
                        FS::SaveText(&edit_buffer, TMP_EDITOR_FILE);
                        if (remoteclient != nullptr)
                        {
                            remoteclient->Put(TMP_EDITOR_FILE, selected_remote_file.path);
                            selected_action = ACTION_REFRESH_REMOTE_FILES;
                        }
                    }
                    editor_inprogress = false;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }

                ImGui::Separator();
                ImGui::BeginChild("Editor##ChildWindow", ImVec2(1275, 680));
                int j = 0;
                static int insert_item = -1;
                for (std::vector<std::string>::iterator it = edit_buffer.begin(); it != edit_buffer.end(); it++)
                {
                    ImGui::Text("%s", ICON_FA_CARET_RIGHT);
                    ImGui::SameLine();

                    sprintf(id, "%d##editor", j);
                    ImGui::PushID(id);
                    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
                    if (ImGui::Selectable(it->c_str(), false, ImGuiSelectableFlags_DontClosePopups, ImVec2(1275, 0)))
                    {
                        edit_line_num = j;
                        snprintf(edit_line, 1023, "%s", it->c_str());
                        ResetImeCallbacks();
                        ime_single_field = edit_line;
                        ime_field_size = 1023;
                        ime_after_update = AfterEditorCallback;
                        ime_callback = SingleValueImeCallback;
                        Dialog::initImeDialog(lang_strings[STR_EDIT], edit_line, 1023, ORBIS_TYPE_BASIC_LATIN, 420, 290);
                        gui_mode = GUI_MODE_IME;
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopID();
                    if ((gui_mode != GUI_MODE_IME && j == edit_line_num) || edit_line_to_select == j)
                    {
                        SetNavFocusHere();
                        edit_line_num = -1;
                        edit_line_to_select = -1;
                    }
                    if (ImGui::IsItemHovered())
                    {
                        if (ImGui::CalcTextSize(it->c_str()).x > 1275)
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("%s", it->c_str());
                            ImGui::EndTooltip();
                        }
                    }
                    if (ImGui::IsItemFocused())
                    {
                        if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false))
                        {
                            insert_item = j;
                            editor_modified = true;
                        }
                        else if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false))
                        {
                            edit_buffer.erase(it--);
                            editor_modified = true;
                            edit_line_to_select = j;
                        }
                        else if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false))
                        {
                            copy_text = std::string(it->c_str());
                        }
                        else if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false))
                        {
                            it->clear();
                            it->append(copy_text);
                        }
                    }
                    j++;
                }
                if (insert_item > -1)
                {
                    if (insert_item == edit_buffer.size() - 1)
                        edit_buffer.push_back(std::string());
                    else
                        edit_buffer.insert(edit_buffer.begin() + insert_item + 1, std::string());
                }
                insert_item = -1;
                ImGui::EndChild();

                ImGui::Text("%s%s", (editor_modified ? "**" : ""), edit_file);
                ImGui::Separator();
                ImGui::Text("L1 - %s        R1 - %s        %s - %s        %s - %s", lang_strings[STR_DELETE_LINE], lang_strings[STR_INSERT_LINE],
                            ICON_OF_SQUARE, lang_strings[STR_COPY_LINE], ICON_OF_TRIANGLE, lang_strings[STR_PASTE_LINE]);

                ImGui::EndPopup();
            }
        }
    }

    void ShowSettingsDialog()
    {
        if (show_settings)
        {
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            ImGuiStyle *style = &ImGui::GetStyle();
            ImVec4 *colors = style->Colors;

            SetModalMode(true);
            ImGui::OpenPopup(lang_strings[STR_SETTINGS]);

            ImGui::SetNextWindowPos(ImVec2(1050, 80));
            ImGui::SetNextWindowSizeConstraints(ImVec2(850, 80), ImVec2(850, 750), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_SETTINGS], NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                char id[192];
                ImVec2 field_size;
                float width;

                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s", lang_strings[STR_GLOBAL]);
                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::Text("%s", lang_strings[STR_LANGUAGE]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::SetNextItemWidth(690);
                if (ImGui::BeginCombo("##Language", language, ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLargest))
                {
                    for (int n = 0; n < langs.size(); n++)
                    {
                        const bool is_selected = strcmp(langs[n].c_str(), language) == 0;
                        if (ImGui::Selectable(langs[n].c_str(), is_selected))
                        {
                            sprintf(language, "%s", langs[n].c_str());
                        }

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::Separator();

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::Text("%s", lang_strings[STR_AUTO_DELETE_TMP_PKG]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(805);
                ImGui::Checkbox("##auto_delete_tmp_pkg", &auto_delete_tmp_pkg);
                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::Text("%s", lang_strings[STR_SHOW_HIDDEN_FILES]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(805);
                ImGui::Checkbox("##show_hidden_files", &show_hidden_files);
                ImGui::Separator();

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::Text("%s", lang_strings[STR_TEMP_DIRECTORY]);
                ImGui::SameLine();
                field_size = ImGui::CalcTextSize(lang_strings[STR_TEMP_DIRECTORY]);
                width = field_size.x + 45;
                sprintf(id, "%s##temp_direcotry", temp_folder);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
                if (ImGui::Button(id, ImVec2(835-width, 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = temp_folder;
                    ime_field_size = 512;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog(lang_strings[STR_COMPRESSED_FILE_PATH], temp_folder, 255, ORBIS_TYPE_BASIC_LATIN, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::PopStyleVar();
                ImGui::Separator();

                // Web Server settings
                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s", lang_strings[STR_WEB_SERVER]);
                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::Text("%s", lang_strings[STR_ENABLE]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(805);
                ImGui::Checkbox("##web_server_enabled", &web_server_enabled);
                ImGui::Separator();

                field_size = ImGui::CalcTextSize(lang_strings[STR_PORT]);
                width = field_size.x + 45;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::Text("%s", lang_strings[STR_PORT]);
                ImGui::SameLine();
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));

                sprintf(id, "%s##http_server_port", txt_http_server_port);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                if (ImGui::Button(id, ImVec2(835-width, 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = txt_http_server_port;
                    ime_field_size = 5;
                    ime_callback = SingleValueImeCallback;
                    ime_after_update = AfterHttpPortChangeCallback;
                    Dialog::initImeDialog(lang_strings[STR_PORT], txt_http_server_port, 5, ORBIS_TYPE_NUMBER, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::Text("%s", lang_strings[STR_COMPRESSED_FILE_PATH]);
                ImGui::SameLine();
                field_size = ImGui::CalcTextSize(lang_strings[STR_COMPRESSED_FILE_PATH]);
                width = field_size.x + 45;
                sprintf(id, "%s##compressed_file_path", compressed_file_path);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                if (ImGui::Button(id, ImVec2(835-width, 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = compressed_file_path;
                    ime_field_size = 512;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog(lang_strings[STR_COMPRESSED_FILE_PATH], compressed_file_path, 512, ORBIS_TYPE_BASIC_LATIN, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::PopStyleVar();
                ImGui::Separator();

                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s", lang_strings[STR_ALLDEBRID]);
                ImGui::Separator();

                field_size = ImGui::CalcTextSize(lang_strings[STR_API_KEY]);
                width = field_size.x + 45;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::Text("%s", lang_strings[STR_API_KEY]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(width);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));

                if (strlen(alldebrid_api_key) > 0)
                    sprintf(id, "%s", "*********************************************##alldebrid_api_key");
                else
                    sprintf(id, "%s", "##alldebrid_api_key");
                if (ImGui::Button(id, ImVec2(835-width, 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = alldebrid_api_key;
                    ime_field_size = 63;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog(lang_strings[STR_API_KEY], alldebrid_api_key, 63, ORBIS_TYPE_BASIC_LATIN, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::PopStyleVar();
                ImGui::Separator();

                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s", lang_strings[STR_REALDEBRID]);
                ImGui::Separator();

                field_size = ImGui::CalcTextSize(lang_strings[STR_API_KEY]);
                width = field_size.x + 45;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::Text("%s", lang_strings[STR_API_KEY]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(width);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));

                if (strlen(realdebrid_api_key) > 0)
                    sprintf(id, "%s", "*********************************************##realdebrid_api_key");
                else
                    sprintf(id, "%s", "##realdebrid_api_key");
                if (ImGui::Button(id, ImVec2(835-width, 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = realdebrid_api_key;
                    ime_field_size = 63;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog(lang_strings[STR_API_KEY], realdebrid_api_key, 63, ORBIS_TYPE_BASIC_LATIN, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::PopStyleVar();
                ImGui::Separator();

                // Google settings
                ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s", lang_strings[STR_GOOGLE]);
                ImGui::Separator();

                ImVec2 id_size, secret_size;
                id_size = ImGui::CalcTextSize(lang_strings[STR_CLIENT_ID]);
                secret_size = ImGui::CalcTextSize(lang_strings[STR_CLIENT_SECRET]);
                width = MAX(id_size.x, secret_size.x) + 45;

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::Text("%s", lang_strings[STR_CLIENT_ID]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(width);
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));

                sprintf(id, "%s##client_id_input", gg_app.client_id);
                if (ImGui::Button(id, ImVec2(835-width, 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = gg_app.client_id;
                    ime_field_size = 139;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog(lang_strings[STR_CLIENT_ID], gg_app.client_id, 139, ORBIS_TYPE_BASIC_LATIN, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15);
                ImGui::Text("%s", lang_strings[STR_CLIENT_SECRET]);
                ImGui::SameLine();
                ImGui::SetCursorPosX(width);
                if (strlen(gg_app.client_secret) > 0)
                    sprintf(id, "%s", "*********************************************##client_secret_input");
                else
                    sprintf(id, "%s", "##client_secret_input");
                if (ImGui::Button(id, ImVec2(835-width, 0)))
                {
                    ResetImeCallbacks();
                    ime_single_field = gg_app.client_secret;
                    ime_field_size = 63;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog(lang_strings[STR_CLIENT_SECRET], gg_app.client_secret, 63, ORBIS_TYPE_BASIC_LATIN, 1050, 80);
                    gui_mode = GUI_MODE_IME;
                }
                ImGui::PopStyleVar();
                ImGui::Separator();
                sprintf(id, "%s##settings", lang_strings[STR_CLOSE]);
                if (ImGui::Button(id, ImVec2(835, 0)))
                {
                    show_settings = false;
                    CONFIG::SaveGlobalConfig();
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::SameLine();

                if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))
                {
                    show_settings = false;
                    CONFIG::SaveGlobalConfig();
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }
    }

    void ShowImageDialog()
    {
        if (view_image)
        {
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            ImGuiStyle *style = &ImGui::GetStyle();
            ImVec4 *colors = style->Colors;

            ImVec2 image_size;
            ImVec2 image_pos;
            ImVec2 view_size;
            image_size.x = texture.width;
            image_size.y = texture.height;
            if (texture.width > MAX_IMAGE_WIDTH || texture.height > MAX_IMAGE_HEIGHT)
            {
                if (texture.width > texture.height)
                {
                    image_size.x = MAX_IMAGE_WIDTH;
                    image_size.y = (texture.height * 1.0f / texture.width * 1.0f) * MAX_IMAGE_WIDTH;
                }
                else
                {
                    image_size.y = MAX_IMAGE_HEIGHT;
                    image_size.x = (texture.width * 1.0f / texture.height * 1.0f) * MAX_IMAGE_HEIGHT;
                }
            }
            view_size.x = image_size.x + 50;
            view_size.y = image_size.y + 50;
            image_pos.x = (1920 - view_size.x) / 2;
            image_pos.y = (1080 - view_size.y) / 2;

            SetModalMode(true);
            ImGui::OpenPopup(lang_strings[STR_VIEW_IMAGE]);

            ImGui::SetNextWindowPos(image_pos);
            ImGui::SetNextWindowSizeConstraints(image_size, view_size, NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_VIEW_IMAGE], NULL, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Image(texture.id, image_size);
                if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))
                {
                    view_image = false;
                    SetModalMode(false);
                    Textures::Free(&texture);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    void ShowPackageInfoDialog()
    {
        if (!paused)
            saved_selected_browser = selected_browser;
        if (show_pkg_info)
        {
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            ImGuiStyle *style = &ImGui::GetStyle();
            ImVec4 *colors = style->Colors;

            SetModalMode(true);
            ImGui::OpenPopup(lang_strings[STR_VIEW_PKG_INFO]);

            ImGui::SetNextWindowPos(ImVec2(360, 240));
            ImGui::SetNextWindowSizeConstraints(ImVec2(1200, 300), ImVec2(1200, 600), NULL, NULL);
            if (ImGui::BeginPopupModal(lang_strings[STR_VIEW_PKG_INFO], NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Image(texture.id, ImVec2(400, 400));
                ImGui::SameLine();

                BeginGroupPanel("SFO Attributes", ImVec2(780, 600));
                ImGui::PushTextWrapPos(1180);
                for (std::map<std::string, std::string>::iterator it = sfo_params.begin(); it != sfo_params.end(); ++it)
                {
                    if (!it->second.empty())
                    {
                        ImGui::TextColored(colors[ImGuiCol_ButtonHovered],"%s:", it->first.c_str());
                        ImGui::SameLine();
                        ImGui::Text("%s", it->second.c_str());
                    }
                }
                ImGui::PopTextWrapPos();
                EndGroupPanel();

                if (saved_selected_browser & REMOTE_BROWSER ||
                    (saved_selected_browser & LOCAL_BROWSER && (strncmp(selected_local_file.path, "/data/", 6) == 0 || strncmp(selected_local_file.path, "/mnt/usb", 8) == 0)))
                {
                    ImGui::SetCursorPos(ImVec2(7, 420));
                    if (ImGui::Button(lang_strings[STR_INSTALL], ImVec2(400, 0)))
                    {
                        if (saved_selected_browser & REMOTE_BROWSER)
                            selected_action = ACTION_INSTALL_REMOTE_PKG;
                        else
                            selected_action = ACTION_INSTALL_LOCAL_PKG;
                        show_pkg_info = false;
                        SetModalMode(false);
                        ImGui::CloseCurrentPopup();
                    }
                }

                if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))
                {
                    show_pkg_info = false;
                    SetModalMode(false);
                    Textures::Free(&texture);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    void MainWindow()
    {
        Windows::SetupWindow();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);

        if (ImGui::Begin("Remote Client", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar))
        {
            ConnectionPanel();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
            BrowserPanel();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
            StatusPanel();
            ShowProgressDialog();
            ShowActionsDialog();
            ShowFavoriteUrlsDialog();
            ShowEditorDialog();
            ShowSettingsDialog();
            ShowImageDialog();
            ShowPackageInfoDialog();
        }
        ImGui::End();
    }

    void ExecuteActions()
    {
        std::vector<char> sfo;
        switch (selected_action)
        {
        case ACTION_CHANGE_LOCAL_DIRECTORY:
            Actions::HandleChangeLocalDirectory(selected_local_file);
            break;
        case ACTION_CHANGE_REMOTE_DIRECTORY:
            Actions::HandleChangeRemoteDirectory(selected_remote_file);
            break;
        case ACTION_REFRESH_LOCAL_FILES:
            Actions::HandleRefreshLocalFiles();
            break;
        case ACTION_REFRESH_REMOTE_FILES:
            Actions::HandleRefreshRemoteFiles();
            break;
        case ACTION_APPLY_LOCAL_FILTER:
            Actions::RefreshLocalFiles(true);
            selected_action = ACTION_NONE;
            break;
        case ACTION_APPLY_REMOTE_FILTER:
            Actions::RefreshRemoteFiles(true);
            selected_action = ACTION_NONE;
            break;
        case ACTION_NEW_LOCAL_FOLDER:
        case ACTION_NEW_REMOTE_FOLDER:
        case ACTION_NEW_LOCAL_FILE:
        case ACTION_NEW_REMOTE_FILE:
            if (gui_mode != GUI_MODE_IME)
            {
                sprintf(dialog_editor_text, "");
                ime_single_field = dialog_editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                ImVec2 pos = (selected_action == ACTION_NEW_LOCAL_FOLDER || selected_action == ACTION_NEW_LOCAL_FILE) ? ImVec2(410, 350) : ImVec2(1330, 350);
                Dialog::initImeDialog((selected_action == ACTION_NEW_LOCAL_FILE || selected_action == ACTION_NEW_REMOTE_FILE)? lang_strings[STR_NEW_FILE]: lang_strings[STR_NEW_FOLDER], dialog_editor_text, 128, ORBIS_TYPE_BASIC_LATIN, pos.x, pos.y);
                gui_mode = GUI_MODE_IME;
            }
            break;
        case ACTION_DELETE_LOCAL:
            activity_inprogess = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            selected_action = ACTION_NONE;
            Actions::DeleteSelectedLocalFiles();
            break;
        case ACTION_DELETE_REMOTE:
            activity_inprogess = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            selected_action = ACTION_NONE;
            Actions::DeleteSelectedRemotesFiles();
            break;
        case ACTION_UPLOAD:
            sprintf(status_message, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                sprintf(activity_message, "%s", "");
                stop_activity = false;
                Actions::UploadFiles();
                confirm_transfer_state = -1;
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_DOWNLOAD:
            sprintf(status_message, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                sprintf(activity_message, "%s", "");
                stop_activity = false;
                Actions::DownloadFiles();
                confirm_transfer_state = -1;
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_EXTRACT_LOCAL_ZIP:
            sprintf(status_message, "%s", "");
            activity_inprogess = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            file_transfering = true;
            selected_action = ACTION_NONE;
            Actions::ExtractLocalZips();
            break;
        case ACTION_EXTRACT_REMOTE_ZIP:
            sprintf(status_message, "%s", "");
            activity_inprogess = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            file_transfering = true;
            selected_action = ACTION_NONE;
            Actions::ExtractRemoteZips();
            break;
        case ACTION_CREATE_LOCAL_ZIP:
            sprintf(status_message, "%s", "");
            activity_inprogess = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            file_transfering = true;
            selected_action = ACTION_NONE;
            Actions::MakeLocalZip();
            break;
        case ACTION_RENAME_LOCAL:
            if (gui_mode != GUI_MODE_IME)
            {
                if (multi_selected_local_files.size() > 0)
                    sprintf(dialog_editor_text, "%s", multi_selected_local_files.begin()->name);
                else
                    sprintf(dialog_editor_text, "%s", selected_local_file.name);
                ime_single_field = dialog_editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_RENAME], dialog_editor_text, 128, ORBIS_TYPE_BASIC_LATIN, 410, 350);
                gui_mode = GUI_MODE_IME;
            }
            break;
        case ACTION_RENAME_REMOTE:
            if (gui_mode != GUI_MODE_IME)
            {
                if (multi_selected_remote_files.size() > 0)
                    sprintf(dialog_editor_text, "%s", multi_selected_remote_files.begin()->name);
                else
                    sprintf(dialog_editor_text, "%s", selected_remote_file.name);
                ime_single_field = dialog_editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_RENAME], dialog_editor_text, 128, ORBIS_TYPE_BASIC_LATIN, 1330, 350);
                gui_mode = GUI_MODE_IME;
            }
            break;
        case ACTION_SHOW_LOCAL_PROPERTIES:
            ShowPropertiesDialog(selected_local_file);
            break;
        case ACTION_SHOW_REMOTE_PROPERTIES:
            ShowPropertiesDialog(selected_remote_file);
            break;
        case ACTION_LOCAL_SELECT_ALL:
            Actions::SelectAllLocalFiles();
            selected_action = ACTION_NONE;
            break;
        case ACTION_REMOTE_SELECT_ALL:
            Actions::SelectAllRemoteFiles();
            selected_action = ACTION_NONE;
            break;
        case ACTION_LOCAL_CLEAR_ALL:
            multi_selected_local_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_REMOTE_CLEAR_ALL:
            multi_selected_remote_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_CONNECT:
            sprintf(status_message, "%s", "");
            Actions::Connect();
            break;
        case ACTION_DISCONNECT:
            sprintf(status_message, "%s", "");
            Actions::Disconnect();
            break;
        case ACTION_DISCONNECT_AND_EXIT:
            Actions::Disconnect();
            HttpServer::Stop();
            GDriveClient::StopRefreshToken();
            done = true;
            break;
        case ACTION_INSTALL_REMOTE_PKG:
            sprintf(status_message, "%s", "");
            activity_inprogess = true;
            file_transfering = true;
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            Actions::InstallRemotePkgs();
            selected_action = ACTION_NONE;
            break;
        case ACTION_INSTALL_LOCAL_PKG:
            activity_inprogess = true;
            sprintf(status_message, "%s", "");
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            Actions::InstallLocalPkgs();
            selected_action = ACTION_NONE;
            break;
        case ACTION_INSTALL_URL_PKG:
            activity_inprogess = true;
            sprintf(status_message, "%s", "");
            sprintf(activity_message, "%s", "");
            stop_activity = false;
            Actions::InstallUrlPkg();
            selected_action = ACTION_NONE;
            break;
        case ACTION_LOCAL_CUT:
        case ACTION_LOCAL_COPY:
            paste_action = selected_action;
            local_paste_files.clear();
            if (multi_selected_local_files.size() > 0)
                std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(local_paste_files));
            else
                local_paste_files.push_back(selected_local_file);
            multi_selected_local_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_REMOTE_CUT:
        case ACTION_REMOTE_COPY:
            paste_action = selected_action;
            remote_paste_files.clear();
            if (multi_selected_remote_files.size() > 0)
                std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(remote_paste_files));
            else
                remote_paste_files.push_back(selected_remote_file);
            multi_selected_remote_files.clear();
            selected_action = ACTION_NONE;
            break;
        case ACTION_LOCAL_PASTE:
            sprintf(status_message, "%s", "");
            sprintf(activity_message, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                sprintf(activity_message, "%s", "");
                stop_activity = false;
                confirm_transfer_state = -1;
                if (paste_action == ACTION_LOCAL_CUT)
                    Actions::MoveLocalFiles();
                else if (paste_action == ACTION_LOCAL_COPY)
                    Actions::CopyLocalFiles();
                else
                {
                    activity_inprogess = false;
                }
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_REMOTE_PASTE:
            sprintf(status_message, "%s", "");
            sprintf(activity_message, "%s", "");
            if (dont_prompt_overwrite || (!dont_prompt_overwrite && confirm_transfer_state == 1))
            {
                activity_inprogess = true;
                sprintf(activity_message, "%s", "");
                stop_activity = false;
                confirm_transfer_state = -1;
                if (paste_action == ACTION_REMOTE_CUT)
                    Actions::MoveRemoteFiles();
                else if (paste_action == ACTION_REMOTE_COPY)
                    Actions::CopyRemoteFiles();
                else
                {
                    activity_inprogess = false;
                }
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_SET_DEFAULT_LOCAL_FOLDER:
            CONFIG::SaveLocalDirecotry(local_directory);
            sprintf(status_message, "\"%s\" %s", local_directory, lang_strings[STR_SET_DEFAULT_DIRECTORY_MSG]);
            selected_action = ACTION_NONE;
            break;
        case ACTION_SET_DEFAULT_REMOTE_FOLDER:
            sprintf(remote_settings->default_directory, "%s", remote_directory);
            CONFIG::SaveConfig();
            sprintf(status_message, "\"%s\" %s", remote_directory, lang_strings[STR_SET_DEFAULT_DIRECTORY_MSG]);
            selected_action = ACTION_NONE;
            break;
        case ACTION_VIEW_LOCAL_IMAGE:
            if (Textures::LoadImageFile(selected_local_file.path, &texture))
            {
                view_image = true;
            }
            selected_action = ACTION_NONE;
            break;
        case ACTION_VIEW_REMOTE_IMAGE:
            remoteclient->Get(TMP_ICON_PATH, selected_remote_file.path);
            if (Textures::LoadImageFile(TMP_ICON_PATH, &texture))
            {
                view_image = true;
            }
            selected_action = ACTION_NONE;
            break;
        case ACTION_LOCAL_EDIT:
            if (selected_local_file.file_size > max_edit_file_size)
                sprintf(status_message, "%s %d", lang_strings[STR_MAX_EDIT_FILE_SIZE_MSG], max_edit_file_size);
            else
            {
                snprintf(edit_file, 255, "%s", selected_local_file.path);
                FS::LoadText(&edit_buffer, selected_local_file.path);
                editor_inprogress = true;
            }
            editor_modified = false;
            selected_action = ACTION_NONE;
            break;
        case ACTION_REMOTE_EDIT:
            if (selected_remote_file.file_size > max_edit_file_size)
                sprintf(status_message, "%s %d", lang_strings[STR_MAX_EDIT_FILE_SIZE_MSG], max_edit_file_size);
            else if (remoteclient != nullptr && remoteclient->Get(TMP_EDITOR_FILE, selected_remote_file.path))
            {
                snprintf(edit_file, 255, "%s", selected_remote_file.path);
                FS::LoadText(&edit_buffer, TMP_EDITOR_FILE);
                editor_inprogress = true;
            }
            editor_modified = false;
            selected_action = ACTION_NONE;
            break;
        case ACTION_VIEW_LOCAL_PKG:
            INSTALLER::ExtractLocalPkg(selected_local_file.path, TMP_SFO_PATH, TMP_ICON_PATH);
            Textures::LoadImageFile(TMP_ICON_PATH, &texture);
            sfo = FS::Load(TMP_SFO_PATH);
            sfo_params = SFO::GetParams(sfo.data(), sfo.size());
            show_pkg_info = true;
            selected_action = ACTION_NONE;
            break;
        case ACTION_VIEW_REMOTE_PKG:
            if (INSTALLER::ExtractRemotePkg(selected_remote_file.path, TMP_SFO_PATH, TMP_ICON_PATH))
            {
                Textures::LoadImageFile(TMP_ICON_PATH, &texture);
                sfo = FS::Load(TMP_SFO_PATH);
                sfo_params = SFO::GetParams(sfo.data(), sfo.size());
                show_pkg_info = true;
            }
            selected_action = ACTION_NONE;
            break;
        default:
            break;
        }
    }

    void ResetImeCallbacks()
    {
        ime_callback = nullptr;
        ime_after_update = nullptr;
        ime_before_update = nullptr;
        ime_cancelled = nullptr;
        ime_field_size = 1;
    }

    void HandleImeInput()
    {
        if (Dialog::isImeDialogRunning())
        {
            int ime_result = Dialog::updateImeDialog();
            if (ime_result == IME_DIALOG_RESULT_FINISHED || ime_result == IME_DIALOG_RESULT_CANCELED)
            {
                if (ime_result == IME_DIALOG_RESULT_FINISHED)
                {
                    if (ime_before_update != nullptr)
                    {
                        ime_before_update(ime_result);
                    }

                    if (ime_callback != nullptr)
                    {
                        ime_callback(ime_result);
                    }
                    if (ime_after_update != nullptr)
                    {
                        ime_after_update(ime_result);
                    }
                }
                else if (ime_cancelled != nullptr)
                {
                    ime_cancelled(ime_result);
                }

                ResetImeCallbacks();
                gui_mode = GUI_MODE_BROWSER;
            }
        }
    }

    void SingleValueImeCallback(int ime_result)
    {
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            char *new_value = (char *)Dialog::getImeDialogInputText();
            snprintf(ime_single_field, ime_field_size, "%s", new_value);
        }
    }

    void MultiValueImeCallback(int ime_result)
    {
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            char *new_value = (char *)Dialog::getImeDialogInputText();
            char *initial_value = (char *)Dialog::getImeDialogInitialText();
            if (strlen(initial_value) == 0)
            {
                ime_multi_field->push_back(std::string(new_value));
            }
            else
            {
                for (int i = 0; i < ime_multi_field->size(); i++)
                {
                    if (strcmp((*ime_multi_field)[i].c_str(), initial_value) == 0)
                    {
                        (*ime_multi_field)[i] = std::string(new_value);
                    }
                }
            }
        }
    }

    void NullAfterValueChangeCallback(int ime_result) {}

    void AfterLocalFileChangesCallback(int ime_result)
    {
        selected_action = ACTION_REFRESH_LOCAL_FILES;
    }

    void AfterRemoteFileChangesCallback(int ime_result)
    {
        selected_action = ACTION_REFRESH_REMOTE_FILES;
    }

    void AfterPackageUrlCallback(int ime_result)
    {
        selected_action = ACTION_INSTALL_URL_PKG;
    }

    void AfterFavoriteUrlCallback(int ime_result)
    {
        CONFIG::SaveFavoriteUrl(favorite_url_idx, favorite_urls[favorite_url_idx]);
    }

    void AfterFolderNameCallback(int ime_result)
    {
        switch (selected_action)
        {
        case ACTION_NEW_LOCAL_FOLDER:
            Actions::CreateNewLocalFolder(dialog_editor_text);
            break;
        case ACTION_NEW_REMOTE_FOLDER:
            Actions::CreateNewRemoteFolder(dialog_editor_text);
            break;
        case ACTION_RENAME_LOCAL:
            if (multi_selected_local_files.size() > 0)
                Actions::RenameLocalFolder(multi_selected_local_files.begin()->path, dialog_editor_text);
            else
                Actions::RenameLocalFolder(selected_local_file.path, dialog_editor_text);
            break;
        case ACTION_RENAME_REMOTE:
            if (multi_selected_remote_files.size() > 0)
                Actions::RenameRemoteFolder(multi_selected_remote_files.begin()->path, dialog_editor_text);
            else
                Actions::RenameRemoteFolder(selected_remote_file.path, dialog_editor_text);
            break;
        case ACTION_NEW_LOCAL_FILE:
            Actions::CreateLocalFile(dialog_editor_text);
            break;
        case ACTION_NEW_REMOTE_FILE:
            Actions::CreateRemoteFile(dialog_editor_text);
            break;
        default:
            break;
        }
        selected_action = ACTION_NONE;
    }

    void CancelActionCallBack(int ime_result)
    {
        selected_action = ACTION_NONE;
    }

    void AfterExtractFolderCallback(int ime_result)
    {
        selected_action = ACTION_EXTRACT_LOCAL_ZIP;
    }

    void AfterExtractRemoteFolderCallback(int ime_result)
    {
        selected_action = ACTION_EXTRACT_REMOTE_ZIP;
    }

    void AfterZipFileCallback(int ime_result)
    {
        selected_action = ACTION_CREATE_LOCAL_ZIP;
    }

    void AferServerChangeCallback(int ime_result)
    {
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            CONFIG::SetClientType(remote_settings);
            if (strncasecmp(remote_settings->server, "https://archive.org/", 20) == 0)
            {
                sprintf(remote_settings->http_server_type, "%s", HTTP_SERVER_ARCHIVEORG);
            }
            else if (strncasecmp(remote_settings->server, "https://myrient.erista.me/", 26) == 0)
            {
                sprintf(remote_settings->http_server_type, "%s", HTTP_SERVER_MYRIENT);
            }
            else if (strncasecmp(remote_settings->server, "https://github.com/", 19) == 0)
            {
                snprintf(remote_settings->http_server_type, 24, "%s", HTTP_SERVER_GITHUB);
            }
        }
    }

    void AfterHttpPortChangeCallback(int ime_result)
    {
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            http_server_port = atoi(txt_http_server_port);
        }
    }

    void AfterEditorCallback(int ime_result)
    {
        if (ime_result == IME_DIALOG_RESULT_FINISHED)
        {
            std::string str = std::string(edit_line);
            edit_buffer[edit_line_num] = str;
            editor_modified = true;
        }
    }
}
