#include "private.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <mach-o/fat.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>

typedef struct {
    int32_t cputype;
    char *name;
} arch_name_t;

static const arch_name_t cpu_archs[] = {
    {CPU_TYPE_X86_64, "x86_64"},
    {CPU_TYPE_ARM64, "arm64"}
};

/* (g)lobals */
static int32_t g_searched_arch = 0;

static char *arch2str(int32_t arch) {
    for (int i = 0; i < ARRAY_LEN(cpu_archs); i++) {
        if (arch == cpu_archs[i].cputype)
            return cpu_archs[i].name;
    }
    return NULL;
}

int find_symbol(FILE *fp, int offset, int32_t cputype, patch_off_t *poffs) {
    int found = 0;
    if (o_patch_arch == 0 || (cputype & o_patch_arch) == cputype) {
        g_searched_arch |= cputype;
        fseek(fp, offset, SEEK_SET);
        if (lookup_symbol_macho(fp, o_symbol, poffs))
            found = 1;
        else
            fprintf(stderr, "symbol not found for arch '%s'!\n", arch2str(cputype));
    }
    return found;
}

int patch_file(FILE* fp, patch_off_t poff) {
    const data_t *final_patch = &o_patch_data;
    if (o_use_builtin_patch) {
        if (poff.cputype == CPU_TYPE_X86_64)
            final_patch = &builtin_patches[o_builtin_idx].x86_64_p;
        else if (poff.cputype == CPU_TYPE_ARM64)
            final_patch = &builtin_patches[o_builtin_idx].arm64_p;
        else {
            fprintf(stderr, "symp: unknown arch in patch_off_t!\n");
            return 1;
        }
    }
    if (poff.maxplen != 0 && final_patch->len > poff.maxplen) {
        fprintf(stderr, "symp: patch length(%zu) exceeded! (max %d)\n", final_patch->len, poff.maxplen);
        return 1;
    }
    fseek(fp, poff.fileoff, SEEK_SET);
    if (fwrite(final_patch->buf, final_patch->len, 1, fp) != 1) {
        perror("fwrite");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int error = 0;

    if (parse_arguments(argc, argv) != 0)
        return 1;
    if (o_mode == USAGE_MODE)
        return 0; /* already printed */

    int npoffs = 0;
    patch_off_t poffs[2]; /* only two archs are supported currently */

    char *fmode = "rb";
    if (o_mode == PATCH_MODE)
        fmode = "rb+";
    FILE *fp = fopen(o_file, fmode);
    if (fp == NULL) {
        perror("fopen");
        return 1;
    }

    uint32_t file_magic;
    fread(&file_magic, sizeof(uint32_t), 1, fp);
    switch(file_magic) {
    case MH_MAGIC_64: { /* 64-bit Mach-O file */
        int32_t cputype;
        fread(&cputype, sizeof(int32_t), 1, fp);
        npoffs += find_symbol(fp, 0, cputype, poffs);
        break;
    }
    case FAT_CIGAM: { /* FAT file (on little-endian host CPU) */
        uint32_t nfat_arch;
        fread(&nfat_arch, sizeof(int32_t), 1, fp);
        nfat_arch = OSSwapInt32(nfat_arch);
        size_t total_size = nfat_arch * sizeof(struct fat_arch);
        struct fat_arch *archs = read_file(fp, total_size);
        for (int i = 0; i < nfat_arch; i++) {
            const int32_t cputype = OSSwapInt32(archs[i].cputype);
            const int32_t offset = OSSwapInt32(archs[i].offset);
            npoffs += find_symbol(fp, offset, cputype, poffs + npoffs);
        }
        free(archs);
        break;
    }
    default:
        fprintf(stderr, "symp: not a valid Mach-O or FAT file\n");
        goto err_ret;
    }

    /* offered arch option but some arch is missing.. */
    if (o_patch_arch != 0 && g_searched_arch != o_patch_arch) {
        error = 1;
        int32_t unsearched_arch = o_patch_arch ^ g_searched_arch;
        for (int i = 0; i < ARRAY_LEN(cpu_archs); i++) {
            if (unsearched_arch & cpu_archs[i].cputype)
                fprintf(stderr, "symp: offered arch '%s' not found in the file\n", cpu_archs[i].name);
        }
        goto err_ret;
    }

    if (npoffs == 0) {
        error = 1;
        printf("no matches found!\n");
        goto err_ret;
    }

    if (o_mode == LOOKUP_MODE) {
        for (int i = 0; i < npoffs; i++) {
            printf("0x%lx\n", poffs[i].fileoff);
        }
        if (npoffs == 1)
            printf("1 match found\n");
        else
            printf("%d matches found\n", npoffs);

    }
    else if (o_mode == PATCH_MODE) {
        int patched = 0;
        for (; patched < npoffs; patched++) {
            if (patch_file(fp, poffs[patched]) != 0) {
                error = 1;
                break;
            }
        }
        if (patched == 1)
            printf("1(%d) match patched\n", npoffs);
        else {
            fprintf(stderr, "symp: warning, multiple arches used the same patch\n");
            printf("%d(%d) matches patched\n", patched, npoffs);
        }
    }
    else {
        error = 1;
        fprintf(stderr, "symp: unknown working mode!\n");
    }

err_ret:
    fclose(fp);
    free(o_patch_data.buf);
    return error;
}
