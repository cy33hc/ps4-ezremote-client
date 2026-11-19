#include "ui_state.h"

bool paused = false;
int view_mode;
bool is_server_started = false;
bool ezremote_server_version_match = true;
bool handle_updates = false;

std::vector<DirEntry> local_files;
std::vector<DirEntry> remote_files;
std::set<DirEntry> multi_selected_local_files;
std::set<DirEntry> multi_selected_remote_files;
std::vector<DirEntry> local_paste_files;
std::vector<DirEntry> remote_paste_files;
std::vector<DownloadProgress> bg_download_progress;
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
bool quit_pending = false;
bool set_focus_to_local = false;
bool set_focus_to_remote = false;
bool select_url_inprogress = false;
int favorite_url_idx = 0;
char extract_zip_folder[256];
char zip_file_path[384];
bool show_settings = false;
bool show_activities = false;
bool show_bg_download_progress = false;
uint64_t refresh_bg_download_time;

std::vector<std::string> edit_buffer;
bool editor_inprogress = false;
char edit_line[1024];
int edit_line_num = 0;
char label[256];
bool editor_modified = false;
char edit_file[256];
int edit_line_to_select = -1;
std::string copy_text;

bool view_image = false;
bool show_pkg_info = false;
std::map<std::string, std::string> sfo_params;

bool dont_prompt_overwrite = false;
bool dont_prompt_overwrite_cb = false;
int confirm_transfer_state = -1;
int overwrite_type = OVERWRITE_PROMPT;

int confirm_state = CONFIRM_NONE;
int confirm_kind = CONFIRM_KIND_YESNO;
char confirm_message[256];
ACTIONS action_to_take = ACTION_NONE;

bool done = false;

namespace Windows
{
    void SetModalMode(bool modal)
    {
        paused = modal;
    }
}
