#pragma once

struct romfs_header_t
{
	uint32 header_magic;
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
};

struct romfs_direntry_t
{
	uint32be parent;
	uint32be listNext;	   // offset to next directory entry in linked list of parent directory (aka "sibling")
	uint32be dirListHead;  // offset to first entry in linked list of directory entries (aka "child")
	uint32be fileListHead; // offset to first entry in linked list of file entries (aka "file")
	uint32be hash;
	uint32be name_size;
	std::string name;
};

struct romfs_fentry_t
{
	uint32be parent;
	uint32be listNext; // offset to next file entry in linked list of parent directory (aka "sibling")
	uint64be offset;
	uint64be size;
	uint32be hash;
	uint32be name_size;
	std::string name;
};

#define ROMFS_ENTRY_EMPTY 0xFFFFFFFF
