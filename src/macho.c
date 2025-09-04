#include "private.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <mach-o/nlist.h>
#include <mach-o/loader.h>

typedef enum {
    HEX_OFFSET, C_SYMBOL, OBJC_SYMBOL
} sym_type_t;

static sym_type_t determine_sym_type(const char *symbol_name) {
    return C_SYMBOL;
}

long int solve_c_symbol(FILE *fp, const char* symbol_name) {
    long int base_offset = ftell(fp);
    long int symbol_address = 0;
    long int text_vm_slide = 0;
    struct mach_header_64 *header = read_file(fp, sizeof(struct mach_header_64));
    struct load_command* commands = read_file(fp, header->sizeofcmds);
    struct load_command* command = commands;
    struct symtab_command* symtab_cmd = NULL;
    for (int i = 0; i < header->ncmds; i++) {
        if (command->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *seg_cmd = (void *)command;
            if (strcmp(seg_cmd->segname, "__TEXT") == 0) {
                /* addr_vm - text_vm = addr_file - text_file */
                text_vm_slide = seg_cmd->fileoff - seg_cmd->vmaddr;
            }
        }
        else if (command->cmd == LC_SYMTAB) {
            symtab_cmd = (struct symtab_command*)command;
            break;
        }
        command = (void*)command + command->cmdsize;
    }
    free(header);

    if (symtab_cmd == NULL) {
        free(commands);
        fprintf(stderr, "symp: LC_SYMTAB load command not found!\n");
        return 0;
    }

    struct nlist_64* nl_tbl = read_file_off(fp, symtab_cmd->nsyms * sizeof(struct nlist_64), base_offset + symtab_cmd->symoff);
    char* str_tbl = read_file_off(fp, symtab_cmd->strsize, base_offset + symtab_cmd->stroff);

    /* symtab search */
    for (int i = 0; i < symtab_cmd->nsyms; i++) {
        if (strcmp(symbol_name, str_tbl + nl_tbl[i].n_un.n_strx) == 0) {
            symbol_address = base_offset + nl_tbl[i].n_value;
            break;
        }
    }
    if (symbol_address != 0) {
        /* n_value in nlist is the offset from vmaddr of the image */
        symbol_address += text_vm_slide;
    }

    free(commands);
    free(nl_tbl);
    free(str_tbl);
    return symbol_address;
}

/* 
 * return true if found
 * fp -> start of macho file 
 */
bool lookup_symbol_macho(FILE *fp, const char *symbol_name, long int *fileoffout) {
    long int symbol_address = 0;
    switch(determine_sym_type(symbol_name)) {
    case HEX_OFFSET:
        break;
    case C_SYMBOL:
        symbol_address = solve_c_symbol(fp, symbol_name);
        break;
    case OBJC_SYMBOL:
        break;
    default:
        break;
    }
    if (symbol_address != 0) {
        *fileoffout = symbol_address;
        return true;
    }
    return false;
}
