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

// Declare your in-memory data structures here

struct superblock* SBLOCK;
bitmap_t INODE_BMAP;
bitmap_t DBLOCK_BMAP;
#define ERROR_VALUE -1
#define DEBUG 0

int SUPERBLOCK_BLOCK_SIZE;
int INODE_BITMAP_BLOCK_SIZE;
int DBLOCK_BITMAP_BLOCK_SIZE;
int INODE_TABLE_BLOCK_SIZE;
int BLOCK_SIZE_IN_CHARS;
int INODES_PER_BLOCK;
int DIRENTS_PER_BLOCK;

/**
 * Helper function to initialize memory for superblock, bitmaps, 
 * and other useful global values.
 */
int init_FS_globals(){

    if(DEBUG){
        printf("IN init_FS_GLOBALS\n");
    }

    SUPERBLOCK_BLOCK_SIZE = ceil((double) 
                            sizeof(struct superblock) / BLOCK_SIZE);
    INODES_PER_BLOCK = floor((double) BLOCK_SIZE / sizeof(struct inode));
    DIRENTS_PER_BLOCK = floor((double) BLOCK_SIZE / sizeof(struct dirent));
    INODE_BITMAP_BLOCK_SIZE = ceil((double) (MAX_INUM / 8) / BLOCK_SIZE);
    DBLOCK_BITMAP_BLOCK_SIZE = ceil((double) (MAX_DNUM / 8) / BLOCK_SIZE);
    INODE_TABLE_BLOCK_SIZE = ceil(MAX_INUM / INODES_PER_BLOCK);
    BLOCK_SIZE_IN_CHARS = ceil(BLOCK_SIZE / 8);

    SBLOCK = calloc(sizeof(struct superblock), sizeof(struct superblock));
    check_SBLOCK();

    INODE_BMAP = (bitmap_t) malloc(MAX_INUM / 8);
    DBLOCK_BMAP = (bitmap_t) malloc(MAX_DNUM / 8);

    if(INODE_BMAP == NULL || DBLOCK_BMAP == NULL){

    }

}
/**
 * This is for error checking, just checks if the superblock
 * is initialized, and exits the program if it isn't. Used to cut down
 * redundancy in code.
 */
int check_SBLOCK(){

    if(SBLOCK == NULL){
        perror("SUPERBLOCK NOT INITIALIZED\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

    if(DEBUG){
        printf("in get_avail_ino\n");
    }

    //check superblock is initialized
    check_SBLOCK();
	
	// Step 1: Read inode bitmap from disk
    void* inode_buffer = (void*) malloc(BLOCK_SIZE);
    bio_read(SBLOCK->i_bitmap_blk, inode_buffer);

    //check bitmap copied into inode_buffer
    if(inode_buffer == NULL){
        perror("INODE BITMAP NOT READ FROM DISK\n");
        exit(EXIT_FAILURE);
    }
    //copy inode bitmap from inode_buffer into global
    memcpy(INODE_BMAP, inode_buffer, MAX_INUM / 8);

	// Step 2: Traverse inode bitmap to find an available slot

    int inode_num = -1;
    //iterate over inodes
    while(inode_num < MAX_INUM){
        inode_num++;
        //bitmap is available/unused if it has 0 value
        if(get_bitmap(INODE_BMAP, inode_num) == 0){
            break;
        } 
    }
    //no available inodes?
    if(inode_num == ERROR_VALUE){
        perror("NO AVAILABLE INODES IN TABLE\n");
        exit(EXIT_FAILURE);
    }

	// Step 3: Update inode bitmap and write to disk

    //update inode bitmap
    set_bitmap(INODE_BMAP, inode_num);

    //write to disk
    bio_write(SBLOCK->i_bitmap_blk, (void*) INODE_BMAP);

    int iterator = 0;
    while(iterator < INODE_BITMAP_BLOCK_SIZE){
        //write to disk
        bio_write(SBLOCK->i_bitmap_blk + iterator,
                  INODE_BMAP[iterator * BLOCK_SIZE_IN_CHARS]);
    }

    //free(inode_buffer);
	return inode_num;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

    if(DEBUG){
        printf("IN GET_AVAIL_BLKNO\n");
    }

	// Step 1: Read data block bitmap from disk
	//SBLOCK->d_bitmap_blk is the start address of data block bitmap

    check_SBLOCK();

    void* dblock_buffer = (void*) malloc(BLOCK_SIZE);
    bio_read(SBLOCK->d_bitmap_blk, dblock_buffer);

    //check bitmap copied into buffer
    if(dblock_buffer == NULL){
        perror("DBLOCK BITMAP NOT READ FROM DISK\n");
        exit(EXIT_FAILURE);
    }
    //copy inode bitmap from buffer into global
    memcpy(DBLOCK_BMAP, dblock_buffer, MAX_DNUM / 8);

	
    // Step 2: Traverse data block bitmap to find an available slot

    int dblock_index = -1;

    while(dblock_index < MAX_DNUM){
        dblock_index++;
        //get available data block from bitmap
        if(get_bitmap(DBLOCK_BMAP, dblock_index) == 0){
            break;
        }
    }

    //check to see we found an empty data block
    if(dblock_index == ERROR_VALUE){
        perror("NO AVAILABLE DATA BLOCKS\n");
        exit(EXIT_FAILURE);
    }

	// Step 3: Update data block bitmap and write to disk

    set_bitmap(DBLOCK_BMAP, dblock_index);
    bio_write(SBLOCK->d_bitmap_blk, (void*) DBLOCK_BMAP);
    free(dblock_buffer);

	return dblock_index;
}

/* 
 * inode operationsx
 */
int readi(uint16_t ino, struct inode *inode) {
	//uint16_t is an unsigned 16 bit integer

    if(DEBUG){
        printf("IN READI\n");
    }

  	// Step 1: Get the inode's on-disk block number

    check_SBLOCK();

    // i_start_blk = start address of inode region
    int block_num = SBLOCK->i_start_blk + (ino / INODES_PER_BLOCK);

  	// Step 2: Get offset of the inode in the inode on-disk block

    int offset = ino % INODES_PER_BLOCK;

  	// Step 3: Read the block from disk and then copy into inode structure

    struct inode* buffer_block = (void*) malloc(BLOCK_SIZE);

    if(buffer_block == NULL){
        perror("READI: DID NOT ALLOCATE MEMORY FOR BUFFER_BLOCK\n");
        exit(EXIT_FAILURE);
    }

    //read from disk
    if(bio_read(block_num, buffer_block) == NULL){
        perror("FAILURE TO READ INODE FROM INODE TABLE\n");
        exit(EXIT_FAILURE);
    }

    struct inode* inode_block = (struct inode*) buffer_block;

    if(DEBUG){
        printf("READING INTO INODE:\n"
                + "ino: %d, "
                + "block number: %d "
                + "offset: %d\n",
                ino,
                block_num,
                offset);
    }

    //change input inode's pointer to block's data
    //same effect as copying all fields into inode
    *inode = inode_block[offset];
    free(buffer_block);

	return 1;
}


int writei(uint16_t ino, struct inode *inode) {

    if(DEBUG){
        printf("in writei\n");
    }
    check_SBLOCK();
	// Step 1: Get the block number where this inode resides on disk

    int offset = ino % INODES_PER_BLOCK;
    int block_num = SBLOCK->i_start_blk + (ino / INODES_PER_BLOCK);

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

    /*
        struct dirent {
            uint16_t ino;					inode number of the directory entry 
            uint16_t valid;					validity of the directory entry 
            char name[252];					name of the directory entry 
        };
    */

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)

  //have to find where directory is in file system
  //copy filePath from args
  char* filePath = (char*) malloc(sizeof(fname));
  strcpy(fname, filePath);
  //split via strtok?

  uint16_t inodeStatus; //see whether inode is valid? have to find inode in directory

  if(inodeStatus < 0){//error checking
      perror("dir_find failed\n");
      return ERROR_VALUE;
  }

  


  //uint16_t inodeStatus =





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

    init_FS_globals();

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
