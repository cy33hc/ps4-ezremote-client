#ifndef EZREMOTE_DAEMON_H
#define EZREMOTE_DAEMON_H

#include <string>

// True when Lnc reports the title as a running application.
bool is_daemon_running(const std::string &title_id);

// Installs/updates the daemon files and launches it if it is not running.
// Returns false when the files could not be staged.
bool boot_daemon_services(const std::string &title_id, const std::string &eboot_name);

// Asks Lnc to kill the running daemon. Returns true once it is no longer
// running (or was not running to begin with).
bool stop_daemon(const std::string &title_id);

#endif
