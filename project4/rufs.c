/*
 *  Copyright (C) 2023 CS416 Rutgers CS
 *	Tiny File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>
#include <math.h> 

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here
struct superblock * super_block; 
int num_blocks_super;
int num_blocks_inode_bitmap; 
int num_blocks_data_bitmap; 
int initialized = 0; 
/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {
	int avail_ino; 
	// Step 1: Read inode bitmap from disk
	int inode_bitmap_block = super_block->i_bitmap_blk; 
	void * inode_bitmap_data = malloc(num_blocks_inode_bitmap * BLOCK_SIZE);
	bio_read(inode_bitmap_block, inode_bitmap_data);
	bitmap_t inode_bitmap = malloc(MAX_INUM * sizeof(char)/8);
	memcpy(inode_bitmap, inode_bitmap_data, MAX_INUM * sizeof(char)/8);
	// Step 2: Traverse inode bitmap to find an available slot
	for(int i = 1; i < MAX_INUM;i++){
		if(get_bitmap(inode_bitmap, i) == 0){
			set_bitmap(inode_bitmap, i); 
			avail_ino = i; 
			break; 
		}
	}

	// Step 3: Update inode bitmap and write to disk 
	memcpy(inode_bitmap_data, inode_bitmap, MAX_INUM * sizeof(char)/8);
	bio_write(inode_bitmap_block, inode_bitmap_data);
	free(inode_bitmap_data);
	free(inode_bitmap);
	return avail_ino;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {
	int avail_blkno; 
	// Step 1: Read data block bitmap from disk
	int data_bitmap_block = super_block->d_bitmap_blk; 
	void * data_bitmap_data = malloc(num_blocks_data_bitmap * BLOCK_SIZE); 
	bio_read(data_bitmap_block, data_bitmap_data);
	bitmap_t data_bitmap = malloc(MAX_DNUM * sizeof(char)/8); 
	memcpy(data_bitmap, data_bitmap_data, MAX_DNUM * sizeof(char)/8);
	// Step 2: Traverse data block bitmap to find an available slot
	for(int i = 0; i < MAX_DNUM;i++){
		if(get_bitmap(data_bitmap, i) == 0){
			set_bitmap(data_bitmap, i); 
			avail_blkno = i; 
			break; 
		}
	}
	// Step 3: Update data block bitmap and write to disk 
	memcpy(data_bitmap_data, data_bitmap, MAX_DNUM * sizeof(char)/8);
	bio_write(data_bitmap_block, data_bitmap_data);
	free(data_bitmap_data);
	free(data_bitmap);
	return avail_blkno;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

	// Step 1: Get the inode's on-disk block number
	int inode_block_number = super_block->i_start_blk + (ino * sizeof(struct inode) / BLOCK_SIZE);

	// Step 2: Get offset of the inode in the inode on-disk block
	int offset = (ino * sizeof(struct inode)) % BLOCK_SIZE; 

	// Step 3: Read the block from disk and then copy into inode structure
	struct inode * inode_block = malloc(BLOCK_SIZE); 
	bio_read(inode_block_number, inode_block); 
	memcpy(inode, &inode_block[offset], sizeof(struct inode)); 
	free(inode_block); 
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	int inode_block_number = super_block->i_start_blk + (ino * sizeof(struct inode) / BLOCK_SIZE);

	// Step 2: Get the offset in the block where this inode resides on disk
	int offset = (ino * sizeof(struct inode)) % BLOCK_SIZE; 

	// Step 3: Write inode to disk 
	struct inode * inode_block = malloc(BLOCK_SIZE); 
	bio_read(inode_block_number, inode_block); 
	memcpy(&inode_block[offset], inode, sizeof(struct inode)); 
	bio_write(inode_block_number, inode_block); 
	free(inode_block); 
	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

	int found = 0; 
  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode * dir_inode = malloc(sizeof(struct inode)); 
	readi(ino, dir_inode);
  // Step 2: Get data block of current directory from inode
  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure

	int direct_ptr[16];
	memcpy(direct_ptr, dir_inode->direct_ptr, sizeof(dir_inode->direct_ptr));
	struct dirent * dirent_list = malloc(BLOCK_SIZE); 
	int dirent_list_len = BLOCK_SIZE/(sizeof(struct dirent)); 

	for(int i = 0; i < 16; i++){
		int data_block = direct_ptr[i]; 
		if(data_block != 0){
			bio_read(data_block, dirent_list); 
			for(int j = 0; j < dirent_list_len; j++){
				struct dirent entry = dirent_list[j]; 
				if(entry.valid == 1 && entry.len == name_len && strcmp(entry.name,fname) == 0){
					found = 1; 
					memcpy(dirent, &dirent_list[j], sizeof(struct dirent)) ;
					break; 
				}
			}
		}
		if(found){
			break;
		}
	}
	if (found){
		free(dir_inode);
		free(dirent_list);
		return 0; 
	}
	else{
		free(dir_inode);
		free(dirent_list);
		return ENOENT;
	}
	
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	int found = 0; 
	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	int direct_ptr[16];
	memcpy(direct_ptr, dir_inode.direct_ptr, sizeof(dir_inode.direct_ptr));	// Step 2: Check if fname (directory name) is already used in other entries
	struct dirent * dirent_list = malloc(BLOCK_SIZE);
	int dirent_list_len = BLOCK_SIZE/sizeof(struct dirent); 
	for (int i = 0; i < 16; i++){
		int data_block = direct_ptr[i]; 
		if(data_block != 0){
			bio_read(data_block, dirent_list); 
			for (int j = 0; j < dirent_list_len; j++){
				struct dirent entry = dirent_list[j]; 
				if(entry.valid == 1 && entry.len == name_len && strcmp(entry.name, fname) == 0){
					found = 1; 
					break; 
				}
			}
		}
		if(found){
			break;
		}
	}
	// Step 3: Add directory entry in dir_inode's data block and write to disk
	if(found){
		return EEXIST; 
	}
	int allocated = 0; 
	int data_block; 
	for (int i = 0; i < 16; i++){
		data_block = direct_ptr[i]; 
		if(data_block != 0){
			bio_read(data_block, dirent_list); 
			for (int j = 0; j < dirent_list_len; j++){
				struct dirent entry = dirent_list[j]; 
				if(entry.valid == 0){
					dirent_list[j].valid = 1; 
					dirent_list[j].ino = f_ino;
					strcpy(dirent_list[j].name, fname);
					dirent_list[j].len = name_len; 
					allocated = 1; 
					break;
				}
			}
		}
		if(allocated){
			break; 
		}
	}
	if(allocated){
		dir_inode.size += sizeof(struct dirent); 
		dir_inode.vstat.st_size += sizeof(struct dirent); 
		writei(dir_inode.ino, &dir_inode); 
		bio_write(data_block, dirent_list); 
		free(dirent_list);
		return 0; 
	}
	// Allocate a new data block for this directory if it does not exist
	else{
		//Find a datablock that has a value of 0 (not initialiezed)
		int new_data_block_ptr; 
		for(int i = 0; i < 16; i++){
			if(direct_ptr[i] == 0){
				new_data_block_ptr = i; 
				break; 
			}
		}
		int new_data_block = get_avail_blkno(); 
		dir_inode.direct_ptr[new_data_block_ptr] = new_data_block; 
		// Update directory inode
		dir_inode.size += sizeof(struct dirent); 
		dir_inode.vstat.st_size += sizeof(struct dirent); 
		writei(dir_inode.ino, &dir_inode); 

		//Initialize new_data_block 
		struct dirent * data_block_dirents = malloc(BLOCK_SIZE); 
		for(int i = 0; i < dirent_list_len; i++){
			data_block_dirents[i].valid = 0; 
		}
		//Initialize directory entry 
		data_block_dirents[0].ino = f_ino; 
		data_block_dirents[0].valid = 1; 
		strcpy(data_block_dirents[0].name, fname); 
		data_block_dirents[0].len = name_len; 
		bio_write(new_data_block, data_block_dirents); 
		free(data_block_dirents);
		return 0; 
	}
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	char * path_name = malloc(strlen(path) + 1); 
	strcpy(path_name, path); 
	struct dirent * next_dirent = malloc(sizeof(struct dirent)); 
	char* token = strtok(path_name, "/");
	int curr_ino = ino; 
	while(token != NULL && strlen(token) > 0){
		if(dir_find(curr_ino, token, strlen(token), next_dirent) == 0){
			curr_ino = next_dirent->ino; 
		} 
		else{
			free(path_name);
			return ENOENT;
		}
		// first set inode of the current directory to the indoe of the other and find that inode  
		// find directory entry that corresponds to the next string token in the current inode 
		
	}
	readi(curr_ino, inode); 
	// Note: You could either implement it in a iterative way or recursive way
	free(path_name);
	return 0;
}

/* 
 * Make file system
 */
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

	// write superblock information
	num_blocks_super = (int) ceil(sizeof(struct superblock)/BLOCK_SIZE);
	struct superblock * super_block_data = malloc(num_blocks_super * BLOCK_SIZE); 
	super_block_data->magic_num = MAGIC_NUM; 
	super_block_data->max_inum = MAX_INUM; 
	super_block_data->max_dnum = MAX_DNUM;

	// initialize inode bitmap 
	super_block_data->i_bitmap_blk = num_blocks_super; 
	num_blocks_inode_bitmap = (int)ceil(MAX_INUM/8/BLOCK_SIZE); 
	void *inode_bitmap_data = malloc(num_blocks_inode_bitmap * BLOCK_SIZE);
	memset(inode_bitmap_data, 0, MAX_INUM * sizeof(char)/8);

	// initialize data block bitmap
	super_block_data->d_bitmap_blk = num_blocks_super + num_blocks_inode_bitmap;
	num_blocks_data_bitmap = (int) ceil(MAX_DNUM/8/BLOCK_SIZE); 
	void *data_bitmap_data = malloc(num_blocks_data_bitmap * BLOCK_SIZE); 
	memset(data_bitmap_data, 0, MAX_INUM * sizeof(char)/8);

	//Find start block of inode region 
	super_block_data->i_start_blk = num_blocks_super + num_blocks_inode_bitmap + num_blocks_data_bitmap;
	
	//Calculate number of blocks for all inodes 
	int num_blocks_inodes = (int) ceil(MAX_INUM * sizeof(struct inode)/BLOCK_SIZE); 
	
	//Find start block of data block region 
	super_block_data->d_start_blk = num_blocks_super + num_blocks_inode_bitmap + num_blocks_data_bitmap + num_blocks_inodes;

	//Write to disk for superblock information 
	bio_write(0, super_block_data); 

	// update bitmap information for root directory
	// leave inode bit map 0 unset for unallocated inodes 
	set_bitmap((bitmap_t)inode_bitmap_data, 1);
	bio_write(super_block_data->i_bitmap_blk, inode_bitmap_data); 
	bio_write(super_block_data->d_bitmap_blk, data_bitmap_data);
	// update inode for root directory
	struct inode * inode_region = malloc(BLOCK_SIZE);
	struct inode root_inode; 
	root_inode.ino = 1; 
	root_inode.valid = 1; 
	root_inode.size = 0; 
	root_inode.type = S_IFDIR; 
	root_inode.link = 2; 
	root_inode.vstat.st_uid = getuid(); 
	root_inode.vstat.st_gid = getegid(); 
	root_inode.vstat.st_nlink = 2; 
	root_inode.vstat.st_mtime = time(NULL); 
	root_inode.vstat.st_size = 0; 
	root_inode.vstat.st_mode = S_IFDIR | 0755; 
	// initialize direct ptrs to invalid block 
	for (int i = 0; i < 16; i++){
		root_inode.direct_ptr[i] = 0; 
	}
	inode_region[1] = root_inode; 
	bio_write(super_block_data->i_start_blk, inode_region); 
	
	free(super_block_data); 
	free(inode_bitmap_data);
	free(data_bitmap_data);
	free(inode_region);

	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {
	// Step 1a: If disk file is not found, call mkfs
	if(dev_open(diskfile_path) != 0){
		rufs_mkfs(); 
	} 
  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	void* super_block_data = malloc(num_blocks_super * BLOCK_SIZE);
	bio_read(0, super_block_data);
	super_block = malloc(sizeof(struct superblock));
	memcpy(super_block, super_block_data, sizeof(struct superblock));
	free(super_block_data); 

	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	free(super_block); 
	// Step 2: Close diskfile
	dev_close(); 

}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	struct inode * inode = malloc(sizeof(struct inode)); 
	if (get_node_by_path(path, 1, inode) != 0){
		return ENOENT; 
	}
	// Step 2: fill attribute of file into stbuf from inode
	stbuf->st_uid = inode->vstat.st_uid; 
	stbuf->st_gid = inode->vstat.st_gid; 
	stbuf->st_nlink = inode->link; 
	stbuf->st_size = inode->vstat.st_size; 
	stbuf->st_mode = inode->vstat.st_mode; 
	time(&stbuf->st_mtime);
	free(inode); 
	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode * inode = malloc(sizeof(struct inode)); 
	if (get_node_by_path(path, 1, inode) != 0){
		return ENOENT; 
	}
	// Step 2: If not find, return -1

    return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode * inode = malloc(sizeof(struct inode)); 
	if (get_node_by_path(path, 1, inode) != 0){
		return ENOENT; 
	}
	// Step 2: Read directory entries from its data blocks, and copy them to filler
	struct dirent * dirent_list = malloc(BLOCK_SIZE); 
	int dirent_list_len = BLOCK_SIZE/(sizeof(struct dirent)); 
	for(int i = 0; i < 16; i++){
		int data_block = inode->direct_ptr[i]; 
		if(data_block != 0){
			bio_read(data_block, dirent_list); 
			for(int j = 0; j < dirent_list_len; j++){
				struct dirent entry = dirent_list[j]; 
				if(entry.valid == 1){
					filler(buffer, entry.name, NULL, 0); 
				}
			}
		}
	}
	free(inode); 
	free(dirent_list); 
	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char * path_name = malloc(strlen(path) + 1); 
	strcpy(path_name, path); 
	char * dirName = dirname(path_name); 
	char * baseName = basename(path_name); 
	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode * dir_inode = malloc(sizeof(struct inode)); 
	if (get_node_by_path(dirName, 1, dir_inode) != 0){
		return ENOENT; 
	}
	struct dirent * dirent = malloc(sizeof(struct dirent)); 
	if(dir_find(dir_inode->ino, baseName, strlen(baseName), dirent) == 0){
		free(dirent);
		return -ENOENT; 
	}
	free(dirent); 
	// Step 3: Call get_avail_ino() to get an available inode number
	int new_inode_num = get_avail_ino(); 
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	if(dir_add(*dir_inode, new_inode_num, baseName, strlen(baseName)) != 0){
		return ENOENT; 
	}
	// Step 5: Update inode for target directory
	struct inode* new_inode = malloc(sizeof(struct inode)); 
	new_inode->ino = new_inode_num; 
	new_inode->valid = 1; 
	new_inode->size = 0; 
	new_inode->type = S_IFDIR; 
	new_inode->link = 2; 
	new_inode->vstat.st_uid = getuid(); 
	new_inode->vstat.st_gid = getegid(); 
	new_inode->vstat.st_nlink = 2; 
	new_inode->vstat.st_mtime = time(NULL); 
	new_inode->vstat.st_size = 0; 
	new_inode->vstat.st_mode = S_IFDIR | mode; 


	// Step 6: Call writei() to write inode to disk
	writei(new_inode_num, new_inode); 
	// Insert directory entries for current and parent. 
	char curr_dir[] = "."; 
	char parent_dir[] = ".."; 
	readi(new_inode_num, new_inode); 
	dir_add(*new_inode, new_inode_num, curr_dir, strlen(curr_dir)); 
	readi(new_inode_num, new_inode); 
	dir_add(*new_inode, new_inode_num, parent_dir, strlen(parent_dir)); 
	free(new_inode); 
	return 0;
}

static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char * path_name = malloc(strlen(path) + 1); 
	strcpy(path_name, path); 
	char * dirName = dirname(path_name); 
	char * baseName = basename(path_name); 

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode * dir_inode = malloc(sizeof(struct inode)); 
	if (get_node_by_path(dirName, 1, dir_inode) != 0){
		return ENOENT; 
	}
	struct dirent * dirent = malloc(sizeof(struct dirent)); 
	if(dir_find(dir_inode->ino, baseName, strlen(baseName), dirent) == 0){
		free(dirent);
		return -ENOENT; 
	}
	free(dirent); 
	// Step 3: Call get_avail_ino() to get an available inode number
	int new_inode_num = get_avail_ino(); 

	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	if(dir_add(*dir_inode, new_inode_num, baseName, strlen(baseName)) != 0){
		return ENOENT; 
	}
	// Step 5: Update inode for target file
	struct inode* new_inode = malloc(sizeof(struct inode)); 
	new_inode->ino = new_inode_num; 
	new_inode->valid = 1; 
	new_inode->size = 0; 
	new_inode->type = S_IFREG; 
	new_inode->link = 1; 
	new_inode->vstat.st_uid = getuid(); 
	new_inode->vstat.st_gid = getegid(); 
	new_inode->vstat.st_nlink = 1; 
	new_inode->vstat.st_mtime = time(NULL); 
	new_inode->vstat.st_size = 0; 
	new_inode->vstat.st_mode = S_IFREG | mode; 


	// Step 6: Call writei() to write inode to disk
	writei(new_inode_num, new_inode); 


	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);
	return fuse_stat;
}

