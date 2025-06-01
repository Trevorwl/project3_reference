#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdint.h>

#include "disk.h"
#include "fs.h"

/*
 * These 4 variables can be assigned in fs_mount
 */
bool disk_mounted = false;

struct fdTable* fd_table = NULL;

size_t root_directory_index = 0;

size_t data_blocks=0;

struct DiskMetadata* diskMetadata = NULL;

bool isValidFileName(const char *filename);

bool isValidMetadata();

int fs_mount(const char *diskname)
{
    int ret = block_disk_open(diskname);

    if(ret != 0){
        return ret;
    }

    init_bounce_buffer();
    clear_bounce_buffer();

    ret = block_read(SUPERBLOCK_INDEX, bounce_buffer);

    if(ret != 0){
        block_disk_close();
        return ret;
    }

    diskMetadata = (struct DiskMetadata*)calloc(1,sizeof(struct DiskMetadata));

    memcpy(diskMetadata, bounce_buffer, sizeof(struct DiskMetadata));

    if(isValidMetadata() == false){

        block_disk_close();

        free(diskMetadata);
        diskMetadata=NULL;

        return -1;
    }

    disk_mounted = true;

    root_directory_index = diskMetadata->rootDirectoryIndex;

    data_blocks=diskMetadata->totalDataBlocks;

    fd_table = fd_table_constructor();

    return 0;
}

int fs_umount(void)
{
    int close_status = block_disk_close();

    if(close_status==0){

        free(diskMetadata);
        diskMetadata=NULL;

        fdTable_destructor(fd_table);
        fd_table=NULL;

        disk_mounted = false;
        root_directory_index=0;
        data_blocks=0;
    }

    return close_status;
}

int fs_info(void)
{
    if(disk_mounted==false){
        return -1;
    }

    init_bounce_buffer();
    clear_bounce_buffer();

    size_t fat_occupied_entries = 0;

    for(size_t fBlock = FAT_BLOCK_START_INDEX; fBlock < root_directory_index; fBlock++){

        if(block_read(fBlock, bounce_buffer)){
            return -1;
        }

        uint16_t* fatEntry = (uint16_t*)bounce_buffer;

        for(int entry = 0; entry < FAT_ENTRIES; entry++){

            if(*fatEntry != 0){
                fat_occupied_entries++;
            }

            fatEntry++;
        }

        clear_bounce_buffer();
    }

    if(block_read(root_directory_index, bounce_buffer)){
        return -1;
    }

    int dirFreeEntries = 0;

    struct DirEntry* dirEntry = (struct DirEntry*)bounce_buffer;

    for(int entry = 0; entry < FS_FILE_MAX_COUNT; entry++){

        if(*(dirEntry->filename) == '\0'){
            dirFreeEntries++;
        }

        dirEntry ++;
    }

    printf("FS Info:\n"
            "total_blk_count=%hu\n"
            "fat_blk_count=%hhu\n"
            "rdir_blk=%hu\n"
            "data_blk=%hu\n"
            "data_blk_count=%hu\n",
            diskMetadata->totalBlocks,
            diskMetadata->totalFatBlocks,
            diskMetadata->rootDirectoryIndex,
            diskMetadata->dataStartIndex,
            diskMetadata->totalDataBlocks);

    size_t fat_available=(size_t)diskMetadata->totalDataBlocks - fat_occupied_entries;

    printf("fat_free_ratio=%zu/%zu\n", fat_available, (size_t)diskMetadata->totalDataBlocks);

    printf("rdir_free_ratio=%d/%d\n",dirFreeEntries, FS_FILE_MAX_COUNT);

    return 0;
}

int fs_create(const char *filename)
{
    if(disk_mounted==false){
        return -1;
    }

    if(isValidFileName(filename)==false){
        return -1;
    }

    init_bounce_buffer();
    clear_bounce_buffer();

    block_read(root_directory_index,bounce_buffer);

    struct DirEntry* entry=(struct DirEntry*)bounce_buffer;

    int availableIndex=-1;

    for(int i=0;i<FS_FILE_MAX_COUNT;i++){

        if(strcmp(filename,(char*)entry->filename)==0){
            return -1;
        }

        if(availableIndex == -1 && *(entry->filename)=='\0'){
            availableIndex = i;
        }

        entry++;
    }

    if(availableIndex == -1){
        return -1;
    }

    entry = (struct DirEntry*)bounce_buffer + availableIndex;

    memset(entry,0,sizeof(struct DirEntry));

    strcpy((char*)entry->filename,filename);

    entry->index=FAT_EOC;

    block_write(root_directory_index,bounce_buffer);

	return 0;
}

int fs_delete(const char *filename)
{
    if(disk_mounted==false){
        return -1;
    }

    if(filename==NULL){
        return -1;
    }

    if(isValidFileName(filename)==false){
        return -1;
    }

    init_bounce_buffer();
    clear_bounce_buffer();

    block_read(root_directory_index, bounce_buffer);

    struct DirEntry* dir_entry = (struct DirEntry*)bounce_buffer;

    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){

        if(strcmp(filename,(char*)dir_entry->filename) == 0){

            if(isOpenByName(fd_table,(char*)dir_entry->filename)==true){

                return -1;
            }

            uint16_t dir_entry_index = dir_entry->index;

            memset(dir_entry,0,sizeof(struct DirEntry));

            block_write(root_directory_index, bounce_buffer);

            if(dir_entry_index==FAT_EOC){
                return 0;
            }

            return erase_file(dir_entry_index);
        }

        dir_entry++;
    }

    return -1;
}

int fs_ls(void)
{
    if(disk_mounted==false){
        return -1;
    }

    init_bounce_buffer();
    clear_bounce_buffer();

    block_read(root_directory_index, bounce_buffer);

    struct DirEntry* dir_entry = (struct DirEntry*)bounce_buffer;

    printf("FS Ls:\n");

    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){

        if(*(dir_entry->filename)!='\0'){

            printf("file: %s, size: %u, data_blk: %hu\n",
                    (char*)dir_entry->filename,
                    dir_entry->size,
                    dir_entry->index);
        }

        dir_entry++;
    }

    return 0;
}

int fs_open(const char *filename)
{
    if(disk_mounted==false){
        return -1;
    }

    if(filename==NULL){
        return -1;
    }

    if(isValidFileName(filename)==false){
        return -1;
    }

    if(table_is_full(fd_table)==true){
        return -1;
    }

    init_bounce_buffer();
    clear_bounce_buffer();

    block_read(root_directory_index, bounce_buffer);

    struct DirEntry* dir_entry = (struct DirEntry*)bounce_buffer;

    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){

        if(strcmp(filename,(char*)dir_entry->filename) == 0){

            int fd = addFd(fd_table,(char*)dir_entry->filename);

            struct fdNode* fdEntry = getFdEntry(fd_table,fd);

            fdEntry->dir_entry_index = i;

            if(dir_entry->index!=FAT_EOC){
                fdEntry->size = dir_entry->size;

                fdEntry->first_data_block= dir_entry->index;
            }

            return fd;
        }

        dir_entry++;
    }

    return -1;
}

int fs_close(int fd)
{
    if(disk_mounted==false){
        return -1;
    }

    if(fd<0 || fd >= FS_OPEN_MAX_COUNT){
        return -1;
    }

    if(isOpenByFd(fd_table,fd)==false){
        return -1;
    }

    removeFd(fd_table,fd);

    return 0;
}

int fs_stat(int fd)
{
   if(disk_mounted==false){
       return -1;
   }

   if(fd<0 || fd >= FS_OPEN_MAX_COUNT){
       return -1;
   }

   if(isOpenByFd(fd_table,fd)==false){
       return -1;
   }

   struct fdNode* fdEntry=getFdEntry(fd_table,fd);

   return fdEntry->size;
}

int fs_lseek(int fd, size_t offset)
{
    if(disk_mounted==false){
        return -1;
    }

    if(fd<0 || fd >= FS_OPEN_MAX_COUNT){
         return -1;
     }

     if(isOpenByFd(fd_table,fd)==false){
         return -1;
     }

     struct fdNode* fdEntry=getFdEntry(fd_table,fd);

     if(offset < 0 || offset > fdEntry->size){
         return -1;
     }

     fdEntry->offset=offset;

     return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
    if(buf==NULL){
       return -1;
    }

    if(disk_mounted==false){
        return -1;
    }

    if(fd<0 || fd >= FS_OPEN_MAX_COUNT){
        return -1;
    }

    if(isOpenByFd(fd_table,fd)==false){
        return -1;
    }

    if(count<=0){
        return 0;
    }

    struct fdNode* fdEntry = getFdEntry(fd_table,fd);

    if(fdEntry->first_data_block == FAT_EOC){
        //disk full
        if(add_file_to_disk(fdEntry)==false){
            return 0;
        }
    }

    size_t total_block_count = total_file_blocks(fdEntry->first_data_block);

    size_t total_bytes = total_block_count * (size_t)BLOCK_SIZE;

    size_t end_offset = fdEntry->offset + count;

    if(end_offset > total_bytes){

        size_t needed_space = end_offset - total_bytes;

        size_t needed_blocks = total_block_size(needed_space);

        size_t new_blocks = allocate_more_blocks(fdEntry, needed_blocks);

        if(new_blocks==0 && fdEntry->offset==total_bytes){//end of file and no more space allocated
            return 0;
        }

        total_block_count = total_file_blocks(fdEntry->first_data_block);

        total_bytes = total_block_count * (size_t)BLOCK_SIZE;

        if(end_offset > total_bytes){

           end_offset = total_bytes;
        }
    }

    init_bounce_buffer();
    clear_bounce_buffer();

    size_t write_block =  skip_blocks(fdEntry->first_data_block, current_block_offset(fdEntry->offset));

    size_t raw_write_block = get_actual_block_index(write_block);

    size_t bytesWritten = 0;

    size_t character = 0;

    while(fdEntry->offset < end_offset){

         size_t write_characters = 0;

         size_t offset_in_block = fdEntry->offset % (size_t)BLOCK_SIZE;

         if(current_block_offset(fdEntry->offset) < current_block_offset(end_offset)){

             //we will write until end of block
             write_characters = (size_t)BLOCK_SIZE - offset_in_block;

         } else {

             //we will write somewhere within the block
             write_characters = end_offset % (size_t)BLOCK_SIZE - offset_in_block;
         }

         if(write_characters == BLOCK_SIZE){

            block_write(raw_write_block, &buf[character]);

         } else {

            clear_bounce_buffer();

            block_read(raw_write_block, bounce_buffer);

            memcpy(&bounce_buffer[offset_in_block], &buf[character], write_characters);

            block_write(raw_write_block, bounce_buffer);

         }

         fdEntry->offset += write_characters;

         character += write_characters;

         bytesWritten += write_characters;

         if(fdEntry->offset > fdEntry->size){
             fdEntry->size = fdEntry->offset;
         }

         if(fdEntry->offset < end_offset){

             write_block =  skip_blocks(write_block, 1);
             raw_write_block =  get_actual_block_index(write_block);
         }
    }

    clear_bounce_buffer();

    block_read(root_directory_index,bounce_buffer);

    struct DirEntry* dir_entry=(struct DirEntry*)bounce_buffer + fdEntry->dir_entry_index;

    dir_entry->size=(uint32_t)fdEntry->size;

    block_write(root_directory_index,bounce_buffer);

    return bytesWritten;
}

int fs_read(int fd, void *buf, size_t count)
{
    if(buf==NULL){
       return -1;
    }

    if(disk_mounted==false){
        return -1;
    }

    if(fd<0 || fd >= FS_OPEN_MAX_COUNT){
        return -1;
    }

    if(isOpenByFd(fd_table,fd)==false){
        return -1;
    }

    if(count<=0){
        return 0;
    }

    struct fdNode* fdEntry = getFdEntry(fd_table,fd);

    if(fdEntry->first_data_block == FAT_EOC){
        return 0;
    }

    if(fdEntry->offset >= fdEntry->size){
        return 0;
    }

    size_t end_offset = fdEntry->offset + count;

    if(end_offset > fdEntry->size){
        end_offset = fdEntry->size;
    }

    init_bounce_buffer();
    clear_bounce_buffer();

    size_t read_block = skip_blocks(fdEntry->first_data_block, current_block_offset(fdEntry->offset));

    size_t raw_read_block = get_actual_block_index(read_block);

    size_t bytesRead = 0;

    size_t character = 0;

    while(fdEntry->offset < end_offset){

        size_t read_characters = 0;

        size_t offset_in_block = fdEntry->offset % (size_t)BLOCK_SIZE;

        if(current_block_offset(fdEntry->offset) < current_block_offset(end_offset)){

            //we will write until end of block
            read_characters = (size_t)BLOCK_SIZE - offset_in_block;

          } else {

              //we will write somewhere within the block
              read_characters = end_offset % (size_t)BLOCK_SIZE - offset_in_block;
          }

          if(read_characters == BLOCK_SIZE){

             block_read(raw_read_block, &buf[character]);

          } else {

             clear_bounce_buffer();

             block_read(raw_read_block, bounce_buffer);

             memcpy(&buf[character], &bounce_buffer[offset_in_block], read_characters);
          }

          fdEntry->offset += read_characters;

          character += read_characters;

          bytesRead += read_characters;

          if(fdEntry->offset < end_offset){

              read_block =  skip_blocks(read_block, 1);
              raw_read_block =  get_actual_block_index(read_block);
          }
    }

    return bytesRead;
}

bool isValidFileName(const char *filename){
      size_t nameLen = strlen(filename);

      if(nameLen==0){
          return false;
      }

      if(nameLen + 1 > (size_t)FS_FILENAME_LEN){
          return false;
      }

      if (strchr(filename, '/') != NULL) {
          return false;
      }

      return true;
}

bool isValidMetadata(){
    if (strncmp((char*)diskMetadata->signature, "ECS150FS", 8) != 0){
        return false;
    }


    size_t totalBlocks = (size_t)diskMetadata->totalBlocks;
    size_t min_blocks = 4;//1 super+1 fat+1 root directory+1 data block

    if(totalBlocks != block_disk_count()){
        return false;
    }

    if(totalBlocks < min_blocks){
        return false;
    }

    size_t rootDirectoryIndex = (size_t)diskMetadata->rootDirectoryIndex;
    size_t dataStartIndex = (size_t)diskMetadata->dataStartIndex;
    size_t totalDataBlocks = (size_t)diskMetadata->totalDataBlocks;
    size_t totalFatBlocks = (size_t)diskMetadata->totalFatBlocks;

    if(totalDataBlocks < 1 || totalFatBlocks < 1){
        return false;
    }

    //excluding fat blocks, at least 3 blocks must be reserved(super+root+data)
    //excluding data blocks, at least 3 blocks must be reserved(super+root+fat)
    if(totalDataBlocks > totalBlocks - 3|| totalFatBlocks > totalBlocks-3){
        return false;
    }

    size_t extra_fat_block = (totalDataBlocks % (size_t)FAT_ENTRIES == 0) ? 0 : 1;

    size_t fat_blocks_required = totalDataBlocks / (size_t)FAT_ENTRIES + extra_fat_block;

    if(fat_blocks_required != totalFatBlocks){
        return false;
    }

    if(totalDataBlocks > totalFatBlocks * (size_t)FAT_ENTRIES){
        return false;
    }

    //1+ totalFatBlocks and 1 + totalFatBlocks + 1 because of 0-based indexing
    if(rootDirectoryIndex!= 1 + totalFatBlocks
            || dataStartIndex!= 1 + totalFatBlocks + 1){
        return false;
    }

    return true;
}

