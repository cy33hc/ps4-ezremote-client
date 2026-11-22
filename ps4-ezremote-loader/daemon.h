#ifndef EZREMOTE_LOADER_DAEMON_H
#define EZREMOTE_LOADER_DAEMON_H

#include <stdbool.h>

void Notify(const char *fmt, ...);
bool is_headless_mode(void);
int update_assets(const char *title_id, const char *eboot_name);
bool is_daemon_running(const char *title_id);
bool stop_daemon(const char *title_id);
bool launch_daemon(const char *title_id);

#endif
