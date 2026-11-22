#include "daemon.h"

#include <dirent.h>
#include <fcntl.h>
#include <orbis/libkernel.h>
#include <stdarg.h>
#include <stdio.h>
#include <orbis/SystemService.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orbis_jbc.h"

#define LOADER_MAX_PATH 1024
#define LOADER_APP_TITLE_ID "RMTC00001"
#define LOADER_DAEMON_PATH "/system/vsh/app"

extern int sceLncUtilInitialize(void);
extern int sceLncUtilGetAppId(const char *title_id);
extern int sceLncUtilKillApp(uint32_t app_id);


void Notify(const char *fmt, ...)
{
    OrbisNotificationRequest request;
    memset(&request, 0, sizeof(request));

    va_list args;
    va_start(args, fmt);
    vsnprintf(request.message, sizeof(request.message), fmt, args);
    va_end(args);

    request.type = NotificationRequest;
    request.unk3 = 0;
    request.useIconImageUri = 0;
    request.targetId = -1;
    sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}

static bool JoinPath(char *out, size_t cap, const char *base, const char *name)
{
    size_t base_len = strlen(base);
    size_t name_len = name ? strlen(name) : 0;
    if (base_len + name_len + 2 > cap)
        return false;

    memcpy(out, base, base_len);
    size_t pos = base_len;
    if (pos > 0 && out[pos - 1] != '/')
        out[pos++] = '/';
    if (name_len > 0)
    {
        memcpy(out + pos, name, name_len);
        pos += name_len;
    }
    out[pos] = '\0';
    return true;
}

static bool AppendSuffix(char *path, size_t cap, const char *suffix)
{
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);
    if (path_len + suffix_len >= cap)
        return false;
    memcpy(path + path_len, suffix, suffix_len + 1);
    return true;
}

static bool FileExists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool FolderExists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void MkDirs(const char *path)
{
    char current[LOADER_MAX_PATH];
    size_t len = strlen(path);
    if (len >= sizeof(current))
        return;

    memcpy(current, path, len + 1);
    for (size_t i = 0; i < len; i++)
    {
        if (current[i] == '/' && i > 0)
        {
            current[i] = '\0';
            mkdir(current, 0777);
            current[i] = '/';
        }
    }
    mkdir(current, 0777);
}

static bool CopyFile(const char *src, const char *dst)
{
    int in = open(src, O_RDONLY);
    if (in < 0)
    {
        return false;
    }

    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0)
    {
        close(in);
        return false;
    }

    unsigned char buffer[64 * 1024];
    bool ok = true;
    for (;;)
    {
        ssize_t read_len = read(in, buffer, sizeof(buffer));
        if (read_len < 0)
        {
            ok = false;
            break;
        }
        if (read_len == 0)
            break;

        ssize_t written = 0;
        while (written < read_len)
        {
            ssize_t n = write(out, buffer + written, (size_t)(read_len - written));
            if (n <= 0)
            {
                ok = false;
                break;
            }
            written += n;
        }
        if (!ok)
            break;
    }

    close(in);
    close(out);
    return ok;
}

static int RmRecursive(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;

    if (!S_ISDIR(st.st_mode))
        return unlink(path) == 0 ? 1 : 0;

    DIR *dir = opendir(path);
    if (dir == NULL)
        return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char child[LOADER_MAX_PATH];
        if (!JoinPath(child, sizeof(child), path, entry->d_name) || !RmRecursive(child))
        {
            closedir(dir);
            return 0;
        }
    }
    closedir(dir);
    return rmdir(path) == 0 ? 1 : 0;
}

static void ReadVersion(const char *path, char *out, size_t cap)
{
    out[0] = '\0';
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return;

    ssize_t len = read(fd, out, cap - 1);
    close(fd);
    if (len <= 0)
    {
        out[0] = '\0';
        return;
    }
    out[len] = '\0';
    while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r'))
        out[--len] = '\0';
}

static bool ReadMd5(const char *path, char out[33])
{
    out[0] = '\0';
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    ssize_t len = read(fd, out, 32);
    close(fd);
    if (len != 32)
        return false;
    out[32] = '\0';
    return true;
}

static bool IsDaemonOutdated(const char *title_id, const char *eboot_name)
{
    char daemon_path[LOADER_MAX_PATH];
    char dest_md5[LOADER_MAX_PATH];
    char app0[LOADER_MAX_PATH];
    char app0_daemon[LOADER_MAX_PATH];
    char src_md5[LOADER_MAX_PATH];

    if (!JoinPath(daemon_path, sizeof(daemon_path), LOADER_DAEMON_PATH, title_id) ||
        !JoinPath(dest_md5, sizeof(dest_md5), daemon_path, "eboot.md5") ||
        !JoinPath(app0, sizeof(app0), "/mnt/sandbox/pfsmnt/" LOADER_APP_TITLE_ID "-app0", NULL) ||
        !JoinPath(app0_daemon, sizeof(app0_daemon), app0, "daemon") ||
        !JoinPath(src_md5, sizeof(src_md5), app0_daemon, eboot_name) ||
        !AppendSuffix(src_md5, sizeof(src_md5), ".md5"))
        return true;

    if (!FileExists(dest_md5) || !FileExists(src_md5))
        return true;

    char src[33];
    char dst[33];
    if (!ReadMd5(src_md5, src) || !ReadMd5(dest_md5, dst))
        return true;
    return strncmp(src, dst, 32) != 0;
}

bool is_daemon_running(const char *title_id)
{
    uint32_t app_id = (uint32_t)sceLncUtilGetAppId(title_id);
    return app_id != 0 && (app_id & ~0xFFFFFFu) == 0x60000000u;
}

bool stop_daemon(const char *title_id)
{
    sceLncUtilInitialize();
    if (!is_daemon_running(title_id))
        return true;

    uint32_t app_id = (uint32_t)sceLncUtilGetAppId(title_id);
    sceLncUtilKillApp(app_id);

    for (int i = 0; i < 50; i++)
    {
        if (!is_daemon_running(title_id))
            return true;
        usleep(100000);
    }
    return false;
}

static bool CopyDirectory(const char *src, const char *dst)
{
    DIR *dir = opendir(src);
    if (dir == NULL)
    {
        return false;
    }

    MkDirs(dst);

    struct dirent *entry;
    bool ok = true;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char src_path[LOADER_MAX_PATH];
        char dst_path[LOADER_MAX_PATH];
        if (!JoinPath(src_path, sizeof(src_path), src, entry->d_name) ||
            !JoinPath(dst_path, sizeof(dst_path), dst, entry->d_name))
        {
            ok = false;
            break;
        }

        struct stat st;
        if (stat(src_path, &st) != 0)
        {
            ok = false;
            break;
        }

        if (S_ISDIR(st.st_mode))
        {
            if (!CopyDirectory(src_path, dst_path))
            {
                ok = false;
                break;
            }
        }
        else if (S_ISREG(st.st_mode))
        {
            if (!CopyFile(src_path, dst_path))
            {
                ok = false;
                break;
            }
        }
    }

    closedir(dir);
    return ok;
}

bool is_headless_mode(void)
{
    int fd = open("/data/ezremote-client/config.ini", O_RDONLY);
    if (fd < 0)
        return false;

    char buffer[4096];
    ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (len <= 0)
        return false;
    buffer[len] = '\0';

    bool in_global = false;
    bool result = false;
    char *line = buffer;

    while (line != NULL && *line != '\0')
    {
        char *next = strchr(line, '\n');
        if (next != NULL)
            *next++ = '\0';

        while (*line == ' ' || *line == '\t' || *line == '\r')
            line++;
        char *end = line + strlen(line);
        while (end > line && (end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'))
            *--end = '\0';

        if (*line == '[' && end > line && end[-1] == ']')
        {
            *--end = '\0';
            in_global = strcasecmp(line + 1, "global") == 0;
        }
        else if (in_global)
        {
            char *equals = strchr(line, '=');
            if (equals != NULL)
            {
                *equals++ = '\0';
                if (strcasecmp(line, "headless_mode") == 0)
                {
                    while (*equals == ' ' || *equals == '\t')
                        equals++;
                    result = strcasecmp(equals, "true") == 0 ||
                             strcasecmp(equals, "yes") == 0 ||
                             strcasecmp(equals, "on") == 0 ||
                             strcmp(equals, "1") == 0;
                    break;
                }
            }
        }

        line = next;
    }

    return result;
}

bool launch_daemon(const char *title_id)
{
    struct DaemonLncAppParam
    {
        uint32_t size;
        int32_t userId;
        uint32_t appAttr;
        uint32_t enableCrashReport;
        uint64_t checkFlag;
    };

    sceLncUtilInitialize();

    // 0x8094000c means the daemon is already running; that is success for us.
    if (is_daemon_running(title_id))
    {
        Notify("ezRemote Client daemon is already running");
        return true;
    }

    struct DaemonLncAppParam param;
    memset(&param, 0, sizeof(param));
    param.size = sizeof(param);
    param.userId = -1;
    param.appAttr = 0;
    param.enableCrashReport = 0;
    param.checkFlag = 0;

    int32_t app_id = sceLncUtilLaunchApp(title_id, NULL,
                                         (LncAppParam *)&param);
    if (app_id > 0)
        return true;

    // Launch is asynchronous; wait for the app id to appear.
    for (int i = 0; i < 20; i++)
    {
        if (is_daemon_running(title_id))
            return true;
        usleep(100000);
    }
    return false;
}

int update_assets(const char *title_id, const char *eboot_name)
{
    char app0[LOADER_MAX_PATH];
    char app0_daemon[LOADER_MAX_PATH];
    char daemon_path[LOADER_MAX_PATH];
    char daemon_sce_sys[LOADER_MAX_PATH];
    char param_sfo[LOADER_MAX_PATH];
    char src_eboot[LOADER_MAX_PATH];
    char dest_eboot[LOADER_MAX_PATH];
    char src_eboot_md5[LOADER_MAX_PATH];
    char dest_eboot_md5[LOADER_MAX_PATH];
    char src_sfo[LOADER_MAX_PATH];
    char src_assets[LOADER_MAX_PATH];
    char dest_assets[LOADER_MAX_PATH];
    char src_daemon[LOADER_MAX_PATH];
    char dest_daemon[LOADER_MAX_PATH];
    char src_version[LOADER_MAX_PATH];
    char dest_version[LOADER_MAX_PATH];

    JoinPath(app0, sizeof(app0), "/mnt/sandbox/pfsmnt/" LOADER_APP_TITLE_ID "-app0", NULL);
    JoinPath(app0_daemon, sizeof(app0_daemon), app0, "daemon");
    JoinPath(daemon_path, sizeof(daemon_path), LOADER_DAEMON_PATH, title_id);
    JoinPath(daemon_sce_sys, sizeof(daemon_sce_sys), daemon_path, "sce_sys");
    JoinPath(param_sfo, sizeof(param_sfo), daemon_sce_sys, "param.sfo");
    JoinPath(src_eboot, sizeof(src_eboot), app0_daemon, eboot_name);
    AppendSuffix(src_eboot, sizeof(src_eboot), ".self");
    JoinPath(dest_eboot, sizeof(dest_eboot), daemon_path, "eboot.bin");
    JoinPath(src_eboot_md5, sizeof(src_eboot_md5), app0_daemon, eboot_name);
    AppendSuffix(src_eboot_md5, sizeof(src_eboot_md5), ".md5");
    JoinPath(dest_eboot_md5, sizeof(dest_eboot_md5), daemon_path, "eboot.md5");
    JoinPath(src_sfo, sizeof(src_sfo), app0_daemon, eboot_name);
    AppendSuffix(src_sfo, sizeof(src_sfo), ".sfo");
    JoinPath(src_assets, sizeof(src_assets), app0, "assets");
    JoinPath(dest_assets, sizeof(dest_assets), daemon_path, "assets");
    JoinPath(src_daemon, sizeof(src_daemon), app0, "daemon");
    JoinPath(dest_daemon, sizeof(dest_daemon), daemon_path, "daemon");
    JoinPath(src_version, sizeof(src_version), app0, "version.txt");
    JoinPath(dest_version, sizeof(dest_version), daemon_path, "version.txt");

    bool daemon_missing = !FileExists(dest_eboot) || IsDaemonOutdated(title_id, eboot_name);
    bool assets_outdated = false;
    if (FileExists(src_version))
    {
        char src_ver[128];
        char dst_ver[128];
        ReadVersion(src_version, src_ver, sizeof(src_ver));
        ReadVersion(dest_version, dst_ver, sizeof(dst_ver));
        assets_outdated = strcmp(src_ver, dst_ver) != 0 || !FolderExists(dest_assets);
    }

    if (!daemon_missing && !assets_outdated)
    {
        return 0;
    }

    stop_daemon(title_id);

    int mount_ret = mount_large_fs("/dev/da0x4.crypt", "/system", "exfatfs", "511", MNT_UPDATE);
    if (mount_ret != 0)
        return -1;

    if (daemon_missing)
    {
        RmRecursive(daemon_path);
        MkDirs(daemon_path);
        MkDirs(daemon_sce_sys);

        if (!CopyFile(src_sfo, param_sfo) ||
            !CopyFile(src_eboot, dest_eboot) ||
            !CopyFile(src_eboot_md5, dest_eboot_md5))
        {
            return -1;
        }
    }

    if (daemon_missing || assets_outdated)
    {
        RmRecursive(dest_assets);
        if (!CopyDirectory(src_assets, dest_assets))
        {
            return -1;
        }

        RmRecursive(dest_daemon);
        if (!CopyDirectory(src_daemon, dest_daemon))
        {
            return -1;
        }

        if (!CopyFile(src_version, dest_version))
        {
            return -1;
        }
    }

    return 1;
}
