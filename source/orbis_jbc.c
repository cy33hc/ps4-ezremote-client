#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/uio.h>
#include <orbis/libkernel.h>
#include <libjbc.h>

#define SYSCALL(nr, fn) __attribute__((naked)) fn\
{\
    asm volatile("mov $" #nr ", %rax\nmov %rcx, %r10\nsyscall\nret");\
}

SYSCALL(22, static int unmount(const char* path, int flags))
SYSCALL(378, static int nmount(struct iovec* iov, unsigned int niov, int flags))

static void build_iovec(struct iovec** iov, int* iovlen, const char* name, const void* val, size_t len) {
	int i;

	if (*iovlen < 0)
		return;

	i = *iovlen;
	*iov = (struct iovec*)realloc(*iov, sizeof **iov * (i + 2));
	if (*iov == NULL) {
		*iovlen = -1;
		return;
	}

	(*iov)[i].iov_base = strdup(name);
	(*iov)[i].iov_len = strlen(name) + 1;
	++i;

	(*iov)[i].iov_base = (void*)val;
	if (len == (size_t)-1) {
		if (val != NULL)
			len = strlen((const char*)val) + 1;
		else
			len = 0;
	}
	(*iov)[i].iov_len = (int)len;

	*iovlen = ++i;
}

int mount_large_fs(const char* device, const char* mountpoint, const char* fstype, const char* mode, unsigned int flags)
{
	struct iovec* iov = NULL;
	int iovlen = 0;

	unmount(mountpoint, 0);

	build_iovec(&iov, &iovlen, "fstype", fstype, -1);
	build_iovec(&iov, &iovlen, "fspath", mountpoint, -1);
	build_iovec(&iov, &iovlen, "from", device, -1);
	build_iovec(&iov, &iovlen, "large", "yes", -1);
	build_iovec(&iov, &iovlen, "timezone", "static", -1);
	build_iovec(&iov, &iovlen, "async", "", -1);
	build_iovec(&iov, &iovlen, "ignoreacl", "", -1);

	if (mode) {
		build_iovec(&iov, &iovlen, "dirmask", mode, -1);
		build_iovec(&iov, &iovlen, "mask", mode, -1);
	}

	return nmount(iov, iovlen, flags);
}

// Variables for (un)jailbreaking
jbc_cred g_Cred;
jbc_cred g_RootCreds;

// Verify jailbreak
static int is_jailbroken()
{
    FILE *s_FilePointer = fopen("/user/.jailbreak", "w");

    if (!s_FilePointer)
        return 0;

    fclose(s_FilePointer);
    remove("/user/.jailbreak");
    return 1;
}

// Jailbreaks creds
static int jailbreak()
{
    if (is_jailbroken())
    {
        return 1;
    }

    jbc_get_cred(&g_Cred);
    g_RootCreds = g_Cred;
    jbc_jailbreak_cred(&g_RootCreds);
    jbc_set_cred(&g_RootCreds);

    return (is_jailbroken());
}

// Initialize jailbreak
int initialize_jbc()
{
    // Pop notification depending on jailbreak result
    if (!jailbreak())
    {
        return 0;
    }

    return 1;
}

// Unload libjbc libraries
void terminate_jbc()
{
    if (!is_jailbroken())
        return;

    // Restores original creds
    jbc_set_cred(&g_Cred);
}
