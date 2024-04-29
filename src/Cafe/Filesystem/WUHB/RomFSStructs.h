#pragma once

#include <cstdint>

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
    uint32be parent;
    uint32be sibling;
    uint32be child;
    uint32be file;
    uint32be hash;
    uint32be name_size;
    std::string name;
} romfs_direntry_t;

typedef struct {
    uint32be parent;
    uint32be sibling;
    uint64be offset;
    uint64be size;
    uint32be hash;
    uint32be name_size;
    std::string name;
} romfs_fentry_t;

#define ROMFS_ENTRY_EMPTY 0xFFFFFFFF
