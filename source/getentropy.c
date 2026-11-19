#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#ifdef EZREMOTE_PAYLOAD
/* The payload SDK's libc has no getrandom(2); arc4random_buf(3) is available
 * and already backed by the kernel's random source. */
extern void arc4random_buf(void *buf, size_t nbytes);

int getentropy(void *buffer, size_t len)
{
    if (len > 256) {
        errno = EIO;
        return -1;
    }

    arc4random_buf(buffer, len);
    return 0;
}

#else

#include <sys/random.h>

int getentropy(void *buffer, size_t len)
{
    int cs, ret = 0;
    char *pos = buffer;

    if (len > 256) {
        errno = EIO;
        return -1;
    }

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cs);

    while (len) {
        ret = getrandom(pos, len, 0);
        if (ret < 0) {
            if (errno == EINTR) continue;
            else break;
        }
        pos += ret;
        len -= ret;
        ret = 0;
    }

    pthread_setcancelstate(cs, 0);

    return ret;
}

#endif /* EZREMOTE_PAYLOAD */
