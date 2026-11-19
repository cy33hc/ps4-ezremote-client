/*
 * Payload-only shims for the two app libraries the client normally imports:
 * libSceBgft and libSceAppInstUtil.
 *
 * A GoldHEN payload is not an application, so the loader cannot resolve those
 * DT_NEEDED entries (it reports "Unable to load the library 'libSceBgft.so'").
 * Instead, the real sprx modules are loaded lazily with
 * sceKernelLoadStartModule() and the few functions the client uses are
 * forwarded through dlsym'ed pointers. If a module cannot be loaded, the
 * wrapper returns an error and install/uninstall operations fail cleanly.
 */
#ifdef EZREMOTE_PAYLOAD

#include <mutex>
#include <orbis/libkernel.h>
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>

namespace
{
    struct Symbol
    {
        const char *name;
        void **addr;
    };

    int LoadAndResolve(const char *const *paths, const Symbol *symbols, int count)
    {
        int handle = -1;
        for (int i = 0; paths[i] != nullptr && handle <= 0; i++)
            handle = sceKernelLoadStartModule(paths[i], 0, nullptr, 0, nullptr, nullptr);

        if (handle <= 0)
            return -1;

        for (int i = 0; i < count; i++)
        {
            if (sceKernelDlsym(handle, symbols[i].name, symbols[i].addr) < 0)
                return -1;
        }
        return 0;
    }

    // ---------------- BGFT ----------------

    typedef int (*BgftInitFn)(OrbisBgftInitParams *);
    typedef int (*BgftTermFn)();
    typedef int (*BgftGetProgressFn)(OrbisBgftTaskId, OrbisBgftTaskProgress *);
    typedef int (*BgftStartTaskFn)(OrbisBgftTaskId);
    typedef int (*BgftRegisterPkgFn)(OrbisBgftDownloadParam *, OrbisBgftTaskId *);
    typedef int (*BgftRegisterTaskFn)(OrbisBgftDownloadParam *, OrbisBgftTaskId *);
    typedef int (*BgftRegisterStorageFn)(OrbisBgftDownloadParamEx *, OrbisBgftTaskId *);

    BgftInitFn g_bgft_init;
    BgftTermFn g_bgft_term;
    BgftGetProgressFn g_bgft_get_progress;
    BgftStartTaskFn g_bgft_start_task;
    BgftRegisterPkgFn g_bgft_debug_register;
    BgftRegisterTaskFn g_bgft_register;
    BgftRegisterStorageFn g_bgft_register_storage;

    std::once_flag g_bgft_once;
    int g_bgft_status = -1;

    void LoadBgft()
    {
        static const char *const paths[] = {
            "/system/common/lib/libSceBgft.sprx",
            "/system/priv/lib/libSceBgft.sprx",
            nullptr,
        };
        const Symbol symbols[] = {
            {"sceBgftServiceIntInit", reinterpret_cast<void **>(&g_bgft_init)},
            {"sceBgftServiceIntTerm", reinterpret_cast<void **>(&g_bgft_term)},
            {"sceBgftServiceDownloadGetProgress", reinterpret_cast<void **>(&g_bgft_get_progress)},
            {"sceBgftServiceDownloadStartTask", reinterpret_cast<void **>(&g_bgft_start_task)},
            {"sceBgftServiceIntDebugDownloadRegisterPkg", reinterpret_cast<void **>(&g_bgft_debug_register)},
            {"sceBgftServiceIntDownloadRegisterTask", reinterpret_cast<void **>(&g_bgft_register)},
            {"sceBgftServiceIntDownloadRegisterTaskByStorageEx", reinterpret_cast<void **>(&g_bgft_register_storage)},
        };
        g_bgft_status = LoadAndResolve(paths, symbols, sizeof(symbols) / sizeof(symbols[0]));
    }

    int EnsureBgft()
    {
        std::call_once(g_bgft_once, LoadBgft);
        return g_bgft_status;
    }

    // ---------------- AppInstUtil ----------------

    typedef int (*AppInstGetTitleIdFn)(const char *, char *, int *);
    typedef int (*AppInstUninstallFn)(const char *);

    AppInstGetTitleIdFn g_appinst_get_title_id;
    AppInstUninstallFn g_appinst_uninstall;

    std::once_flag g_appinst_once;
    int g_appinst_status = -1;

    void LoadAppInst()
    {
        static const char *const paths[] = {
            "/system/common/lib/libSceAppInstUtil.sprx",
            "/system/priv/lib/libSceAppInstUtil.sprx",
            nullptr,
        };
        const Symbol symbols[] = {
            {"sceAppInstUtilGetTitleIdFromPkg", reinterpret_cast<void **>(&g_appinst_get_title_id)},
            {"sceAppInstUtilAppUnInstall", reinterpret_cast<void **>(&g_appinst_uninstall)},
        };
        g_appinst_status = LoadAndResolve(paths, symbols, sizeof(symbols) / sizeof(symbols[0]));
    }

    int EnsureAppInst()
    {
        std::call_once(g_appinst_once, LoadAppInst);
        return g_appinst_status;
    }
}

extern "C"
{
    int sceBgftServiceIntInit(OrbisBgftInitParams *params)
    {
        return EnsureBgft() == 0 ? g_bgft_init(params) : -1;
    }

    int sceBgftServiceIntTerm()
    {
        return EnsureBgft() == 0 ? g_bgft_term() : -1;
    }

    int sceBgftServiceDownloadGetProgress(OrbisBgftTaskId taskId, OrbisBgftTaskProgress *progress)
    {
        return EnsureBgft() == 0 ? g_bgft_get_progress(taskId, progress) : -1;
    }

    int sceBgftServiceDownloadStartTask(OrbisBgftTaskId taskId)
    {
        return EnsureBgft() == 0 ? g_bgft_start_task(taskId) : -1;
    }

    int sceBgftServiceIntDebugDownloadRegisterPkg(OrbisBgftDownloadParam *params, OrbisBgftTaskId *taskId)
    {
        return EnsureBgft() == 0 ? g_bgft_debug_register(params, taskId) : -1;
    }

    int sceBgftServiceIntDownloadRegisterTask(OrbisBgftDownloadParam *params, OrbisBgftTaskId *taskId)
    {
        return EnsureBgft() == 0 ? g_bgft_register(params, taskId) : -1;
    }

    int sceBgftServiceIntDownloadRegisterTaskByStorageEx(OrbisBgftDownloadParamEx *params, OrbisBgftTaskId *taskId)
    {
        return EnsureBgft() == 0 ? g_bgft_register_storage(params, taskId) : -1;
    }

    int sceAppInstUtilGetTitleIdFromPkg(const char *pkgPath, char *titleId, int *isApp)
    {
        return EnsureAppInst() == 0 ? g_appinst_get_title_id(pkgPath, titleId, isApp) : -1;
    }

    int sceAppInstUtilAppUnInstall(const char *titleId)
    {
        return EnsureAppInst() == 0 ? g_appinst_uninstall(titleId) : -1;
    }
}

#endif  // EZREMOTE_PAYLOAD
