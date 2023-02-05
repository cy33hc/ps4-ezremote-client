#pragma once

#define SWAP16(x) \
	((uint16_t)( \
		(((uint16_t)(x) & UINT16_C(0x00FF)) << 8) | \
		(((uint16_t)(x) & UINT16_C(0xFF00)) >> 8) \
	))

#define SWAP32(x) \
	((uint32_t)( \
		(((uint32_t)(x) & UINT32_C(0x000000FF)) << 24) | \
		(((uint32_t)(x) & UINT32_C(0x0000FF00)) <<  8) | \
		(((uint32_t)(x) & UINT32_C(0x00FF0000)) >>  8) | \
		(((uint32_t)(x) & UINT32_C(0xFF000000)) >> 24) \
	))

#define SWAP64(x) \
	((uint64_t)( \
		(uint64_t)(((uint64_t)(x) & UINT64_C(0x00000000000000FF)) << 56) | \
		(uint64_t)(((uint64_t)(x) & UINT64_C(0x000000000000FF00)) << 40) | \
		(uint64_t)(((uint64_t)(x) & UINT64_C(0x0000000000FF0000)) << 24) | \
		(uint64_t)(((uint64_t)(x) & UINT64_C(0x00000000FF000000)) <<  8) | \
		(uint64_t)(((uint64_t)(x) & UINT64_C(0x000000FF00000000)) >>  8) | \
		(uint64_t)(((uint64_t)(x) & UINT64_C(0x0000FF0000000000)) >> 24) | \
		(uint64_t)(((uint64_t)(x) & UINT64_C(0x00FF000000000000)) >> 40) | \
		(uint64_t)(((uint64_t)(x) & UINT64_C(0xFF00000000000000)) >> 56) \
	))

#define LE16(x) (x)
#define LE32(x) (x)
#define LE64(x) (x)

#define BE16(x) SWAP16(x)
#define BE32(x) SWAP32(x)
#define BE64(x) SWAP64(x)

#define PKG_MAGIC 0x7F434E54

#define PKG_CONTENT_FLAGS_FIRST_PATCH 0x00100000
#define PKG_CONTENT_FLAGS_PATCHGO 0x00200000
#define PKG_CONTENT_FLAGS_REMASTER 0x00400000
#define PKG_CONTENT_FLAGS_PS_CLOUD 0x00800000
#define PKG_CONTENT_FLAGS_GD_AC 0x02000000
#define PKG_CONTENT_FLAGS_NON_GAME 0x04000000
#define PKG_CONTENT_FLAGS_0x8000000 0x08000000 /* has data? */
#define PKG_CONTENT_FLAGS_SUBSEQUENT_PATCH 0x40000000
#define PKG_CONTENT_FLAGS_DELTA_PATCH 0x41000000
#define PKG_CONTENT_FLAGS_CUMULATIVE_PATCH 0x60000000

typedef struct
{
    uint32_t pkg_magic;                 // 0x000 - 0x7F434E54
    uint32_t pkg_type;                  // 0x004
    uint32_t pkg_0x008;                 // 0x008 - unknown field
    uint32_t pkg_file_count;            // 0x00C
    uint32_t pkg_entry_count;           // 0x010
    uint16_t pkg_sc_entry_count;        // 0x014
    uint16_t pkg_entry_count_2;         // 0x016 - same as pkg_entry_count
    uint32_t pkg_table_offset;          // 0x018 - file table offset
    uint32_t pkg_entry_data_size;       // 0x01C
    uint64_t pkg_body_offset;           // 0x020 - offset of PKG entries
    uint64_t pkg_body_size;             // 0x028 - length of all PKG entries
    uint64_t pkg_content_offset;        // 0x030
    uint64_t pkg_content_size;          // 0x038
    unsigned char pkg_content_id[0x24]; // 0x040 - packages' content ID as a 36-byte string
    unsigned char pkg_padding[0xC];     // 0x064 - padding
    uint32_t pkg_drm_type;              // 0x070 - DRM type
    uint32_t pkg_content_type;          // 0x074 - Content type
    uint32_t pkg_content_flags;         // 0x078 - Content flags
    uint32_t pkg_promote_size;          // 0x07C
    uint32_t pkg_version_date;          // 0x080
    uint32_t pkg_version_hash;          // 0x084
    uint32_t pkg_0x088;                 // 0x088
    uint32_t pkg_0x08C;                 // 0x08C
    uint32_t pkg_0x090;                 // 0x090
    uint32_t pkg_0x094;                 // 0x094
    uint32_t pkg_iro_tag;               // 0x098
    uint32_t pkg_drm_type_version;      // 0x09C

    unsigned char digest_entries1[0x20];     // 0x100 - sha256 digest for main entry 1
    unsigned char digest_entries2[0x20];     // 0x120 - sha256 digest for main entry 2
    unsigned char digest_table_digest[0x20]; // 0x140 - sha256 digest for digest table
    unsigned char digest_body_digest[0x20];  // 0x160 - sha256 digest for main table

    uint32_t pfs_image_count;              // 0x404 - count of PFS images
    uint64_t pfs_image_flags;              // 0x408 - PFS flags
    uint64_t pfs_image_offset;             // 0x410 - offset to start of external PFS image
    uint64_t pfs_image_size;               // 0x418 - size of external PFS image
    uint64_t mount_image_offset;           // 0x420
    uint64_t mount_image_size;             // 0x428
    uint64_t pkg_size;                     // 0x430
    uint32_t pfs_signed_size;              // 0x438
    uint32_t pfs_cache_size;               // 0x43C
    unsigned char pfs_image_digest[0x20];  // 0x440
    unsigned char pfs_signed_digest[0x20]; // 0x460
    uint64_t pfs_split_size_nth_0;         // 0x480
    uint64_t pfs_split_size_nth_1;         // 0x488
    unsigned char pkg_digest[0x20];        // 0xFE0
} pkg_header;

enum pkg_content_type {
	PKG_CONTENT_TYPE_GD = 0x1A, /* pkg_ps4_app, pkg_ps4_patch, pkg_ps4_remaster */
	PKG_CONTENT_TYPE_AC = 0x1B, /* pkg_ps4_ac_data, pkg_ps4_sf_theme, pkg_ps4_theme */
	PKG_CONTENT_TYPE_AL = 0x1C, /* pkg_ps4_ac_nodata */
	PKG_CONTENT_TYPE_DP = 0x1E, /* pkg_ps4_delta_patch */
};

namespace INSTALLER
{
    int Init(void);
    void Exit(void);

    int InstallRemotePkg(const char *filename, pkg_header *header);
    int InstallLocalPkg(const char *filename, pkg_header *header, bool remove_after_install=false);
}