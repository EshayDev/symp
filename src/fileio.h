#ifndef FILEIO_H
#define FILEIO_H

#include <stdio.h>

void *read_file(FILE *fp, const size_t len);

void *read_file_off(FILE *fp, const size_t len, const long int offset);

#endif