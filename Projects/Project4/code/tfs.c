/*
 *  Copyright (C) 2019 CS416 Spring 2019
 *	
 *	Tiny File System
 *
 *	File:	tfs.c
 *  Author: Yujie REN
 *	Date:	April 2019
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

#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];

bitmap_t ibmap = NULL;
bitmap_t dbmap = NULL;

struct superblock *superblock;
struct inode *inodes[MAX_INUM+2];
struct dirent *dirent;


// Declare your in-memory data structures here

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {
	
	// Step 1: Read inode bitmap from disk
	//superblock->i_bitmap_blk is the start address of inode bitmap
	bio_read(superblock->i_bitmap_blk, ibmap);

	// Step 2: Traverse inode bitmap to find an available slot
	for(int i = 0; i < MAX_INUM; i++){
		if(get_bitmap(ibmap, i) == 0){
			// Step 3: Update inode bitmap and write to disk
			set_bitmap(ibmap, i);
			bio_write(superblock->i_bitmap_blk, ibmap);
			return i;
		}
	}

	return 0;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	//superblock->d_bitmap_blk is the start address of data block bitmap
	bio_read(superblock->d_bitmap_blk, dbmap);

	// Step 2: Traverse data block bitmap to find an available slot
	for(int i = 0; i < MAX_DNUM; i++){
		if(get_bitmap(dbmap, i) == 0){
			// Step 3: Update data block bitmap and write to disk
			set_bitmap(dbmap, i);
			bio_write(superblock->d_bitmap_blk, dbmap);
			return i;
		}
	}

	return 0;
}

/* 
 * inode operationsx
 */
int readi(uint16_t ino, struct inode *inode) {
	//uint16_t is an unsigned 16 bit integer

  	// Step 1: Get the inode's on-disk block number
	// i_start_blk = start address of inode region
	int offset = BLOCK_SIZE/sizeof(struct inode);
	int iBlockNum = superblock->i_start_blk + ino/offset;
	struct inode *iBlock = malloc(BLOCK_SIZE);
	int status = bio_read(iBlockNum, iBlock);

  	// Step 2: Get offset of the inode in the inode on-disk block
	int iOffsetInBlk = ino % offset;

  	// Step 3: Read the block from disk and then copy into inode structure
	inodes[ino - 1]->ino = iBlock[iOffsetInBlk].ino;
	inodes[ino - 1]->valid = iBlock[iOffsetInBlk].valid;
	inodes[ino - 1]->size = iBlock[iOffsetInBlk].size;
	inodes[ino - 1]->type = iBlock[iOffsetInBlk].type;
	inodes[ino - 1]->link = iBlock[iOffsetInBlk].link;
	
	for(int i = 0; i < DIRECT_PTRS;i++){

		inodes[ino - 1]->direct_ptr[i] = iBlock[iOffsetInBlk].direct_ptr[i];
		if(i < INDIRECT_PTRS){

			inodes[ino - 1]->indirect_ptr[i] = iBlock[iOffsetInBlk].indirect_ptr[i];
			
		}

	}
	inodes[ino - 1]->vstat.st_dev = iBlock[iOffsetInBlk].vstat.st_dev;
	inodes[ino - 1]->vstat.st_ino = iBlock[iOffsetInBlk].vstat.st_ino;
	inodes[ino - 1]->vstat.st_mode = iBlock[iOffsetInBlk].vstat.st_mode;
	inodes[ino - 1]->vstat.st_nlink = iBlock[iOffsetInBlk].vstat.st_nlink;
	inodes[ino - 1]->vstat.st_uid = iBlock[iOffsetInBlk].vstat.st_uid;
	inodes[ino - 1]->vstat.st_gid = iBlock[iOffsetInBlk].vstat.st_gid;
	inodes[ino - 1]->vstat.st_atime = iBlock[iOffsetInBlk].vstat.st_atime;
	inodes[ino - 1]->vstat.st_mtime = iBlock[iOffsetInBlk].vstat.st_mtime;

	free(iBlock);
	iBlock = NULL;
	
	return status;
}


int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	int offset = BLOCK_SIZE/sizeof(struct inode);
	int iBlockNum = superblock->i_start_blk + ino/offset;
	struct inode *iBlock = malloc(BLOCK_SIZE);
	int status = bio_read(iBlockNum, iBlock);

	// Step 2: Get the offset in the block where this inode resides on disk
	int iOffsetInBlk = ino % offset;

	// Step 3: Write inode to disk 
	inodes[ino - 1]->ino = iBlock[iOffsetInBlk].ino;
	inodes[ino - 1]->valid = iBlock[iOffsetInBlk].valid;
	inodes[ino - 1]->size = iBlock[iOffsetInBlk].size;
	inodes[ino - 1]->type = iBlock[iOffsetInBlk].type;
	inodes[ino - 1]->link = iBlock[iOffsetInBlk].link;
	
	for(int i = 0; i < DIRECT_PTRS;i++){

		inodes[ino - 1]->direct_ptr[i] = iBlock[iOffsetInBlk].direct_ptr[i];
		if(i < INDIRECT_PTRS){

			inodes[ino - 1]->indirect_ptr[i] = iBlock[iOffsetInBlk].indirect_ptr[i];
			
		}

	}
	inodes[ino - 1]->vstat.st_dev = iBlock[iOffsetInBlk].vstat.st_dev;
	inodes[ino - 1]->vstat.st_ino = iBlock[iOffsetInBlk].vstat.st_ino;
	inodes[ino - 1]->vstat.st_mode = iBlock[iOffsetInBlk].vstat.st_mode;
	inodes[ino - 1]->vstat.st_nlink = iBlock[iOffsetInBlk].vstat.st_nlink;
	inodes[ino - 1]->vstat.st_uid = iBlock[iOffsetInBlk].vstat.st_uid;
	inodes[ino - 1]->vstat.st_gid = iBlock[iOffsetInBlk].vstat.st_gid;
	inodes[ino - 1]->vstat.st_atime = iBlock[iOffsetInBlk].vstat.st_atime;
	inodes[ino - 1]->vstat.st_mtime = iBlock[iOffsetInBlk].vstat.st_mtime;

	free(iBlock);
	iBlock = NULL;
	
	return status;

	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)

  // Step 2: Get data block of current directory from inode

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure

	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return 0;
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
	// Note: You could either implement it in a iterative way or recursive way

	return 0;
}

/* 
 * Make file system
 */
int tfs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile

	// write superblock information

	// initialize inode bitmap

	// initialize data block bitmap

	// update bitmap information for root directory

	// update inode for root directory

	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs

  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk

	return NULL;
}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile

}

static int tfs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

    return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	

	return 0;
}

static int tfs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int tfs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int tfs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}

