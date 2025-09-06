#include "private.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <mach-o/loader.h>

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

uint64_t trie_query(const uint8_t *export, const char *name) {
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
