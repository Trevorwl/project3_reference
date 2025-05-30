#include "fdTable.h"
#include <stdlib.h>
#include "fs.h"
#include "disk.h"
#include <stdbool.h>
#include <string.h>

bool isOpenByName(struct fdTable* fdTable,char* filename){
    if(fdTable==NULL || filename==NULL || fdTable->fdsOccupied==0){
        return false;
    }

    for(int i=0;i< FS_OPEN_MAX_COUNT;i++){
        struct fdNode* fdNode=fdTable->fdTable[i];

        if(fdNode->in_use==true && strcmp(fdNode->filename,filename)==0){
            return true;
        }
    }

    return false;
}

bool isOpenByFd(struct fdTable* fdTable,int fd){
    struct fdNode* fdNode=fdTable->fdTable[fd];

    return fdNode->in_use==true;
}

bool table_is_full(struct fdTable* fdTable){
    return fdTable->fdsOccupied==FS_OPEN_MAX_COUNT;
}

struct fdNode* getFdEntry(struct fdTable* fdTable,int fd){
    struct fdNode* fdNode=fdTable->fdTable[fd];

    return fdNode;
}

struct fdTable* fd_table_constructor(){
    struct fdTable* fdTable=(struct fdTable*)calloc(1,sizeof(struct fdTable));

    fdTable->fdsOccupied=0;

    for(int entry=0;entry<FS_OPEN_MAX_COUNT;entry++){
        fdTable->fdTable[entry] = fdNode_constructor();
    }

    return fdTable;
}

void fdTable_destructor(struct fdTable* fdTable){
    if(!fdTable){
        return;
    }

    for(int i=0;i<FS_OPEN_MAX_COUNT;i++){
        fdNode_destructor(fdTable->fdTable[i]);
        fdTable->fdTable[i]=NULL;
    }

    free(fdTable);
}

struct fdNode* fdNode_constructor(){
    struct fdNode* fdNode=(struct fdNode*)calloc(1,sizeof(struct fdNode));
    fdNode->in_use=false;
    fdNode->filename=NULL;
    fdNode->dir_entry_index=0;

    fdNode->size=0;
    fdNode->offset=0;
    fdNode->first_data_block = FAT_EOC;

    return fdNode;
}

void fdNode_destructor(struct fdNode* fdNode){
    if(!fdNode){
        return;
    }

    if(fdNode->filename!=NULL){
        free(fdNode->filename);
        fdNode->filename=NULL;
    }

    free(fdNode);
}

int addFd(struct fdTable* fdTable,char* filename){
    if(fdTable==NULL || filename==NULL
            || fdTable->fdsOccupied == FS_OPEN_MAX_COUNT){

        return -1;
    }

    for(int fd = 0;fd < FS_OPEN_MAX_COUNT; fd++){

        struct fdNode* fdNode=fdTable->fdTable[fd];

        if(fdNode->in_use==false){

            fdNode->in_use=true;
            fdNode->filename = strdup(filename);
            fdTable->fdsOccupied++;
            return fd;
        }
    }

    return -1;
}

void removeFd(struct fdTable* fdTable,int fd){
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT){
        return;
    }

    struct fdNode* fdNode=fdTable->fdTable[fd];

    if (fdNode->in_use==false){
        return;
    }

    fdNode->in_use=false;
    free(fdNode->filename);
    fdNode->filename=NULL;

    fdNode->size=0;
    fdNode->offset=0;
    fdNode->first_data_block=FAT_EOC;
    fdNode->dir_entry_index=0;

    fdTable->fdsOccupied--;
}








