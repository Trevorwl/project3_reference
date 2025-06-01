#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"
#include "utilities.h"

int main(int argc,char** argv){
    size_t REQUESTED_BLOCKS = 100;
    size_t DATA_BLOCKS = REQUESTED_BLOCKS - 1;

    size_t* file_sizes = (size_t*)calloc(1,sizeof(size_t) * FS_FILE_MAX_COUNT);

    size_t files = 0;

    size_t blocks_filled = 0;

    bool filled_disk = false;

    srand(time(0));

    while(filled_disk == false){

        files = 0;
        blocks_filled = 0;

        for(size_t file = 0; file < FS_FILE_MAX_COUNT; file++){

            size_t write_bytes =  (size_t)1 + (size_t)(rand() % (BLOCK_SIZE * 7));

            size_t required_blocks = total_block_size(write_bytes);

            if(blocks_filled + required_blocks <= DATA_BLOCKS){
                file_sizes[files++] = write_bytes;

            } else {
                if(DATA_BLOCKS - blocks_filled > 0){
                    write_bytes = (DATA_BLOCKS - blocks_filled) * BLOCK_SIZE;

                    file_sizes[files++] = write_bytes;
                }

                filled_disk = true;
                break;
            }


            blocks_filled += required_blocks;
        }
    }

    unlink("fa_test1.fs");
    create_disk(REQUESTED_BLOCKS,"fa_test1.fs");
    fs_mount("fa_test1.fs");

    printf("------filling disk------\n\n");

    char name[32];

    for(size_t file = 0; file < files; file++){

        size_t write_bytes = file_sizes[file];

        uint8_t* buf = (uint8_t*)calloc(1, write_bytes);
        memset(buf, '0' + rand() % 10, write_bytes);

        memset(name, 0, 32 * sizeof(char));
        sprintf(name,"file%zu.txt",file);

        fs_create(name);
        int fd = fs_open(name);

        fs_write(fd,buf,write_bytes);

        struct fdNode* fdNode = getFdEntry(fd_table,fd);

        print_allocated_blocks(fdNode);
        printf("\n");

        fs_close(fd);

        free(buf);
    }

    free(file_sizes);

    size_t new_block=0;

    if(first_block_available(&new_block)!=false){
        printf("error: disk not written to properly\n");

        fs_umount();
        unlink("fa_test1.fs");
        exit(EXIT_FAILURE);
    }

    printf("----closing random files----\n\n");

    for(size_t file = 0; file < files; file++){
        if(file % (rand() % 10 + 1) == 0){
            memset(name, 0, 32 * sizeof(char));
            sprintf(name,"file%zu.txt",file);

            int fd=fs_open(name);

            struct fdNode* fdNode=getFdEntry(fd_table,fd);

            print_allocated_blocks(fdNode);
            printf("\n");

            fs_close(fd);

            fs_delete(name);
        }
    }

    printf("----allocating new file----\n\n");

    memset(name, 0, 32 * sizeof(char));
    sprintf(name,"file129.txt");

    fs_create(name);

    size_t writeBytes = free_blocks() * BLOCK_SIZE;

    uint8_t* buf=(uint8_t*)calloc(1, writeBytes);
    memset(buf, '0' + rand() % 10, writeBytes);

    int fd=fs_open(name);

    fs_write(fd,buf,writeBytes);

    struct fdNode* fdNode = getFdEntry(fd_table,fd);

    print_allocated_blocks(fdNode);
    printf("\n");

    fs_close(fd);

    free(buf);

    fs_umount();
    unlink("fa_test1.fs");
}
