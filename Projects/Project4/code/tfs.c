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
void init_FS_globals();

int diskFound = 0;

/**
 * Helper function to initialize memory for superblock, bitmaps, 
 * and other useful global values.
 */
void init_FS_globals(){

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
        perror("INIT_FS_GLOBALS: UNABLE TO MALLOC BMAPS\n");
        exit(EXIT_FAILURE);
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

    int inode_num = 0;
    //iterate over inodes
    while(inode_num < MAX_INUM){
        
        //bitmap is available/unused if it has 0 value
        if(get_bitmap(INODE_BMAP, inode_num) == 0){
            break;
        } 
        inode_num++;
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
    int retVal = 0;

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
                    //set fields of new block (valid, name)
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
    int invalid_ptr_index = 0;
    while(invalid_ptr_index < DIRECT_PTRS){
        

        if(dir_inode.direct_ptr[invalid_ptr_index] == ERROR_VALUE){
            //not allocated, use this index
            break;
        }
        invalid_ptr_index++;
    }
    dir_inode.direct_ptr[invalid_ptr_index] = new_dir_block_num;

    //allocate and zero out space
    struct dirent* temp_dirent = (struct dirent*)calloc(sizeof(struct dirent),
                                                         sizeof(struct dirent));
    temp_dirent->valid = 0; //mark as invalid
    temp_dirent->ino = 0; //default values for ino

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

    int dir_ptr_index = 0; //direct pointers
    int dirent_ptr_index = 0; //dirent pointers
    struct dirent* dir_block;
    int exitFlag = 0;

    //dirent to write to disk if needed
    struct dirent* temp_dirent = (struct dirent*) calloc(sizeof(struct dirent),
                                                    sizeof(struct dirent));
    temp_dirent->ino = 0;
    temp_dirent->valid = 0;
    


    while(dir_ptr_index < DIRECT_PTRS){
        if(dir_inode.direct_ptr[dir_ptr_index] != ERROR_VALUE){
            //invalid block
            void* temp_block = (void*) malloc(BLOCK_SIZE);
            bio_read(SBLOCK->d_start_blk 
                        + dir_inode.direct_ptr[dir_ptr_index],
                        temp_block);
            dir_block = (struct dirent*) temp_block;

            // Step 2: Check if fname exist
            //iterate through dirents in block
            while(dirent_ptr_index < DIRENTS_PER_BLOCK){
                if(dir_block[dirent_ptr_index].valid == 1
                    && strncmp(dir_block[dirent_ptr_index].name, fname, name_len) == 0){
                    exitFlag = 1;
                    //block is valid and name matches

                	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

                    //write dirent to disk
                    
                    void* write_block = (void*) malloc(BLOCK_SIZE);

                    bio_read(SBLOCK->d_start_blk 
                                + dir_inode.direct_ptr[dir_ptr_index],
                                 write_block);
                    struct dirent* temp_dir_block = (struct dirent*) 
                                                    write_block;
                    if(temp_dir_block == NULL){
                        perror("DIR_REMOVE: COULD NOT READ DATA BLOCK\n");
                        exit(EXIT_FAILURE);
                    }
                    temp_dir_block[dirent_ptr_index] = *temp_dirent;


                    //write to disk
                    bio_write(SBLOCK->d_start_blk 
                                + dir_inode.direct_ptr[dir_ptr_index],
                                 (void*) temp_dir_block);
                    
                    free(write_block);
                    free(temp_dirent);
                    return SUCCESS;
                }
                dirent_ptr_index++;

            }
        }
        dir_ptr_index++;

    }
	if(exitFlag == 0){
        //never found dir to remove?
        free(temp_dirent);
    }
	return ERROR_VALUE;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

    //use strtok? - separate by / character
    if(DEBUG){
        printf("IN GET_NODE_BY_PATH\n");
    }
    char* path_copy = strdup(path);
    int retVal = 0;
    //get inode information given inode number
    readi(ino, inode);

    //get next token in path separated by / character
    char* path_token = strtok_r(path_copy, "/", &path_copy);

    if(DEBUG){
        printf("GET_NODE_BY_PATH: CURRENT TOKEN: %s\n", path_token);
    }

    //if token is NULL, we're at the end of the path
    //we can check the inode here to determine if we've found the
    //desired inode (this is based on the inode's valid status)

    if(path_token == NULL){
        if(inode->valid == 0){
            //inode is valid, this is the inode we were looking for
            return 0;
        }
        //inode node valid, inode doesn't exist by path
        return ERROR_VALUE;
    }

    //have to check remaining path/tokens for the inode
    //if we have path remaining, we have to be in a directory.
    //check that the current token of the path is in a dirent entry in the
    //directory's data blocks.

    struct dirent* current_dirent = (struct dirent*) 
                                    malloc(sizeof(struct dirent));
    
    int dir_status = dir_find(ino, 
                            path_token, 
                            strlen(path_token) + 1, 
                            current_dirent);
    /*
        if dirent does not exist for this path, there shouldn't be a corresponding inode, so we return an error.
    */

    if(dir_status == ERROR_VALUE){
        if(DEBUG){
            perror("GET_NODE_BY_PATH: NO DIRENT FOUND FOR TOKEN\n");
        }
        free(current_dirent);
        return ERROR_VALUE;
    }

    /*
        If dirent is found, recurse on the found dirent
    */

    int next_inode = current_dirent->ino;
    free(current_dirent);

    retVal = get_node_by_path(path_copy, next_inode, inode);
    free(path_copy);
	return retVal;
}

/* 
 * Make file system
 */
int tfs_mkfs() {
	
    //initialize superblock and other globals
    init_FS_globals();

	// Call dev_init() to initialize (Create) Diskfile
	
	if(dev_open(diskfile_path) < 0) {
		if(DEBUG){
            printf("TFS_MKFS: CANNOT FIND DISKFILE PATH\n");
        }

		dev_init(diskfile_path);

        //write superblock information
		SBLOCK->magic_num = MAGIC_NUM;
		SBLOCK->max_inum = MAX_INUM;
		SBLOCK->max_dnum = MAX_DNUM;

		SBLOCK->i_bitmap_blk = SUPERBLOCK_BLOCK_SIZE;
		SBLOCK->d_bitmap_blk = SBLOCK->i_bitmap_blk + INODE_BITMAP_BLOCK_SIZE;
		SBLOCK->i_start_blk = SBLOCK->d_bitmap_blk + DBLOCK_BITMAP_BLOCK_SIZE;
		SBLOCK->d_start_blk = SBLOCK->i_start_blk + INODE_TABLE_BLOCK_SIZE;
		bio_write(0, (void*)SBLOCK);


		// initialize inode bitmap
		bio_write(SBLOCK->i_bitmap_blk, (void*) INODE_BMAP) ;

		// initialize data block bitmap
		bio_write(SBLOCK->d_bitmap_blk, (void*) DBLOCK_BMAP);

		// allocate all inodes and write them into disk
        int inum_iterator = 1;
        while(inum_iterator < MAX_INUM){
            struct inode* temp_inode = calloc(sizeof(struct inode), 
                                        sizeof(struct inode));
            temp_inode->valid = 0;

            int index = 0;
            //set direct/indirect links to invalid
            while(index < DIRECT_PTRS){
                temp_inode->direct_ptr[index] = 0;
                if(index < INDIRECT_PTRS){
                    temp_inode->indirect_ptr[index] = 0;
                }
            }
            temp_inode->ino = inum_iterator;

            //write inode block to disk
            writei(inum_iterator, temp_inode);
            free(temp_inode);
            inum_iterator++;
        }

		// update bitmap information for root directory
		set_bitmap(INODE_BMAP, 0);
		bio_write(SBLOCK->i_bitmap_blk, (void*) INODE_BMAP) ;
		

		// update inode for root directory
		// folder = 1
		// valid = 1

		struct inode* rootInode = malloc(sizeof(struct inode));
 		memset(rootInode, 0, sizeof(struct inode));
		rootInode->ino = 0;
  		rootInode->type = 1;
		rootInode->valid = 1;
		rootInode->size = 0;
		rootInode->link = 2;

        inum_iterator = 0;
        //set pointers for root node to ERROR_VALUE (-1)
        while(inum_iterator < DIRECT_PTRS){
            rootInode->direct_ptr[inum_iterator] = ERROR_VALUE;

            if(inum_iterator < INDIRECT_PTRS){
                rootInode->indirect_ptr[inum_iterator] = ERROR_VALUE;
            }
            
            inum_iterator++;
        }

        //update access/modify/create time
  		time(&(rootInode->vstat.st_atime));
  		time(&(rootInode->vstat.st_mtime));
  		time(&(rootInode->vstat.st_ctime));
		
		// this has 2 links because "." points to this inode as well
		
		// root directory's first direct ptr should point to a block of dirents	
		int root_dirent_block_no = get_avail_blkno(); 
		set_bitmap(DBLOCK_BMAP, root_dirent_block_no);
  		bio_write(SBLOCK->d_bitmap_blk, (void*)DBLOCK_BMAP);

		rootInode->direct_ptr[0] = root_dirent_block_no;

        struct dirent* new_dirent = (struct dirent*) 
                                    calloc(sizeof(struct dirent), 
                                    sizeof(struct dirent));

    		new_dirent->ino = 0;
    		new_dirent->valid = 0;
        
        int dirent_iterator = 0;

        while(dirent_iterator < DIRENTS_PER_BLOCK){
			void *block = (void*) malloc(BLOCK_SIZE);
			bio_read(SBLOCK->d_start_blk + root_dirent_block_no, block);
			struct dirent* block_of_dirents = (struct dirent*)block;
			if(block_of_dirents == NULL){
				perror("ERROR:: Could not read data block");
				exit(-1);
			}
			block_of_dirents[dirent_iterator] = *new_dirent;
			bio_write(SBLOCK->d_start_blk + root_dirent_block_no, (void*)block_of_dirents);
			free(block);
            dirent_iterator++;
        }
		free(new_dirent);


		writei(0, rootInode);
		
		// Add "." to root inode
		dir_add(*rootInode, rootInode, ".", 2);

		free(rootInode);
	}
	else{
		// create a buffer
		void* buf = calloc(1, BLOCK_SIZE);
		
    		// Load superblock into memory
   		bio_read(0, buf);
		memcpy(SBLOCK, buf, sizeof(struct superblock));

    		// Load inode bitmap into memory
		memset(buf, 0, BLOCK_SIZE);
		bio_read(SBLOCK->i_bitmap_blk, buf);
		memcpy(INODE_BMAP, buf, MAX_INUM / INDIRECT_PTRS);
		

    		// Load datablock bitmap into memory
    		memset(buf, 0, BLOCK_SIZE);
		bio_read(SBLOCK->d_bitmap_blk, buf);
		memcpy(DBLOCK_BMAP, buf, MAX_DNUM / INDIRECT_PTRS);

		free(buf);
	}
	diskFound = 1;

	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs
	if (diskFound){
		tfs_mkfs();
	}

  	// Step 1b: If disk file is found, just initialize in-memory data structures
  	// and read superblock from disk

	if (!SBLOCK){
        void* temp_buffer = (void*) malloc(BLOCK_SIZE);
        bio_read(0, temp_buffer);
        SBLOCK = (struct superblock*) temp_buffer;
        free(temp_buffer);
	}
	if (!INODE_BMAP){
		if ((INODE_BMAP = calloc(1, MAX_INUM / INDIRECT_PTRS)) == NULL) {
            perror("TFS_INIT: Unable to allocate the inode bitmap.");
            exit(EXIT_FAILURE);
		}
	}
	if (!DBLOCK_BMAP){
		if ((DBLOCK_BMAP = calloc(1, MAX_INUM / INDIRECT_PTRS)) == NULL) {
            perror("TFS_INIT: Unable to allocate the DBLOCK bitmap.");
            exit(EXIT_FAILURE);
		}
	}
    return NULL;
}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	free(SBLOCK);
	free(INODE_BMAP);
	free(DBLOCK_BMAP);
	// Step 2: Close diskfile
	dev_close();

}

static int tfs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	struct inode inode;
  	int status = get_node_by_path(path, 0, &inode);
  	if (status == ERROR_VALUE) {
    	perror("TFS_GETATTR: Couldn't find inode for the path\n");
    	exit(EXIT_FAILURE);
  	}
    if(DEBUG){
  	    printf("TFS_GETATTR: Found inode for the file\n");
    }

	//folder/dir
	if (inode.type == 1){
		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);
	}
	else{
        //file
		stbuf->st_mode = S_IFREG | 0644;
		stbuf->st_nlink = 1;
		stbuf->st_size = inode.size;
	}
	
  

	// Step 2: fill attribute of file into stbuf from inode
  	stbuf->st_uid = getuid();
  	stbuf->st_gid = getgid();

  	stbuf->st_atime = inode.vstat.st_atime;
  	stbuf->st_mtime = inode.vstat.st_mtime;
  	stbuf->st_ctime = inode.vstat.st_ctime;

	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode dirInode;
  	int status = get_node_by_path(path, 0, &dirInode);
	
	// Step 2: If not find, return -1
	if(status == ERROR_VALUE){
		return ERROR_VALUE;
	}

    return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode dirInode;
    //0 represents root node
  	int status = get_node_by_path(path, 0, &dirInode);

	// Step 2: Read directory entries from its data blocks, and copy them to filler
	for(int i = 0; i < DIRECT_PTRS; i++) {
    		if(dirInode.direct_ptr[i] != -1) {
      
      			struct dirent *dirent_block = malloc(BLOCK_SIZE);
      			bio_read(SBLOCK->d_start_blk + dirInode.direct_ptr[i], dirent_block);

      			for(int j = 0; j < DIRENTS_PER_BLOCK; j++) {
        			if(dirent_block[j].valid == 1) {
          				filler(buffer, dirent_block[j].name, NULL, 0);
        			}
      			}

      			free(dirent_block);
    		}
  	}

	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char *path_copy = calloc(1, strlen(path) + 1);
  	strcpy(path_copy, path);
  	char* directory = dirname(path_copy);
  	char* base = basename(path);
	
	if (DEBUG){
  		printf("TFS_MKDIR: directory_name = %s, base_name = %s\n", directory, base);
	}

	// Step 2: Call get_node_by_path() to get inode of parent directory
  	struct inode parentDirInode;
  	get_node_by_path(directory, 0, &parentDirInode);

	// Step 3: Call get_avail_ino() to get an available inode number
  	int dirIno = get_avail_ino();
  	set_bitmap(INODE_BMAP, dirIno);
  	bio_write(SBLOCK->i_bitmap_blk, (void*) INODE_BMAP);

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	dir_add(parentDirInode, dirIno, base, strlen(base) + 1);

	// Step 5: Update inode for target directory

	struct inode* dirInode = malloc(sizeof(struct inode));
	memset(dirInode, 0, sizeof(struct inode));

    
	dirInode->ino = dirIno;
	dirInode->type = 1;
	dirInode->valid = 1;
	dirInode->size = 0;
	dirInode->link = 2;

	int i = 0;
    //set pointers to invalid
	while (i < DIRECT_PTRS){
		dirInode->direct_ptr[i] = ERROR_VALUE;

        if(i < INDIRECT_PTRS){
            dirInode->indirect_ptr[i] = ERROR_VALUE;
        }
		i++;
	}

    //update access/creation/modify time
	time(&(dirInode->vstat.st_atime));
  	time(&(dirInode->vstat.st_mtime));
  	time(&(dirInode->vstat.st_ctime));

	// Step 6: Call writei() to write inode to disk
	writei(dirIno, dirInode);

  	// "." is for the new dir
  	dir_add(*dirInode, dirIno, ".", 2);

  	// reread due to writing to disk
  	readi(dirIno, dirInode);

  	// ".." is for the parent inode
  	dir_add(*dirInode, parentDirInode.ino, "..", 3);

	free(dirInode);

	

	return 0;
}

static int tfs_rmdir(const char *path) {

    if(DEBUG){
        printf("IN TFS_RMDIR\n");
    }
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

    char* path_copy = strdup(path);
    char* parent_dir = dirname(path_copy);
    char* target_name = basename(path_copy);

	// Step 2: Call get_node_by_path() to get inode of target directory

    struct inode remove_dir;
    int retVal = get_node_by_path(parent_dir, 0, &remove_dir);

    if(retVal == ERROR_VALUE){
        perror("TFS_RMDIR: GET_NODE_BY_PATH FAILED (step 2)\n");
        return ERROR_VALUE;
    }

	// Step 3: Clear data block bitmap of target directory

    int direct_ptr_index = 0;
    while(direct_ptr_index < DIRECT_PTRS){
        if(remove_dir.direct_ptr[direct_ptr_index] != ERROR_VALUE){
            //dir found, clear dblock_bmap
            unset_bitmap(DBLOCK_BMAP, remove_dir.direct_ptr[direct_ptr_index]);
            
        }

    }
    //update disk
    bio_write(SBLOCK->d_bitmap_blk, (void*) DBLOCK_BMAP);
    remove_dir.link = remove_dir.link - 1;

	// Step 4: Clear inode bitmap and its data block

    if(remove_dir.link <= 0){
        //update disk, mark as invalid after clearing inode bitmap
        unset_bitmap(INODE_BMAP, remove_dir.ino);
        bio_write(SBLOCK->i_bitmap_blk, (void*) INODE_BMAP);
        remove_dir.valid = ERROR_VALUE;
    }
    writei(remove_dir.ino, &remove_dir);

	// Step 5: Call get_node_by_path() to get inode of parent directory

    struct inode parent;
    retVal = get_node_by_path(parent_dir, 0, &parent);
    if(retVal == ERROR_VALUE){
        perror("TFS_RMDIR: GET_NODE_BY_PATH FAILED (step 5)\n");
        return ERROR_VALUE;
    }

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

    retVal = dir_remove(parent, target_name, strlen(target_name));

    if(retVal == ERROR_VALUE){
        perror("TFS_RMDIR: DIR_REMOVE FAILED (step 6)\n");
        return ERROR_VALUE;
    }

	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    if(DEBUG){
        printf("IN TFS_CREATE\n");
    }

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

    char* path_copy = (char*) malloc(strlen(path) + 1);
    strcpy(path_copy, path);
    char* dir_name = dirname(path_copy);
    char* base_name = basename(path_copy);
    int retVal = 0;

	// Step 2: Call get_node_by_path() to get inode of parent directory
    
    struct inode parent_dir;
    retVal = get_node_by_path(dir_name, 0, &parent_dir);

    if(retVal == ERROR_VALUE){
        perror("TFS_CREATE: get_node_by_path failed (step 2)\n");
        exit(EXIT_FAILURE);
    }


	// Step 3: Call get_avail_ino() to get an available inode number

    int new_ino_num = get_avail_ino();
    set_bitmap(INODE_BMAP, new_ino_num);
    bio_write(SBLOCK->i_bitmap_blk, (void*) INODE_BMAP);


	// Step 4: Call dir_add() to add directory entry of target file to parent directory

    dir_add(parent_dir, new_ino_num, base_name, strlen(base_name) + 1);

	// Step 5: Update inode for target file

    struct inode* new_file_ino = (struct inode*) calloc(sizeof(struct inode),
                                                     sizeof(struct inode));
    int iterator = 0;
    //set direct and indirect pointers.
    while(iterator < DIRECT_PTRS){
        new_file_ino->direct_ptr[iterator] = ERROR_VALUE;
        if(iterator < INDIRECT_PTRS){
            new_file_ino->indirect_ptr[iterator] = ERROR_VALUE;
        }
        iterator++;
    }

    new_file_ino->ino = new_ino_num;
    //0 to mark as new file inode
    new_file_ino->type = 0;
    //new file ino is valid
    new_file_ino->valid = 1;
    new_file_ino->size = 0;
    //link to dir
    new_file_ino->link = 1;

    //update access/creation times
    time(&(new_file_ino->vstat.st_ctime));
    time(&(new_file_ino->vstat.st_mtime));
    time(&(new_file_ino->vstat.st_atime));

   	// Step 6: Call writei() to write inode to disk

    writei(new_ino_num, new_file_ino);
    free(new_file_ino);

	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

    if(DEBUG){
        printf("IN TFS_OPEN\n");
    }
	// Step 1: Call get_node_by_path() to get inode from path

    struct inode dir_ino;
    int retVal = get_node_by_path(path, 0, &dir_ino);
	// Step 2: If not find, return -1

    if(retVal == ERROR_VALUE){
        //not found
        return ERROR_VALUE;
    }

	return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

    if(DEBUG){
        printf("IN TFS_READ\n");
    }
	// Step 1: You could call get_node_by_path() to get inode from path

    struct inode file_ino;
    //0 represents root inode
    int retVal = get_node_by_path(path, 0, &file_ino);

    if(retVal != 0){
        perror("TFS_READ: FILE/DIR DOES NOT EXIST\n");
        exit(EXIT_FAILURE);
    }

	// Step 2: Based on size and offset, read its data blocks from disk

    int bytes_read = 0;
    int start_block = offset / BLOCK_SIZE;
    int size_in_blocks = ceil(size / BLOCK_SIZE);

    int block_iterator = 0;

    while(block_iterator < size_in_blocks){
        int block_offset = block_iterator * BLOCK_SIZE_IN_CHARS;

        if(file_ino.direct_ptr[block_iterator] == ERROR_VALUE){
            //file doesn't have existing datablock?
            //we can't read something that doesn't exist
            perror("TFS_READ: DATA BLOCK DOES NOT EXIST\n");
            return 0;
        }

        //read data block and copy to the designated spot in the buffer.

        void* temp_buffer = (void*) malloc(BLOCK_SIZE);
        bio_read(SBLOCK->d_start_blk 
                + file_ino.direct_ptr[start_block], 
                temp_buffer);
        
        if(size < BLOCK_SIZE){

            // Step 3: copy the correct amount of data from offset to buffer
            memcpy((buffer + block_offset), temp_buffer, BLOCK_SIZE);
            bytes_read = bytes_read + BLOCK_SIZE;
            size = 0;
        }
        else{
            //probably shouldn't be here, shouldn't be possible for
            //size to be greater than BLOCK_SIZE

	        // Step 3: copy the correct amount of data from offset to buffer
            memcpy((buffer + block_offset), temp_buffer, BLOCK_SIZE);
            size = size - BLOCK_SIZE;
            bytes_read = bytes_read + BLOCK_SIZE;
        }
        offset = 0;
        start_block++;
        block_iterator++;
        free(temp_buffer);

    }


    //update metadata/access time
    time(&(file_ino.vstat.st_atime));
    writei(file_ino.ino, &file_ino);
    
	// Note: this function should return the amount of bytes you copied to buffer
	return bytes_read;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

    if(DEBUG){
        printf("IN TFS_WRITE\n");
    }
    int retVal = 0;
    size_t bytes_written = 0;
    int block_offset = offset / BLOCK_SIZE;
    int offset_in_block = offset % BLOCK_SIZE;
    int size_in_blocks = (size / BLOCK_SIZE) + 1;

    char* path_copy = strdup(path);

	// Step 1: You could call get_node_by_path() to get inode from path

    struct inode file_ino;
    retVal = get_node_by_path(path_copy, 1, &file_ino);
    

    if(retVal != 0){
        //node not found?
        if(DEBUG){
            printf("TFS_WRITE: GET_NODE_BY_PATH FAILED\n");
        }
        return 0;
    }

	// Step 2: Based on size and offset, read its data blocks from disk
   

    int block_iterator = 0;
    while(block_iterator < size_in_blocks){
        int blockSizeIterator = block_iterator * BLOCK_SIZE_IN_CHARS;
        //check to make sure block exists
        //if block does not exist, create one
        if(file_ino.direct_ptr[block_offset] == ERROR_VALUE){
            int new_block_num = get_avail_blkno();
            file_ino.direct_ptr[block_offset] = new_block_num;
            void* temp_buffer = (void*) malloc(BLOCK_SIZE);
            if(size < BLOCK_SIZE){
                //read data
                memcpy(temp_buffer, 
                        (buffer + blockSizeIterator), 
                        BLOCK_SIZE);
                bytes_written = bytes_written + size;
                size = 0;
            
            }
            else{
                //have to change sizes of file inode, bytes_written, size
                memcpy(temp_buffer, 
                        (buffer + blockSizeIterator), 
                        BLOCK_SIZE);

                //increase file inode's size by BLOCK_SIZE
                file_ino.size = file_ino.size + BLOCK_SIZE;
                //increase bytes_written by BLOCK_SIZE
                bytes_written = bytes_written + BLOCK_SIZE;
                //decrease size by BLOCK_SIZE
                size = size - BLOCK_SIZE;
            }

	        // Step 3: Write the correct amount of data from offset to disk
            bio_write(SBLOCK->d_start_blk + new_block_num, temp_buffer);
            free(temp_buffer);
        }
        else{
            //need to read and memcpy to specific offset, as data block already
            //exists
            void* temp_buffer = (void*) malloc(BLOCK_SIZE);
            bio_read(SBLOCK->d_start_blk 
                    + file_ino.direct_ptr[block_offset], 
                    temp_buffer);

            if(size < BLOCK_SIZE){
                memcpy(temp_buffer + offset_in_block, 
                        buffer + blockSizeIterator, 
                        size);
                if((size + offset) <= strlen(temp_buffer)){
                    memset((temp_buffer + offset_in_block + size), 
                            '\0', 
                            (BLOCK_SIZE - size - offset));
                }
                bytes_written = bytes_written + size;
                size = 0;
            }
            else{
                memcpy(temp_buffer + offset_in_block, 
                        buffer + blockSizeIterator, 
                        size);

                bytes_written = bytes_written + BLOCK_SIZE;
                file_ino.size = file_ino.size + BLOCK_SIZE;
                size = size - BLOCK_SIZE;
            }

	        // Step 3: Write the correct amount of data from offset to disk
            bio_write(SBLOCK->d_start_blk 
                        + file_ino.direct_ptr[block_offset], 
                        temp_buffer);
            free(temp_buffer);
            
        }
        offset = 0;
        block_offset++;
        block_iterator++;
    }

	// Step 4: Update the inode info and write it to disk

    time(&(file_ino.vstat.st_mtime));
    writei(file_ino.ino, &file_ino);

	// Note: this function should return the amount of bytes you write to disk

    free(path_copy);
	return bytes_written;
}

static int tfs_unlink(const char *path) {

    if(DEBUG){
        printf("IN TFS_UNLINK\n");
    }
	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

    char* path_copy = strdup(path);
    char* dir_name = dirname(path_copy);
    char* base_name = basename(path_copy);
    int errorFlag = 0;

	// Step 2: Call get_node_by_path() to get inode of target file

    struct inode target;
    //root inode is 0
    errorFlag = get_node_by_path(path_copy, 0, &target);

    //check to make sure we've found target
    if(errorFlag == ERROR_VALUE){
        printf("TFS_UNLINK: TARGET NOT FOUND\n");
        exit(EXIT_FAILURE);
    }

	// Step 3: Clear data block bitmap of target file

    int direct_ptr_index = 0;
    while (direct_ptr_index < DIRECT_PTRS){
        if(target.direct_ptr[direct_ptr_index] != ERROR_VALUE){
            unset_bitmap(DBLOCK_BMAP, target.direct_ptr[direct_ptr_index]);
        }
        direct_ptr_index++;
    }
    bio_write(SBLOCK->d_bitmap_blk, (void*) DBLOCK_BMAP);


	// Step 4: Clear inode bitmap and its data block

    //decrease link count
    target.link = target.link - 1;
    if(target.link <= 0){
        unset_bitmap(INODE_BMAP, target.ino);
        //update disk
        bio_write(SBLOCK->i_bitmap_blk, (void*) INODE_BMAP);
        //set valid bit as invalid
        target.valid = 0;
    }

	// Step 5: Call get_node_by_path() to get inode of parent directory

    struct inode parent;
    errorFlag = get_node_by_path(dir_name, 0, &parent);

    if(errorFlag == ERROR_VALUE){
        printf("TFS_UNLINK: PARENT DIR INODE NOT FOUND\n");
        exit(EXIT_FAILURE);
    }


	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

    errorFlag = dir_remove(parent, base_name, strlen(base_name));

    if(errorFlag == ERROR_VALUE){
        printf("TFS_UNLINK: DIR_REMOVE FAILED\n");
        exit(EXIT_FAILURE);
    }

    free(path_copy);

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
