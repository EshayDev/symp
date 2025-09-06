#ifndef SYM_PRIVATE
#define SYM_PRIVATE

#include <stdio.h>
#include <stdint.h>

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

/* 
 * defined in macho.c
 * fp -> start of macho file 
 */
macho_info_t *parse_macho(FILE *fp);

/* defined in exports.c */
uint64_t trie_query(const uint8_t *export, const char *name);

#endif