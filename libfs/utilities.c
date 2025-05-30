#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include<stdbool.h>

#include "disk.h"
#include "fs.h"
#include "utilities.h"

uint8_t* utilities_buffer = NULL;

int erase_all_files(){
    if(disk_mounted==false){
        return -1;
    }

    if(fd_table->fdsOccupied!=0){
        return -1;
    }

    if(utilities_buffer==NULL){
        utilities_buffer = (uint8_t*)calloc(1, bounce_buffer_size);
    } else {
       memset(utilities_buffer,0,bounce_buffer_size);
    }

    block_read(root_directory_index,utilities_buffer);

    struct DirEntry* entry = (struct DirEntry*)utilities_buffer;

    for(int i=0;i<FS_FILE_MAX_COUNT;i++){

        if(*(entry->filename)!='\0'){

            if(entry->index!=FAT_EOC){
                erase_file(entry->index);
            }

            memset(entry,0,sizeof(struct DirEntry));
            *(entry->filename)='\0';
        }

        entry++;
    }

    block_write(root_directory_index, utilities_buffer);

    return 0;
}

void print_allocated_blocks(struct fdNode* fd){
    size_t current_block=fd->first_data_block;

    printf("Allocated blocks for %s:\n",fd->filename);

    while(current_block!=FAT_EOC){
        printf("%zu ",current_block);

        current_block = get_fat_entry(current_block);
    }

    printf("\n");
}

void hex_dump_file(struct fdNode* fd){
    size_t current_block=fd->first_data_block;

    if(utilities_buffer==NULL){
        utilities_buffer = (uint8_t*)calloc(1, bounce_buffer_size);
    } else {
       memset(utilities_buffer,0,bounce_buffer_size);
    }

    while(current_block!=FAT_EOC){
        block_read(get_actual_block_index(current_block),utilities_buffer);

        hex_dump(utilities_buffer,bounce_buffer_size);
        printf("\n-----------------------\n");

        memset(utilities_buffer,0,bounce_buffer_size);

        current_block = get_fat_entry(current_block);
    }
}

void hex_dump(void *data, size_t length) {
    unsigned char *byte = (unsigned char *)data;

    for (size_t i = 0; i < length; ++i) {
        if (i % 16 == 0){
            printf("%08zx  ", i);
        }

        printf("%02x ", byte[i]);

        if ((i + 1) % 8 == 0){
            printf(" ");
        }

        if ((i + 1) % 16 == 0 || i + 1 == length) {
            size_t bytes_missing = 16 - ((i + 1) % 16);

            if (bytes_missing < 16) {

                for (size_t j = 0; j < bytes_missing; ++j){
                    printf("   ");
                }
                if (bytes_missing >= 8){
                    printf("  ");
                }
            }

            printf(" |");

            for (size_t j = i - (i % 16); j <= i; j++){

                printf("%c", (byte[j] >= 32 && byte[j] <= 126) ? byte[j] : '.');
            }

            printf("|\n");
        }
    }
}

