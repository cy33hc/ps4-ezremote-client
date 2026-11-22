/* The payload SDK's libc does not provide memccpy(3); some of the third-party
 * libraries linked into the client call it. */
#include <stddef.h>

void *memccpy(void *dest, const void *src, int c, size_t n)
{
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;

    while (n--)
    {
        *d++ = *s;
        if (*s == (unsigned char) c)
            return d;
        s++;
    }

    return NULL;
}
