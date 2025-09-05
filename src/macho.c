#include "private.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <mach-o/nlist.h>
#include <mach-o/loader.h>

typedef enum {
    HEX_OFFSET, REGULAR_SYMBOL, OBJC_SYMBOL
} sym_type_t;

typedef struct {
    int32_t cputype;
    uint64_t base_offset;

    /* __TEXT vm slide */
    uint64_t vm_slide;

    /* from LC_SYMTAB */
    uint32_t symoff;
    uint32_t nsyms;
    uint32_t stroff;
    uint32_t strsize;

    /* from LC_DYLD_INFO(_ONLY) or LC_DYLD_EXPORTS_TRIE */
    uint32_t export_off;
    uint32_t export_size;

    /* from LC_DYSYMTAB */
    uint32_t indirectsymoff;

    /* from S_SYMBOL_STUBS section */
    uint32_t stubs_off;
    uint64_t stubs_size;
    uint32_t indirectsym_idx;
    uint32_t stub_len;

} macho_info_t;

static sym_type_t get_sym_type(const char *symbol_name) {
    size_t len = strlen(symbol_name);
    if (symbol_name[0] == '0' &&
        (symbol_name[1] == 'x' || symbol_name[1] == 'X')) {
        int i = 1;
        while (symbol_name[++i] == '0'); /* remove 0 prefix */
        if (len - i <= 16) {
            for (char c = symbol_name[i]; c; c = symbol_name[++i]) {
                if (!(c >= '0' && c <= '9') && !(c >= 'A' && c <= 'F') && !(c >= 'a' && c <= 'f')) {
                    i = -1;
                    fprintf(stderr, "symp: warning, invalid char '%c' in hex number, treated as string\n", c);
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
            fprintf(stderr, "symp: warning, objc symbol should use 1 space to seperate cls and sel, treated as string\n");
        }
    }
    return REGULAR_SYMBOL;
}

/* convert a VALID uint64 hex str to num */
uint64_t str2uint64(const char* str) {
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

/* fp -> start of macho file */
static macho_info_t *parse_macho(FILE *fp) {
    macho_info_t *macho_info = malloc(sizeof(macho_info_t));
    memset(macho_info, 0, sizeof(macho_info_t));
    macho_info->base_offset = ftell(fp);

    uint64_t text_vm_slide = 0;
    const struct mach_header_64 *header = read_file(fp, sizeof(struct mach_header_64));
    const struct load_command* commands = read_file(fp, header->sizeofcmds);
    const struct load_command* command = commands;
    const struct symtab_command* symtab_cmd = NULL;
    const struct dysymtab_command *dysymtab_cmd = NULL;
    const struct dyld_info_command *dyldinfo_cmd = NULL;
    const struct linkedit_data_command *export_trie = NULL;
    const struct section_64 *symbol_stubs_sect = NULL;

    for (int i = 0; i < header->ncmds; i++) {
        switch(command->cmd) {
        case LC_SEGMENT_64: {
            const struct segment_command_64 *seg_cmd = (void *)command;
            if (strcmp(seg_cmd->segname, "__TEXT") == 0) {
                /* addr_vm - text_vm = addr_file - text_file */
                text_vm_slide = seg_cmd->fileoff - seg_cmd->vmaddr;

                const struct section_64 *text_sect = (void *)(seg_cmd + 1);
                for (int j = 0; j < seg_cmd->nsects; j++) {
                    if ((text_sect[j].flags & SECTION_TYPE) == S_SYMBOL_STUBS) {
                        symbol_stubs_sect = text_sect + j;
                        break;
                    }
                }
            }
            break;
        }
        case LC_SYMTAB: 
            symtab_cmd = (struct symtab_command*)command;
            break;
        case LC_DYSYMTAB:
            dysymtab_cmd = (struct dysymtab_command *)command;
            break;
        case LC_DYLD_INFO:
        case LC_DYLD_INFO_ONLY:
            dyldinfo_cmd = (struct dyld_info_command *)command;
            break;
        case LC_DYLD_EXPORTS_TRIE:
            export_trie = (struct linkedit_data_command *)command;
            break;
        default:
            break;
        }
        command = (void*)command + command->cmdsize;
    }

    macho_info->cputype = header->cputype;
    macho_info->vm_slide = text_vm_slide;

    free((void *)header);

    if (symtab_cmd != NULL) {
        macho_info->symoff = symtab_cmd->symoff;
        macho_info->nsyms = symtab_cmd->nsyms;
        macho_info->stroff = symtab_cmd->stroff;
        macho_info->strsize = symtab_cmd->strsize;
    }

    if (dyldinfo_cmd != NULL) {
        macho_info->export_off = dyldinfo_cmd->export_off;
        macho_info->export_size = dyldinfo_cmd->export_size;
    }
    else if (export_trie != NULL) {
        macho_info->export_off = export_trie->dataoff;
        macho_info->export_size = export_trie->datasize;
    }

    if (dysymtab_cmd != NULL) {
        macho_info->indirectsymoff = dysymtab_cmd->indirectsymoff;
    }

    if (symbol_stubs_sect != NULL) {
        macho_info->stubs_off = symbol_stubs_sect->offset;
        macho_info->stubs_size = symbol_stubs_sect->size;
        macho_info->indirectsym_idx = symbol_stubs_sect->reserved1;
        macho_info->stub_len = symbol_stubs_sect->reserved2;
    }

    free((void *)commands);
    return macho_info;
}

static uint64_t read_uleb128(const uint8_t **p) {
    int bit = 0;
    uint64_t result = 0;
    do {
        uint64_t slice = **p & 0x7f;
        result |= (slice << bit);
        bit += 7;
    } while (*(*p)++ & 0x80);
    return result;
}

static uint64_t trie_query(const uint8_t *export, const char *name) {
    // documents in <mach-o/loader.h>
    uint64_t symbol_address = 0;
    uint64_t node_off = 0;
    const char *rest_name = name;
    bool go_child = true;
    while (go_child) {
        const uint8_t *cur_pos = export + node_off;
        uint64_t info_len = read_uleb128(&cur_pos);
        const uint8_t *child_off = cur_pos + info_len;
        if (rest_name[0] == '\0') {
            if (info_len != 0) {
                uint64_t flag = read_uleb128(&cur_pos);
                if (flag == EXPORT_SYMBOL_FLAGS_KIND_REGULAR) {
                    symbol_address = read_uleb128(&cur_pos);
                }
            }
            break;
        }
        else {
            go_child = false;
            cur_pos = child_off;
            uint8_t child_count = *(uint8_t *)cur_pos++;
            for (int i = 0; i < child_count; i++) {
                char *cur_str = (char *)cur_pos;
                size_t cur_len = strlen(cur_str);
                cur_pos += cur_len + 1;
                uint64_t next_off = read_uleb128(&cur_pos);
                if (strncmp(rest_name, cur_str, cur_len) == 0) {
                    /* this edge matched the symbol */
                    go_child = true;
                    rest_name += cur_len;
                    node_off = next_off;
                    break;
                }
            }
        }
    }
    return symbol_address;
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

/* 
 * return true if found
 * update fileoff and maxplen of the patch_off_t
 * fp -> start of macho file
 */
bool lookup_symbol_macho(FILE *fp, const char *symbol_name, patch_off_t *poffout) {
    bool found = false;
    long symbol_address = 0;
    const macho_info_t *macho_info = parse_macho(fp);

    switch(get_sym_type(symbol_name)) {
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
