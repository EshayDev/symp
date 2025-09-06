#include "private.h"
#include "../fileio.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <mach-o/loader.h>

macho_info_t *parse_macho(FILE *fp) {
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
            if (strcmp(seg_cmd->segname, SEG_TEXT) == 0) { /* __TEXT */
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
