#ifndef __ORBIS_JBC_H__
#define __ORBIS_JBC_H__

#define	MNT_UPDATE	0x0000000000010000ULL

int initialize_jbc();
void terminate_jbc();
int mount_large_fs(const char* device, const char* mountpoint, const char* fstype, const char* mode, unsigned int flags);

#endif
