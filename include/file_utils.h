#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdio.h>
#include <sys/types.h>

off_t find_file_size(FILE *fp);
char *get_content_type(const char *filename);

#endif
