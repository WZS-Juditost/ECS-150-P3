#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF

struct __attribute__ ((__packed__)) SuperBlock {
	uint64_t signature;
	uint16_t num_of_blocks;
	uint16_t root_dir_idx;
	uint16_t data_blk_start_idx;
	uint16_t num_of_data_blks;
	uint8_t num_of_FAT_blks;
	uint8_t padding[4079];
};

struct __attribute__ ((__packed__)) RootDir {
	char filename[FS_FILENAME_LEN];
	uint32_t size;
	uint16_t first_data_indx;
	uint16_t padding[5];
};

struct FAT {
	uint16_t words;
};

// still need a struct for file descriptor
struct __attribute__((__packed__)) FileDescriptorTable {
	int status;
	int offset;
	int entry_in_fdt;
	char filename[FS_FILENAME_LEN];
};

struct SuperBlock *Superblock;
struct FAT *FATblock;
struct RootDir *RootDirblock;
struct FileDescriptorTable FDTable[FS_OPEN_MAX_COUNT];



// helper function
/* checks whether there is any file in the FST */
/* return 1 if not empty, 0 if empty */
int file_exist(const char* file) {
	int not_empty = 0;
	for(int i = 0; i < FS_FILENAME_LEN; i++) {
		if(file[i] != 0) {
			not_empty++;
		}
		if(not_empty > 0) {
			return 1;
		}
	}
	return 0;
}

/* returns 1 if file open, 0 if not open */
int file_is_open(const char* file) {
	uint size = strlen(file);
	uint same_part = 0;
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		same_part = 0;
		if(FDTable[i].filename != NULL && strlen(FDTable[i].filename) == size) {
			for(uint j = 0; j < size; j++){
				if(FDTable[i].filename[j] == file[j]) {
					same_part++;
				}
				if(same_part == size) {
					if(FDTable[i].status == 1) {
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

int fs_mount(const char *diskname)
{
	if(block_disk_open(diskname) < 0) {
		return -1;
	}

	/* Opening new disk*/
	Superblock = malloc(BLOCK_SIZE);

	/* Read into Superblock */
	if(block_read(0, (void*)Superblock) < 0) {
		return -1;
	}

	/* Allocate space for FAT */
	FATblock = malloc(Superblock->num_of_FAT_blks * BLOCK_SIZE);
	for(int i = 0; i < Superblock->num_of_FAT_blks; i++) {
		if (block_read(i + 1, (void*)FATblock + (i * BLOCK_SIZE)) < 0) {
			return -1;
		}
	}

	/* Read into RootDirblock */
	RootDirblock = malloc(sizeof(struct RootDir) * FS_FILE_MAX_COUNT);
	if(block_read(Superblock->num_of_FAT_blks + 1, (void*)RootDirblock) < 0) {
		return -1;
	}

	/* Allocate memory for FD table */
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		FDTable[i].status = 0;
		FDTable[i].offset = -1;
		FDTable[i].entry_in_fdt = FAT_EOC;
		memset(FDTable[i].filename, 0, FS_FILENAME_LEN);
	}

	return 0;
}

int fs_umount(void)
{
	if(!Superblock) {
		return -1;
	}

	for(int i = 0; i < Superblock->num_of_FAT_blks; i++) {
		if (block_write(i + 1, (void*)FATblock + (i * BLOCK_SIZE)) < 0) {
			return -1;
		}
	}

	if(block_write(Superblock->num_of_FAT_blks + 1, (void*)RootDirblock) < 0) {
		return -1;
	}

	free(Superblock);
	free(FATblock);
	free(RootDirblock);

	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if(FDTable[i].status != 0) {
			return -1;
		}
	}

	if(block_disk_close() < 0) {
		return -1;
	}

	return 0;
}

int fs_info(void)
{
	if(Superblock == NULL) {
		return -1;
	}

	int free_FAT = 0;
	for(int i = 1; i < Superblock->num_of_data_blks; i++) {
		if(FATblock[i].words == 0) {
			free_FAT++;
		}
	}

	int free_rdir = 0;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(!file_exist(RootDirblock[i].filename)) {
			free_rdir++;
		}
	}

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", Superblock->num_of_blocks);
	printf("fat_blk_count=%d\n", Superblock->num_of_FAT_blks);
	printf("rdir_blk=%d\n", Superblock->num_of_FAT_blks + 1);
	printf("data_blk=%d\n", Superblock->num_of_FAT_blks + 2);
	printf("data_blk_count=%d\n", Superblock->num_of_data_blks);
	printf("fat_free_ratio=%d/%d\n", free_FAT, Superblock->num_of_data_blks);
	printf("rdir_free_ratio=%d/%d\n", free_rdir, FS_FILE_MAX_COUNT);

	return 0;
}

int fs_create(const char *filename)
{
	if(Superblock == NULL) {
		return -1;
	}

	if(filename == NULL) {
		return -1;
	}

	uint size = strlen(filename);
	if(size > FS_FILENAME_LEN) {
		return -1;
	}

	/* if FST full, return -1 */
	int occupy_dir = 0;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(file_exist(RootDirblock[i].filename)) {
			occupy_dir++;
		}
		if(occupy_dir == FS_FILE_MAX_COUNT) {
			return -1;
		}
	}

	/* matching filename, return -1 */
	uint same_part = 0;
	for(uint i = 0; i < FS_FILE_MAX_COUNT; i++){
		same_part = 0;
		if(RootDirblock[i].filename != NULL && strlen(RootDirblock[i].filename) == size) {
			for(uint j = 0; j < size; j++){
				if(RootDirblock[i].filename[j] == filename[j]) {
					same_part++;
				}
				if(same_part == size) {
					return -1;
				}
			}
		}
	}

	/* Writing FAT and RootDir to create file */
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(!file_exist(RootDirblock[i].filename)) {
			strcpy(RootDirblock[i].filename, filename);
			RootDirblock[i].size = 0;
			RootDirblock[i].first_data_indx = FAT_EOC;
			return 0;
		}
	}

	return -1;

}

int fs_delete(const char *filename)
{
	if(Superblock == NULL) {
		return -1;
	}

	if(filename == NULL) {
		return -1;
	}

	uint size = strlen(filename);
	if(size > FS_FILENAME_LEN) {
		return -1;
	}

	/* return -1, if filename not in FST */
	uint same_part = 0;
	uint target = 0;
	for(uint i = 0; i < FS_FILE_MAX_COUNT; i++){
		same_part = 0;
		if(RootDirblock[i].filename != NULL && strlen(RootDirblock[i].filename) == size) {
			for(uint j = 0; j < size; j++){
				if(RootDirblock[i].filename[j] == filename[j]) {
					same_part++;
				}
				if(same_part == size) {
					target++;
				}
			}
			if(target > 0) {
				break;
			}else if(i == FS_FILE_MAX_COUNT - 1 && target == 0) {
				return -1;
			}
		}
	}

	/* if file open, can't delete it */
	if(file_is_open(filename)) {
		return -1;
	}

	/* find file in RootDir */
	int entry_in_rdir = -1;
	same_part = 0;
	for(uint i = 0; i < FS_FILE_MAX_COUNT; i++) {
		same_part = 0;
		if(RootDirblock[i].filename != NULL && strlen(RootDirblock[i].filename) == size) {
			for(uint j = 0; j < size; j++){
				if(RootDirblock[i].filename[j] == filename[j]) {
					same_part++;
				}
				if(same_part == size) {
					entry_in_rdir = i;
				}
			}
		}
	}

	struct RootDir *curr_rdir = &RootDirblock[entry_in_rdir];
	u_int16_t starting_index = curr_rdir->first_data_indx;

 	curr_rdir->size = 0;
	memset(curr_rdir->filename, 0, FS_FILENAME_LEN);

	/* deleting entries of current file in FAT and RootDir */
	while(starting_index != FAT_EOC) {
		uint16_t temp = FATblock[starting_index].words;
		FATblock[starting_index].words = 0;
		starting_index = temp;
	}
	curr_rdir->first_data_indx = FAT_EOC;
	return 0;
}

int fs_ls(void)
{
	if(Superblock == NULL) {
		return -1;
	}

	printf("FS Ls:\n");
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if(file_exist(RootDirblock[i].filename)) {
			printf("file: %s, size: %d, data_blk: %d\n", RootDirblock[i].filename, RootDirblock[i].size, RootDirblock[i].first_data_indx);
		}
	}

	return 0;
}

int fs_open(const char *filename)
{
	if(Superblock == NULL) {
		return -1;
	}

	if(filename == NULL) {
		return -1;
	}

	uint size = strlen(filename);
	if(size > FS_FILENAME_LEN) {
		return -1;
	}

	/* if FST full, return -1 */
	uint occupy_fdt = 0;
	for(int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if(file_exist(FDTable[i].filename)) {
			occupy_fdt++;
		}
		if(occupy_fdt == FS_OPEN_MAX_COUNT) {
			return -1;
		}
	}

	/* no matching filename, return -1 */
	uint same_part = 0;
	uint target = 0;
	for(uint i = 0; i < FS_FILE_MAX_COUNT; i++){
		same_part = 0;
		if(RootDirblock[i].filename != NULL && strlen(RootDirblock[i].filename) == size) {
			for(uint j = 0; j < size; j++){
				if(RootDirblock[i].filename[j] == filename[j]) {
					same_part++;
				}
				if(same_part == size) {
					target++;
				}
			}
			if(target > 0) {
				break;
			}else if(i == FS_FILE_MAX_COUNT - 1 && target == 0) {
				return -1;
			}
		}
	}

	/* write into empty slot in the FDT */
	for(uint i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (FDTable[i].status == 0 && FDTable[i].offset == -1 && FDTable[i].entry_in_fdt == FAT_EOC) {
			FDTable[i].status = 1;
			FDTable[i].offset = 0;
            FDTable[i].entry_in_fdt = i;
			strcpy(FDTable[i].filename, filename);
            return FDTable[i].entry_in_fdt;
        }
    }

	return -1;
}

int fs_close(int fd)
{
	if(Superblock == NULL) {
		return -1;
	}

	/* fd out of range */
	if(fd < 0 || fd > FS_OPEN_MAX_COUNT) {
		return -1;
	}

	if(!file_is_open(FDTable[fd].filename)) {
		return -1;
	}

	/* delete entry from FDTable */
	if (FDTable[fd].status != 0) {
		FDTable[fd].status = 0;
		FDTable[fd].offset = -1;
		FDTable[fd].entry_in_fdt = FAT_EOC;
		memset(FDTable[fd].filename, 0, FS_FILENAME_LEN);
	}

	return 0;
}

int fs_stat(int fd)
{
	if(Superblock == NULL) {
		return -1;
	}

	/* fd out of range */
	if(fd < 0 || fd > FS_OPEN_MAX_COUNT) {
		return -1;
	}

	/* file not open */
	if(!file_is_open(FDTable[fd].filename)) {
		return -1;
	}


	uint size = strlen(FDTable[fd].filename);
	if(size > FS_FILENAME_LEN) {
		return -1;
	}

	/* get the length from the RDblock */
	uint same_part = 0;
	for(uint i = 0; i < FS_FILE_MAX_COUNT; i++){
		same_part = 0;
		if(RootDirblock[i].filename != NULL && strlen(RootDirblock[i].filename) == size) {
			for(uint j = 0; j < size; j++){
				if(RootDirblock[i].filename[j] == FDTable[fd].filename[j]) {
					same_part++;
				}
				if(same_part == size) {
					return RootDirblock[i].size;
				}
			}
		}
	}

	return -1;
}

int fs_lseek(int fd, size_t offset)
{
	/* not mounted */
	if(Superblock == NULL) {
		return -1;
	}

	/* fd out of range */ 
	if(fd < 0 || fd > FS_OPEN_MAX_COUNT) {
		return -1;
	}

	/* file not open */
	if(!file_is_open(FDTable[fd].filename)) {
		return -1;
	}

	/* size of filename */
	uint size = strlen(FDTable[fd].filename);
	if(size > FS_FILENAME_LEN) {
		return -1;
	}

	/* set the offset in the RDblock */
	uint same_part = 0;
	for(uint i = 0; i < FS_FILE_MAX_COUNT; i++){
		same_part = 0;
		if(RootDirblock[i].filename != NULL && strlen(RootDirblock[i].filename) == size) {
			for(uint j = 0; j < size; j++){
				if(RootDirblock[i].filename[j] == FDTable[fd].filename[j]) {
					same_part++;
				}
				if(same_part == size) {
					/* offset greater that size of file */
					if(offset > RootDirblock[i].size) {
						return -1;
					}
					else {
						FDTable[fd].offset = offset;
					} 
				}
			}
		}
	}

	return 0;
}

int fs_rw_check(int fd, void *buf) 
{
	/* not mounted */
	if(Superblock == NULL) {
		return -1;
	}

	/* fd out of range */ 
	if(fd < 0 || fd > FS_OPEN_MAX_COUNT) {
		return -1;
	}

	/* file not open */
	if(!file_is_open(FDTable[fd].filename)) {
		return -1;
	}

	/* buf NULL */
	if(buf == NULL) {
		return -1;
	}

	/* size of filename */
	int size = strlen(FDTable[fd].filename);
	if(size > FS_FILENAME_LEN) {
		return -1;
	}

	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* input checking */
	if(fs_rw_check(fd, buf) == -1) {
		return -1;
	}

	uint8_t bounce_buffer[BLOCK_SIZE];
	size_t total_bit_written = 0;
	unsigned int offset = FDTable[fd].offset;
	char *file = FDTable[fd].filename;
	uint size = strlen(file);
	
	/* check whether file is in RootDir */
	int entry_in_rdir = -1;
	uint same_part = 0;
	for(uint i = 0; i < FS_FILE_MAX_COUNT; i++) {
		same_part = 0;
		if(RootDirblock[i].filename != NULL && strlen(RootDirblock[i].filename) == size) {
			for(uint j = 0; j < size; j++){

				if(RootDirblock[i].filename[j] == file[j]) {
					same_part++;
				}
				if(same_part == size) {
					entry_in_rdir = i;
				}
			}
		}
	}
	if(entry_in_rdir == -1) {
		return -1;
	}

	/*  */
	uint16_t first_block_index = RootDirblock[entry_in_rdir].first_data_indx;
	if(first_block_index == FAT_EOC) {
		uint16_t first_avaliable_block = FAT_EOC;
		int i = 1;
		while(i < Superblock->num_of_data_blks) {
			if(FATblock[i].words == 0) {
				first_avaliable_block = i;
				break;
			}
			i++;
		}
		if(first_avaliable_block == FAT_EOC) {
			return total_bit_written;
		}
		first_block_index = first_avaliable_block;
		FATblock[first_block_index].words = FAT_EOC;
	}

	uint16_t curr_block = first_block_index;
	while(count > 0)
	{
		while(offset >= BLOCK_SIZE) {
			uint16_t next_block = FATblock[curr_block].words;
			if(next_block == 0 || next_block == FAT_EOC) {
				uint16_t next_avaliable_block = FAT_EOC;
				int i = 1;
				while(i < Superblock->num_of_data_blks) {
					if(FATblock[i].words == 0) {
						next_avaliable_block = i;
						break;
					}
					i++;
				}
				if(next_avaliable_block == FAT_EOC) {
					if(RootDirblock[entry_in_rdir].size < offset) {
						RootDirblock[entry_in_rdir].size = offset;
					}
					if(RootDirblock[entry_in_rdir].size > 0) {
						RootDirblock[entry_in_rdir].first_data_indx = first_block_index;
					}
					FDTable[fd].offset = offset;
					return total_bit_written;
				}
				next_block = next_avaliable_block;
				FATblock[curr_block].words = next_block;
				FATblock[next_block].words = FAT_EOC;
				curr_block = next_block;
				offset -= BLOCK_SIZE;
			}
		}

		size_t bit_to_write = BLOCK_SIZE - offset;
		if (bit_to_write > count) {
			bit_to_write = count;
		}

		if(block_read(curr_block + Superblock->data_blk_start_idx, bounce_buffer) < 0) {
			break;
		}

		memcpy(bounce_buffer + offset, buf + total_bit_written, bit_to_write);

		if(block_write(curr_block + Superblock->data_blk_start_idx, bounce_buffer) < 0) {
			break;
		}

		total_bit_written += bit_to_write;
		offset += bit_to_write;
		count -= bit_to_write;
	}

	if(RootDirblock[entry_in_rdir].size < offset) {
		RootDirblock[entry_in_rdir].size = offset;
	}
	if(RootDirblock[entry_in_rdir].size > 0) {
		RootDirblock[entry_in_rdir].first_data_indx = first_block_index;
	}
	FDTable[fd].offset = offset;
	return total_bit_written;
}

int fs_read(int fd, void *buf, size_t count)
{
	if(fs_rw_check(fd, buf) == -1) {
		return -1;
	}

	uint8_t bounce_buffer[BLOCK_SIZE];
	size_t total_bit_read = 0;
	unsigned int offset = FDTable[fd].offset;
	char *file = FDTable[fd].filename;
	uint size = strlen(file);
	
	int entry_in_rdir = -1;
	uint same_part = 0;
	for(uint i = 0; i < FS_FILE_MAX_COUNT; i++) {
		same_part = 0;
		if(RootDirblock[i].filename != NULL && strlen(RootDirblock[i].filename) == size) {
			for(uint j = 0; j < size; j++){
				if(RootDirblock[i].filename[j] == file[j]) {
					same_part++;
				}
				if(same_part == size) {
					entry_in_rdir = i;
				}
			}
		}
	}
	if(entry_in_rdir == -1) {
		return -1;
	}

	uint16_t first_block_index = RootDirblock[entry_in_rdir].first_data_indx;
	if(first_block_index == FAT_EOC) {
		return total_bit_read;
	}

	uint16_t curr_block = first_block_index;

	while(count > 0)
	{
		while(offset >= BLOCK_SIZE) {
			uint16_t next_block = FATblock[curr_block].words;
			if(next_block == FAT_EOC) {
				FDTable[fd].offset = offset;
				return total_bit_read;
			}
			curr_block = next_block;
			offset -= BLOCK_SIZE;
		}

		size_t bit_to_read = BLOCK_SIZE - offset;
		if (bit_to_read > count) {
			bit_to_read = count;
		}

		if(block_read(curr_block + Superblock->data_blk_start_idx, bounce_buffer) < 0) {
			break;
		}

		memcpy(buf + total_bit_read, bounce_buffer + offset, bit_to_read);

		total_bit_read += bit_to_read;
		offset += bit_to_read;
		count -= bit_to_read;
	}

	FDTable[fd].offset = offset;
	return total_bit_read;
}

