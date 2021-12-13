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
#include <math.h>

#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here

struct superblock* SBLOCK;
bitmap_t INODE_BMAP;
bitmap_t DBLOCK_BMAP;
#define ERROR_VALUE -1
#define SUCCESS 1

#ifndef DEBUG
    #define DEBUG 0
#endif

int SUPERBLOCK_BLOCK_SIZE;
int INODE_BITMAP_BLOCK_SIZE;
int DBLOCK_BITMAP_BLOCK_SIZE;
int INODE_TABLE_BLOCK_SIZE;
int BLOCK_SIZE_IN_CHARS;
int INODES_PER_BLOCK;
int DIRENTS_PER_BLOCK;

int check_SBLOCK();
int init_FS_globals();

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
                  INODE_BMAP[(int) (iterator * BLOCK_SIZE_IN_CHARS)]);
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

    int dblock_index = 0;

    while(dblock_index < MAX_DNUM){
        
        //get available data block from bitmap
        if(get_bitmap(DBLOCK_BMAP, dblock_index) == 0){
            break;
        }
        dblock_index++;
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

    //error condition for success of readi
    //int retVal = 0;

    //read from disk
    bio_read(block_num, buffer_block);


    struct inode* inode_block = (struct inode*) buffer_block;

    if(DEBUG){
        printf("READI: READING INTO INODE:\n"
                "ino: %d, "
                "block number: %d "
                "offset: %d\n",
                ino,
                block_num,
                offset);
    }

    //change input inode's pointer to block's data
    //same effect as copying all fields into inode
    *inode = inode_block[offset];
    free(buffer_block);

	return SUCCESS;
}


int writei(uint16_t ino, struct inode *inode) {

    if(DEBUG){
        printf("in writei\n");
    }
    check_SBLOCK();
	// Step 1: Get the block number where this inode resides on disk
    int block_num = SBLOCK->i_start_blk + (ino / INODES_PER_BLOCK);

	// Step 2: Get the offset in the block where this inode resides on disk
	int offset = ino % INODES_PER_BLOCK;

	// Step 3: Write inode to disk 

    void* inode_buffer = (void*) malloc(BLOCK_SIZE);

    //error condition for success of writei
    //int retVal = 0;

    bio_read(block_num, inode_buffer);


    //update access time
    //st_mtime = time of last modification to inode
    //st_ctime = time of creation/last metadata change on linux?
    time(&(inode->vstat.st_mtime));
    //time(&(inode->vstat.st_ctime));

    if(DEBUG){
        printf("WRITEI: WRITING INTO INODE:\n"
                "ino: %d, "
                "block number: %d "
                "offset: %d\n",
                ino,
                block_num,
                offset);
    }

    struct inode* inode_block = (struct inode*) inode_buffer;

    //change pointer to given inode to be written
    inode_block[offset] = *inode;

    //write that block to disk based on given input inode
    bio_write(block_num, (void*) inode_block);

    //get rid of temporary block
    free(inode_block);

	return SUCCESS;
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

    if(DEBUG){
        printf("IN DIR_FIND\n");
    }

    //flag stating whether we found a match
    //will change to 1 if found
    int retVal = ERROR_VALUE;
    // Step 1: Call readi() to get the inode using ino (inode number of current directory)

    struct inode current_inode;
    readi(ino, &current_inode);

    // Step 2: Get data block of current directory from inode

    void* buffer = (void*) BLOCK_SIZE;

    if(buffer == NULL){
        perror("DIR_FIND: BUFFER NOT ALLOCATED\n");
        exit(EXIT_FAILURE);
    }

    int iterator = 0;
    while(iterator < DIRECT_PTRS){
        

        //check if direct pointer is valid
        //points to data block that contains dirents

        if(current_inode.direct_ptr[iterator] != ERROR_VALUE){
            bio_read(SBLOCK->d_start_blk + current_inode.direct_ptr[iterator],
                     buffer);
            struct dirent* dir_block = (struct dirent*) buffer;
            
        

        // Step 3: Read directory's data block and check each directory entry.
        //If the name matches, then copy directory entry to dirent structure

            int dir_index = 0;
            while(dir_index < DIRENTS_PER_BLOCK){
                
                
                //get current dirent
                struct dirent current_dirent = dir_block[dir_index];

                if(current_dirent.valid == 1){
                    //current dirent is valid
                    //check if name matches what we're looking for
                    
                    //use name_len to compare fname and current dirent name
                    //using strncmp
                    int isMatch = strncmp(fname, current_dirent.name, name_len);
                    if(isMatch == 0){
                        //strncmp returns 0 for match between strings
                        //match found
                        //copy directory entry to dirent structure
                        *dirent = current_dirent;
                        free(buffer);
                        retVal = 1;
                        return retVal;
                    }
                }
                dir_index++;
            }
        }
        iterator++;

    }

    //should probably not reach here, means dir not found
    //perror("AT END OF DIR_FIND, CHECK CODE\n");
    free(buffer);
	return retVal;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

    if(DEBUG){
        printf("IN DIR_ADD\n");
    }

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode

    int dir_ptr_index = 0;
    int dirent_block_index = 0; 

    while (dir_ptr_index < DIRECT_PTRS){
        

        if(dir_inode.direct_ptr[dir_ptr_index] != ERROR_VALUE){

            void* temp_dir_block = (void*) malloc(BLOCK_SIZE);
            bio_read(SBLOCK->d_start_blk 
                        + dir_inode.direct_ptr[dir_ptr_index], 
                        temp_dir_block);

            struct dirent *dir_block = (struct dirent*) temp_dir_block;

            // Step 2: Check if fname (directory name) is already used in other entries

            while(dirent_block_index < DIRENTS_PER_BLOCK){
                

                if(dir_block[dirent_block_index].valid == 1
                    && strcmp(dir_block[dirent_block_index].name, fname) == 0){
                    //valid dir, check against fname
                    //name matches, and valid dir, dir already exists
                    //return error

                    if(DEBUG){
                        printf("DIR_ADD: DIR ALREADY EXISTS\n");
                    }

                    return ERROR_VALUE;
                    
                }
                dirent_block_index++;
            }

        }
        dir_ptr_index++;
    }
	
	
	// Step 3: Add directory entry in dir_inode's data block and write to disk

    dir_ptr_index = 0;
    dirent_block_index = 0;
    struct dirent* usable_block;

    while(dir_ptr_index < DIRECT_PTRS){
        

        if(dir_inode.direct_ptr[dir_ptr_index] != ERROR_VALUE){
            struct dirent *dir_block = (struct dirent*) malloc(BLOCK_SIZE);
            bio_read(SBLOCK->d_start_blk 
                    + dir_inode.direct_ptr[dir_ptr_index],
                    dir_block);

            while(dirent_block_index < DIRENTS_PER_BLOCK){

                
                if(dir_block[dirent_block_index].valid == 0){
                    //block not allocated, use this one
                    //invalid dirent = available to write new dirent
                    usable_block = dir_block;

                    // Allocate a new data block for this directory if it does not exist
                    //set fields of new block (valid, name, len)
                    usable_block[dirent_block_index].len = name_len;
                    strcpy(usable_block[dirent_block_index].name, fname);
                    usable_block[dirent_block_index].ino = f_ino;

                    //write to disk
                    bio_write(SBLOCK->d_start_blk 
                                + dir_inode.direct_ptr[dir_ptr_index],
                                (void*) usable_block);

                    //dir added successfully
                    return SUCCESS;
                }
                dirent_block_index++;
            }
        }
        dir_ptr_index++;
  
    }

    
    //no valid dirent block found, have to make a new block
    int new_dir_block_num = get_avail_blkno();
    set_bitmap(DBLOCK_BMAP, new_dir_block_num);
    // Write directory entry
    bio_write(SBLOCK->d_bitmap_blk, (void*) DBLOCK_BMAP);

    //get next invalid direct ptr
    int invalid_ptr_index = -1;
    while(invalid_ptr_index < DIRECT_PTRS){
        invalid_ptr_index++;

        if(dir_inode.direct_ptr[invalid_ptr_index] == ERROR_VALUE){
            //not allocated, use this index
            break;
        }
    }
    dir_inode.direct_ptr[invalid_ptr_index] = new_dir_block_num;

    //allocate and zero out space
    struct dirent* temp_dirent = (struct dirent*)calloc(sizeof(struct(dirent)),
                                                         sizeof(struct dirent));
    temp_dirent->valid = ERROR_VALUE; //mark as invalid
    temp_dirent->ino = 0; //default values for ino and len
    temp_dirent->len = 0;

    //invalid_ptr index marks new block to write to
    //iterate through each dirent in block
    for(int dir_offset = 0; dir_offset < DIRENTS_PER_BLOCK; dir_offset++){
        //write dirent to index of a data block number
        void* temp_block = (void*) malloc(BLOCK_SIZE);
        struct dirent* temp_dirent_block;
        bio_read(SBLOCK->d_start_blk + invalid_ptr_index, temp_block);
        temp_dirent_block = (struct dirent*) temp_block;
        if(temp_dirent_block == NULL){
            perror("DIR_ADD: BIO_READ FAILED\n");
            exit(EXIT_FAILURE);
        }
        temp_dirent_block[dir_offset] = *temp_dirent;
        bio_write(SBLOCK->d_start_blk + invalid_ptr_index, 
                    (void*) temp_dirent_block);
        free(temp_block);
    }
    free(temp_dirent);

    //update inodes
    writei(dir_inode.ino, &dir_inode);
    retVal = dir_add(dir_inode, f_ino, fname, name_len);

    return retVal;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

    if(DEBUG){
        printf("IN DIR_REMOVE\n");
    }
	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode

    int dir_ptr_index = -1; //direct pointers
    int dirent_ptr_index = -1; //dirent pointers

    while(dir_ptr_index < DIRECT_PTRS){
        if(dir_inode.direct_ptr[dir_ptr_index] != ERROR_VALUE){
            //invalid block

        }
    }
	
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
