#include "private.h"
#include "../fileio.h"

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

/* copied from dyld source code */
#define ISA_MASK 0x7fffffffffffULL
#define FAST_DATA_MASK 0xfffffffcUL

struct objc_class_t {
    uint64_t isaVMAddr;
    uint64_t superclassVMAddr;
    uint64_t methodCacheBuckets;
    uint64_t methodCacheProperties;
    uint64_t dataVMAddrAndFastFlags;
};

struct class_ro_t {
    uint32_t flags;
    uint32_t instanceStart;
    union {
        uint32_t   instanceSize;
        uint64_t   pad;
    } instanceSize;
    uint64_t ivarLayoutVMAddr;
    uint64_t nameVMAddr;
    uint64_t baseMethodsVMAddr;
    uint64_t baseProtocolsVMAddr;
    uint64_t ivarsVMAddr;
    uint64_t weakIvarLayoutVMAddr;
    uint64_t basePropertiesVMAddr;
};

struct method_list_t {
    uint32_t entsize;
    uint32_t count;
};

struct method_t {
    uint64_t nameVMAddr;   // SEL
    uint64_t typesVMAddr;  // const char *
    uint64_t impVMAddr;    // IMP
};

struct relative_method_t {
    int32_t nameOffset;   // SEL*
    int32_t typesOffset;  // const char *
    int32_t impOffset;    // IMP
};

/* self-defined */
struct sects_off_t {
    uint32_t sect_off[2];
    const void *sect_data[2];
};

const void *auto_add(const struct sects_off_t *sects, uint64_t offset) {
    const uint32_t *sect_off = sects->sect_off;
    int greater = sect_off[0] < sect_off[1] ? 1 : 0;
    if (offset >= sect_off[greater])
        return offset + sects->sect_data[greater];
    else
        return offset + sects->sect_data[1 - greater];
}

long solve_objc_symbol(FILE *fp, const macho_objc_info_t *macho_info, const char* symbol_name) {
    uint64_t symbol_address = 0;
    const long base_offset = macho_info->base_offset;

    if (macho_info->cstring_off        == 0 || 
        macho_info->const_off          == 0 ||
        macho_info->objc_classlist_off == 0 ||
        macho_info->objc_data_off      == 0 ||
        macho_info->objc_const_off     == 0 ||
        macho_info->objc_methlist_off  == 0 ||
        macho_info->objc_methname_off  == 0 ||
        macho_info->objc_selrefs_off   == 0) {
        fprintf(stderr, "symp: incomplete objc info sections!\n");
        return 0;
    }

    void *objc_methlist_buf  = read_file_off(fp, macho_info->objc_methlist_size, base_offset + macho_info->objc_methlist_off);
    void *cstring_buf        = read_file_off(fp, macho_info->cstring_size, base_offset + macho_info->cstring_off);
    void *cdata_buf          = read_file_off(fp, macho_info->cdata_size, base_offset + macho_info->cdata_off);
    void *const_buf          = read_file_off(fp, macho_info->const_size, base_offset + macho_info->const_off);
    void *objc_methname_buf  = read_file_off(fp, macho_info->objc_methname_size, base_offset + macho_info->objc_methname_off);
    void *objc_classlist_buf = read_file_off(fp, macho_info->objc_classlist_size, base_offset + macho_info->objc_classlist_off);
    void *objc_selrefs_buf   = read_file_off(fp, macho_info->objc_selrefs_size, base_offset + macho_info->objc_selrefs_off);
    void *objc_data_buf      = read_file_off(fp, macho_info->objc_data_size, base_offset + macho_info->objc_data_off);
    void *objc_const_buf     = read_file_off(fp, macho_info->objc_const_size, base_offset + macho_info->objc_const_off);

    const void     *objc_methlist  = objc_methlist_buf - macho_info->objc_methlist_off;
    const char     *cdata          = cdata_buf - macho_info->cdata_off;
    const char     *cstring        = cstring_buf - macho_info->cstring_off;
    const char     *const_data     = const_buf - macho_info->const_off;
    const char     *objc_methname  = objc_methname_buf - macho_info->objc_methname_off;
    const void     *objc_selrefs   = objc_selrefs_buf - macho_info->objc_selrefs_off;
    const void     *objc_data      = objc_data_buf - macho_info->objc_data_off;
    const void     *objc_const     = objc_const_buf - macho_info->objc_const_off;
    const uint64_t *objc_classlist = objc_classlist_buf;

    const struct sects_off_t string_sects = {{macho_info->cstring_off, macho_info->const_off}, {cstring, const_data}}; /* and __objc_classname */
    const struct sects_off_t objc_class_sects = {{macho_info->objc_data_off, macho_info->cdata_off}, {objc_data, cdata}};
    const struct sects_off_t objc_data_sects = {{macho_info->objc_data_off, macho_info->objc_const_off}, {objc_data, objc_const}};

    uint64_t nclasses = macho_info->objc_classlist_size / sizeof(uint64_t);
    for (int i = 0; i < nclasses; i++) {
        const struct objc_class_t *objc_cls = auto_add(&objc_class_sects, objc_classlist[i] & ISA_MASK);
        const struct class_ro_t *class_data = auto_add(&objc_data_sects, objc_cls->dataVMAddrAndFastFlags & FAST_DATA_MASK);
        const char *class_name = auto_add(&string_sects, class_data->nameVMAddr & ISA_MASK);
        if ((class_data->baseMethodsVMAddr) != 0) {
            const struct method_list_t *method_list = objc_methlist + (class_data->baseMethodsVMAddr & ISA_MASK);
            uint32_t entsize = method_list->entsize & 0x0000FFFC; /* methodListSizeMask */
            const void *cur_method = (void *)(method_list + 1);
            for (int j = 0; j < method_list->count; j++) {
                uint64_t method_sel_off = 0;
                uint64_t method_imp_off = 0;
                if ((method_list->entsize & 0x80000000) != 0) { /* usesRelativeOffsets */
                    const struct relative_method_t *rel_method = cur_method;
                    method_sel_off = (uint64_t)rel_method - (uint64_t)objc_methlist + offsetof(struct relative_method_t, nameOffset) + rel_method->nameOffset;
                    method_imp_off = (uint64_t)rel_method - (uint64_t)objc_methlist + offsetof(struct relative_method_t, impOffset) + rel_method->impOffset;
                }
                else {
                    const struct method_t *method = cur_method;
                    method_sel_off = method->nameVMAddr & ISA_MASK;
                    method_imp_off = method->impVMAddr & ISA_MASK;
                }
                const uint64_t *method_sel = objc_selrefs + method_sel_off;
                const char *method_name = objc_methname + (*method_sel & ISA_MASK);
                cur_method += entsize;
                printf("[%s %s] 0x%llx\n", class_name, method_name, method_imp_off);
            }
        }
    }

    free(objc_methlist_buf);
    free(cstring_buf);
    free(const_buf);
    free(objc_methname_buf);
    free(objc_classlist_buf);
    free(objc_selrefs_buf);
    free(objc_data_buf);
    free(objc_const_buf);

    return (long)symbol_address;
}
