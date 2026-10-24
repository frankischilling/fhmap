#include "../fhmap.h"
#include <assert.h>
#include <stdio.h>

FHMAP_DECLARE(tu_map, uint64_t, uint64_t);

int main(void)
{
    tu_map map;
    tu_map_iter it;
    const uint64_t *key;
    uint64_t *value;
    uint64_t i, sum = 0;
    size_t seen = 0;
    assert(tu_map_init_seed(&map, 32, 7));
    for (i = 0; i < 32; ++i)
        assert(tu_map_put(&map, i, i) == FHMAP_PUT_INSERTED);
    it = tu_map_iter_begin(&map);
    while (tu_map_iter_next(&map, &it, &key, &value)) {
        assert(*key == *value);
        sum += *value;
        assert(++seen <= 32);
    }
    assert(seen == 32 && sum == 496);
    assert(!tu_map_iter_next(&map, &it, NULL, NULL));
    tu_map_destroy(&map);
    puts("fhmap separate translation units passed");
    return 0;
}
