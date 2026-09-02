#include "keyset.h"

#include <stdlib.h>
#include <string.h>

#define KEYSET_MINIMUM_CAPACITY 8u

static uint64_t hash_key(maelys_json_keyset_key_t key) {
    uint64_t hash = UINT64_C(1469598103934665603) ^ (uint64_t)key.parent;
    for (size_t i = 0u; i < key.size; ++i) {
        hash ^= (unsigned char)key.bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int keys_equal(
    maelys_json_keyset_key_t a, maelys_json_keyset_key_t b) {
    return a.parent == b.parent && a.size == b.size &&
        (a.size == 0u || memcmp(a.bytes, b.bytes, a.size) == 0);
}

/* Returns the slot holding `key`, or the empty slot where it belongs. */
static size_t find_slot(
    const maelys_json_keyset_t *set, const size_t *slots, size_t capacity,
    maelys_json_keyset_key_t key, int *out_found) {
    size_t mask = capacity - 1u;
    size_t slot = (size_t)(hash_key(key) & mask);
    for (;;) {
        size_t stored = slots[slot];
        if (stored == MAELYS_JSON_VALUE_NONE) {
            *out_found = 0;
            return slot;
        }
        if (keys_equal(set->key_of(set->context, stored), key)) {
            *out_found = 1;
            return slot;
        }
        slot = (slot + 1u) & mask;
    }
}

static size_t *allocate_slots(size_t capacity) {
    size_t *slots = malloc(capacity * sizeof(*slots));
    if (!slots) {
        return NULL;
    }
    for (size_t i = 0u; i < capacity; ++i) {
        slots[i] = MAELYS_JSON_VALUE_NONE;
    }
    return slots;
}

maelys_json_result_t maelys_json_keyset_init(
    maelys_json_keyset_t *set, size_t expected,
    maelys_json_keyset_key_fn key_of, const void *context) {
    MAELYS_JSON_ASSERT(set && key_of && expected <= SIZE_MAX / 4u);
    size_t capacity = KEYSET_MINIMUM_CAPACITY;
    while (capacity < expected * 2u) {
        capacity <<= 1u;
    }
    *set = (maelys_json_keyset_t){
        .capacity = capacity, .key_of = key_of, .context = context
    };
    set->slots = allocate_slots(capacity);
    return set->slots ? MAELYS_JSON_OK : MAELYS_JSON_ERR_MEMORY;
}

void maelys_json_keyset_release(maelys_json_keyset_t *set) {
    if (!set) {
        return;
    }
    free(set->slots);
    *set = (maelys_json_keyset_t){0};
}

int maelys_json_keyset_contains(
    const maelys_json_keyset_t *set, maelys_json_keyset_key_t key) {
    MAELYS_JSON_ASSERT(set && set->slots);
    int found;
    (void)find_slot(set, set->slots, set->capacity, key, &found);
    return found;
}

static maelys_json_result_t grow(maelys_json_keyset_t *set) {
    if (set->capacity > SIZE_MAX / 2u / sizeof(size_t)) {
        return MAELYS_JSON_ERR_MEMORY;
    }
    size_t capacity = set->capacity * 2u;
    size_t *slots = allocate_slots(capacity);
    if (!slots) {
        return MAELYS_JSON_ERR_MEMORY;
    }
    for (size_t i = 0u; i < set->capacity; ++i) {
        size_t stored = set->slots[i];
        if (stored == MAELYS_JSON_VALUE_NONE) {
            continue;
        }
        int found;
        size_t slot = find_slot(set, slots, capacity,
            set->key_of(set->context, stored), &found);
        MAELYS_JSON_ASSERT(!found);
        slots[slot] = stored;
    }
    free(set->slots);
    set->slots = slots;
    set->capacity = capacity;
    return MAELYS_JSON_OK;
}

maelys_json_result_t maelys_json_keyset_insert(
    maelys_json_keyset_t *set, size_t index) {
    MAELYS_JSON_ASSERT(set && set->slots && index != MAELYS_JSON_VALUE_NONE);
    maelys_json_keyset_key_t key = set->key_of(set->context, index);
    int found;
    size_t slot = find_slot(set, set->slots, set->capacity, key, &found);
    if (found) {
        return MAELYS_JSON_ERR_DUPLICATE_KEY;
    }
    if ((set->count + 1u) * 2u > set->capacity) {
        maelys_json_result_t result = grow(set);
        if (result != MAELYS_JSON_OK) {
            return result;
        }
        slot = find_slot(set, set->slots, set->capacity, key, &found);
        MAELYS_JSON_ASSERT(!found);
    }
    set->slots[slot] = index;
    ++set->count;
    return MAELYS_JSON_OK;
}
