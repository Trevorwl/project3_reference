
#ifndef FDTABLE_H_
#define FDTABLE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "fs.h"
#include "disk.h"

/** Maximum filename length (including the NULL character) */
#ifndef FS_FILENAME_LEN
#define FS_FILENAME_LEN 16
#endif

/** Maximum number of files in the root directory */
#ifndef FS_FILE_MAX_COUNT
#define FS_FILE_MAX_COUNT 128
#endif


/** Maximum number of open files */
#ifndef FS_OPEN_MAX_COUNT
#define FS_OPEN_MAX_COUNT 32
#endif

/*
 * fdNode is a data structure that represents a fd entry.
 */
struct fdNode{
    /*
     * Whether is fdNode is in use
     */
    bool in_use;
    char* filename;

    /*
     * The directory number is the index of the file in the directory
     */
    size_t dir_entry_index;

    size_t size;
    size_t offset;

    /*
     * The first data block index for the file corresponding to this fd
     */
    size_t first_data_block;
};

struct fdTable{
    struct fdNode* fdTable[FS_OPEN_MAX_COUNT];

   /*
    * Number of slots in fdTable occupied
    */
    size_t fdsOccupied;
};

struct fdNode* getFdEntry(struct fdTable*,int fd);
bool isOpenByFd(struct fdTable*,int fd);

bool isOpenByName(struct fdTable* fdTable,char* filename);

bool table_is_full(struct fdTable*);

struct fdTable* fd_table_constructor();
void fdTable_destructor(struct fdTable*);

struct fdNode* fdNode_constructor();
void fdNode_destructor(struct fdNode*);

int addFd(struct fdTable*,char* filename);
void removeFd(struct fdTable*,int fd);


#endif
