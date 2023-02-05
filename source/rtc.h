#include <stdint.h>

#ifdef __cplusplus 
extern "C" {
#endif

typedef struct OrbisTick {
        uint64_t mytick;
} OrbisTick;

typedef struct OrbisDateTime {
        unsigned short year;
        unsigned short month;
        unsigned short day;
        unsigned short hour;
        unsigned short minute;
        unsigned short second;
        unsigned int microsecond;
} OrbisDateTime;

extern int (*sceRtcGetTick)(const OrbisDateTime *inOrbisDateTime, OrbisTick *outTick);
extern int (*sceRtcSetTick)(OrbisDateTime *outOrbisDateTime, const OrbisTick *inputTick);
extern int (*sceRtcConvertLocalTimeToUtc)(const OrbisTick *local_time, OrbisTick *utc);
extern int (*sceRtcConvertUtcToLocalTime)(const OrbisTick *utc, OrbisTick *local_time);
extern int (*sceRtcGetCurrentClockLocalTime)(OrbisDateTime *time) ;

int load_rtc_module();
void convertUtcToLocalTime(const OrbisDateTime *utc, OrbisDateTime *local_time);
void convertLocalTimeToUtc(const OrbisDateTime *local_time, OrbisDateTime *utc);

#ifdef __cplusplus
}
#endif
