#include "fs.h"
#include "fdTable.h"
#include "disk.h"
#include "utilities.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

void do_sequential_writes();
void do_long_write();
void erase_files();

int main(int argc, char** argv){

    char* command=argv[1];

    fs_mount("disk.fs");

    if(strcmp(command,"long_write")==0){
        do_long_write();
    }

    if(strcmp(command,"1_to_9999")==0){
        do_sequential_writes();
    }

    if(strcmp(command,"erase")==0){
        erase_files();
    }

    fs_umount();
}

void erase_files(){

     erase_all_files();
}

//void test_fat_allocation(){
//    if(disk_mounted==false){
//       fs_mount("disk.fs");
//    }
//
//    erase_all_files();
//
//
//}


//prints from 1 to 9999
void do_sequential_writes(){

    fs_delete("1_to_9999");
    fs_create("1_to_9999");

    int fd=fs_open("1_to_9999");

    for(int i=0;i<10000;i++){
        char c[32];

        sprintf(c,"%d\n",i);

        fs_write(fd,c,strlen(c));
    }


    fs_lseek(fd,0);

    size_t stat=fs_stat(fd);
    char* buf=(char*)calloc(1,stat + 1);

    buf[stat]=0;

    fs_read(fd,buf,stat);

    printf("%s",buf);
    printf("\n");

    struct fdNode* fdNode=getFdEntry(fd_table,fd);

    print_allocated_blocks(fdNode);

    fs_close(fd);
}

void do_long_write(){

     size_t bufLength = 48894;  //bytes required for write

     char* buf = (char*)calloc(1, bufLength);

     size_t offset = 0;

     char storeNum[32];

     for(int i = 1; i < 10001; i++){

         sprintf(storeNum, "%d\n", i);

         //we will write number and \n without null terminator
         int length = strlen(storeNum);

         memcpy(&buf[offset], storeNum, length);

         offset += length;
     }

      fs_delete("long_write");
      fs_create("long_write");

      int fd=fs_open("long_write");

      fs_write(fd,buf,bufLength);

      struct fdNode* fdNode=getFdEntry(fd_table,fd);

      hex_dump_file(fdNode);

      print_allocated_blocks(fdNode);

      fs_close(fd);
}
