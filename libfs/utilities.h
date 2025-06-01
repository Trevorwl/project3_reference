
#ifndef UTILITIES_H_
#define UTILITIES_H_

#include "disk.h"
#include "fs.h"
#include "fdTable.h"

extern uint8_t* utilities_buffer;

int erase_all_files();

void print_allocated_blocks(struct fdNode* fd);

void hex_dump_file(struct fdNode* fd);

void hex_dump(void *data, size_t length);

int create_disk(size_t data_blocks,char* filename);

size_t free_blocks();


#endif
