#include "resolve.h"
#include "private.h"
#include "../fileio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <mach-o/nlist.h>
#include <mach-o/loader.h>

typedef enum {
    HEX_OFFSET, REGULAR_SYMBOL, OBJC_SYMBOL
} sym_type_t;

static sym_type_t determine_type(const char *symbol_name) {
    size_t len = strlen(symbol_name);
    if (symbol_name[0] == '0' &&
        (symbol_name[1] == 'x' || symbol_name[1] == 'X')) {
        int i = 1;
        while (symbol_name[++i] == '0'); /* remove 0 prefix */
        if (len - i <= 16) {
            for (char c = symbol_name[i]; c; c = symbol_name[++i]) {
                if (!(c >= '0' && c <= '9') && !(c >= 'A' && c <= 'F') && !(c >= 'a' && c <= 'f')) {
                    i = -1;
                    fprintf(stderr, "symp: warning, invalid char '%c' in hex number, treated as regular symbol\n", c);
                    break;
                }
            }
            if (i != -1) return HEX_OFFSET;
        }
    }
    if ((symbol_name[0] == '+' || symbol_name[0] == '-') &&
        (symbol_name[1] == '[' && symbol_name[len - 1] == ']')) {
        int space_cnt = 0;
        for (int i = 2; i < len; i++) {
            if (symbol_name[i] == ' ') space_cnt++;
        }
        if (space_cnt == 1) return OBJC_SYMBOL;
        else {
            fprintf(stderr, "symp: warning, objc symbol should use 1 space to seperate cls and sel, treated as regular symbol\n");
        }
    }
    return REGULAR_SYMBOL;
}

/* convert a VALID uint64 hex str to num */
static uint64_t str2uint64(const char* str) {
    int i = 1;
    uint64_t num = 0;
    while (str[++i] == '0'); /* remove 0 prefix */
    /* assume the length is valid too */
    for (char c = str[i]; c; c = str[++i]) {
        num <<= 4;
        if (c >= '0' && c <= '9') num |= c - '0';
        else if (c >= 'A' && c <= 'F') num |= c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') num |= c - 'a' + 10;
        /* assume all the chars are valid */
    }
    return num;
}

static long int solve_symbol(FILE *fp, const macho_info_t *macho_info, const char* symbol_name) {
    uint64_t symbol_address = 0;
    const long int base_offset = macho_info->base_offset;

    if (macho_info->export_off != 0) {
        /* export table search */
        uint8_t *export_trie = read_file_off(fp, macho_info->export_size, base_offset + macho_info->export_off);
        symbol_address = trie_query(export_trie, symbol_name);
        free(export_trie);
        if (symbol_address != 0) {
            /* trie value is the location from mach_header */
            symbol_address += base_offset;
            goto ret;
        }
    }

    /* these tables are both needed for symtab search and symbol stubs search */
    const struct nlist_64* nl_tbl = read_file_off(fp, macho_info->nsyms * sizeof(struct nlist_64), base_offset + macho_info->symoff);
    const char* str_tbl = read_file_off(fp, macho_info->strsize, base_offset + macho_info->stroff);

    if (macho_info->indirectsymoff != 0 && macho_info->stubs_off != 0) {
        /* symbol stubs search */
        uint32_t entry_off = macho_info->indirectsymoff + macho_info->indirectsym_idx * sizeof(uint32_t);
        uint64_t nstubs = macho_info->stubs_size / macho_info->stub_len;
        const uint32_t *indirectsym_entry = read_file_off(fp, nstubs * sizeof(uint32_t), base_offset + entry_off);
        for (int i = 0; i < nstubs; i++) {
            uint32_t nl_idx = indirectsym_entry[i];
            if (strcmp(symbol_name, str_tbl + nl_tbl[nl_idx].n_un.n_strx) == 0) {
                /* stubs_off is direct file offset */
                symbol_address = base_offset + macho_info->stubs_off + i * (uint64_t)macho_info->stub_len;
                break;
            }
        }
        free((void *)indirectsym_entry);
        if (symbol_address != 0)
            goto sym_ret;
    }

    if (macho_info->symoff != 0) {
        /* symtab search */
        for (int i = 0; i < macho_info->nsyms; i++) {
            if ((nl_tbl[i].n_type & N_TYPE) != N_SECT) 
                continue;
            if (strcmp(symbol_name, str_tbl + nl_tbl[i].n_un.n_strx) == 0) {
                /* n_value in nlist is the offset from vmaddr of the image */
                symbol_address = base_offset + macho_info->vm_slide + nl_tbl[i].n_value;
                goto sym_ret;
            }
        }
    }

sym_ret:
    free((void *)nl_tbl);
    free((void *)str_tbl);

ret:
    return (long)symbol_address;
}

bool lookup_symbol_macho(FILE *fp, const char *symbol_name, patch_off_t *poffout) {
    bool found = false;
    long symbol_address = 0;
    const macho_info_t *macho_info = parse_macho(fp);

    switch(determine_type(symbol_name)) {
    case HEX_OFFSET: {
        uint64_t vm_offset = str2uint64(symbol_name);
        symbol_address = vm_offset + macho_info->base_offset + macho_info->vm_slide;
        break;
    }
    case REGULAR_SYMBOL:
        symbol_address = solve_symbol(fp, macho_info, symbol_name);
        break;
    // case OBJC_SYMBOL:
    //     break;
    default:
        break;
    }
    if (symbol_address != 0) {
        found = true;
        poffout->cputype = macho_info->cputype;
        poffout->fileoff = symbol_address;
        poffout->maxplen = macho_info->stub_len;
    }
    free((void *)macho_info);
    return found;
}
