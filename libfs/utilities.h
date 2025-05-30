
#ifndef UTILITIES_H_
#define UTILITIES_H_

#include "disk.h"
#include "fs.h"
#include "fdTable.h"

int erase_all_files();

void print_allocated_blocks(struct fdNode* fd);

void hex_dump_file(struct fdNode* fd);

void hex_dump(void *data, size_t length);


#endif
