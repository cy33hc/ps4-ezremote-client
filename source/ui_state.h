#ifndef EZ_UI_STATE_H
#define EZ_UI_STATE_H

#include <stdint.h>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "common.h"
#include "actions.h"

#define CONFIRM_KIND_YESNO 0
#define CONFIRM_KIND_OVERWRITE 1

// State shared by the native UI and the headless payload build. Nothing here
// pulls in ImGui or SDL, so the headless target can link it without the UI.
extern bool paused;
extern int view_mode;
extern bool is_server_started;
extern bool ezremote_server_version_match;
extern bool handle_updates;

extern std::vector<DirEntry> local_files;
extern std::vector<DirEntry> remote_files;
extern std::set<DirEntry> multi_selected_local_files;
extern std::set<DirEntry> multi_selected_remote_files;
extern std::vector<DirEntry> local_paste_files;
extern std::vector<DirEntry> remote_paste_files;
extern std::vector<DownloadProgress> bg_download_progress;
extern DirEntry selected_local_file;
extern DirEntry selected_remote_file;
extern ACTIONS selected_action;
extern ACTIONS paste_action;
extern char status_message[1024];
extern char local_file_to_select[256];
extern char remote_file_to_select[256];
extern char local_filter[32];
extern char remote_filter[32];
extern char dialog_editor_text[1024];
extern char activity_message[1024];
extern int selected_browser;
extern int saved_selected_browser;
extern bool quit_pending;
extern bool set_focus_to_local;
extern bool set_focus_to_remote;
extern bool select_url_inprogress;
extern int favorite_url_idx;
extern char extract_zip_folder[256];
extern char zip_file_path[384];
extern bool show_settings;
extern bool show_activities;
extern bool show_bg_download_progress;
extern uint64_t refresh_bg_download_time;

extern std::vector<std::string> edit_buffer;
extern bool editor_inprogress;
extern char edit_line[1024];
extern int edit_line_num;
extern char label[256];
extern bool editor_modified;
extern char edit_file[256];
extern int edit_line_to_select;
extern std::string copy_text;

extern bool view_image;
extern bool show_pkg_info;
extern std::map<std::string, std::string> sfo_params;

extern bool dont_prompt_overwrite;
extern bool dont_prompt_overwrite_cb;
extern int confirm_transfer_state;
extern int overwrite_type;
extern int confirm_state;
extern int confirm_kind;
extern char confirm_message[256];
extern ACTIONS action_to_take;

// Owned by the HTTP server / render loop. Defined in ui_state.cpp so the
// headless build has it without gui.cpp.
extern bool done;

namespace Windows
{
    void SetModalMode(bool modal);
}

#endif
