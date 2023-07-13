#include <stdlib.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>

#include "system.h"

int (*sceRtcGetTick)(const OrbisDateTime *inOrbisDateTime, OrbisTick *outTick);
int (*sceRtcSetTick)(OrbisDateTime *outOrbisDateTime, const OrbisTick *inputTick);
int (*sceRtcConvertLocalTimeToUtc)(const OrbisTick *local_time, OrbisTick *utc);
int (*sceRtcConvertUtcToLocalTime)(const OrbisTick *utc, OrbisTick *local_time);
int (*sceRtcGetCurrentClockLocalTime)(OrbisDateTime *time);
int (*sceRtcGetCurrentTick)(OrbisTick *outTick);
int (*sceRtcFormatRFC3339LocalTime)(char *pszDateTime, const OrbisTick *tick);
unsigned int (*sceRtcGetTickResolution)();
int (*sceShellUIUtilLaunchByUri)(const char *uri, SceShellUIUtilLaunchByUriParam *param);
int (*sceShellUIUtilInitialize)();

void convertUtcToLocalTime(const OrbisDateTime *utc, OrbisDateTime *local_time)
{
    OrbisTick utc_tick;
    OrbisTick local_tick;
    sceRtcGetTick(utc, &utc_tick);
    sceRtcConvertUtcToLocalTime(&utc_tick, &local_tick);
    sceRtcSetTick(local_time, &local_tick);
}

void convertLocalTimeToUtc(const OrbisDateTime *local_time, OrbisDateTime *utc)
{
    OrbisTick utc_tick;
    OrbisTick local_tick;
    sceRtcGetTick(local_time, &local_tick);
    sceRtcConvertLocalTimeToUtc(&local_tick, &utc_tick);
    sceRtcSetTick(utc, &utc_tick);
}

int load_sys_modules()
{
    int handle = sceKernelLoadStartModule("/system/common/lib/libSceRtc.sprx", 0, NULL, 0, NULL, NULL);
    if (handle == 0)
    {
        return -1;
    }

    sceKernelDlsym(handle, "sceRtcGetTick", (void **)&sceRtcGetTick);
    if (sceRtcGetTick == NULL)
    {
        return -1;
    }

    sceKernelDlsym(handle, "sceRtcSetTick", (void **)&sceRtcSetTick);
    if (sceRtcSetTick == NULL)
    {
        return -1;
    }

    sceKernelDlsym(handle, "sceRtcConvertLocalTimeToUtc", (void **)&sceRtcConvertLocalTimeToUtc);
    if (sceRtcConvertLocalTimeToUtc == NULL)
    {
        return -1;
    }

    sceKernelDlsym(handle, "sceRtcConvertUtcToLocalTime", (void **)&sceRtcConvertUtcToLocalTime);
    if (sceRtcConvertUtcToLocalTime == NULL)
    {
        return -1;
    }

    sceKernelDlsym(handle, "sceRtcGetCurrentClockLocalTime", (void **)&sceRtcGetCurrentClockLocalTime);
    if (sceRtcGetCurrentClockLocalTime == NULL)
    {
        return -1;
    }

    sceKernelDlsym(handle, "sceRtcGetCurrentTick", (void **)&sceRtcGetCurrentTick);
    if (sceRtcGetCurrentTick == NULL)
    {
        return -1;
    }

    sceKernelDlsym(handle, "sceRtcFormatRFC3339LocalTime", (void **)&sceRtcFormatRFC3339LocalTime);
    if (sceRtcFormatRFC3339LocalTime == NULL)
    {
        return -1;
    }

    sceKernelDlsym(handle, "sceRtcGetTickResolution", (void **)&sceRtcGetTickResolution);
    if (sceRtcGetTickResolution == NULL)
    {
        return -1;
    }

    handle = sceKernelLoadStartModule("/system/common/lib/libSceShellUIUtil.sprx", 0, NULL, 0, 0, 0);
    if (handle == 0)
    {
        return -1;
    }

    sceKernelDlsym(handle, "sceShellUIUtilInitialize", (void **)&sceShellUIUtilInitialize);
    if (sceShellUIUtilInitialize == NULL)
    {
        return -1;
    }

    sceKernelDlsym(handle, "sceShellUIUtilLaunchByUri", (void **)&sceShellUIUtilLaunchByUri);
    if (sceShellUIUtilLaunchByUri == NULL)
    {
        return -1;
    }

    if (sceShellUIUtilInitialize() < 0) return -1;

    return 0;
}