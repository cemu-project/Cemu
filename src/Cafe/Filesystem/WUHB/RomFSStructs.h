#pragma once

#include <cstdint>
typedef struct filepath {
	char char_path[PATH_MAX];
	char os_path[PATH_MAX];
	bool valid;
} filepath_t;

struct romfs_fent_ctx;

typedef struct romfs_dirent_ctx {
    filepath_t sum_path;
    filepath_t cur_path;
    uint32_t entry_offset;
    struct romfs_dirent_ctx *parent; /* Parent node */
    struct romfs_dirent_ctx *child; /* Child node */
    struct romfs_dirent_ctx *sibling; /* Sibling node */
    struct romfs_fent_ctx *file; /* File node */
    struct romfs_dirent_ctx *next; /* Next node */
} romfs_dirent_ctx_t;

typedef struct romfs_fent_ctx {
    filepath_t sum_path;
    filepath_t cur_path;
    uint32_t entry_offset;
    uint64_t offset;
    uint64_t size;
    romfs_dirent_ctx_t *parent; /* Parent dir */
    struct romfs_fent_ctx *sibling; /* Sibling file */
    struct romfs_fent_ctx *next; /* Logical next file */
} romfs_fent_ctx_t;

typedef struct {
    romfs_fent_ctx_t *files;
    uint64_t num_dirs;
    uint64_t num_files;
    uint64_t dir_table_size;
    uint64_t file_table_size;
    uint64_t dir_hash_table_size;
    uint64_t file_hash_table_size;
    uint64_t file_partition_size;
} romfs_ctx_t;

typedef struct {
    uint32_t header_magic;
    uint32be header_size;
    uint64be dir_hash_table_ofs;
    uint64be dir_hash_table_size;
    uint64be dir_table_ofs;
    uint64be dir_table_size;
    uint64be file_hash_table_ofs;
    uint64be file_hash_table_size;
    uint64be file_table_ofs;
    uint64be file_table_size;
    uint64be file_partition_ofs;
} romfs_header_t;

typedef struct {
    uint32_t parent;
    uint32_t sibling;
    uint32_t child;
    uint32_t file;
    uint32_t hash;
    uint32_t name_size;
    std::string name;
} romfs_direntry_t;

typedef struct {
    uint32_t parent;
    uint32_t sibling;
    uint64_t offset;
    uint64_t size;
    uint32_t hash;
    uint32_t name_size;
    std::string name;
} romfs_fentry_t;

typedef struct romfs_infos {
    uint32_t * dir_hash_table;
    uint32_t * file_hash_table;

    romfs_direntry_t * dir_table;
    romfs_fentry_t * file_table;

    uint32_t file_hash_table_entry_count;
    uint32_t dir_hash_table_entry_count;
} romfs_infos_t;

#define ROMFS_ENTRY_EMPTY 0xFFFFFFFF
#define ROMFS_FILEPARTITION_OFS 0x200
