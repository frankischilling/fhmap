#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static size_t calls, live;
static int fail_next;
static void *checked_alloc(size_t size)
{
    void *p;
    ++calls;
    if (fail_next) { fail_next = 0; return NULL; }
    p = malloc(size);
    if (p) ++live;
    return p;
}
static void checked_free(void *p)
{
    if (p) { assert(live != 0); --live; free(p); }
}
#define FHMAP_MALLOC(user, size) checked_alloc(size)
#define FHMAP_FREE(user, pointer) checked_free(pointer)
#define FHMAP_IMPLEMENTATION
#include "../fhmap.h"

int main(void)
{
    fhmap map;
    fhmap_config cfg = fhmap_config_make(sizeof(uint64_t), sizeof(uint64_t));
    uint64_t key, value = 1234;
    size_t before, visited;
    fhmap_iter it;
    cfg.seed = 123;
    cfg.initial_capacity = 14;
    assert(fhmap_init(&map, &cfg));
    for (key = 0; key < 14; ++key)
        assert(fhmap_put(&map, &key, &key) == FHMAP_PUT_INSERTED);
    before = calls;
    key = 1;
    assert(fhmap_put(&map, &key, &value) == FHMAP_PUT_UPDATED);
    assert(calls == before);
    it = fhmap_iter_begin(&map);
    assert(fhmap_iter_next(&map, &it, NULL, NULL));
    visited = 1;
    key = 14; fail_next = 1;
    assert(fhmap_put(&map, &key, &value) == FHMAP_PUT_ERROR);
    assert(fhmap_size(&map) == 14 && fhmap_capacity(&map) == 16);
    while (fhmap_iter_next(&map, &it, NULL, NULL)) ++visited;
    assert(visited == 14);
    for (key = 0; key < 14; ++key)
        assert(*(uint64_t *)fhmap_get(&map, &key) == (key == 1 ? value : key));
    for (key = 0; key < 8; ++key) assert(fhmap_remove(&map, &key));
    key = 20; fail_next = 1;
    assert(fhmap_put(&map, &key, &value) == FHMAP_PUT_ERROR);
    assert(map.size == 6 && map.tombstones == 8);
    before = calls;
    key = 10;
    assert(fhmap_put(&map, &key, &value) == FHMAP_PUT_UPDATED);
    assert(calls == before && map.size == 6 && map.tombstones == 8);
    key = 20;
    assert(fhmap_put(&map, &key, &value) == FHMAP_PUT_INSERTED);
    assert(map.size == 7 && map.tombstones == 0);
    for (key = 8; key < 14; ++key)
        assert(*(uint64_t *)fhmap_get(&map, &key) == (key == 10 ? value : key));
    fhmap_destroy(&map);
    assert(live == 0);
    puts("fhmap allocation rollback tests passed");
    return 0;
}
