#include <stdlib.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>

#include "rtc.h"

int (*sceRtcGetTick)(const OrbisDateTime *inOrbisDateTime, OrbisTick *outTick);
int (*sceRtcSetTick)(OrbisDateTime *outOrbisDateTime, const OrbisTick *inputTick);
int (*sceRtcConvertLocalTimeToUtc)(const OrbisTick *local_time, OrbisTick *utc);
int (*sceRtcConvertUtcToLocalTime)(const OrbisTick *utc, OrbisTick *local_time);
int (*sceRtcGetCurrentClockLocalTime)(OrbisDateTime *time);

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

int load_rtc_module()
{
    int rtc_handle = sceKernelLoadStartModule("/system/common/lib/libSceRtc.sprx", 0, NULL, 0, NULL, NULL);
    if (rtc_handle == 0)
    {
        return -1;
    }

    sceKernelDlsym(rtc_handle, "sceRtcGetTick", (void **)&sceRtcGetTick);
    if (sceRtcGetTick == NULL)
    {
        return -1;
    }

    sceKernelDlsym(rtc_handle, "sceRtcSetTick", (void **)&sceRtcSetTick);
    if (sceRtcSetTick == NULL)
    {
        return -1;
    }

    sceKernelDlsym(rtc_handle, "sceRtcConvertLocalTimeToUtc", (void **)&sceRtcConvertLocalTimeToUtc);
    if (sceRtcConvertLocalTimeToUtc == NULL)
    {
        return -1;
    }

    sceKernelDlsym(rtc_handle, "sceRtcConvertUtcToLocalTime", (void **)&sceRtcConvertUtcToLocalTime);
    if (sceRtcConvertUtcToLocalTime == NULL)
    {
        return -1;
    }

    sceKernelDlsym(rtc_handle, "sceRtcGetCurrentClockLocalTime", (void **)&sceRtcGetCurrentClockLocalTime);
    if (sceRtcGetCurrentClockLocalTime == NULL)
    {
        return -1;
    }

    return 0;
}