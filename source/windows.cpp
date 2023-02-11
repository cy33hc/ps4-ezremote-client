#include <SDL2/SDL.h>
#include <orbis/SystemService.h>
#include <stdio.h>
#include <algorithm>
#include <set>
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

extern "C"
{
#include "inifile.h"
}

bool paused = false;
int view_mode;
static float scroll_direction = 0.0f;
static ime_callback_t ime_callback = nullptr;
static ime_callback_t ime_after_update = nullptr;
static ime_callback_t ime_before_update = nullptr;
static ime_callback_t ime_cancelled = nullptr;
static std::vector<std::string> *ime_multi_field;
static char *ime_single_field;
static int ime_field_size;

static char txt_server_port[6];
static char txt_http_port[6];

bool handle_updates = false;
int64_t bytes_transfered;
int64_t bytes_to_download;
std::vector<DirEntry> local_files;
std::vector<DirEntry> remote_files;
std::set<DirEntry> multi_selected_local_files;
std::set<DirEntry> multi_selected_remote_files;
DirEntry selected_local_file;
DirEntry selected_remote_file;
ACTIONS selected_action;
char status_message[1024];
char local_file_to_select[256];
char remote_file_to_select[256];
char local_filter[32];
char remote_filter[32];
char editor_text[1024];
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
char label[256];

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
        dont_prompt_overwrite = false;
        confirm_transfer_state = -1;
        dont_prompt_overwrite_cb = false;
        overwrite_type = OVERWRITE_PROMPT;

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
        cur_down[SDL_CONTROLLER_BUTTON_START] = SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_START);

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

        if (prev_down[SDL_CONTROLLER_BUTTON_START] && !cur_down[SDL_CONTROLLER_BUTTON_START] && !paused)
        {
            selected_action = ACTION_DISCONNECT_AND_EXIT;
        }

        prev_down[SDL_CONTROLLER_BUTTON_X] = cur_down[SDL_CONTROLLER_BUTTON_X];
        prev_down[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = cur_down[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER];
        prev_down[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = cur_down[SDL_CONTROLLER_BUTTON_LEFTSHOULDER];
        prev_down[SDL_CONTROLLER_BUTTON_START] = cur_down[SDL_CONTROLLER_BUTTON_START];
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
        if (multi_selected_local_files.size()>0)
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
            zipname = zipname.substr(zipname.find_last_of("/")+1);
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

        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        if (strncmp(files.begin()->directory, "/data", 5) != 0 &&
            strncmp(files.begin()->directory, "/mnt/usb", 8) != 0)
        {
            zipfolder = "/data";
        }
        else if (files.size() > 1)
        {
            zipfolder = files.begin()->directory;
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
        std::string hidden_password = std::string("xxxxxxx");
        ImVec2 pos;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
        bool is_connected = remoteclient != nullptr && remoteclient->IsConnected();

        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetItemDefaultFocus();
        }
        if (is_connected)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.3f);
        }
        if (ImGui::Button(lang_strings[STR_CONNECT], ImVec2(180, 0)))
        {
            selected_action = ACTION_CONNECT;
        }
        if (is_connected)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_CONNECT]);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();

        if (!is_connected)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.3f);
        }
        if (ImGui::Button(lang_strings[STR_DISCONNECT], ImVec2(200, 0)))
        {
            selected_action = ACTION_DISCONNECT;
        }
        if (!is_connected)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s", lang_strings[STR_DISCONNECT]);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();

        ImGui::SetNextItemWidth(120);
        if (ImGui::BeginCombo("##Site", display_site, ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLargest | ImGuiComboFlags_NoArrowButton))
        {
            static char site_id[64];
            for (int n = 0; n < sites.size(); n++)
            {
                const bool is_selected = strcmp(sites[n].c_str(), last_site) == 0;
                sprintf(site_id, "%s %d", lang_strings[STR_SITE], n + 1);
                if (ImGui::Selectable(site_id, is_selected))
                {
                    sprintf(last_site, "%s", sites[n].c_str());
                    sprintf(display_site, "%s", site_id);
                    remote_settings = &site_settings[sites[n]];
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 1.0f));
        sprintf(id, "%s##server", remote_settings->server);
        pos = ImGui::GetCursorPos();
        if (ImGui::Button(id, ImVec2(650, 0)))
        {
            ime_single_field = remote_settings->server;
            ResetImeCallbacks();
            ime_field_size = 255;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_SERVER], remote_settings->server, 255, ORBIS_TYPE_BASIC_LATIN, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        }
        ImGui::SameLine();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_USERNAME]);
        ImGui::SameLine();

        sprintf(id, "%s##username", remote_settings->username);
        pos = ImGui::GetCursorPos();
        if (ImGui::Button(id, ImVec2(180, 0)))
        {
            ime_single_field = remote_settings->username;
            ResetImeCallbacks();
            ime_field_size = 32;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_USERNAME], remote_settings->username, 32, ORBIS_TYPE_BASIC_LATIN, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        }
        ImGui::SameLine();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);
        ImGui::TextColored(colors[ImGuiCol_ButtonHovered], "%s:", lang_strings[STR_PASSWORD]);
        ImGui::SameLine();

        sprintf(id, "%s##password", hidden_password.c_str());
        pos = ImGui::GetCursorPos();
        if (ImGui::Button(id, ImVec2(100, 0)))
        {
            ime_single_field = remote_settings->password;
            ResetImeCallbacks();
            ime_field_size = 24;
            ime_callback = SingleValueImeCallback;
            Dialog::initImeDialog(lang_strings[STR_PASSWORD], remote_settings->password, 24, ORBIS_TYPE_BASIC_LATIN, pos.x, pos.y);
            gui_mode = GUI_MODE_IME;
        }

        ImGui::PopStyleVar();
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
                if (item.isDir)
                {
                    selected_local_file = item;
                    selected_action = ACTION_CHANGE_LOCAL_DIRECTORY;
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
            }
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            {
                if (strcmp(local_file_to_select, item.name) == 0)
                {
                    SetNavFocusHere();
                    ImGui::SetScrollHereY(0.5f);
                    sprintf(local_file_to_select, "");
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
                if (item.isDir)
                {
                    selected_remote_file = item;
                    selected_action = ACTION_CHANGE_REMOTE_DIRECTORY;
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
    }

    void StatusPanel()
    {
        ImGui::Dummy(ImVec2(0, 5));
        BeginGroupPanel(lang_strings[STR_MESSAGES], ImVec2(1905, 100));
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::Dummy(ImVec2(1880, 30));
        ImGui::SetCursorPos(pos);
        ImGui::PushTextWrapPos(1880);
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

    int getSelectableFlag()
    {
        int flag = ImGuiSelectableFlags_Disabled;
        bool local_browser_selected = saved_selected_browser & LOCAL_BROWSER;
        bool remote_browser_selected = saved_selected_browser & REMOTE_BROWSER;
        if ((local_browser_selected && selected_local_file.selectable) ||
            (remote_browser_selected && selected_remote_file.selectable))
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

        if (ImGui::IsKeyDown(ImGuiKey_GamepadFaceUp))
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
            ImGui::SetNextWindowPos(ImVec2(410, 350));
        }
        else if (remote_browser_selected)
        {
            ImGui::SetNextWindowPos(ImVec2(1330, 350));
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(230, 150), ImVec2(230, 450), NULL, NULL);
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


            ImGui::PushID("Delete##settings");
            if (ImGui::Selectable(lang_strings[STR_DELETE], false, getSelectableFlag() | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
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
            flags = getSelectableFlag();
            if ((local_browser_selected && multi_selected_local_files.size()>1) ||
                (remote_browser_selected && multi_selected_remote_files.size()>1))
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

            ImGui::PushID("New Folder##settings");
            if (ImGui::Selectable(lang_strings[STR_NEW_FOLDER], false, ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
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

            if (local_browser_selected)
            {
                ImGui::PushID("Extract##settings");
                if (ImGui::Selectable(lang_strings[STR_EXTRACT], false, getSelectableFlag() | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
                {
                    ResetImeCallbacks();
                    sprintf(extract_zip_folder, "%s", getExtractFolder().c_str());
                    ime_single_field = extract_zip_folder;
                    ime_field_size = 255;
                    ime_callback = SingleValueImeCallback;
                    ime_after_update = AfterExtractFolderCallback;
                    Dialog::initImeDialog(lang_strings[STR_EXTRACT_LOCATION], extract_zip_folder, 255, ORBIS_TYPE_BASIC_LATIN, 600, 350);
                    gui_mode = GUI_MODE_IME;
                    file_transfering = true;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                ImGui::Separator();

                ImGui::PushID("Compress##settings");
                if (ImGui::Selectable(lang_strings[STR_COMPRESS], false, getSelectableFlag() | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
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

                flags = ImGuiSelectableFlags_Disabled;
                if (remoteclient != nullptr && selected_local_file.selectable)
                {
                    flags = ImGuiSelectableFlags_None;
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
                if (ImGui::Selectable(lang_strings[STR_INSTALL], false, getSelectableFlag() | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
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
                if (ImGui::Selectable(lang_strings[STR_DOWNLOAD], false, getSelectableFlag() | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
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
                if (remoteclient != nullptr && remoteclient->clientType() != CLIENT_TYPE_WEBDAV)
                {
                    flags = ImGuiSelectableFlags_Disabled;
                }
                if (ImGui::Selectable(lang_strings[STR_INSTALL], false, getSelectableFlag() | ImGuiSelectableFlags_DontClosePopups, ImVec2(220, 0)))
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
                    progress = bytes_transfered * 1.0f / (float)bytes_to_download;
                    ImGui::ProgressBar(progress, ImVec2(625, 0));
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
                if (ImGui::Button(lang_strings[STR_ONETIME_URL], ImVec2(535, 0)))
                {
                    ResetImeCallbacks();
                    sprintf(install_pkg_url, "%s", "");
                    ime_single_field = install_pkg_url;
                    ime_field_size = 511;
                    ime_after_update = AfterPackageUrlCallback;
                    ime_callback = SingleValueImeCallback;
                    Dialog::initImeDialog("URL", install_pkg_url, 511, ORBIS_TYPE_BASIC_LATIN, 600, 340);
                    gui_mode = GUI_MODE_IME;
                    select_url_inprogress = false;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                sprintf(id, "%s##favoriteurl", lang_strings[STR_CANCEL]);
                if (ImGui::Button(id, ImVec2(535, 0)))
                {
                    select_url_inprogress = false;
                    SetModalMode(false);
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::Checkbox("##auto_delete_tmp_pkg", &auto_delete_tmp_pkg))
                {
                    CONFIG::SaveConfig();
                }
                ImGui::SameLine();
                ImGui::Text("%s", lang_strings[STR_AUTO_DELETE_TMP_PKG]);

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
                        sprintf(install_pkg_url, "%s", favorite_urls[j]);
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
        }
        ImGui::End();
    }

    void ExecuteActions()
    {
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
            if (gui_mode != GUI_MODE_IME)
            {
                sprintf(editor_text, "");
                ime_single_field = editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                ImVec2 pos = selected_action == ACTION_NEW_LOCAL_FOLDER ? ImVec2(410, 350) : ImVec2(1330, 350);
                Dialog::initImeDialog(lang_strings[STR_NEW_FOLDER], editor_text, 128, ORBIS_TYPE_BASIC_LATIN, pos.x, pos.y);
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
                stop_activity = false;
                Actions::DownloadFiles();
                confirm_transfer_state = -1;
                selected_action = ACTION_NONE;
            }
            break;
        case ACTION_EXTRACT_LOCAL_ZIP:
            sprintf(status_message, "%s", "");
            activity_inprogess = true;
            stop_activity = false;
            file_transfering = true;
            selected_action = ACTION_NONE;
            Actions::ExtractLocalZips();
            break;
        case ACTION_CREATE_LOCAL_ZIP:
            sprintf(status_message, "%s", "");
            activity_inprogess = true;
            stop_activity = false;
            file_transfering = true;
            selected_action = ACTION_NONE;
            Actions::MakeLocalZip();
            break;
        case ACTION_RENAME_LOCAL:
            if (gui_mode != GUI_MODE_IME)
            {
                if (multi_selected_local_files.size()>0)
                    sprintf(editor_text, "%s", multi_selected_local_files.begin()->name);
                else
                    sprintf(editor_text, "%s", selected_local_file.name);
                ime_single_field = editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_RENAME], editor_text, 128, ORBIS_TYPE_BASIC_LATIN, 410, 350);
                gui_mode = GUI_MODE_IME;
            }
            break;
        case ACTION_RENAME_REMOTE:
            if (gui_mode != GUI_MODE_IME)
            {
                if (multi_selected_remote_files.size()>0)
                    sprintf(editor_text, "%s", multi_selected_remote_files.begin()->name);
                else
                    sprintf(editor_text, "%s", selected_remote_file.name);
                ime_single_field = editor_text;
                ResetImeCallbacks();
                ime_field_size = 128;
                ime_after_update = AfterFolderNameCallback;
                ime_cancelled = CancelActionCallBack;
                ime_callback = SingleValueImeCallback;
                Dialog::initImeDialog(lang_strings[STR_RENAME], editor_text, 128, ORBIS_TYPE_BASIC_LATIN, 1330, 350);
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
            done = true;
            break;
        case ACTION_INSTALL_REMOTE_PKG:
            sprintf(status_message, "%s", "");
            activity_inprogess = true;
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
        if (selected_action == ACTION_NEW_LOCAL_FOLDER)
        {
            Actions::CreateNewLocalFolder(editor_text);
        }
        else if (selected_action == ACTION_NEW_REMOTE_FOLDER)
        {
            Actions::CreateNewRemoteFolder(editor_text);
        }
        else if (selected_action == ACTION_RENAME_LOCAL)
        {
            if (multi_selected_local_files.size()>0)
                Actions::RenameLocalFolder(multi_selected_local_files.begin()->path, editor_text);
            else
                Actions::RenameLocalFolder(selected_local_file.path, editor_text);
        }
        else if (selected_action == ACTION_RENAME_REMOTE)
        {
            if (multi_selected_remote_files.size()>0)
                Actions::RenameRemoteFolder(multi_selected_remote_files.begin()->path, editor_text);
            else
                Actions::RenameRemoteFolder(selected_remote_file.path, editor_text);
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

    void AfterZipFileCallback(int ime_result)
    {
        selected_action = ACTION_CREATE_LOCAL_ZIP;
    }
}
