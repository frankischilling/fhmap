# fhmap API reference

## Including the header

`fhmap.h` contains declarations, typed wrappers, and an implementation section.
Define `FHMAP_IMPLEMENTATION` in exactly one C file before including the header.
The library requires a C11 compiler on Linux.

The functions listed here form the public raw API. Names beginning with
`fhmap__` are implementation details.

`FHMAP_VERSION_MAJOR`, `FHMAP_VERSION_MINOR`, `FHMAP_VERSION_PATCH`, and
`FHMAP_VERSION_STRING` expose the header version at compile time.

## Types

### `fhmap`

The table object. It can live on the stack, in static storage, or inside another
object. Its fields are exposed so C can allocate it without an opaque heap
handle, but applications must treat the fields as read-only.

Initialize with `fhmap_init` and release with `fhmap_destroy`. Copying an
initialized `fhmap` by value creates two aliases to the same allocation and is
therefore invalid.

### `fhmap_config`

| Field | Meaning |
|---|---|
| `key_size` | Fixed key size in bytes; must be nonzero |
| `value_size` | Fixed value size in bytes; must be nonzero |
| `initial_capacity` | Minimum number of elements the initial table should hold |
| `key_align` | Power-of-two key alignment |
| `value_align` | Power-of-two value alignment |
| `seed` | Per-map seed passed to the hash function |
| `hash` | Hash callback; `NULL` selects `fhmap_hash_bytes` |
| `equal` | Equality callback; `NULL` selects byte equality |
| `user` | Context passed to callbacks and allocator macros |

Start with `fhmap_config_make`; do not leave alignment fields at zero.

### `fhmap_put_result`

| Value | Meaning |
|---|---|
| `FHMAP_PUT_ERROR` | Invalid argument or allocation failure |
| `FHMAP_PUT_UPDATED` | An existing key's value was replaced |
| `FHMAP_PUT_INSERTED` | A new key/value pair was inserted |

### `fhmap_iter`

Iterator state returned by `fhmap_iter_begin`. It records the table generation
and caches the occupied slots in one control-byte group. `fhmap_iter_next`
returns `false` if a structural mutation invalidated it. Use the iteration
functions rather than editing its fields. Copying a valid iterator by value
creates an independent cursor over the same unchanged map.

Version 2 changes this structure's layout. Rebuild the implementation and all
callers against the same header; old compiled callers are not binary compatible.
On the measured 64-bit build, the structure grows from 16 to 24 bytes. The
`fhmap` table object and its allocations are unchanged.

## Configuration and lifetime

### `fhmap_config fhmap_config_make(size_t key_size, size_t value_size)`

Returns a valid baseline configuration using byte hashing/equality, byte
alignment, a randomized seed, and zero requested elements. Override alignment
when returned key/value pointers will be dereferenced as typed objects.

### `bool fhmap_init(fhmap *map, const fhmap_config *config)`

Initializes `map`. Returns `false` for an invalid configuration, arithmetic
overflow, or allocation failure. With non-NULL arguments, it zeroes `map`
before validating the configuration, so a failed initialization needs no
destruction. A NULL `config` leaves `map` untouched. Destroy a live map before
initializing it again; otherwise its old allocation is leaked.

Alignments must be powers of two no greater than `FHMAP_MAX_ALIGNMENT`.

### `void fhmap_destroy(fhmap *map)`

Releases the table allocation and zeroes the object. Accepts `NULL` and a
zero-initialized map. It does not call destructors or free resources referenced
by key/value bytes.

### `void fhmap_clear(fhmap *map)`

Removes all elements without changing capacity or allocation size. Invalidates
all iterators and returned pointers.

### `bool fhmap_reserve(fhmap *map, size_t element_capacity)`

Ensures the map can contain at least `element_capacity` items without a capacity
growth. Returns `false` only for an invalid map, overflow, or allocation failure.
It never shrinks. A successful reallocation invalidates pointers and iterators.

### `bool fhmap_compact(fhmap *map)`

Rehashes into the smallest supported capacity for the current size and removes
all tombstones. Returns `false` if allocation fails. It is a no-op when the map
is already compact. A rehash invalidates pointers and iterators.

## Lookup and mutation

### `void *fhmap_get(fhmap *map, const void *key)`

### `const void *fhmap_get_const(const fhmap *map, const void *key)`

Return the value storage for `key`, or `NULL` if absent/invalid. Expected
lookup complexity is O(1). The pointer remains valid until a structural
mutation; the non-const form permits in-place value updates.

### `bool fhmap_contains(const fhmap *map, const void *key)`

Returns whether `key` exists.

### `void *fhmap_emplace(fhmap *map, const void *key, bool *inserted)`

Returns value storage for `key`, inserting the key if needed. When the key is
new, the value bytes are uninitialized and `*inserted` is set to
`true`. For an existing key they are unchanged and `*inserted` is `false`.
`inserted` may be `NULL`. A `NULL` return indicates invalid input or allocation
failure.

Use this to initialize a large value directly in its table slot.

### `fhmap_put_result fhmap_put(fhmap *map, const void *key, const void *value)`

Inserts or replaces a key/value pair and reports which occurred. It copies
exactly `key_size` bytes for a new key and `value_size` bytes for the value.

### `bool fhmap_remove(fhmap *map, const void *key)`

Removes `key`. Returns `true` if it existed. Removal leaves a tombstone so probe
chains remain valid; later insertions reuse tombstones and periodically rebuild
a tombstone-heavy table.

### `bool fhmap_remove_copy(fhmap *map, const void *key, void *value_out)`

Like `fhmap_remove`, but copies the removed value into `value_out` before
removal. `value_out` may be `NULL`, in which case this is equivalent to
`fhmap_remove`.

## Observation

### `size_t fhmap_size(const fhmap *map)`

Number of live entries.

### `size_t fhmap_capacity(const fhmap *map)`

Number of raw buckets. New tables have at least 16 buckets and grow in powers
of two. The maximum live population before growth is 87.5% of this value.

### `size_t fhmap_memory_usage(const fhmap *map)`

Bytes in the allocation owned directly by the table. This excludes the `fhmap`
object and any external memory referenced by pointer-like keys or values.

### `double fhmap_load_factor(const fhmap *map)`

`size / capacity`, or zero for an invalid/empty-capacity map.

## Iteration

```c
fhmap_iter fhmap_iter_begin(const fhmap *map);

bool fhmap_iter_next(fhmap *map,
                     fhmap_iter *iter,
                     const void **key_out,
                     void **value_out);
```

Begin an iterator, then call `next` until it returns `false`. Either output
pointer may be `NULL`. Iteration order is unspecified and changes after a
rehash, seed change, or different insertion history. Editing the bytes of a
value through `value_out` is allowed. Do not edit a key.

Inserting a new key, successful removal, clear, and rehash invalidate the
iterator. Reserve and compact invalidate it when they rehash. Replacing an
existing value through `fhmap_put` leaves the iterator valid. Do not use an
iterator after destroying its map.

## Hash and equality callbacks

```c
typedef uint64_t (*fhmap_hash_fn)(const void *key,
                                  size_t key_size,
                                  uint64_t seed,
                                  void *user);

typedef bool (*fhmap_equal_fn)(const void *stored_key,
                               const void *query_key,
                               size_t key_size,
                               void *user);
```

The hash must be stable while a key is stored. If equality says two keys are
equal, their hashes must be equal for the same seed. `fhmap` stores the complete
key bytes supplied on first insertion even when custom equality ignores part of
the object.

`fhmap_hash_bytes` hashes all key bytes. Default equality compares all key
bytes, with specialized 4-, 8-, and 16-byte paths.

`fhmap_hash_cstr` and `fhmap_equal_cstr` treat the key object as a stored
`const char *` and operate on pointed-to string contents. The string remains
owned by the caller.

## Typed wrappers

At file scope:

```c
FHMAP_DECLARE(NAME, KEY_TYPE, VALUE_TYPE);
```

This defines:

```text
NAME
NAME_iter
NAME_init / NAME_init_seed / NAME_destroy / NAME_clear
NAME_reserve / NAME_compact
NAME_size / NAME_capacity / NAME_memory_usage
NAME_get / NAME_get_const / NAME_contains
NAME_emplace / NAME_put / NAME_remove / NAME_remove_copy
NAME_iter_begin / NAME_iter_next
```

The wrappers pass keys and values by value. For unusually large
key types, the raw pointer API can avoid the wrapper's temporary copy.

The macro uses byte hash/equality. Struct keys must have deterministic padding;
zero-initialize them or configure raw callbacks that compare only meaningful
fields. A pointer key is compared by address unless the raw API installs a
different callback.
