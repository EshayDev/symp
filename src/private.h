#ifndef SYMP_PRIVATE_H
#define SYMP_PRIVATE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef enum {
	USAGE_MODE,
	LOOKUP_MODE,
	PATCH_MODE
} work_mode_t;

typedef struct {
	size_t len;
    uint8_t *buf;
} data_t;

typedef struct {
	char *name;
	data_t x86_64_p, arm64_p;
} builtin_patch_t;

typedef struct {
    int cputype;
    int maxplen;  /* max patch lenth */
    long fileoff;
} patch_off_t;

/* defined in builtin.c */
extern builtin_patch_t builtin_patches[];
extern int builtin_patches_count;

/* (o)ptions, defined in cli.c */
extern work_mode_t o_mode;
extern char *o_symbol, *o_file;
extern int o_patch_arch;
extern data_t o_patch_data;
extern bool o_use_builtin_patch;
extern int o_builtin_idx;

int parse_arguments(int argc, char **argv);

/* defined in fileio.c */
void *read_file(FILE *fp, size_t len);
void *read_file_off(FILE *fp, size_t len, long offset);

/* defined in macho.c */
bool lookup_symbol_macho(FILE *fp, const char *symbol_name, patch_off_t *poffout);

#endif