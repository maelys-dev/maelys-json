/* SPDX-License-Identifier: MPL-2.0 */
/* The library alone is compiled with allocation calls redirected here. */
#include "maelys/json.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef union allocation {
    max_align_t alignment;
    size_t size;
} allocation;
static size_t live, peak, calls, fail_at;
void *maelys_test_malloc(size_t size);
void *maelys_test_calloc(size_t count, size_t size);
void *maelys_test_realloc(void *pointer, size_t size);
void maelys_test_free(void *pointer);
void *maelys_test_malloc(size_t size) {
    if (++calls == fail_at || size > SIZE_MAX - sizeof(allocation)) {
        return NULL;
    }
    allocation *a = malloc(sizeof(*a) + size);
    if (!a) {
        return NULL;
    }
    a->size = size;
    live += size;
    if (live > peak) {
        peak = live;
    }
    return a + 1;
}
void *maelys_test_calloc(size_t count, size_t size) {
    if (size && count > SIZE_MAX / size) {
        return NULL;
    }
    void *p = maelys_test_malloc(count * size);
    if (p) {
        memset(p, 0, count * size);
    }
    return p;
}
void maelys_test_free(void *pointer) {
    if (!pointer) {
        return;
    }
    allocation *a = (allocation *)pointer - 1;
    live -= a->size;
    free(a);
}
void *maelys_test_realloc(void *pointer, size_t size) {
    if (!pointer) {
        return maelys_test_malloc(size);
    }
    if (!size) {
        maelys_test_free(pointer);
        return NULL;
    }
    if (++calls == fail_at || size > SIZE_MAX - sizeof(allocation)) {
        return NULL;
    }
    allocation *a = (allocation *)pointer - 1;
    size_t old_size = a->size;
    allocation *grown = realloc(a, sizeof(*a) + size);
    if (!grown) {
        return NULL;
    }
    grown->size = size;
    live = live - old_size + size;
    if (live > peak) {
        peak = live;
    }
    return grown + 1;
}
static void require(int ok) {
    if (!ok) {
        abort();
    }
}
static void probe(
    const char *text, maelys_json_profile_t profile, size_t maximum_retained) {
    size_t bound, allocations = 0u;
    maelys_json_limits_t limits = {65535u, 256u, 65535u};
    require(maelys_json_document_parse_allocation_bound(
                strlen(text), profile, &limits, &bound) == MAELYS_JSON_OK);
    for (size_t failure = 0u; failure <= allocations; ++failure) {
        calls = live = peak = 0u;
        fail_at = failure;
        maelys_json_document_t *d = NULL;
        maelys_json_error_t why;
        maelys_json_result_t r = maelys_json_document_parse(
            text, strlen(text), profile, &limits, &d, &why);
        require(peak <= bound && why.code == r);
        if (!failure) {
            allocations = calls;
            if (maximum_retained) {
                require(r == MAELYS_JSON_OK && live < maximum_retained);
            }
        } else {
            require(r == MAELYS_JSON_ERR_MEMORY);
        }
        maelys_json_document_release(d);
        require(!live);
    }
}
int main(void) {
    const char *texts[] = {"0", "{}", "{\"x\":1,\"x\":2}",
        "{\"x\":{\"a\":1,\"a\":2},\"x\":0}",
        "{\"x\":[{\"a\":1,\"a\":null}],\"y\":false}",
        "{\"x\":true,\"x\":null,\"x\":[]}",
        "{\"x\":{\"x\":0},\"x\":{\"x\":{\"x\":1}}}"};
    for (size_t i = 0u; i < sizeof(texts) / sizeof(texts[0]); ++i) {
        for (int p = 1; p <= 3; ++p) {
            probe(texts[i], (maelys_json_profile_t)p, 0u);
        }
    }
    char many[32768];
    size_t used = 0u;
    many[used++] = '{';
    for (size_t i = 0u; i < 1000u; ++i) {
        int n = snprintf(many + used, sizeof(many) - used,
            "%s\"x\":{\"a\":%zu}", i ? "," : "", i);
        require(n > 0 && (size_t)n < sizeof(many) - used);
        used += (size_t)n;
    }
    many[used++] = '}';
    many[used] = 0;
    probe(many, MAELYS_JSON_PROFILE_RFC8259_LAST_WINS, used + 4096u);
    puts("allocation bound and every parse allocation failure: OK");
    return 0;
}
