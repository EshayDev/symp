#include "private.h"

#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <mach-o/fat.h>

work_mode_t o_mode = LOOKUP_MODE;
char *o_symbol, *o_file;
int o_patch_arch = 0;
data_t o_patch_data = {0, NULL};
bool o_use_builtin_patch = false;
int o_builtin_idx = -1;

static void usage() {
    puts("symp - a symbol patching tool");
    puts("usage: symp [options] -- <symbol> <file>");
    puts("options:");
    puts("  -a, --arch <arch>         arch of the binary to be patched, only x86_64 and arm64 are supported");
    puts("  -p, --patch <patch>       use builtin patches, available: ret, ret0, ret1, ret2");
    puts("  -b, --binary <binary>     use a binary file as patch");
    puts("  -x, --hex <hex string>    hex string of the patch");
}

int parse_arguments(int argc, char **argv) {
    if (argc <= 2) {
        usage();
        return 1;
    }

    size_t xlen = 0;
    uint8_t *xbuf = NULL;
    while(1) {
        static struct option long_options[] = {
            {"arch",   required_argument, 0, 'a'},
            {"patch",  required_argument, 0, 'p'},
            {"binary", required_argument, 0, 'b'},
            {"hex",    required_argument, 0, 'x'},
            {"help",   no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        int option_index = 0;
        int c = getopt_long(argc, argv, "a:p:b:x:h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'a':
            if (strcmp("x86_64", optarg) == 0)
                o_patch_arch |= CPU_TYPE_X86_64;
            else if (strcmp("arm64", optarg) == 0)
                o_patch_arch |= CPU_TYPE_ARM64;
            else {
                fprintf(stderr, "symp: unsupported arch %s\n", optarg);
                goto err;
            }
            break;
        case 'b':
            if (xbuf || o_use_builtin_patch) {
                fprintf(stderr, "symp: only one of -p/-b/-x should be offered\n");
                goto err;
            }
            FILE * bfp = fopen(optarg, "rb");
            if (bfp != 0) {
                fseek(bfp, 0, SEEK_END);
                xlen = ftell(bfp);
                xbuf = malloc(xlen);
                fread(xbuf, xlen, 1, bfp);
                fclose(bfp);
            }
            else {
                perror("fopen");
                goto err;
            }
            break;
        case 'p':
            if (xbuf || o_use_builtin_patch) {
                fprintf(stderr, "symp: only one of -p/-b/-x should be offered\n");
                goto err;
            }
            o_use_builtin_patch = true;
            for (int i = 0; i < builtin_patches_count; i++) {
                if (strcmp(builtin_patches[i].name, optarg) == 0) {
                    o_builtin_idx = i;
                    break;
                }
            }
            if (o_builtin_idx == -1) {
                fprintf(stderr, "symp: unknow patch %s\n", optarg);
                goto err;
            }
            break;
        case 'x':
            if (xbuf || o_use_builtin_patch) {
                fprintf(stderr, "symp: only one of -p/-b/-x should be offered\n");
                goto err;
            }
            xlen = 0;
            size_t buf_size = strlen(optarg) >> 1;
            xbuf = malloc(buf_size);
            memset(xbuf, 0, buf_size);
            for (int i = 0; optarg[i]; i++) {
                char ch = optarg[i];
                if (ch >= '0' && ch <= '9') xbuf[xlen>>1] |= (ch-'0') << (((xlen+1)%2)*4), ++xlen;
                else if (ch >= 'A' && ch <= 'F') xbuf[xlen>>1] |= (ch-'A'+10) << (((xlen+1)%2)*4), ++xlen;
                else if (ch >= 'a' && ch <= 'f') xbuf[xlen>>1] |= (ch-'a'+10) << (((xlen+1)%2)*4), ++xlen;
                else if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
                    fprintf(stderr, "symp: invalid character '%c' in hex string\n", ch);
                    goto err;
                }
            }
            if (xlen%2 != 0) {
                fprintf(stderr, "symp: hex string length should be oven\n");
                goto err;
            }
            xlen >>= 1;
            break;
        case 'h':
            usage();
            o_mode = USAGE_MODE;
            free(xbuf);
            return 0;
        case '?':
            goto err;
        default:
            abort();
        }
    }

    if (argc - optind != 2) {
        if (argc - optind < 2)
            fprintf(stderr, "symp: arguments not enough!\n");
        else
            fprintf(stderr, "symp: too many arguments!\n");
        goto err;
    }

    if (xbuf != NULL) {
        o_mode = PATCH_MODE;
        o_patch_data.len = xlen;
        o_patch_data.buf = xbuf;
    }
    o_symbol = argv[optind++];
    o_file = argv[optind++];
    return 0;

err:
    free(xbuf);
    return 1;
}
