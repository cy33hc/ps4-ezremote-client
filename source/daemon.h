#ifndef EZREMOTE_DAEMON_H
#define EZREMOTE_DAEMON_H

#include <string>

bool is_daemon_running(const std::string &title_id);

bool update_assets(const std::string &title_id, const std::string &eboot_name);

bool start_daemon(const std::string &title_id);

bool stop_daemon(const std::string &title_id);

bool start_self(const std::string &self_path);
#endif
